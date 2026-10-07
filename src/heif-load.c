/* gimp-raw-heic -- HEIF/HEIC/AVIF: HDR (PQ / HLG) loading and thumbnails
 * Copyright (C) 2026 Marek Zettel
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * GIMP's own file-heif loader already handles SDR images well (8/10/12 bit,
 * alpha, ICC/nclx, image selection, orientation, metadata).  It does not
 * handle the HDR transfer functions PQ (SMPTE ST 2084) and HLG (ARIB
 * STD-B67): such images get an sRGB tone curve and look washed out.
 *
 * This loader therefore decodes HDR images itself (to scene-linear 32-bit
 * float, Rec. 2100 primaries, SDR reference white = 1.0) and hands every
 * other file to GIMP's built-in procedure through the PDB.
 */

#include "config.h"

#include <math.h>
#include <string.h>

#include <gexiv2/gexiv2.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libheif/heif.h>

#include "rawheic.h"


enum
{
  HDR_LINEAR,   /* scene-linear float, values above 1.0 kept */
  HDR_SDR,      /* tone-mapped to SDR, 16-bit sRGB           */
  HDR_BUILTIN   /* hand everything to GIMP's file-heif       */
};

/* BT.2408: SDR reference white is 203 cd/m² */
#define REFERENCE_WHITE_NITS 203.0


static GimpValueArray * heif_load_run  (GimpProcedure         *procedure,
                                        GimpRunMode            run_mode,
                                        GFile                 *file,
                                        GimpMetadata          *metadata,
                                        GimpMetadataLoadFlags *flags,
                                        GimpProcedureConfig   *config,
                                        gpointer               run_data);
static GimpValueArray * heif_thumb_run (GimpProcedure         *procedure,
                                        GFile                 *file,
                                        gint                   size,
                                        GimpProcedureConfig   *config,
                                        gpointer               run_data);


GimpProcedure *
heif_load_procedure_new (GimpPlugIn  *plug_in,
                         const gchar *name)
{
  GimpProcedure *procedure;

  if (! strcmp (name, HEIF_THUMB_PROC))
    {
      procedure = gimp_thumbnail_procedure_new (plug_in, name,
                                                GIMP_PDB_PROC_TYPE_PLUGIN,
                                                heif_thumb_run, NULL, NULL);
      gimp_procedure_set_documentation (procedure,
                                        _("Load the embedded thumbnail of a HEIF/AVIF file"),
                                        _("Fast thumbnail from the preview stored in HEIF files."),
                                        name);
      gimp_procedure_set_attribution (procedure, "Marek Zettel", "Marek Zettel", "2026");
      return procedure;
    }

  procedure = gimp_load_procedure_new (plug_in, name,
                                       GIMP_PDB_PROC_TYPE_PLUGIN,
                                       heif_load_run, NULL, NULL);

  gimp_procedure_set_menu_label (procedure, _("HEIF/HEIC/AVIF with HDR (PQ/HLG)"));
  gimp_procedure_set_documentation (procedure,
                                    _("Load HEIF/HEIC/AVIF images, including HDR"),
                                    _("Decodes HDR images (PQ and HLG transfer) into "
                                      "scene-linear floating point; all other images "
                                      "are loaded by GIMP's built-in HEIF loader."),
                                    name);
  gimp_procedure_set_attribution (procedure, "Marek Zettel", "Marek Zettel", "2026");

  gimp_file_procedure_set_format_name (GIMP_FILE_PROCEDURE (procedure), "HEIF");
  gimp_file_procedure_set_mime_types  (GIMP_FILE_PROCEDURE (procedure),
                                       "image/heif,image/heic,image/avif");
  /* magic-less on purpose, see raw-load.c: wins over file-heif by extension */
  gimp_file_procedure_set_extensions  (GIMP_FILE_PROCEDURE (procedure),
                                       "heic,heif,hif,avif");
  gimp_load_procedure_set_thumbnail_loader (GIMP_LOAD_PROCEDURE (procedure),
                                            HEIF_THUMB_PROC);

  gimp_procedure_add_choice_argument (procedure, "hdr-mode",
                                      _("_HDR images"),
                                      _("What to do with HDR (PQ/HLG) images"),
                                      gimp_choice_new_with_values (
                                        "linear",  HDR_LINEAR,
                                        _("Scene-linear 32-bit float (keep HDR)"), NULL,
                                        "sdr",     HDR_SDR,
                                        _("Tone-map to SDR (16-bit sRGB)"), NULL,
                                        "builtin", HDR_BUILTIN,
                                        _("Use GIMP's built-in loader for all files"), NULL,
                                        NULL),
                                      "linear", G_PARAM_READWRITE);

  return procedure;
}

