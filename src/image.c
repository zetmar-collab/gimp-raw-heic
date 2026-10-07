/* gimp-raw-heic -- turning developed pixels into GIMP images
 * Copyright (C) 2026 Marek Zettel
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <string.h>

#include "rawheic.h"


static GimpPrecision
precision_for (const RawSettings *s)
{
  switch (s->depth)
    {
    case DEPTH_U8:
      /* 8-bit linear posterises badly: always perceptual */
      return GIMP_PRECISION_U8_NON_LINEAR;
    case DEPTH_FLOAT:
      return s->linear ? GIMP_PRECISION_FLOAT_LINEAR : GIMP_PRECISION_FLOAT_NON_LINEAR;
    case DEPTH_U16:
    default:
      return s->linear ? GIMP_PRECISION_U16_LINEAR : GIMP_PRECISION_U16_NON_LINEAR;
    }
}

static gboolean
image_is_linear (const RawSettings *s)
{
  return s->linear && s->depth != DEPTH_U8;
}

/* Write linear float RGB in the settings' primaries into @drawable;
 * babl converts to the drawable's own space and encoding.
 */
static gboolean
write_pixels (GimpDrawable      *drawable,
              const RawImage    *img,
              ColorSpace         cs,
              GError           **error)
{
  GimpColorProfile *src_profile = rawheic_profile_new (cs, TRUE);
  const Babl       *src_format;
  GeglBuffer       *buffer;
  gint              y, rows;

  src_format = gimp_color_profile_get_format (src_profile,
                                              babl_format (img->pixels16 ? "RGB u16"
                                                                         : "RGB float"),
                                              GIMP_COLOR_RENDERING_INTENT_RELATIVE_COLORIMETRIC,
                                              error);
  g_object_unref (src_profile);

  if (! src_format)
    return FALSE;

  buffer = gimp_drawable_get_buffer (drawable);

  /* in strips so that progress can move on very large images */
  rows = MAX (1, (gint) (16 * 1024 * 1024 / ((gsize) img->width * 12)));
  for (y = 0; y < img->height; y += rows)
    {
      gint          h   = MIN (rows, img->height - y);
      gsize         off = (gsize) y * img->width * 3;
      gconstpointer src = img->pixels16 ? (gconstpointer) (img->pixels16 + off)
                                        : (gconstpointer) (img->pixels + off);

      gegl_buffer_set (buffer, GEGL_RECTANGLE (0, y, img->width, h), 0,
                       src_format, src, GEGL_AUTO_ROWSTRIDE);
    }

  g_object_unref (buffer);

  return TRUE;
}

GimpImage *
rawheic_image_from_raw (RawImage           *img,
                        const RawSettings  *s,
                        GFile              *file,
                        GError            **error)
{
  GimpImage        *image;
  GimpLayer        *layer;
  GimpColorProfile *profile;
  gchar            *name;

  image = gimp_image_new_with_precision (img->width, img->height,
                                         GIMP_RGB, precision_for (s));
  if (! image)
    {
      g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_NOMEM,
                           _("Could not create the image."));
      return NULL;
    }

  profile = rawheic_profile_new (s->color_space, image_is_linear (s));
  gimp_image_set_color_profile (image, profile);
  g_object_unref (profile);

  name  = file ? g_file_get_basename (file) : g_strdup (_("RAW"));
  layer = gimp_layer_new (image, name, img->width, img->height,
                          GIMP_RGB_IMAGE, 100.0,
                          gimp_image_get_default_new_layer_mode (image));
  g_free (name);

  gimp_image_insert_layer (image, layer, NULL, 0);
  rawheic_mark_layer (layer);

  if (! write_pixels (GIMP_DRAWABLE (layer), img, s->color_space, error))
    {
      gimp_image_delete (image);
      return NULL;
    }

  return image;
}

void
rawheic_mark_layer (GimpLayer *layer)
{
  GimpParasite *p = gimp_parasite_new (LAYER_PARASITE, GIMP_PARASITE_PERSISTENT,
                                       2, "1");

  gimp_item_attach_parasite (GIMP_ITEM (layer), p);
  gimp_parasite_free (p);
}

void
rawheic_attach_source (GimpImage         *image,
                       const RawSettings *s,
                       const gchar       *path)
{
  gchar        *text = raw_settings_serialize (s, path);
  GimpParasite *parasite;

  parasite = gimp_parasite_new (SOURCE_PARASITE, GIMP_PARASITE_PERSISTENT,
                                strlen (text) + 1, text);
  gimp_image_attach_parasite (image, parasite);
  gimp_parasite_free (parasite);
  g_free (text);
}

/* used by redevelop.c */
gboolean
rawheic_write_layer (GimpDrawable   *drawable,
                     const RawImage *img,
                     ColorSpace      cs,
                     GError        **error)
{
  return write_pixels (drawable, img, cs, error);
}
