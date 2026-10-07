/* gimp-raw-heic -- RAW development settings
 * Copyright (C) 2026 Marek Zettel
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <string.h>

#include <glib/gstdio.h>

#include "rawheic.h"

typedef struct
{
  const gchar *nick;
  gint         id;
  const gchar *label;
} ChoiceItem;

static const ChoiceItem wb_items[] =
{
  { "camera",  WB_CAMERA, N_("As shot (camera)") },
  { "auto",    WB_AUTO,   N_("Automatic") },
  { "manual",  WB_MANUAL, N_("Manual (temperature / tint)") },
  { "spot",    WB_SPOT,   N_("Neutral point (picked area)") },
  { NULL, 0, NULL }
};

static const ChoiceItem hl_items[] =
{
  { "clip",    HL_CLIP,    N_("Clip") },
  { "unclip",  HL_UNCLIP,  N_("Unclip") },
  { "blend",   HL_BLEND,   N_("Blend") },
  { "rebuild", HL_REBUILD, N_("Rebuild") },
  { NULL, 0, NULL }
};

/* ids are LibRaw user_qual values */
static const ChoiceItem demosaic_items[] =
{
  { "linear", 0,  N_("Bilinear (fastest)") },
  { "vng",    1,  N_("VNG") },
  { "ppg",    2,  N_("PPG") },
  { "ahd",    3,  N_("AHD") },
  { "dcb",    4,  N_("DCB") },
  { "dht",    11, N_("DHT") },
  { "aahd",   12, N_("AAHD") },
  { NULL, 0, NULL }
};

static const ChoiceItem fbdd_items[] =
{
  { "off",   0, N_("Off") },
  { "light", 1, N_("Light") },
  { "full",  2, N_("Full") },
  { NULL, 0, NULL }
};

static const ChoiceItem cs_items[] =
{
  { "srgb",       CS_SRGB,       N_("sRGB") },
  { "adobe",      CS_ADOBE,      N_("Adobe RGB (1998)") },
  { "display-p3", CS_DISPLAY_P3, N_("Display P3") },
  { "prophoto",   CS_PROPHOTO,   N_("ProPhoto RGB") },
  { "rec2020",    CS_REC2020,    N_("Rec. 2020") },
  { NULL, 0, NULL }
};

static const ChoiceItem depth_items[] =
{
  { "u8",    DEPTH_U8,    N_("8-bit integer") },
  { "u16",   DEPTH_U16,   N_("16-bit integer") },
  { "float", DEPTH_FLOAT, N_("32-bit floating point") },
  { NULL, 0, NULL }
};


static GimpChoice *
make_choice (const ChoiceItem *items)
{
  GimpChoice *choice = gimp_choice_new ();
  gint        i;

  for (i = 0; items[i].nick; i++)
    gimp_choice_add (choice, items[i].nick, items[i].id,
                     _(items[i].label), NULL);

  return choice;
}

static const gchar *
nick_for_id (const ChoiceItem *items, gint id)
{
  gint i;

  for (i = 0; items[i].nick; i++)
    if (items[i].id == id)
      return items[i].nick;

  return items[0].nick;
}

static gint
id_for_nick (const ChoiceItem *items, const gchar *nick, gint fallback)
{
  gint i;

  if (nick)
    for (i = 0; items[i].nick; i++)
      if (! strcmp (items[i].nick, nick))
        return items[i].id;

  return fallback;
}

const gchar *
raw_settings_wb_nick (WbMode mode)
{
  return nick_for_id (wb_items, mode);
}

