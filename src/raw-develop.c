/* gimp-raw-heic -- RAW development with LibRaw
 * Copyright (C) 2026 Marek Zettel
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <math.h>
#include <string.h>
#include <wchar.h>

#include <libraw/libraw.h>

#include "rawheic.h"


typedef struct
{
  RawProgressFunc func;
  gpointer        data;
  gboolean        cancelled;
} ProgressCtx;


static void
set_libraw_error (GError     **error,
                  const gchar *path,
                  gint         code)
{
  gchar *name = g_filename_display_basename (path);

  if (code == LIBRAW_CANCELLED_BY_CALLBACK)
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED,
                 _("Loading '%s' was cancelled."), name);
  else if (code == LIBRAW_FILE_UNSUPPORTED)
    g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                 _("'%s' is not a supported RAW file or the camera is unknown to LibRaw %s."),
                 name, libraw_version ());
  else if (code == LIBRAW_UNSUFFICIENT_MEMORY || code == LIBRAW_TOO_BIG ||
           code == LIBRAW_MEMPOOL_OVERFLOW)
    g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOMEM,
                 _("Not enough memory to load '%s'."), name);
  else if (code == LIBRAW_DATA_ERROR || code == LIBRAW_IO_ERROR)
    g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_IO,
                 _("'%s' is damaged or truncated (%s)."),
                 name, libraw_strerror (code));
  else
    g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                 _("Could not load '%s': %s"), name, libraw_strerror (code));

  g_free (name);
}

static int
progress_cb (void                 *data,
             enum LibRaw_progress  stage,
             int                   iteration,
             int                   expected)
{
  ProgressCtx *ctx = data;
  gint         bit = 0;
  guint        s   = (guint) stage;

  if (! ctx->func)
    return 0;

  while (s > 1 && bit < 31)
    {
      s >>= 1;
      bit++;
    }

  /* stages 0..19 cover the whole decode/process pipeline */
  if (! ctx->func (MIN (bit, 19) / 20.0, ctx->data))
    {
      ctx->cancelled = TRUE;
      return 1;
    }

  return 0;
}

static gint
open_raw (libraw_data_t *lr,
          const gchar   *path)
{
#ifdef G_OS_WIN32
  gunichar2 *wpath = g_utf8_to_utf16 (path, -1, NULL, NULL, NULL);
  gint       ret;

  if (! wpath)
    return LIBRAW_IO_ERROR;

  ret = libraw_open_wfile (lr, (const wchar_t *) wpath);
  g_free (wpath);

  return ret;
#else
  return libraw_open_file (lr, path);
#endif
}

static gboolean
check_sizes (libraw_data_t *lr,
             const gchar   *path,
             GError       **error)
{
  libraw_image_sizes_t *sz = &lr->sizes;

  if (sz->width < 1 || sz->height < 1 ||
      sz->width > MAX_DIMENSION || sz->height > MAX_DIMENSION ||
      (gint64) sz->width * sz->height > MAX_PIXELS ||
      sz->raw_width < sz->width || sz->raw_height < sz->height)
    {
      gchar *name = g_filename_display_basename (path);

      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                   _("'%s' reports an invalid image size (%u x %u)."),
                   name, sz->width, sz->height);
      g_free (name);
      return FALSE;
    }

  return TRUE;
}

/* --- white balance from temperature / tint ------------------------------ */

static void
temperature_to_xy (gdouble  T,
                   gdouble *x,
                   gdouble *y)
{
  if (T >= 4000.0)
    {
      /* CIE daylight locus */
      gdouble t = 1.0 / T;

      if (T <= 7000.0)
        *x = -4.6070e9 * t * t * t + 2.9678e6 * t * t + 0.09911e3 * t + 0.244063;
      else
        *x = -2.0064e9 * t * t * t + 1.9018e6 * t * t + 0.24748e3 * t + 0.237040;

      *y = -3.000 * *x * *x + 2.870 * *x - 0.275;
    }
  else
    {
      /* Planckian locus, Kim et al. cubic spline */
      gdouble t = 1.0 / T;

      if (T <= 4000.0)
        *x = -0.2661239e9 * t * t * t - 0.2343589e6 * t * t + 0.8776956e3 * t + 0.179910;
      else
        *x = -3.0258469e9 * t * t * t + 2.1070379e6 * t * t + 0.2226347e3 * t + 0.240390;

      if (T <= 2222.0)
        *y = -1.1063814 * pow (*x, 3) - 1.34811020 * *x * *x + 2.18555832 * *x - 0.20219683;
      else
        *y = -0.9549476 * pow (*x, 3) - 1.37418593 * *x * *x + 2.09137015 * *x - 0.16748867;
    }
}

