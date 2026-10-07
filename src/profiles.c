/* gimp-raw-heic -- ICC profiles for the output colour spaces
 * Copyright (C) 2026 Marek Zettel
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <lcms2.h>

#include "rawheic.h"

typedef struct
{
  const gchar *name;
  cmsCIExyY    white;
  cmsCIExyYTRIPLE primaries;
  gint         trc;   /* 0 = sRGB, 1 = gamma 2.2 (Adobe), 2 = gamma 1.8, 3 = Rec.709 */
} SpaceDef;

#define D65 { 0.3127, 0.3290, 1.0 }
#define D50 { 0.3457, 0.3585, 1.0 }

static const SpaceDef spaces[CS_N] =
{
  [CS_SRGB]       = { "sRGB",          D65, { { 0.640, 0.330, 1 }, { 0.300, 0.600, 1 }, { 0.150, 0.060, 1 } }, 0 },
  [CS_ADOBE]      = { "Adobe RGB (1998) compatible", D65,
                      { { 0.640, 0.330, 1 }, { 0.210, 0.710, 1 }, { 0.150, 0.060, 1 } }, 1 },
  [CS_DISPLAY_P3] = { "Display P3",    D65, { { 0.680, 0.320, 1 }, { 0.265, 0.690, 1 }, { 0.150, 0.060, 1 } }, 0 },
  [CS_PROPHOTO]   = { "ProPhoto RGB compatible", D50,
                      { { 0.7347, 0.2653, 1 }, { 0.1596, 0.8404, 1 }, { 0.0366, 0.0001, 1 } }, 2 },
  [CS_REC2020]    = { "Rec. 2020",     D65, { { 0.708, 0.292, 1 }, { 0.170, 0.797, 1 }, { 0.131, 0.046, 1 } }, 3 },
};

static void
set_text_tag (cmsHPROFILE      profile,
              cmsTagSignature  sig,
              const gchar     *text)
{
  cmsMLU *mlu = cmsMLUalloc (NULL, 1);

  cmsMLUsetASCII (mlu, "en", "US", text);
  cmsWriteTag (profile, sig, mlu);
  cmsMLUfree (mlu);
}

GimpColorProfile *
rawheic_profile_new (ColorSpace cs,
                     gboolean   linear)
{
  /* lcms parametric type 4: Y = (aX + b)^g  for X >= d,  Y = cX otherwise */
  static const cmsFloat64Number srgb_p[5]   = { 2.4, 1.0 / 1.055, 0.055 / 1.055, 1.0 / 12.92, 0.04045 };
  static const cmsFloat64Number rec709_p[5] = { 1.0 / 0.45, 1.0 / 1.099, 0.099 / 1.099, 1.0 / 4.5, 0.081 };
  const SpaceDef   *def;
  cmsToneCurve     *curve;
  cmsToneCurve     *curves[3];
  cmsHPROFILE       lcms;
  GimpColorProfile *profile;
  gchar            *desc;

  cs = CLAMP (cs, 0, CS_N - 1);

  if (cs == CS_SRGB)
    return linear ? gimp_color_profile_new_rgb_srgb_linear ()
                  : gimp_color_profile_new_rgb_srgb ();

  def = &spaces[cs];

  if (linear)
    curve = cmsBuildGamma (NULL, 1.0);
  else
    switch (def->trc)
      {
      case 1:  curve = cmsBuildGamma (NULL, 563.0 / 256.0); break;
      case 2:  curve = cmsBuildGamma (NULL, 1.8); break;
      case 3:  curve = cmsBuildParametricToneCurve (NULL, 4, rec709_p); break;
      default: curve = cmsBuildParametricToneCurve (NULL, 4, srgb_p); break;
      }

  curves[0] = curves[1] = curves[2] = curve;
  lcms = cmsCreateRGBProfile (&def->white, &def->primaries, curves);
  cmsFreeToneCurve (curve);

  if (! lcms)
    return linear ? gimp_color_profile_new_rgb_srgb_linear ()
                  : gimp_color_profile_new_rgb_srgb ();

  desc = g_strdup_printf ("%s%s (gimp-raw-heic)", def->name,
                          linear ? " linear" : "");
  set_text_tag (lcms, cmsSigProfileDescriptionTag, desc);
  set_text_tag (lcms, cmsSigDeviceModelDescTag, desc);
  set_text_tag (lcms, cmsSigDeviceMfgDescTag, "gimp-raw-heic");
  set_text_tag (lcms, cmsSigCopyrightTag, "Public Domain");
  g_free (desc);

  profile = gimp_color_profile_new_from_lcms_profile (lcms, NULL);
  cmsCloseProfile (lcms);

  return profile;
}

GimpColorProfile *
rawheic_profile_rec2100_linear (void)
{
  return rawheic_profile_new (CS_REC2020, TRUE);
}