void
raw_settings_defaults (RawSettings *s)
{
  memset (s, 0, sizeof (*s));

  s->wb_mode           = WB_CAMERA;
  s->wb_temperature    = 5500.0;
  s->wb_tint           = 0.0;
  s->spot[0]           = 0.45;
  s->spot[1]           = 0.45;
  s->spot[2]           = 0.1;
  s->spot[3]           = 0.1;
  s->exposure          = 0.0;
  s->exposure_preserve = 0.0;
  s->highlight         = HL_CLIP;
  s->rebuild_level     = 5;
  s->brightness        = 1.0;
  s->auto_bright       = FALSE;
  s->demosaic          = 3;
  s->denoise           = 0.0;
  s->fbdd              = 0;
  s->ca_red            = 1.0;
  s->ca_blue           = 1.0;
  s->hot_pixels        = FALSE;
  s->median_passes     = 0;
  s->color_space       = CS_SRGB;
  s->linear            = TRUE;
  s->depth             = DEPTH_U16;
  s->half_size         = FALSE;
  s->auto_rotate       = TRUE;
  s->camera_crop       = TRUE;
}

void
raw_settings_add_arguments (GimpProcedure *procedure)
{
  RawSettings d;

  raw_settings_defaults (&d);

#define RW G_PARAM_READWRITE
  gimp_procedure_add_choice_argument (procedure, "wb-mode",
                                      _("White balance"),
                                      _("White balance mode"),
                                      make_choice (wb_items),
                                      nick_for_id (wb_items, d.wb_mode), RW);
  gimp_procedure_add_double_argument (procedure, "wb-temperature",
                                      _("Temperature (K)"),
                                      _("Colour temperature for manual white balance"),
                                      2000.0, 25000.0, d.wb_temperature, RW);
  gimp_procedure_add_double_argument (procedure, "wb-tint",
                                      _("Tint"),
                                      _("Tint for manual white balance: negative = green, positive = magenta"),
                                      -100.0, 100.0, d.wb_tint, RW);
  gimp_procedure_add_double_argument (procedure, "spot-x", _("Spot X"),
                                      _("Neutral area left edge (0..1)"),
                                      0.0, 1.0, d.spot[0], RW);
  gimp_procedure_add_double_argument (procedure, "spot-y", _("Spot Y"),
                                      _("Neutral area top edge (0..1)"),
                                      0.0, 1.0, d.spot[1], RW);
  gimp_procedure_add_double_argument (procedure, "spot-width", _("Spot width"),
                                      _("Neutral area width (0..1)"),
                                      0.0, 1.0, d.spot[2], RW);
  gimp_procedure_add_double_argument (procedure, "spot-height", _("Spot height"),
                                      _("Neutral area height (0..1)"),
                                      0.0, 1.0, d.spot[3], RW);
  gimp_procedure_add_double_argument (procedure, "exposure",
                                      _("Exposure (EV)"),
                                      _("Exposure correction applied before demosaicing"),
                                      -2.0, 3.0, d.exposure, RW);
  gimp_procedure_add_double_argument (procedure, "exposure-preserve",
                                      _("Preserve highlights"),
                                      _("Highlight preservation for positive exposure (0..1)"),
                                      0.0, 1.0, d.exposure_preserve, RW);
  gimp_procedure_add_choice_argument (procedure, "highlight",
                                      _("Highlight recovery"),
                                      _("How clipped highlights are handled; modes other than Clip darken the image by about 1 EV to make room for the recovered highlights (compensate with Exposure)"),
                                      make_choice (hl_items),
                                      nick_for_id (hl_items, d.highlight), RW);
  gimp_procedure_add_int_argument    (procedure, "rebuild-level",
                                      _("Rebuild level"),
                                      _("Strength of the 'rebuild' highlight mode"),
                                      3, 9, d.rebuild_level, RW);
  gimp_procedure_add_double_argument (procedure, "brightness",
                                      _("Brightness"),
                                      _("Output brightness multiplier"),
                                      0.25, 4.0, d.brightness, RW);
  gimp_procedure_add_boolean_argument (procedure, "auto-bright",
                                       _("Automatic brightness"),
                                       _("Stretch the histogram like dcraw (clips one percent of pixels)"),
                                       d.auto_bright, RW);
  gimp_procedure_add_choice_argument (procedure, "demosaic",
                                      _("Demosaicing"),
                                      _("Interpolation algorithm (X-Trans sensors always use the X-Trans algorithm)"),
                                      make_choice (demosaic_items),
                                      nick_for_id (demosaic_items, d.demosaic), RW);
  gimp_procedure_add_double_argument (procedure, "denoise",
                                      _("Noise reduction (wavelet)"),
                                      _("Wavelet denoising threshold, 0 = off (typical 100..1000)"),
                                      0.0, 2000.0, d.denoise, RW);
  gimp_procedure_add_choice_argument (procedure, "fbdd",
                                      _("FBDD noise reduction"),
                                      _("FBDD noise reduction before demosaicing"),
                                      make_choice (fbdd_items),
                                      nick_for_id (fbdd_items, d.fbdd), RW);
  gimp_procedure_add_double_argument (procedure, "ca-red",
                                      _("Chromatic aberration – red"),
                                      _("Red channel scale, 1.0 = off"),
                                      0.99, 1.01, d.ca_red, RW);
  gimp_procedure_add_double_argument (procedure, "ca-blue",
                                      _("Chromatic aberration – blue"),
                                      _("Blue channel scale, 1.0 = off"),
                                      0.99, 1.01, d.ca_blue, RW);
  gimp_procedure_add_boolean_argument (procedure, "hot-pixels",
                                       _("Remove hot / dead pixels"),
                                       _("Replace isolated outlier sensor pixels"),
                                       d.hot_pixels, RW);
  gimp_procedure_add_int_argument    (procedure, "median-passes",
                                      _("Colour artefact median passes"),
                                      _("3x3 median filter passes on colour differences"),
                                      0, 10, d.median_passes, RW);
  gimp_procedure_add_choice_argument (procedure, "color-space",
                                      _("Output colour space"),
                                      _("Colour space of the loaded image"),
                                      make_choice (cs_items),
                                      nick_for_id (cs_items, d.color_space), RW);
  gimp_procedure_add_boolean_argument (procedure, "linear",
                                       _("Linear light (no gamma)"),
                                       _("Keep scene-linear data; otherwise use the colour space's tone curve"),
                                       d.linear, RW);
  gimp_procedure_add_choice_argument (procedure, "depth",
                                      _("Precision"),
                                      _("Bit depth of the loaded image"),
                                      make_choice (depth_items),
                                      nick_for_id (depth_items, d.depth), RW);
  gimp_procedure_add_boolean_argument (procedure, "half-size",
                                       _("Half size (fast)"),
                                       _("Load at half resolution"),
                                       d.half_size, RW);
  gimp_procedure_add_boolean_argument (procedure, "auto-rotate",
                                       _("Rotate according to camera orientation"),
                                       _("Apply the orientation stored by the camera"),
                                       d.auto_rotate, RW);
  gimp_procedure_add_boolean_argument (procedure, "camera-crop",
                                       _("Apply camera default crop"),
                                       _("Use the default crop stored in the file (DNG, CR3, ...)"),
                                       d.camera_crop, RW);
#undef RW
}