/* ---------------------------------------------------------------------- */

static const gchar *
builtin_proc_for (GFile *file)
{
  gchar       *base  = g_file_get_basename (file);
  gchar       *lower = base ? g_ascii_strdown (base, -1) : NULL;
  gboolean     avif  = lower && g_str_has_suffix (lower, ".avif");
  const gchar *proc  = avif ? "file-heif-av1-load" : "file-heif-load";

  g_free (lower);
  g_free (base);

  return proc;
}

static GimpImage *
load_builtin (GimpRunMode   run_mode,
              GFile        *file,
              GimpPDBStatusType *status,
              GError      **error)
{
  GimpProcedure       *proc;
  GimpProcedureConfig *config;
  GimpValueArray      *ret;
  GimpImage           *image = NULL;

  proc = gimp_pdb_lookup_procedure (gimp_get_pdb (), builtin_proc_for (file));
  if (! proc)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   _("GIMP's built-in HEIF loader is not available."));
      *status = GIMP_PDB_EXECUTION_ERROR;
      return NULL;
    }

  config = gimp_procedure_create_config (proc);
  g_object_set (config, "run-mode", run_mode, "file", file, NULL);
  ret = gimp_procedure_run_config (proc, config);
  g_object_unref (config);

  *status = GIMP_VALUES_GET_ENUM (ret, 0);
  if (*status == GIMP_PDB_SUCCESS)
    image = GIMP_VALUES_GET_IMAGE (ret, 1);
  else if (*status != GIMP_PDB_CANCEL)
    g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s",
                 gimp_pdb_get_last_error (gimp_get_pdb ()));

  gimp_value_array_unref (ret);

  return image;
}

typedef struct
{
  GMappedFile          *mapped;
  struct heif_context  *ctx;
  struct heif_image_handle *handle;
} HeifFile;

static void
heif_file_close (HeifFile *hf)
{
  if (hf->handle)
    heif_image_handle_release (hf->handle);
  if (hf->ctx)
    heif_context_free (hf->ctx);
  if (hf->mapped)
    g_mapped_file_unref (hf->mapped);
  memset (hf, 0, sizeof (*hf));
}

/* Paths are read through GLib so non-ASCII Windows paths work. */
static gboolean
heif_file_open (HeifFile    *hf,
                GFile       *file,
                GError     **error)
{
  gchar             *path = g_file_get_path (file);
  struct heif_error  err;

  memset (hf, 0, sizeof (*hf));

  if (! path)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   _("HEIF files can only be opened from a local disk."));
      return FALSE;
    }

  hf->mapped = g_mapped_file_new (path, FALSE, error);
  g_free (path);
  if (! hf->mapped)
    return FALSE;

  hf->ctx = heif_context_alloc ();
  err = heif_context_read_from_memory_without_copy (hf->ctx,
                                                    g_mapped_file_get_contents (hf->mapped),
                                                    g_mapped_file_get_length (hf->mapped),
                                                    NULL);
  if (err.code == heif_error_Ok)
    err = heif_context_get_primary_image_handle (hf->ctx, &hf->handle);

  if (err.code != heif_error_Ok)
    {
      gchar *name = g_file_get_parse_name (file);

      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                   _("Could not read HEIF file '%s': %s"), name, err.message);
      g_free (name);
      heif_file_close (hf);
      return FALSE;
    }

  return TRUE;
}

/* 0 = SDR, else the heif transfer characteristic */
static gint
hdr_transfer (struct heif_image_handle *handle)
{
  struct heif_color_profile_nclx *nclx = NULL;
  gint                            tc   = 0;

  if (heif_image_handle_get_nclx_color_profile (handle, &nclx).code == heif_error_Ok && nclx)
    {
      if (nclx->transfer_characteristics == heif_transfer_characteristic_ITU_R_BT_2100_0_PQ ||
          nclx->transfer_characteristics == heif_transfer_characteristic_ITU_R_BT_2100_0_HLG)
        tc = nclx->transfer_characteristics;

      heif_nclx_color_profile_free (nclx);
    }

  return tc;
}

