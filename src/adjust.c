/* gimp-raw-heic -- Filters > RAW > Quick non-destructive adjustments
 * Copyright (C) 2026 Marek Zettel
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Appends GEGL filters to the selected layer as GIMP 3 non-destructive
 * effects.  They stay editable (Layers dock → effects icon) and are
 * saved in XCF.  Only operations verified to exist in GIMP 3.2 are used.
 */

#include "config.h"

#include <math.h>

#include <libgimp/gimpui.h>

#include "rawheic.h"

static GimpValueArray * adjust_run (GimpProcedure        *procedure,
                                    GimpRunMode           run_mode,
                                    GimpImage            *image,
                                    GimpDrawable        **drawables,
                                    GimpProcedureConfig  *config,
                                    gpointer              run_data);

GimpProcedure *
adjust_procedure_new (GimpPlugIn  *plug_in,
                      const gchar *name)
{
  GimpProcedure *procedure;

  procedure = gimp_image_procedure_new (plug_in, name,
                                        GIMP_PDB_PROC_TYPE_PLUGIN,
                                        adjust_run, NULL, NULL);

  gimp_procedure_set_image_types (procedure, "RGB*");
  gimp_procedure_set_sensitivity_mask (procedure, GIMP_PROCEDURE_SENSITIVE_DRAWABLE);
  gimp_procedure_set_menu_label (procedure, _("Quick adjustments (non-destructive)..."));
  gimp_procedure_add_menu_path (procedure, "<Image>/Filters/RAW");
  gimp_procedure_set_documentation (procedure,
                                    _("Add editable exposure, white balance and tone filters"),
                                    _("Adds GEGL filters as non-destructive layer effects: "
                                      "exposure, colour temperature, shadows/highlights, "
                                      "contrast, saturation and optionally empty Levels "
                                      "and Curves.  Each one stays editable later."),
                                    name);
  gimp_procedure_set_attribution (procedure, "Marek Zettel", "Marek Zettel", "2026");

#define RW G_PARAM_READWRITE
  gimp_procedure_add_double_argument (procedure, "exposure", _("Exposure (EV)"),
                                      _("Exposure in stops"), -5.0, 5.0, 0.0, RW);
  gimp_procedure_add_double_argument (procedure, "black-level", _("Black level"),
                                      _("Black point offset"), -0.1, 0.1, 0.0, RW);
  gimp_procedure_add_double_argument (procedure, "temperature-from", _("Original temperature (K)"),
                                      _("Light the photo was taken in"), 1000.0, 12000.0, 6500.0, RW);
  gimp_procedure_add_double_argument (procedure, "temperature-to", _("Intended temperature (K)"),
                                      _("Light it should look like"), 1000.0, 12000.0, 6500.0, RW);
  gimp_procedure_add_double_argument (procedure, "shadows", _("Shadows"),
                                      _("Lift (+) or deepen (-) shadows"), -100.0, 100.0, 0.0, RW);
  gimp_procedure_add_double_argument (procedure, "highlights", _("Highlights"),
                                      _("Recover (-) or boost (+) highlights"), -100.0, 100.0, 0.0, RW);
  gimp_procedure_add_double_argument (procedure, "contrast", _("Contrast"),
                                      _("Contrast change"), -0.5, 0.5, 0.0, RW);
  gimp_procedure_add_double_argument (procedure, "saturation", _("Saturation"),
                                      _("Saturation scale, 1 = unchanged"), 0.0, 2.0, 1.0, RW);
  gimp_procedure_add_boolean_argument (procedure, "add-levels", _("Add Levels for later editing"),
                                       _("Add an unchanged Levels effect"), FALSE, RW);
  gimp_procedure_add_boolean_argument (procedure, "add-curves", _("Add Curves for later editing"),
                                       _("Add an unchanged Curves effect"), FALSE, RW);
#undef RW

  return procedure;
}