void
raw_settings_from_config (RawSettings         *s,
                          GimpProcedureConfig *config)
{
  gchar *wb, *hl, *dm, *fb, *cs, *dp;

  raw_settings_defaults (s);

  g_object_get (config,
                "wb-mode",           &wb,
                "wb-temperature",    &s->wb_temperature,
                "wb-tint",           &s->wb_tint,
                "spot-x",            &s->spot[0],
                "spot-y",            &s->spot[1],
                "spot-width",        &s->spot[2],
                "spot-height",       &s->spot[3],
                "exposure",          &s->exposure,
                "exposure-preserve", &s->exposure_preserve,
                "highlight",         &hl,
                "rebuild-level",     &s->rebuild_level,
                "brightness",        &s->brightness,
                "auto-bright",       &s->auto_bright,
                "demosaic",          &dm,
                "denoise",           &s->denoise,
                "fbdd",              &fb,
                "ca-red",            &s->ca_red,
                "ca-blue",           &s->ca_blue,
                "hot-pixels",        &s->hot_pixels,
                "median-passes",     &s->median_passes,
                "color-space",       &cs,
                "linear",            &s->linear,
                "depth",             &dp,
                "half-size",         &s->half_size,
                "auto-rotate",       &s->auto_rotate,
                "camera-crop",       &s->camera_crop,
                NULL);

  s->wb_mode     = id_for_nick (wb_items,       wb, WB_CAMERA);
  s->highlight   = id_for_nick (hl_items,       hl, HL_CLIP);
  s->demosaic    = id_for_nick (demosaic_items, dm, 3);
  s->fbdd        = id_for_nick (fbdd_items,     fb, 0);
  s->color_space = id_for_nick (cs_items,       cs, CS_SRGB);
  s->depth       = id_for_nick (depth_items,    dp, DEPTH_U16);

  g_free (wb); g_free (hl); g_free (dm); g_free (fb); g_free (cs); g_free (dp);
}

