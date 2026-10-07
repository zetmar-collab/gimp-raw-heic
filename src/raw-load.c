/* gimp-raw-heic -- RAW load and thumbnail procedures
 * Copyright (C) 2026 Marek Zettel
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <gexiv2/gexiv2.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "rawheic.h"

#define RAW_EXTENSIONS \
  "cr2,cr3,crw,nef,nrw,arw,srf,sr2,dng,raf,orf,rw2,rwl,raw,rwz,pef,srw," \
  "3fr,fff,iiq,cap,erf,mrw,mdc,kdc,dcr,dc2,k25,kc2,mef,mos,x3f,gpr,"      \
  "bay,mfw,ari,cs1,sti,ia,rdc,qtk,pxn"

#define RAW_MIME_TYPES \
  "image/x-canon-cr2,image/x-canon-cr3,image/x-canon-crw,"            \
  "image/x-nikon-nef,image/x-nikon-nrw,image/x-sony-arw,"              \
  "image/x-sony-srf,image/x-sony-sr2,image/x-adobe-dng,"               \
  "image/x-fuji-raf,image/x-olympus-orf,image/x-panasonic-rw2,"        \
  "image/x-panasonic-raw,image/x-leica-rwl,image/x-pentax-pef,"        \
  "image/x-samsung-srw,image/x-hasselblad-3fr,image/x-hasselblad-fff," \
  "image/x-phaseone-iiq,image/x-epson-erf,image/x-minolta-mrw,"        \
  "image/x-kodak-kdc,image/x-kodak-dcr,image/x-mamiya-mef,"            \
  "image/x-leaf-mos,image/x-sigma-x3f"

/* No magics on purpose: GIMP first matches *magic-less* procedures by
 * extension and only then tries magics.  Most RAW formats are TIFF
 * containers, so with magics the TIFF loader would grab NEF, ARW, DNG,
 * PEF, ... (and load just the embedded thumbnail).
 */


static GimpValueArray * raw_load_run  (GimpProcedure         *procedure,
                                       GimpRunMode            run_mode,
                                       GFile                 *file,
                                       GimpMetadata          *metadata,
                                       GimpMetadataLoadFlags *flags,
                                       GimpProcedureConfig   *config,
                                       gpointer               run_data);
static GimpValueArray * raw_thumb_run (GimpProcedure         *procedure,
                                       GFile                 *file,
                                       gint                   size,
                                       GimpProcedureConfig   *config,
                                       gpointer               run_data);


GimpProcedure *
raw_load_procedure_new (GimpPlugIn  *plug_in,
                        const gchar *name)
{
  GimpProcedure *procedure;

  procedure = gimp_load_procedure_new (plug_in, name,
                                       GIMP_PDB_PROC_TYPE_PLUGIN,
                                       raw_load_run, NULL, NULL);

  gimp_procedure_set_menu_label (procedure, _("Camera RAW (LibRaw)"));
  gimp_procedure_set_documentation (procedure,
                                    _("Load camera RAW files with LibRaw"),
                                    _("Develops camera RAW files (CR2, CR3, NEF, ARW, "
                                      "RAF, DNG, RW2, ORF, PEF and many more) inside "
                                      "GIMP using LibRaw, without external programs."),
                                    name);
  gimp_procedure_set_attribution (procedure, "Marek Zettel", "Marek Zettel", "2026");

  gimp_file_procedure_set_format_name (GIMP_FILE_PROCEDURE (procedure), "RAW");
  gimp_file_procedure_set_mime_types  (GIMP_FILE_PROCEDURE (procedure), RAW_MIME_TYPES);
  gimp_file_procedure_set_extensions  (GIMP_FILE_PROCEDURE (procedure), RAW_EXTENSIONS);

  gimp_load_procedure_set_handles_raw (GIMP_LOAD_PROCEDURE (procedure), TRUE);
  gimp_load_procedure_set_thumbnail_loader (GIMP_LOAD_PROCEDURE (procedure),
                                            RAW_THUMB_PROC);

  raw_settings_add_arguments (procedure);

  gimp_procedure_add_boolean_aux_argument (procedure, "ask-dialog",
                                           _("Always show this dialog"),
                                           _("Show the import settings for every file "
                                             "(hold Shift while opening to invert)"),
                                           TRUE, G_PARAM_READWRITE);

  return procedure;
}

