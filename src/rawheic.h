/* gimp-raw-heic -- shared declarations
 * Copyright (C) 2026 Marek Zettel
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef RAWHEIC_H
#define RAWHEIC_H

#include <libgimp/gimp.h>
#include <glib/gi18n-lib.h>

#define RAW_LOAD_PROC        "file-raw-heic-raw-load"
#define RAW_THUMB_PROC       "file-raw-heic-raw-load-thumb"
#define HEIF_LOAD_PROC       "file-raw-heic-heif-load"
#define HEIF_THUMB_PROC      "file-raw-heic-heif-load-thumb"
#define REDEVELOP_PROC       "plug-in-raw-heic-redevelop"
#define BATCH_PROC           "plug-in-raw-heic-batch-open"
#define ADJUST_PROC          "plug-in-raw-heic-quick-adjust"
#define ABOUT_PROC           "plug-in-raw-heic-about"

#define SOURCE_PARASITE      "gimp-raw-heic-source"
#define LAYER_PARASITE       "gimp-raw-heic-layer"

/* Hard limits for untrusted input: refuse anything larger. */
#define MAX_DIMENSION        65000
#define MAX_PIXELS           ((gint64) 600 * 1000 * 1000)

typedef enum
{
  WB_CAMERA,
  WB_AUTO,
  WB_MANUAL,
  WB_SPOT
} WbMode;

typedef enum
{
  HL_CLIP,
  HL_UNCLIP,
  HL_BLEND,
  HL_REBUILD
} HighlightMode;

typedef enum
{
  CS_SRGB,
  CS_ADOBE,
  CS_DISPLAY_P3,
  CS_PROPHOTO,
  CS_REC2020,
  CS_N
} ColorSpace;

typedef enum
{
  DEPTH_U8,
  DEPTH_U16,
  DEPTH_FLOAT
} OutputDepth;

typedef struct
{
  WbMode        wb_mode;
  gdouble       wb_temperature;   /* K   */
  gdouble       wb_tint;          /* -100 (green) .. +100 (magenta) */
  gdouble       spot[4];          /* x, y, w, h in 0..1 of the oriented image */
  gdouble       exposure;         /* EV, -2 .. +3 */
  gdouble       exposure_preserve;/* 0 .. 1 */
  HighlightMode highlight;
  gint          rebuild_level;    /* 3 .. 9 */
  gdouble       brightness;       /* 0.25 .. 4 */
  gboolean      auto_bright;
  gint          demosaic;         /* LibRaw user_qual */
  gdouble       denoise;          /* wavelet threshold, 0 = off */
  gint          fbdd;             /* 0, 1, 2 */
  gdouble       ca_red;           /* scale, 1 = off */
  gdouble       ca_blue;
  gboolean      hot_pixels;
  gint          median_passes;
  ColorSpace    color_space;
  gboolean      linear;
  OutputDepth   depth;
  gboolean      half_size;
  gboolean      auto_rotate;
  gboolean      camera_crop;
} RawSettings;

/* Developed image, linear RGB in the requested primaries: either float
 * (previews, scaled output) or, for full-size loads, LibRaw's own 16-bit
 * buffer to avoid a second full-size copy.
 */
typedef struct
{
  gint      width;
  gint      height;
  gfloat   *pixels;     /* width * height * 3, or NULL      */
  guint16  *pixels16;   /* width * height * 3, or NULL      */
  gpointer  mem;        /* libraw_processed_image_t owning pixels16 */
  gchar    *camera;     /* "Make Model" */
} RawImage;

typedef gboolean (* RawProgressFunc) (gdouble fraction, gpointer data);

/* settings.c */
void              raw_settings_defaults        (RawSettings          *s);
void              raw_settings_from_config     (RawSettings          *s,
                                                GimpProcedureConfig  *config);
void              raw_settings_to_config       (const RawSettings    *s,
                                                GimpProcedureConfig  *config);