void
raw_settings_to_config (const RawSettings   *s,
                        GimpProcedureConfig *config)
{
  g_object_set (config,
                "wb-mode",           nick_for_id (wb_items, s->wb_mode),
                "wb-temperature",    s->wb_temperature,
                "wb-tint",           s->wb_tint,
                "spot-x",            s->spot[0],
                "spot-y",            s->spot[1],
                "spot-width",        s->spot[2],
                "spot-height",       s->spot[3],
                "exposure",          s->exposure,
                "exposure-preserve", s->exposure_preserve,
                "highlight",         nick_for_id (hl_items, s->highlight),
                "rebuild-level",     s->rebuild_level,
                "brightness",        s->brightness,
                "auto-bright",       s->auto_bright,
                "demosaic",          nick_for_id (demosaic_items, s->demosaic),
                "denoise",           s->denoise,
                "fbdd",              nick_for_id (fbdd_items, s->fbdd),
                "ca-red",            s->ca_red,
                "ca-blue",           s->ca_blue,
                "hot-pixels",        s->hot_pixels,
                "median-passes",     s->median_passes,
                "color-space",       nick_for_id (cs_items, s->color_space),
                "linear",            s->linear,
                "depth",             nick_for_id (depth_items, s->depth),
                "half-size",         s->half_size,
                "auto-rotate",       s->auto_rotate,
                "camera-crop",       s->camera_crop,
                NULL);
}

#define GROUP "raw"

gchar *
raw_settings_serialize (const RawSettings *s,
                        const gchar       *source_path)
{
  GKeyFile *kf = g_key_file_new ();
  gchar    *data;

  g_key_file_set_integer (kf, GROUP, "version", 1);
  g_key_file_set_string  (kf, GROUP, "source", source_path);
  g_key_file_set_string  (kf, GROUP, "wb-mode", nick_for_id (wb_items, s->wb_mode));
  g_key_file_set_double  (kf, GROUP, "wb-temperature", s->wb_temperature);
  g_key_file_set_double  (kf, GROUP, "wb-tint", s->wb_tint);
  g_key_file_set_double_list (kf, GROUP, "spot", (gdouble *) s->spot, 4);
  g_key_file_set_double  (kf, GROUP, "exposure", s->exposure);
  g_key_file_set_double  (kf, GROUP, "exposure-preserve", s->exposure_preserve);
  g_key_file_set_string  (kf, GROUP, "highlight", nick_for_id (hl_items, s->highlight));
  g_key_file_set_integer (kf, GROUP, "rebuild-level", s->rebuild_level);
  g_key_file_set_double  (kf, GROUP, "brightness", s->brightness);
  g_key_file_set_boolean (kf, GROUP, "auto-bright", s->auto_bright);
  g_key_file_set_string  (kf, GROUP, "demosaic", nick_for_id (demosaic_items, s->demosaic));
  g_key_file_set_double  (kf, GROUP, "denoise", s->denoise);
  g_key_file_set_string  (kf, GROUP, "fbdd", nick_for_id (fbdd_items, s->fbdd));
  g_key_file_set_double  (kf, GROUP, "ca-red", s->ca_red);
  g_key_file_set_double  (kf, GROUP, "ca-blue", s->ca_blue);
  g_key_file_set_boolean (kf, GROUP, "hot-pixels", s->hot_pixels);
  g_key_file_set_integer (kf, GROUP, "median-passes", s->median_passes);
  g_key_file_set_string  (kf, GROUP, "color-space", nick_for_id (cs_items, s->color_space));
  g_key_file_set_boolean (kf, GROUP, "linear", s->linear);
  g_key_file_set_string  (kf, GROUP, "depth", nick_for_id (depth_items, s->depth));
  g_key_file_set_boolean (kf, GROUP, "half-size", s->half_size);
  g_key_file_set_boolean (kf, GROUP, "auto-rotate", s->auto_rotate);
  g_key_file_set_boolean (kf, GROUP, "camera-crop", s->camera_crop);

  data = g_key_file_to_data (kf, NULL, NULL);
  g_key_file_free (kf);

  return data;
}

