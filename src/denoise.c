/* gimp-raw-heic -- wavelet noise reduction
 * Copyright (C) 2026 Marek Zettel
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * LibRaw's own wavelet denoise (params.threshold) crashes in the LibRaw
 * 0.22.2 build shipped by MSYS2 (also in its dcraw_emu), so it is never
 * used.  This is an independent à-trous wavelet soft-threshold denoiser
 * applied to the developed 16-bit linear RGB.  Values are square-root
 * transformed first (stabilises photon noise) so that the threshold has
 * roughly the same meaning as dcraw's "-n".
 */

#include "config.h"

#include <math.h>
#include <string.h>

#include "rawheic.h"

/* relative noise of each level for a B3-like hat filter */
static const gfloat level_noise[5] = { 0.8002f, 0.2735f, 0.1202f, 0.0585f, 0.0291f };

/* 1-D hat transform with mirrored borders; requires size > 2 * sc */
static void
hat_transform (gfloat       *out,
               const gfloat *base,
               gsize         stride,
               gint          size,
               gint          sc)
{
  gint i;

  for (i = 0; i < sc; i++)
    out[i] = 2 * base[stride * i] + base[stride * (sc - i)] + base[stride * (i + sc)];
  for (; i + sc < size; i++)
    out[i] = 2 * base[stride * i] + base[stride * (i - sc)] + base[stride * (i + sc)];
  for (; i < size; i++)
    out[i] = 2 * base[stride * i] + base[stride * (i - sc)] +
             base[stride * (2 * size - 2 - (i + sc))];
}

gboolean
rawheic_wavelet_denoise (guint16 *rgb,
                         gint     width,
                         gint     height,
                         gdouble  threshold)
{
  gsize   size = (gsize) width * height;
  gfloat *fimg;
  gint    c;

  /* the coarsest level uses a step of 16 pixels */
  if (threshold <= 0 || width <= 2 * 16 + 1 || height <= 2 * 16 + 1)
    return TRUE;

  fimg = g_try_malloc (size * 3 * sizeof (gfloat));
  if (! fimg)
    return FALSE;

  for (c = 0; c < 3; c++)
    {
      gsize  i, hpass = 0, lpass = 0;
      gint   lev;

      for (i = 0; i < size; i++)
        fimg[i] = 256.0f * sqrtf ((gfloat) rgb[i * 3 + c]);

      for (lev = 0; lev < 5; lev++)
        {
          gfloat thold = (gfloat) threshold * level_noise[lev];
          gint   sc    = 1 << lev;
          gint   row, col;

          lpass = size * ((lev & 1) + 1);

#pragma omp parallel
          {
            gfloat *temp = g_malloc (MAX (width, height) * sizeof (gfloat));

#pragma omp for
            for (row = 0; row < height; row++)
              {
                gint x;

                hat_transform (temp, fimg + hpass + (gsize) row * width, 1, width, sc);
                for (x = 0; x < width; x++)
                  fimg[lpass + (gsize) row * width + x] = temp[x] * 0.25f;
              }

#pragma omp for
            for (col = 0; col < width; col++)
              {
                gint y;

                hat_transform (temp, fimg + lpass + col, width, height, sc);
                for (y = 0; y < height; y++)
                  fimg[lpass + (gsize) y * width + col] = temp[y] * 0.25f;
              }

            g_free (temp);
          }

#pragma omp parallel for
          for (i = 0; i < size; i++)
            {
              gfloat d = fimg[hpass + i] - fimg[lpass + i];

              if      (d < -thold) d += thold;
              else if (d >  thold) d -= thold;
              else                 d  = 0.0f;

              fimg[hpass + i] = d;
              if (hpass)
                fimg[i] += d;
            }

          hpass = lpass;
        }

#pragma omp parallel for
      for (i = 0; i < size; i++)
        {
          gfloat v = fimg[i] + fimg[lpass + i];

          v = v * v / 65536.0f;
          rgb[i * 3 + c] = (guint16) CLAMP (v + 0.5f, 0.0f, 65535.0f);
        }
    }

  g_free (fimg);

  return TRUE;
}