static inline gfloat
pq_to_linear (gfloat e)
{
  const gdouble m1 = 2610.0 / 16384.0;
  const gdouble m2 = 2523.0 / 4096.0 * 128.0;
  const gdouble c1 = 3424.0 / 4096.0;
  const gdouble c2 = 2413.0 / 4096.0 * 32.0;
  const gdouble c3 = 2392.0 / 4096.0 * 32.0;
  gdouble       p  = pow (MAX (e, 0.0f), 1.0 / m2);
  gdouble       l  = pow (MAX (p - c1, 0.0) / (c2 - c3 * p), 1.0 / m1);

  return (gfloat) (l * 10000.0 / REFERENCE_WHITE_NITS);
}

static inline gfloat
hlg_to_linear (gfloat e)
{
  const gdouble a = 0.17883277, b = 0.28466892, c = 0.55991073;
  /* scene light of the HLG reference white (75% signal) */
  const gdouble ref = (exp ((0.75 - c) / a) + b) / 12.0;
  gdouble       E;

  e = MAX (e, 0.0f);
  E = (e <= 0.5) ? (e * e / 3.0) : ((exp ((e - c) / a) + b) / 12.0);

  return (gfloat) (E / ref);
}

/* luminance-based extended Reinhard, white point 4x reference white */
static void
tone_map_sdr (gfloat *px,
              gsize   n,
              gint    channels)
{
  const gfloat W2 = 16.0f;
  gsize        i;

  for (i = 0; i < n; i++)
    {
      gfloat *p = px + i * channels;
      /* Rec. 2020 luminance */
      gfloat  L = 0.2627f * p[0] + 0.6780f * p[1] + 0.0593f * p[2];

      if (L > 1e-6f)
        {
          gfloat Lt = L * (1.0f + L / W2) / (1.0f + L);
          gfloat s  = Lt / L;

          p[0] *= s; p[1] *= s; p[2] *= s;
        }
    }
}

static GimpImage *
load_hdr (HeifFile  *hf,
          GFile     *file,
          gint       tc,
          gint       mode,
          GError   **error)
{
  struct heif_decoding_options *opts;
  struct heif_image            *img = NULL;
  struct heif_error             err;
  const guint8                 *plane;
  size_t                        stride = 0;
  gboolean                      alpha;
  gint                          w, h, bits, channels, x, y;
  gfloat                       *buf;
  gfloat                        maxv;
  GimpImage                    *image;
  GimpLayer                    *layer;
  GimpColorProfile             *profile;
  GeglBuffer                   *gbuf;
  const Babl                   *fmt;
  gchar                        *name;

  alpha    = heif_image_handle_has_alpha_channel (hf->handle);
  channels = alpha ? 4 : 3;

  opts = heif_decoding_options_alloc ();
  err  = heif_decode_image (hf->handle, &img, heif_colorspace_RGB,
                            alpha ? heif_chroma_interleaved_RRGGBBAA_LE
                                  : heif_chroma_interleaved_RRGGBB_LE,
                            opts);
  heif_decoding_options_free (opts);

  if (err.code != heif_error_Ok)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   _("Decoding the HEIF image failed: %s"), err.message);
      return NULL;
    }

  w     = heif_image_get_primary_width (img);
  h     = heif_image_get_primary_height (img);
  bits  = heif_image_get_bits_per_pixel_range (img, heif_channel_interleaved);
  plane = heif_image_get_plane_readonly2 (img, heif_channel_interleaved, &stride);

  if (! plane || w < 1 || h < 1 || w > MAX_DIMENSION || h > MAX_DIMENSION ||
      (gint64) w * h > MAX_PIXELS || bits < 8 || bits > 16 ||
      stride < (size_t) w * channels * 2)
    {
      heif_image_release (img);
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                   _("The HEIF image has an unsupported layout."));
      return NULL;
    }

  buf = g_try_malloc ((gsize) w * h * channels * sizeof (gfloat));
  if (! buf)
    {
      heif_image_release (img);
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOMEM,
                   _("Not enough memory to load the image."));
      return NULL;
    }

  maxv = (gfloat) ((1 << bits) - 1);