/* Reads values that are present; missing keys keep the defaults. */
gboolean
raw_settings_deserialize (RawSettings  *s,
                          const gchar  *text,
                          gchar       **source_path)
{
  GKeyFile *kf = g_key_file_new ();
  gchar    *str;
  gdouble  *spot;
  gsize     n;

  raw_settings_defaults (s);

  if (! text || ! g_key_file_load_from_data (kf, text, -1, 0, NULL) ||
      ! g_key_file_has_group (kf, GROUP))
    {
      g_key_file_free (kf);
      return FALSE;
    }

#define GET_D(key, field) \
  if (g_key_file_has_key (kf, GROUP, key, NULL)) \
    field = g_key_file_get_double (kf, GROUP, key, NULL)
#define GET_I(key, field) \
  if (g_key_file_has_key (kf, GROUP, key, NULL)) \
    field = g_key_file_get_integer (kf, GROUP, key, NULL)
#define GET_B(key, field) \
  if (g_key_file_has_key (kf, GROUP, key, NULL)) \
    field = g_key_file_get_boolean (kf, GROUP, key, NULL)
#define GET_C(key, field, items) \
  str = g_key_file_get_string (kf, GROUP, key, NULL); \
  field = id_for_nick (items, str, field); \
  g_free (str)

  GET_C ("wb-mode", s->wb_mode, wb_items);
  GET_D ("wb-temperature", s->wb_temperature);
  GET_D ("wb-tint", s->wb_tint);
  spot = g_key_file_get_double_list (kf, GROUP, "spot", &n, NULL);
  if (spot && n == 4)
    memcpy (s->spot, spot, sizeof (s->spot));
  g_free (spot);
  GET_D ("exposure", s->exposure);
  GET_D ("exposure-preserve", s->exposure_preserve);
  GET_C ("highlight", s->highlight, hl_items);
  GET_I ("rebuild-level", s->rebuild_level);
  GET_D ("brightness", s->brightness);
  GET_B ("auto-bright", s->auto_bright);
  GET_C ("demosaic", s->demosaic, demosaic_items);
  GET_D ("denoise", s->denoise);
  GET_C ("fbdd", s->fbdd, fbdd_items);
  GET_D ("ca-red", s->ca_red);
  GET_D ("ca-blue", s->ca_blue);
  GET_B ("hot-pixels", s->hot_pixels);
  GET_I ("median-passes", s->median_passes);
  GET_C ("color-space", s->color_space, cs_items);
  GET_B ("linear", s->linear);
  GET_C ("depth", s->depth, depth_items);
  GET_B ("half-size", s->half_size);
  GET_B ("auto-rotate", s->auto_rotate);
  GET_B ("camera-crop", s->camera_crop);

  /* clamp everything: the parasite may come from an edited XCF */
  s->wb_temperature    = CLAMP (s->wb_temperature, 2000.0, 25000.0);
  s->wb_tint           = CLAMP (s->wb_tint, -100.0, 100.0);
  s->exposure          = CLAMP (s->exposure, -2.0, 3.0);
  s->exposure_preserve = CLAMP (s->exposure_preserve, 0.0, 1.0);
  s->rebuild_level     = CLAMP (s->rebuild_level, 3, 9);
  s->brightness        = CLAMP (s->brightness, 0.25, 4.0);
  s->denoise           = CLAMP (s->denoise, 0.0, 2000.0);
  s->ca_red            = CLAMP (s->ca_red, 0.99, 1.01);
  s->ca_blue           = CLAMP (s->ca_blue, 0.99, 1.01);
  s->median_passes     = CLAMP (s->median_passes, 0, 10);
  for (n = 0; n < 4; n++)
    s->spot[n] = CLAMP (s->spot[n], 0.0, 1.0);

  if (source_path)
    *source_path = g_key_file_get_string (kf, GROUP, "source", NULL);

  g_key_file_free (kf);

  return TRUE;
}