GimpProcedure *
raw_thumb_procedure_new (GimpPlugIn  *plug_in,
                         const gchar *name)
{
  GimpProcedure *procedure;

  procedure = gimp_thumbnail_procedure_new (plug_in, name,
                                            GIMP_PDB_PROC_TYPE_PLUGIN,
                                            raw_thumb_run, NULL, NULL);

  gimp_procedure_set_documentation (procedure,
                                    _("Load the embedded preview of a RAW file"),
                                    _("Fast thumbnail from the JPEG preview that "
                                      "cameras embed in RAW files."),
                                    name);
  gimp_procedure_set_attribution (procedure, "Marek Zettel", "Marek Zettel", "2026");

  return procedure;
}


typedef struct
{
  gdouble last;
} ProgressState;

static gboolean
load_progress (gdouble  fraction,
               gpointer data)
{
  ProgressState *st = data;

  if (fraction - st->last >= 0.02 || fraction >= 1.0)
    {
      st->last = fraction;
      return gimp_progress_update (fraction * 0.9);
    }

  return TRUE;
}

static GimpValueArray *
raw_load_run (GimpProcedure         *procedure,
              GimpRunMode            run_mode,
              GFile                 *file,
              GimpMetadata          *metadata,
              GimpMetadataLoadFlags *flags,
              GimpProcedureConfig   *config,
              gpointer               run_data)
{
  GimpValueArray *return_vals;
  RawSettings     settings;
  ProgressState   pstate = { 0.0 };
  RawImage       *img;
  GimpImage      *image;
  GError         *error = NULL;
  gchar          *path;
  gchar          *name;

  path = rawheic_file_path (file);
  if (! path)
    {
      g_set_error (&error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   _("RAW files can only be opened from a local disk."));
      return gimp_procedure_new_return_values (procedure,
                                               GIMP_PDB_EXECUTION_ERROR, error);
    }

  name = g_file_get_basename (file);

  if (run_mode == GIMP_RUN_INTERACTIVE)
    {
      gboolean ask;

      g_object_get (config, "ask-dialog", &ask, NULL);

      if (ask != rawheic_shift_pressed ())
        {
          gchar *title = g_strdup_printf (_("Develop RAW – %s"), name);

          if (! raw_dialog_run (procedure, config, path, title))
            {
              g_free (title);
              g_free (name);
              g_free (path);
              return gimp_procedure_new_return_values (procedure,
                                                       GIMP_PDB_CANCEL, NULL);
            }
          g_free (title);
        }
    }

  raw_settings_from_config (&settings, config);

  /* File > Open several RAW files: settings chosen once for the queue */
  if (run_mode == GIMP_RUN_NONINTERACTIVE)
    raw_batch_session_lookup (path, &settings);

  gimp_progress_init_printf (_("Developing '%s'"), name);

  img = raw_develop (path, &settings, 0, load_progress, &pstate, &error);
  if (! img)
    {
      g_free (name);
      g_free (path);
      return gimp_procedure_new_return_values (procedure,
                                               g_error_matches (error, G_IO_ERROR,
                                                                G_IO_ERROR_CANCELLED) ?
                                               GIMP_PDB_CANCEL :
                                               GIMP_PDB_EXECUTION_ERROR,
                                               error);
    }

  gimp_progress_set_text_printf (_("Creating image from '%s'"), name);
  image = rawheic_image_from_raw (img, &settings, file, &error);
  raw_image_free (img);

  if (! image)
    {
      g_free (name);
      g_free (path);
      return gimp_procedure_new_return_values (procedure,
                                               GIMP_PDB_EXECUTION_ERROR, error);
    }

  rawheic_attach_source (image, &settings, path);
  gimp_progress_update (1.0);

  /* We rotated the pixels and assigned the profile ourselves. */
  *flags &= ~(GIMP_METADATA_LOAD_ORIENTATION | GIMP_METADATA_LOAD_COLORSPACE);

  if (metadata)
    {
      gexiv2_metadata_try_set_orientation (GEXIV2_METADATA (metadata),
                                           GEXIV2_ORIENTATION_NORMAL, NULL);
      gimp_metadata_set_pixel_size (metadata,
                                    gimp_image_get_width (image),
                                    gimp_image_get_height (image));
      gimp_metadata_set_bits_per_sample (metadata,
                                         settings.depth == DEPTH_U8 ? 8 :
                                         settings.depth == DEPTH_U16 ? 16 : 32);
    }

  g_free (name);
  g_free (path);

  return_vals = gimp_procedure_new_return_values (procedure, GIMP_PDB_SUCCESS, NULL);
  GIMP_VALUES_SET_IMAGE (return_vals, 1, image);

  return return_vals;
}