static gboolean
manual_multipliers (libraw_data_t *lr,
                    gdouble        temperature,
                    gdouble        tint,
                    gfloat         mul[4])
{
  gdouble x, y, xyz[3], cam[4];
  gint    c, j, colors = lr->idata.colors;
  gdouble sum = 0.0;

  if (colors < 3 || colors > 4)
    return FALSE;

  for (c = 0; c < colors; c++)
    for (j = 0; j < 3; j++)
      sum += fabs (lr->color.cam_xyz[c][j]);

  if (sum < 1e-6)
    return FALSE;

  temperature_to_xy (temperature, &x, &y);
  xyz[0] = x / y;
  xyz[1] = 1.0;
  xyz[2] = (1.0 - x - y) / y;

  for (c = 0; c < colors; c++)
    {
      cam[c] = 0.0;
      for (j = 0; j < 3; j++)
        cam[c] += lr->color.cam_xyz[c][j] * xyz[j];

      if (cam[c] <= 1e-9)
        return FALSE;
    }

  for (c = 0; c < colors; c++)
    mul[c] = (gfloat) (cam[1] / cam[c]);

  /* positive tint = magenta = less green */
  mul[1] *= (gfloat) pow (2.0, -tint / 100.0);
  mul[3] = (colors == 4) ? mul[3] * (gfloat) pow (2.0, -tint / 100.0) : mul[1];

  return TRUE;
}

/* Map a rectangle given in oriented-image fractions back to the
 * unrotated visible area, according to the LibRaw flip value.
 */
static void
spot_to_raw (const gdouble spot[4],
             gint          flip,
             gint          width,
             gint          height,
             guint         box[4])
{
  gdouble u0 = spot[0], v0 = spot[1];
  gdouble u1 = spot[0] + spot[2], v1 = spot[1] + spot[3];
  gdouble x0, y0, x1, y1;

  switch (flip)
    {
    case 3: x0 = 1 - u1; x1 = 1 - u0; y0 = 1 - v1; y1 = 1 - v0; break;
    case 6: x0 = v0;     x1 = v1;     y0 = 1 - u1; y1 = 1 - u0; break;
    case 5: x0 = 1 - v1; x1 = 1 - v0; y0 = u0;     y1 = u1;     break;
    default: x0 = u0;    x1 = u1;     y0 = v0;     y1 = v1;     break;
    }

  x0 = CLAMP (x0, 0.0, 1.0); x1 = CLAMP (x1, 0.0, 1.0);
  y0 = CLAMP (y0, 0.0, 1.0); y1 = CLAMP (y1, 0.0, 1.0);

  box[0] = (guint) (x0 * width);
  box[1] = (guint) (y0 * height);
  box[2] = MAX (8, (guint) ((x1 - x0) * width));
  box[3] = MAX (8, (guint) ((y1 - y0) * height));
}

/* --- hot / dead pixels on the Bayer mosaic ------------------------------ */

static void
remove_hot_pixels (libraw_data_t *lr)
{
  libraw_image_sizes_t *sz  = &lr->sizes;
  ushort               *raw = lr->rawdata.raw_image;
  gint                  pitch, row, col;
  gint                  black = lr->color.black;
  gint                  range = (gint) lr->color.maximum - black;

  /* Only plain Bayer data: X-Trans, Foveon and linear DNG are skipped. */
  if (! raw || lr->idata.filters == 0 || lr->idata.filters == 9 ||
      lr->idata.colors != 3 || range <= 0)
    return;

  pitch = sz->raw_pitch / 2;

  if (pitch < sz->raw_width)
    return;

  for (row = sz->top_margin + 2; row < sz->top_margin + sz->height - 2; row++)
    {
      ushort *p = raw + (gsize) row * pitch;

      for (col = sz->left_margin + 2; col < sz->left_margin + sz->width - 2; col++)
        {
          /* same-colour neighbours on a 2x2 Bayer pattern */
          gint n[8] = { p[col - 2], p[col + 2],
                        p[col - 2 * pitch], p[col + 2 * pitch],
                        p[col - 2 * pitch - 2], p[col - 2 * pitch + 2],
                        p[col + 2 * pitch - 2], p[col + 2 * pitch + 2] };
          gint v    = p[col];
          gint hi   = 0, lo = G_MAXINT, sum = 0, i;

          for (i = 0; i < 8; i++)
            {
              hi   = MAX (hi, n[i]);
              lo   = MIN (lo, n[i]);
              sum += n[i];
            }

          if ((v - black > 4 * MAX (hi - black, 0) && v - black > range / 16) ||
              (v <= black && lo - black > range / 8))
            p[col] = (ushort) (sum / 8);
        }
    }
}