#pragma omp parallel for private (x)
  for (y = 0; y < h; y++)
    {
      const guint16 *src = (const guint16 *) (plane + (gsize) y * stride);
      gfloat        *dst = buf + (gsize) y * w * channels;

      for (x = 0; x < w * channels; x++)
        {
          gfloat e = GUINT16_FROM_LE (src[x]) / maxv;

          if (alpha && x % 4 == 3)
            dst[x] = e;
          else
            dst[x] = (tc == heif_transfer_characteristic_ITU_R_BT_2100_0_PQ) ?
                     pq_to_linear (e) : hlg_to_linear (e);
        }
    }

  heif_image_release (img);

  if (mode == HDR_SDR)
    tone_map_sdr (buf, (gsize) w * h, channels);

  image   = gimp_image_new_with_precision (w, h, GIMP_RGB, GIMP_PRECISION_FLOAT_LINEAR);
  profile = rawheic_profile_rec2100_linear ();
  gimp_image_set_color_profile (image, profile);

  name  = g_file_get_basename (file);
  layer = gimp_layer_new (image, name, w, h,
                          alpha ? GIMP_RGBA_IMAGE : GIMP_RGB_IMAGE, 100.0,
                          gimp_image_get_default_new_layer_mode (image));
  g_free (name);
  gimp_image_insert_layer (image, layer, NULL, 0);

  fmt  = gimp_color_profile_get_format (profile,
                                        babl_format (alpha ? "RGBA float" : "RGB float"),
                                        GIMP_COLOR_RENDERING_INTENT_RELATIVE_COLORIMETRIC,
                                        NULL);
  gbuf = gimp_drawable_get_buffer (GIMP_DRAWABLE (layer));
  gegl_buffer_set (gbuf, GEGL_RECTANGLE (0, 0, w, h), 0, fmt, buf, GEGL_AUTO_ROWSTRIDE);
  g_object_unref (gbuf);
  g_object_unref (profile);
  g_free (buf);

  if (mode == HDR_SDR)
    {
      GimpColorProfile *srgb = gimp_color_profile_new_rgb_srgb ();

      gimp_image_convert_color_profile (image, srgb,
                                        GIMP_COLOR_RENDERING_INTENT_PERCEPTUAL, TRUE);
      gimp_image_convert_precision (image, GIMP_PRECISION_U16_NON_LINEAR);
      g_object_unref (srgb);
    }

  return image;
}

static GimpValueArray *
heif_load_run (GimpProcedure         *procedure,
               GimpRunMode            run_mode,
               GFile                 *file,
               GimpMetadata          *metadata,
               GimpMetadataLoadFlags *flags,
               GimpProcedureConfig   *config,
               gpointer               run_data)
{
  GimpValueArray    *return_vals;
  GimpPDBStatusType  status = GIMP_PDB_SUCCESS;
  GimpImage         *image  = NULL;
  GError            *error  = NULL;
  HeifFile           hf;
  gint               mode;
  gint               tc = 0;

  mode = gimp_procedure_config_get_choice_id (config, "hdr-mode");

  if (mode != HDR_BUILTIN)
    {
      if (! heif_file_open (&hf, file, &error))
        return gimp_procedure_new_return_values (procedure,
                                                 GIMP_PDB_EXECUTION_ERROR, error);

      tc = hdr_transfer (hf.handle);

      if (tc)
        {
          gchar *name = g_file_get_basename (file);

          gimp_progress_init_printf (_("Loading HDR image '%s'"), name);
          g_free (name);

          image = load_hdr (&hf, file, tc, mode, &error);
          if (! image)
            status = GIMP_PDB_EXECUTION_ERROR;

          gimp_progress_update (1.0);
        }

      heif_file_close (&hf);
    }

  if (! tc)
    {
      /* SDR (or user's choice): GIMP's own loader does it fully; it
       * already applied orientation, profile and metadata itself.
       */
      image  = load_builtin (run_mode, file, &status, &error);
      *flags = GIMP_METADATA_LOAD_NONE;
    }
  else
    {
      /* libheif applied irot/imir, and we set the profile */
      *flags &= ~(GIMP_METADATA_LOAD_ORIENTATION | GIMP_METADATA_LOAD_COLORSPACE);

      if (metadata)
        gexiv2_metadata_try_set_orientation (GEXIV2_METADATA (metadata),
                                             GEXIV2_ORIENTATION_NORMAL, NULL);
    }

  if (! image)
    return gimp_procedure_new_return_values (procedure, status, error);

  return_vals = gimp_procedure_new_return_values (procedure, GIMP_PDB_SUCCESS, NULL);
  GIMP_VALUES_SET_IMAGE (return_vals, 1, image);

  return return_vals;
}

/* ---------------------------------------------------------------------- */

/* Many phones (e.g. realme/OPPO) store no HEIF thumbnail item but put a
 * JPEG thumbnail into EXIF.  Returns it decoded, or NULL.  It is only used
 * when its orientation matches the (transformed) primary image, because the
 * EXIF thumbnail does not know about HEIF irot/imir transformations.
 */