/* --- thumbnails ------------------------------------------------------- */

static void
size_prepared (GdkPixbufLoader *loader,
               gint             width,
               gint             height,
               gpointer         data)
{
  gint size = GPOINTER_TO_INT (data);

  /* let the JPEG decoder downscale; keep 2x for quality */
  if (width > size * 2 || height > size * 2)
    {
      gdouble scale = (gdouble) (size * 2) / MAX (width, height);

      gdk_pixbuf_loader_set_size (loader,
                                  MAX (1, (gint) (width * scale)),
                                  MAX (1, (gint) (height * scale)));
    }
}

static GimpValueArray *
raw_thumb_run (GimpProcedure       *procedure,
               GFile               *file,
               gint                 size,
               GimpProcedureConfig *config,
               gpointer             run_data)
{
  GimpValueArray *return_vals;
  GdkPixbuf      *pixbuf = NULL;
  GimpImage      *image;
  GimpLayer      *layer;
  GBytes         *bytes;
  GError         *error = NULL;
  gchar          *path;
  gint            width = 0, height = 0, flip = 0, bw = 0, bh = 0;
  gboolean        is_jpeg = FALSE;

  path = rawheic_file_path (file);
  if (! path)
    return gimp_procedure_new_return_values (procedure,
                                             GIMP_PDB_EXECUTION_ERROR, NULL);

  bytes = raw_extract_thumbnail (path, &width, &height, &flip,
                                 &is_jpeg, &bw, &bh, &error);
  g_free (path);

  if (! bytes)
    return gimp_procedure_new_return_values (procedure,
                                             GIMP_PDB_EXECUTION_ERROR, error);

  if (is_jpeg)
    {
      GdkPixbufLoader *loader = gdk_pixbuf_loader_new_with_type ("jpeg", NULL);

      if (loader)
        {
          gsize         len;
          const guchar *data = g_bytes_get_data (bytes, &len);

          g_signal_connect (loader, "size-prepared",
                            G_CALLBACK (size_prepared), GINT_TO_POINTER (size));

          if (gdk_pixbuf_loader_write (loader, data, len, &error) &&
              gdk_pixbuf_loader_close (loader, &error))
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
  else
    {
      pixbuf = gdk_pixbuf_new_from_bytes (bytes, GDK_COLORSPACE_RGB, FALSE, 8,
                                          bw, bh, bw * 3);
    }

  g_bytes_unref (bytes);

  if (! pixbuf)
    return gimp_procedure_new_return_values (procedure,
                                             GIMP_PDB_EXECUTION_ERROR, error);

  /* Embedded previews are stored in sensor orientation. */
  if (flip == 3 || flip == 5 || flip == 6)
    {
      GdkPixbuf *rotated =
        gdk_pixbuf_rotate_simple (pixbuf,
                                  flip == 3 ? GDK_PIXBUF_ROTATE_UPSIDEDOWN :
                                  flip == 6 ? GDK_PIXBUF_ROTATE_CLOCKWISE :
                                              GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE);
      if (rotated)
        {
          g_object_unref (pixbuf);
          pixbuf = rotated;
        }
    }

  image = gimp_image_new (gdk_pixbuf_get_width (pixbuf),
                          gdk_pixbuf_get_height (pixbuf), GIMP_RGB);
  layer = gimp_layer_new_from_pixbuf (image, _("Preview"), pixbuf, 100.0,
                                      GIMP_LAYER_MODE_NORMAL, 0.0, 1.0);
  gimp_image_insert_layer (image, layer, NULL, 0);
  g_object_unref (pixbuf);

  return_vals = gimp_procedure_new_return_values (procedure, GIMP_PDB_SUCCESS, NULL);
  GIMP_VALUES_SET_IMAGE (return_vals, 1, image);
  GIMP_VALUES_SET_INT   (return_vals, 2, width);
  GIMP_VALUES_SET_INT   (return_vals, 3, height);
  GIMP_VALUES_SET_ENUM  (return_vals, 4, GIMP_RGB_IMAGE);
  GIMP_VALUES_SET_INT   (return_vals, 5, 1);
  gimp_value_array_truncate (return_vals, 6);

  return return_vals;
}