/* --- public -------------------------------------------------------------- */

static void
apply_settings (libraw_data_t     *lr,
                const RawSettings *s,
                gboolean           preview)
{
  libraw_output_params_t *p = &lr->params;
  static const gint cs_map[CS_N] = { 1, 2, 7, 4, 8 };

  p->output_bps     = 16;
  p->gamm[0]        = 1.0;   /* linear output, tone curves are applied */
  p->gamm[1]        = 1.0;   /* by GIMP via the ICC profile            */
  p->no_auto_bright = ! s->auto_bright;
  p->bright         = (float) s->brightness;
  p->output_color   = cs_map[CLAMP (s->color_space, 0, CS_N - 1)];
  p->user_flip      = s->auto_rotate ? -1 : 0;
  p->half_size      = s->half_size || preview;
  p->user_qual      = preview ? 0 : s->demosaic;
  p->threshold      = 0.0f;   /* crashes in LibRaw 0.22.2: see denoise.c */
  p->fbdd_noiserd   = s->fbdd;
  p->med_passes     = preview ? 0 : s->median_passes;
  p->aber[0]        = 1.0 / s->ca_red;
  p->aber[2]        = 1.0 / s->ca_blue;

  switch (s->highlight)
    {
    case HL_CLIP:    p->highlight = 0; break;
    case HL_UNCLIP:  p->highlight = 1; break;
    case HL_BLEND:   p->highlight = 2; break;
    case HL_REBUILD: p->highlight = CLAMP (s->rebuild_level, 3, 9); break;
    }

  /* sessions re-process with new settings: reset everything optional */
  p->exp_correc = 0;
  p->exp_shift  = 1.0f;
  p->exp_preser = 0.0f;
  memset (p->cropbox, 0, sizeof (p->cropbox));
  memset (p->cropbox + 2, 0xff, 2 * sizeof (p->cropbox[0]));

  if (fabs (s->exposure) > 0.001)
    {
      p->exp_correc = 1;
      p->exp_shift  = (float) CLAMP (pow (2.0, s->exposure), 0.25, 8.0);
      p->exp_preser = (float) s->exposure_preserve;
    }

  p->use_camera_wb = 0;
  p->use_auto_wb   = 0;
  memset (p->user_mul, 0, sizeof (p->user_mul));
  memset (p->greybox, 0, sizeof (p->greybox));

  switch (s->wb_mode)
    {
    case WB_CAMERA:
      p->use_camera_wb = 1;
      break;

    case WB_AUTO:
      p->use_auto_wb = 1;
      break;

    case WB_MANUAL:
      if (! manual_multipliers (lr, s->wb_temperature, s->wb_tint, p->user_mul))
        p->use_camera_wb = 1;
      break;

    case WB_SPOT:
      p->use_auto_wb = 1;
      spot_to_raw (s->spot, s->auto_rotate ? lr->sizes.flip : 0,
                   lr->sizes.width, lr->sizes.height, p->greybox);
      break;
    }

  if (s->camera_crop)
    {
      libraw_raw_inset_crop_t *c = &lr->sizes.raw_inset_crops[0];

      /* inset crops are relative to the full raw frame */
      if (c->cwidth > 16 && c->cheight > 16 &&
          c->cleft >= lr->sizes.left_margin && c->ctop >= lr->sizes.top_margin &&
          c->cleft - lr->sizes.left_margin + c->cwidth  <= lr->sizes.width &&
          c->ctop  - lr->sizes.top_margin  + c->cheight <= lr->sizes.height &&
          (c->cwidth < lr->sizes.width || c->cheight < lr->sizes.height))
        {
          p->cropbox[0] = c->cleft - lr->sizes.left_margin;
          p->cropbox[1] = c->ctop  - lr->sizes.top_margin;
          p->cropbox[2] = c->cwidth;
          p->cropbox[3] = c->cheight;
        }
    }
}