static GdkPixbuf *
exif_thumbnail (struct heif_image_handle *handle)
{
  heif_item_id     ids[4];
  GdkPixbuf       *pixbuf = NULL;
  gint             n, i;

  n = heif_image_handle_get_list_of_metadata_block_IDs (handle, "Exif", ids, 4);

  for (i = 0; i < n && ! pixbuf; i++)
    {
      gsize           size = heif_image_handle_get_metadata_size (handle, ids[i]);
      guint8         *buf, *app1, *jpeg = NULL;
      guint32         offset;
      gsize           tiff_len;
      gint            jpeg_len = 0;
      GExiv2Metadata *meta;

      if (size < 16 || size > 64 * 1024 * 1024)
        continue;

      buf = g_malloc (size);
      if (heif_image_handle_get_metadata (handle, ids[i], buf).code != heif_error_Ok)
        {
          g_free (buf);
          continue;
        }

      /* HEIF Exif item: 4-byte big-endian offset to the TIFF header */
      offset = ((guint32) buf[0] << 24) | ((guint32) buf[1] << 16) |
               ((guint32) buf[2] << 8)  |  (guint32) buf[3];
      if ((gsize) offset > size - 4 - 8)
        {
          g_free (buf);
          continue;
        }
      tiff_len = size - 4 - offset;

      app1 = g_malloc (6 + tiff_len);
      memcpy (app1, "Exif\0\0", 6);
      memcpy (app1 + 6, buf + 4 + offset, tiff_len);
      g_free (buf);

      meta = gexiv2_metadata_new ();
      if (gexiv2_metadata_from_app1_segment (meta, app1, (glong) (6 + tiff_len), NULL) &&
          gexiv2_metadata_get_exif_thumbnail (meta, &jpeg, &jpeg_len) &&
          jpeg && jpeg_len > 0)
        {
          GdkPixbufLoader *loader = gdk_pixbuf_loader_new_with_type ("jpeg", NULL);

          if (loader)
            {
              if (gdk_pixbuf_loader_write (loader, jpeg, jpeg_len, NULL) &&
                  gdk_pixbuf_loader_close (loader, NULL))
                {
                  pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
                  if (pixbuf)
                    g_object_ref (pixbuf);
                }
              else
                {
                  gdk_pixbuf_loader_close (loader, NULL);
                }
              g_object_unref (loader);
            }
        }

      g_free (jpeg);
      g_free (app1);
      g_object_unref (meta);
    }

  if (pixbuf)
    {
      gint     tw = gdk_pixbuf_get_width (pixbuf);
      gint     th = gdk_pixbuf_get_height (pixbuf);
      gint     iw = heif_image_handle_get_width (handle);
      gint     ih = heif_image_handle_get_height (handle);
      gboolean portrait_t = th > tw;
      gboolean portrait_i = ih > iw;

      if (portrait_t != portrait_i || MAX (tw, th) < 96)
        g_clear_object (&pixbuf);
    }

  return pixbuf;
}