/* --- batch session ------------------------------------------------------
 * GIMP's file-load only passes run-mode and file to a load procedure, so
 * the batch command leaves its settings in a small session file that the
 * RAW loader consults for the listed files only.  The file expires after
 * an hour and is removed when the batch ends.
 */

#define BATCH_GROUP   "batch"
#define BATCH_MAX_AGE (60 * 60)

static gchar *
batch_session_file (void)
{
  return g_build_filename (gimp_directory (), "gimp-raw-heic-batch.ini", NULL);
}

static gchar *
canonical (const gchar *path)
{
  gchar *c = g_canonicalize_filename (path, NULL);
#ifdef G_OS_WIN32
  gchar *l = g_utf8_casefold (c, -1);
  g_free (c);
  c = l;
#endif
  return c;
}

void
raw_batch_session_begin (const RawSettings  *s,
                         gchar             **files)
{
  gchar    *text = raw_settings_serialize (s, "");
  GKeyFile *kf   = g_key_file_new ();
  gchar    *fn   = batch_session_file ();
  gchar   **canon;
  gchar    *data;
  gint      i, n = g_strv_length (files);

  g_key_file_load_from_data (kf, text, -1, 0, NULL);
  canon = g_new0 (gchar *, n + 1);
  for (i = 0; i < n; i++)
    canon[i] = canonical (files[i]);
  g_key_file_set_string_list (kf, BATCH_GROUP, "files", (const gchar * const *) canon, n);
  g_key_file_set_int64 (kf, BATCH_GROUP, "created", g_get_real_time () / G_USEC_PER_SEC);

  data = g_key_file_to_data (kf, NULL, NULL);
  g_file_set_contents (fn, data, -1, NULL);

  g_free (data);
  g_strfreev (canon);
  g_free (fn);
  g_free (text);
  g_key_file_free (kf);
}

void
raw_batch_session_end (void)
{
  gchar *fn = batch_session_file ();

  g_remove (fn);
  g_free (fn);
}

gboolean
raw_batch_session_lookup (const gchar *path,
                          RawSettings *s)
{
  GKeyFile *kf = g_key_file_new ();
  gchar    *fn = batch_session_file ();
  gchar    *data = NULL;
  gboolean  found = FALSE;

  if (g_file_get_contents (fn, &data, NULL, NULL) &&
      g_key_file_load_from_data (kf, data, -1, 0, NULL))
    {
      gint64  created = g_key_file_get_int64 (kf, BATCH_GROUP, "created", NULL);
      gint64  now     = g_get_real_time () / G_USEC_PER_SEC;
      gchar **list    = g_key_file_get_string_list (kf, BATCH_GROUP, "files", NULL, NULL);
      gchar  *me      = canonical (path);
      gint    i;

      if (list && now - created >= 0 && now - created < BATCH_MAX_AGE)
        for (i = 0; list[i] && ! found; i++)
          found = ! strcmp (list[i], me);

      if (found)
        found = raw_settings_deserialize (s, data, NULL);

      g_free (me);
      g_strfreev (list);
    }

  g_free (data);
  g_free (fn);
  g_key_file_free (kf);

  return found;
}