static gfloat *
to_float_scaled (const libraw_processed_image_t *mem,
                 gint                            max_size,
                 gint                           *out_w,
                 gint                           *out_h)
{
  const guint16 *src = (const guint16 *) mem->data;
  gint           w = mem->width, h = mem->height, colors = mem->colors;
  gint           f = 1, ow, oh, x, y;
  gfloat        *dst;

  if (max_size > 0)
    while (MAX (w, h) / f > max_size)
      f++;

  ow = w / f;
  oh = h / f;
  if (ow < 1 || oh < 1)
    return NULL;

  dst = g_try_malloc ((gsize) ow * oh * 3 * sizeof (gfloat));
  if (! dst)
    return NULL;

#pragma omp parallel for private (x)
  for (y = 0; y < oh; y++)
    for (x = 0; x < ow; x++)
      {
        gdouble acc[3] = { 0, 0, 0 };
        gint    dx, dy, c;

        for (dy = 0; dy < f; dy++)
          for (dx = 0; dx < f; dx++)
            {
              const guint16 *px = src + ((gsize) (y * f + dy) * w + (x * f + dx)) * colors;

              for (c = 0; c < 3; c++)
                acc[c] += px[colors == 1 ? 0 : c];
            }

        for (c = 0; c < 3; c++)
          dst[((gsize) y * ow + x) * 3 + c] = (gfloat) (acc[c] / (f * f * 65535.0));
      }

  *out_w = ow;
  *out_h = oh;

  return dst;
}

struct _RawSession
{
  libraw_data_t *lr;
  gchar         *path;
  ProgressCtx    ctx;
  gboolean       hot_done;
};

RawSession *
raw_session_open (const gchar  *path,
                  GError      **error)
{
  RawSession *ses = g_new0 (RawSession, 1);
  gint        ret;

  ses->lr   = libraw_init (0);
  ses->path = g_strdup (path);

  if (! ses->lr)
    {
      g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_NOMEM,
                           _("Could not initialise LibRaw."));
      raw_session_free (ses);
      return NULL;
    }

  libraw_set_progress_handler (ses->lr, progress_cb, &ses->ctx);
  ses->lr->rawparams.max_raw_memory_mb = 8192;

  ret = open_raw (ses->lr, path);
  if (ret == LIBRAW_SUCCESS)
    {
      if (! check_sizes (ses->lr, path, error))
        {
          raw_session_free (ses);
          return NULL;
        }
      ret = libraw_unpack (ses->lr);
    }

  if (ret != LIBRAW_SUCCESS)
    {
      set_libraw_error (error, path, ret);
      raw_session_free (ses);
      return NULL;
    }

  return ses;
}

const gchar *
raw_session_camera (RawSession *ses)
{
  static gchar buf[160];

  g_snprintf (buf, sizeof (buf), "%s %s", ses->lr->idata.make, ses->lr->idata.model);
  return buf;
}

/* Can be called repeatedly with different settings (LibRaw re-copies the
 * unpacked raw data for each dcraw_process()).  Not thread-safe per session.
 */
RawImage *
raw_session_develop (RawSession         *ses,
                     const RawSettings  *s,
                     gint                max_size,
                     RawProgressFunc     progress,
                     gpointer            progress_data,
                     GError            **error)
{
  libraw_data_t            *lr = ses->lr;
  libraw_processed_image_t *mem;
  RawImage                 *img;
  gint                      ret;

  ses->ctx.func      = progress;
  ses->ctx.data      = progress_data;
  ses->ctx.cancelled = FALSE;

  if (s->hot_pixels && ! ses->hot_done)
    {
      remove_hot_pixels (lr);
      ses->hot_done = TRUE;
    }

  apply_settings (lr, s, max_size > 0 && max_size <= 2048);

  ret = libraw_dcraw_process (lr);
  if (ret != LIBRAW_SUCCESS)
    goto fail;

  mem = libraw_dcraw_make_mem_image (lr, &ret);
  if (! mem)
    goto fail;

  if (mem->type != LIBRAW_IMAGE_BITMAP || mem->bits != 16 ||
      (mem->colors != 3 && mem->colors != 1) ||
      mem->width < 1 || mem->height < 1 ||
      (gint64) mem->width * mem->height > MAX_PIXELS ||
      (guint64) mem->data_size < (guint64) mem->width * mem->height * mem->colors * 2)
    {
      libraw_dcraw_clear_mem (mem);
      ret = LIBRAW_DATA_ERROR;
      goto fail;
    }

  if (s->denoise > 0 && mem->colors == 3 &&
      ! rawheic_wavelet_denoise ((guint16 *) mem->data, mem->width, mem->height,
                                 s->denoise))
    {
      libraw_dcraw_clear_mem (mem);
      ret = LIBRAW_UNSUFFICIENT_MEMORY;
      goto fail;
    }

  img = g_new0 (RawImage, 1);
  img->camera = g_strdup (raw_session_camera (ses));

  if (max_size <= 0 && mem->colors == 3)
    {
      /* full size: hand LibRaw's buffer over, no float copy */
      img->mem      = mem;
      img->pixels16 = (guint16 *) mem->data;
      img->width    = mem->width;
      img->height   = mem->height;
      return img;
    }

  img->pixels = to_float_scaled (mem, max_size, &img->width, &img->height);
  libraw_dcraw_clear_mem (mem);

  if (img->pixels)
    return img;

  raw_image_free (img);
  ret = LIBRAW_UNSUFFICIENT_MEMORY;

fail:
  if (ses->ctx.cancelled)
    ret = LIBRAW_CANCELLED_BY_CALLBACK;
  set_libraw_error (error, ses->path, ret);

  return NULL;
}