static GimpValueArray *
heif_thumb_run (GimpProcedure       *procedure,
                GFile               *file,
                gint                 size,
                GimpProcedureConfig *config,
                gpointer             run_data)
{
  GimpValueArray           *return_vals;
  HeifFile                  hf;
  struct heif_image_handle *thumb = NULL;
  struct heif_image        *img   = NULL;
  struct heif_error         err;
  heif_item_id              id;
  const guint8             *plane;
  size_t                    stride;
  GError                   *error = NULL;
  GimpImage                *image;
  GimpLayer                *layer;
  GeglBuffer               *gbuf;
  gint                      full_w, full_h, w, h, y;
  gboolean                  alpha;
  guint8                   *rows;

  if (! heif_file_open (&hf, file, &error))
    return gimp_procedure_new_return_values (procedure,
                                             GIMP_PDB_EXECUTION_ERROR, error);

  full_w = heif_image_handle_get_width (hf.handle);
  full_h = heif_image_handle_get_height (hf.handle);

  /* 1. HEIF thumbnail item, 2. EXIF JPEG thumbnail, 3. decode the image */
  if (heif_image_handle_get_list_of_thumbnail_IDs (hf.handle, &id, 1) == 1 &&
      heif_image_handle_get_thumbnail (hf.handle, id, &thumb).code == heif_error_Ok)
    ;
  else
    thumb = NULL;

  if (! thumb)
    {
      GdkPixbuf *pixbuf = exif_thumbnail (hf.handle);

      if (pixbuf)
        {
          heif_file_close (&hf);

          image = gimp_image_new (gdk_pixbuf_get_width (pixbuf),
                                  gdk_pixbuf_get_height (pixbuf), GIMP_RGB);
          layer = gimp_layer_new_from_pixbuf (image, _("Preview"), pixbuf, 100.0,
                                              GIMP_LAYER_MODE_NORMAL, 0.0, 1.0);
          gimp_image_insert_layer (image, layer, NULL, 0);
          g_object_unref (pixbuf);

          return_vals = gimp_procedure_new_return_values (procedure,
                                                          GIMP_PDB_SUCCESS, NULL);
          GIMP_VALUES_SET_IMAGE (return_vals, 1, image);
          GIMP_VALUES_SET_INT   (return_vals, 2, full_w);
          GIMP_VALUES_SET_INT   (return_vals, 3, full_h);
          GIMP_VALUES_SET_ENUM  (return_vals, 4, GIMP_RGB_IMAGE);
          GIMP_VALUES_SET_INT   (return_vals, 5, 1);
          gimp_value_array_truncate (return_vals, 6);

          return return_vals;
        }
    }

  alpha = heif_image_handle_has_alpha_channel (thumb ? thumb : hf.handle);
  err   = heif_decode_image (thumb ? thumb : hf.handle, &img, heif_colorspace_RGB,
                             alpha ? heif_chroma_interleaved_RGBA
                                   : heif_chroma_interleaved_RGB, NULL);
  if (thumb)
    heif_image_handle_release (thumb);

  if (err.code != heif_error_Ok)
    {
      heif_file_close (&hf);
      g_set_error (&error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s", err.message);
      return gimp_procedure_new_return_values (procedure,
                                               GIMP_PDB_EXECUTION_ERROR, error);
    }

  w     = heif_image_get_primary_width (img);
  h     = heif_image_get_primary_height (img);
  plane = heif_image_get_plane_readonly2 (img, heif_channel_interleaved, &stride);

  if (! plane || w < 1 || h < 1 || w > MAX_DIMENSION || h > MAX_DIMENSION ||
      stride < (size_t) w * (alpha ? 4 : 3))
    {
      heif_image_release (img);
      heif_file_close (&hf);
      return gimp_procedure_new_return_values (procedure,
                                               GIMP_PDB_EXECUTION_ERROR, NULL);
    }

  image = gimp_image_new (w, h, GIMP_RGB);
  layer = gimp_layer_new (image, _("Preview"), w, h,
                          alpha ? GIMP_RGBA_IMAGE : GIMP_RGB_IMAGE, 100.0,
                          GIMP_LAYER_MODE_NORMAL);
  gimp_image_insert_layer (image, layer, NULL, 0);

  rows = g_malloc ((gsize) w * (alpha ? 4 : 3));
  gbuf = gimp_drawable_get_buffer (GIMP_DRAWABLE (layer));
  for (y = 0; y < h; y++)
    {
      memcpy (rows, plane + (gsize) y * stride, (gsize) w * (alpha ? 4 : 3));
      gegl_buffer_set (gbuf, GEGL_RECTANGLE (0, y, w, 1), 0,
                       babl_format (alpha ? "R'G'B'A u8" : "R'G'B' u8"),
                       rows, GEGL_AUTO_ROWSTRIDE);
    }
  g_object_unref (gbuf);
  g_free (rows);

  heif_image_release (img);
  heif_file_close (&hf);

  if (MAX (w, h) > size * 2)
    {
      gdouble s = (gdouble) size / MAX (w, h);

      gimp_image_scale (image, MAX (1, (gint) (w * s)), MAX (1, (gint) (h * s)));
    }

  return_vals = gimp_procedure_new_return_values (procedure, GIMP_PDB_SUCCESS, NULL);
  GIMP_VALUES_SET_IMAGE (return_vals, 1, image);
  GIMP_VALUES_SET_INT   (return_vals, 2, full_w);
  GIMP_VALUES_SET_INT   (return_vals, 3, full_h);
  GIMP_VALUES_SET_ENUM  (return_vals, 4, alpha ? GIMP_RGBA_IMAGE : GIMP_RGB_IMAGE);
  GIMP_VALUES_SET_INT   (return_vals, 5, 1);
  gimp_value_array_truncate (return_vals, 6);

  return return_vals;
}