void              raw_settings_add_arguments   (GimpProcedure        *procedure);
gchar           * raw_settings_serialize       (const RawSettings    *s,
                                                const gchar          *source_path);
gboolean          raw_settings_deserialize     (RawSettings          *s,
                                                const gchar          *text,
                                                gchar               **source_path);
const gchar     * raw_settings_wb_nick         (WbMode                mode);
void              raw_batch_session_begin      (const RawSettings    *s,
                                                gchar               **files);
void              raw_batch_session_end        (void);
gboolean          raw_batch_session_lookup     (const gchar          *path,
                                                RawSettings          *s);

/* raw-develop.c */
typedef struct _RawSession RawSession;
RawSession      * raw_session_open             (const gchar          *path,
                                                GError              **error);
RawImage        * raw_session_develop          (RawSession           *ses,
                                                const RawSettings    *s,
                                                gint                  max_size,
                                                RawProgressFunc       progress,
                                                gpointer              progress_data,
                                                GError              **error);
const gchar     * raw_session_camera           (RawSession           *ses);
void              raw_session_free             (RawSession           *ses);
RawImage        * raw_develop                  (const gchar          *path,
                                                const RawSettings    *s,
                                                gint                  max_size,
                                                RawProgressFunc       progress,
                                                gpointer              progress_data,
                                                GError              **error);
void              raw_image_free               (RawImage             *img);
GBytes          * raw_extract_thumbnail        (const gchar          *path,
                                                gint                 *out_width,
                                                gint                 *out_height,
                                                gint                 *flip,
                                                gboolean             *is_jpeg,
                                                gint                 *bmp_w,
                                                gint                 *bmp_h,
                                                GError              **error);
gboolean          raw_probe                    (const gchar          *path,
                                                gchar               **camera,
                                                gint                 *width,
                                                gint                 *height,
                                                GError              **error);

/* denoise.c */
gboolean          rawheic_wavelet_denoise      (guint16              *rgb,
                                                gint                  width,
                                                gint                  height,
                                                gdouble               threshold);

/* profiles.c */
GimpColorProfile* rawheic_profile_new          (ColorSpace            cs,
                                                gboolean              linear);
GimpColorProfile* rawheic_profile_rec2100_linear (void);

/* image.c */
GimpImage       * rawheic_image_from_raw       (RawImage             *img,
                                                const RawSettings    *s,
                                                GFile                *file,
                                                GError              **error);
void              rawheic_mark_layer           (GimpLayer            *layer);
void              rawheic_attach_source        (GimpImage            *image,
                                                const RawSettings    *s,
                                                const gchar          *path);

/* raw-load.c */
GimpProcedure   * raw_load_procedure_new       (GimpPlugIn           *plug_in,
                                                const gchar          *name);
GimpProcedure   * raw_thumb_procedure_new      (GimpPlugIn           *plug_in,
                                                const gchar          *name);

/* raw-dialog.c */
gboolean          raw_dialog_run               (GimpProcedure        *procedure,
                                                GimpProcedureConfig  *config,
                                                const gchar          *path,
                                                const gchar          *title);

/* redevelop.c */
GimpProcedure   * redevelop_procedure_new      (GimpPlugIn           *plug_in,
                                                const gchar          *name);
GimpProcedure   * batch_procedure_new          (GimpPlugIn           *plug_in,
                                                const gchar          *name);

/* adjust.c */
GimpProcedure   * adjust_procedure_new         (GimpPlugIn           *plug_in,
                                                const gchar          *name);

/* heif-load.c */
GimpProcedure   * heif_load_procedure_new      (GimpPlugIn           *plug_in,
                                                const gchar          *name);

/* image.c (internal) */
gboolean          rawheic_write_layer          (GimpDrawable         *drawable,
                                                const RawImage       *img,
                                                ColorSpace            cs,
                                                GError              **error);

/* util.c */
gchar           * rawheic_file_path            (GFile                *file);
gboolean          rawheic_shift_pressed        (void);

#endif