void
raw_session_free (RawSession *ses)
{
  if (ses)
    {
      if (ses->lr)
        libraw_close (ses->lr);
      g_free (ses->path);
      g_free (ses);
    }
}

RawImage *
raw_develop (const gchar        *path,
             const RawSettings  *s,
             gint                max_size,
             RawProgressFunc     progress,
             gpointer            progress_data,
             GError            **error)
{
  RawSession *ses = raw_session_open (path, error);
  RawImage   *img;

  if (! ses)
    return NULL;

  img = raw_session_develop (ses, s, max_size, progress, progress_data, error);
  raw_session_free (ses);   /* releases LibRaw's internal image before GIMP copies */

  return img;
}

void
raw_image_free (RawImage *img)
{
  if (img)
    {
      g_free (img->pixels);
      if (img->mem)
        libraw_dcraw_clear_mem (img->mem);
      g_free (img->camera);
      g_free (img);
    }
}

gboolean
raw_probe (const gchar  *path,
           gchar       **camera,
           gint         *width,
           gint         *height,
           GError      **error)
{
  libraw_data_t *lr = libraw_init (0);
  gint           ret;
  gboolean       ok = FALSE;

  if (! lr)
    return FALSE;

  ret = open_raw (lr, path);
  if (ret != LIBRAW_SUCCESS)
    set_libraw_error (error, path, ret);
  else if (check_sizes (lr, path, error))
    {
      gboolean swap = lr->sizes.flip == 5 || lr->sizes.flip == 6;

      if (camera)
        *camera = g_strdup_printf ("%s %s", lr->idata.make, lr->idata.model);
      if (width)
        *width = swap ? lr->sizes.height : lr->sizes.width;
      if (height)
        *height = swap ? lr->sizes.width : lr->sizes.height;
      ok = TRUE;
    }

  libraw_close (lr);

  return ok;
}

GBytes *
raw_extract_thumbnail (const gchar  *path,
                       gint         *out_width,
                       gint         *out_height,
                       gint         *flip,
                       gboolean     *is_jpeg,
                       gint         *bmp_w,
                       gint         *bmp_h,
                       GError      **error)
{
  libraw_data_t            *lr = libraw_init (0);
  libraw_processed_image_t *mem;
  GBytes                   *bytes = NULL;
  gint                      ret;

  if (! lr)
    return NULL;

  ret = open_raw (lr, path);
  if (ret == LIBRAW_SUCCESS && check_sizes (lr, path, error))
    {
      gboolean swap = lr->sizes.flip == 5 || lr->sizes.flip == 6;

      *out_width  = swap ? lr->sizes.height : lr->sizes.width;
      *out_height = swap ? lr->sizes.width  : lr->sizes.height;
      *flip       = lr->sizes.flip;

      ret = libraw_unpack_thumb (lr);
      if (ret == LIBRAW_SUCCESS)
        {
          mem = libraw_dcraw_make_mem_thumb (lr, &ret);
          if (mem)
            {
              if (mem->type == LIBRAW_IMAGE_JPEG && mem->data_size > 0)
                {
                  *is_jpeg = TRUE;
                  bytes = g_bytes_new (mem->data, mem->data_size);
                }
              else if (mem->type == LIBRAW_IMAGE_BITMAP && mem->bits == 8 &&
                       mem->colors == 3 && mem->width > 0 && mem->height > 0 &&
                       mem->data_size >= (guint) mem->width * mem->height * 3)
                {
                  *is_jpeg = FALSE;
                  *bmp_w   = mem->width;
                  *bmp_h   = mem->height;
                  bytes = g_bytes_new (mem->data, (gsize) mem->width * mem->height * 3);
                }
              libraw_dcraw_clear_mem (mem);
            }
        }

      if (! bytes)
        set_libraw_error (error, path, ret == LIBRAW_SUCCESS ?
                          LIBRAW_NO_THUMBNAIL : ret);
    }
  else if (ret != LIBRAW_SUCCESS)
    {
      set_libraw_error (error, path, ret);
    }

  libraw_close (lr);

  return bytes;
}