static GimpValueArray *
adjust_run (GimpProcedure        *procedure,
            GimpRunMode           run_mode,
            GimpImage            *image,
            GimpDrawable        **drawables,
            GimpProcedureConfig  *config,
            gpointer              run_data)
{
  GimpDrawable *drawable;
  gdouble       exposure, black, t_from, t_to, shadows, highlights, contrast, saturation;
  gboolean      levels, curves;
  gint          added = 0;

  if (! drawables || ! drawables[0] || drawables[1])
    {
      GError *error = g_error_new_literal (G_FILE_ERROR, G_FILE_ERROR_INVAL,
                                           _("Select exactly one layer."));
      return gimp_procedure_new_return_values (procedure,
                                               GIMP_PDB_CALLING_ERROR, error);
    }
  drawable = drawables[0];

  if (run_mode == GIMP_RUN_INTERACTIVE)
    {
      GtkWidget *dialog;
      gboolean   ok;

      gimp_ui_init (PLUGIN_NAME);
      dialog = gimp_procedure_dialog_new (procedure, config,
                                          _("Quick adjustments (non-destructive)"));
      gimp_procedure_dialog_fill (GIMP_PROCEDURE_DIALOG (dialog), NULL);
      ok = gimp_procedure_dialog_run (GIMP_PROCEDURE_DIALOG (dialog));
      gtk_widget_destroy (dialog);

      if (! ok)
        return gimp_procedure_new_return_values (procedure, GIMP_PDB_CANCEL, NULL);
    }

  g_object_get (config,
                "exposure",         &exposure,
                "black-level",      &black,
                "temperature-from", &t_from,
                "temperature-to",   &t_to,
                "shadows",          &shadows,
                "highlights",       &highlights,
                "contrast",         &contrast,
                "saturation",       &saturation,
                "add-levels",       &levels,
                "add-curves",       &curves,
                NULL);

  gimp_image_undo_group_start (image);

#define MODE GIMP_LAYER_MODE_REPLACE
  if (fabs (exposure) > 1e-4 || fabs (black) > 1e-6)
    {
      gimp_drawable_append_new_filter (drawable, "gegl:exposure", _("Exposure"),
                                       MODE, 1.0,
                                       "exposure", exposure, "black-level", black, NULL);
      added++;
    }

  if (fabs (t_from - t_to) > 0.5)
    {
      gimp_drawable_append_new_filter (drawable, "gegl:color-temperature",
                                       _("Colour temperature"), MODE, 1.0,
                                       "original-temperature", t_from,
                                       "intended-temperature", t_to, NULL);
      added++;
    }

  if (fabs (shadows) > 1e-3 || fabs (highlights) > 1e-3)
    {
      gimp_drawable_append_new_filter (drawable, "gegl:shadows-highlights",
                                       _("Shadows-Highlights"), MODE, 1.0,
                                       "shadows", shadows, "highlights", highlights, NULL);
      added++;
    }

  if (fabs (contrast) > 1e-4)
    {
      gimp_drawable_append_new_filter (drawable, "gimp:brightness-contrast",
                                       _("Contrast"), MODE, 1.0,
                                       "contrast", contrast, NULL);
      added++;
    }

  if (fabs (saturation - 1.0) > 1e-4)
    {
      gimp_drawable_append_new_filter (drawable, "gegl:saturation",
                                       _("Saturation"), MODE, 1.0,
                                       "scale", saturation, NULL);
      added++;
    }

  if (levels)
    {
      gimp_drawable_append_new_filter (drawable, "gimp:levels", _("Levels"),
                                       MODE, 1.0, NULL);
      added++;
    }

  if (curves)
    {
      gimp_drawable_append_new_filter (drawable, "gimp:curves", _("Curves"),
                                       MODE, 1.0, NULL);
      added++;
    }
#undef MODE

  gimp_image_undo_group_end (image);

  if (added == 0 && run_mode == GIMP_RUN_INTERACTIVE)
    gimp_message (_("All values are neutral: no effect was added."));

  if (run_mode != GIMP_RUN_NONINTERACTIVE)
    gimp_displays_flush ();

  return gimp_procedure_new_return_values (procedure, GIMP_PDB_SUCCESS, NULL);
}
