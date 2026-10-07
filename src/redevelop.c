/* gimp-raw-heic -- Filters > RAW > Develop again
 * Copyright (C) 2026 Marek Zettel
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The RAW source path and the settings used are stored in the image as a
 * persistent parasite (saved in XCF).  This procedure reads them, lets the
 * user change the settings and develops the original RAW file again into
 * the existing image, replacing the developed layer or adding a new one.
 */

#include "config.h"

#include <string.h>

#include "rawheic.h"

enum
{
  REDEV_REPLACE,
  REDEV_NEW_LAYER
};

static GimpValueArray * redevelop_run (GimpProcedure        *procedure,
                                       GimpRunMode           run_mode,
                                       GimpImage            *image,
                                       GimpDrawable        **drawables,
                                       GimpProcedureConfig  *config,
                                       gpointer              run_data);


GimpProcedure *
redevelop_procedure_new (GimpPlugIn  *plug_in,
                         const gchar *name)
{
  GimpProcedure *procedure;

  procedure = gimp_image_procedure_new (plug_in, name,
                                        GIMP_PDB_PROC_TYPE_PLUGIN,
                                        redevelop_run, NULL, NULL);

  gimp_procedure_set_image_types (procedure, "RGB*");
  gimp_procedure_set_sensitivity_mask (procedure,
                                       GIMP_PROCEDURE_SENSITIVE_DRAWABLE  |
                                       GIMP_PROCEDURE_SENSITIVE_DRAWABLES |
                                       GIMP_PROCEDURE_SENSITIVE_NO_DRAWABLES);
  gimp_procedure_set_menu_label (procedure, _("Develop RAW again..."));
  gimp_procedure_add_menu_path (procedure, "<Image>/Filters/RAW");
  gimp_procedure_set_documentation (procedure,
                                    _("Develop the original RAW file again with new settings"),
                                    _("Uses the RAW file path and settings stored in the "
                                      "image (also in XCF files) to develop the RAW again, "
                                      "replacing the developed layer or adding a new layer."),
                                    name);
  gimp_procedure_set_attribution (procedure, "Marek Zettel", "Marek Zettel", "2026");

  raw_settings_add_arguments (procedure);

  gimp_procedure_add_choice_argument (procedure, "redevelop-mode",
                                      _("Result"),
                                      _("Replace the developed layer or add a new one"),
                                      gimp_choice_new_with_values (
                                        "replace",   REDEV_REPLACE,
                                        _("Replace the developed layer"), NULL,
                                        "new-layer", REDEV_NEW_LAYER,
                                        _("Add as a new layer"), NULL,
                                        NULL),
                                      "replace", G_PARAM_READWRITE);

  return procedure;
}

/* The layer created from the RAW carries a marker parasite. */
static GimpLayer *
find_raw_layer (GimpImage     *image,
                GimpDrawable **drawables)
{
  GimpLayer **layers;
  GimpLayer  *found = NULL;
  gint        i;

  if (drawables && drawables[0] && ! drawables[1] && GIMP_IS_LAYER (drawables[0]))
    {
      GimpParasite *p = gimp_item_get_parasite (GIMP_ITEM (drawables[0]),
                                                LAYER_PARASITE);
      if (p)
        {
          gimp_parasite_free (p);
          return GIMP_LAYER (drawables[0]);
        }
    }

  layers = gimp_image_get_layers (image);
  for (i = 0; layers && layers[i] && ! found; i++)
    {
      GimpParasite *p = gimp_item_get_parasite (GIMP_ITEM (layers[i]), LAYER_PARASITE);

      if (p)
        {
          found = layers[i];
          gimp_parasite_free (p);
        }
    }
  g_free (layers);

  return found;
}

typedef struct
{
  gdouble last;
} Prog;

static gboolean
redevelop_progress (gdouble  fraction,
                    gpointer data)
{
  Prog *p = data;

  if (fraction - p->last >= 0.02)
    {
      p->last = fraction;
      return gimp_progress_update (fraction * 0.9);
    }

  return TRUE;
}

static GimpValueArray *
redevelop_run (GimpProcedure        *procedure,
               GimpRunMode           run_mode,
               GimpImage            *image,
               GimpDrawable        **drawables,
               GimpProcedureConfig  *config,
               gpointer              run_data)
{
  GimpParasite *parasite;
  RawSettings   stored, s;
  RawImage     *img;
  GimpLayer    *target, *layer;
  GError       *error = NULL;
  gchar        *text, *path = NULL, *name;
  gconstpointer data;
  guint32       size = 0;
  gint          mode, position = 0;
  Prog          prog = { 0.0 };

  parasite = gimp_image_get_parasite (image, SOURCE_PARASITE);
  if (! parasite)
    {
      g_set_error (&error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   _("This image was not loaded from a RAW file by gimp-raw-heic, "
                     "so there is nothing to develop again."));
      return gimp_procedure_new_return_values (procedure,
                                               GIMP_PDB_EXECUTION_ERROR, error);
    }

  data = gimp_parasite_get_data (parasite, &size);
  text = g_strndup (data, size);
  gimp_parasite_free (parasite);

  if (! raw_settings_deserialize (&stored, text, &path) || ! path)
    {
      g_free (text);
      g_free (path);
      g_set_error (&error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   _("The RAW information stored in this image is damaged."));
      return gimp_procedure_new_return_values (procedure,
                                               GIMP_PDB_EXECUTION_ERROR, error);
    }
  g_free (text);

  if (! g_file_test (path, G_FILE_TEST_IS_REGULAR))
    {
      g_set_error (&error, G_FILE_ERROR, G_FILE_ERROR_NOENT,
                   _("The original RAW file was not found:\n%s\n\n"
                     "Put it back at this location to develop it again."), path);
      g_free (path);
      return gimp_procedure_new_return_values (procedure,
                                               GIMP_PDB_EXECUTION_ERROR, error);
    }

  if (run_mode == GIMP_RUN_INTERACTIVE)
    {
      gchar *base  = g_path_get_basename (path);
      gchar *title = g_strdup_printf (_("Develop RAW again – %s"), base);

      /* start from what was used for this image, not from last values */
      raw_settings_to_config (&stored, config);

      if (! raw_dialog_run (procedure, config, path, title))
        {
          g_free (title);
          g_free (base);
          g_free (path);
          return gimp_procedure_new_return_values (procedure, GIMP_PDB_CANCEL, NULL);
        }
      g_free (title);
      g_free (base);
    }

  raw_settings_from_config (&s, config);
  mode = gimp_procedure_config_get_choice_id (config, "redevelop-mode");

  gimp_progress_init (_("Developing RAW again"));
  img = raw_develop (path, &s, 0, redevelop_progress, &prog, &error);
  if (! img)
    {
      g_free (path);
      return gimp_procedure_new_return_values (procedure,
                                               g_error_matches (error, G_IO_ERROR,
                                                                G_IO_ERROR_CANCELLED) ?
                                               GIMP_PDB_CANCEL : GIMP_PDB_EXECUTION_ERROR,
                                               error);
    }

  gimp_image_undo_group_start (image);

  target = find_raw_layer (image, drawables);

  if (mode == REDEV_REPLACE && target)
    {
      GimpItem *parent = gimp_item_get_parent (GIMP_ITEM (target));

      name     = gimp_item_get_name (GIMP_ITEM (target));
      position = gimp_image_get_item_position (image, GIMP_ITEM (target));

      layer = gimp_layer_new (image, name, img->width, img->height,
                              GIMP_RGB_IMAGE, 100.0,
                              gimp_image_get_default_new_layer_mode (image));
      gimp_image_insert_layer (image, layer, GIMP_LAYER (parent), position);

      /* the RAW layer defined the canvas: follow a size change */
      if (gimp_drawable_get_width (GIMP_DRAWABLE (target))  == gimp_image_get_width (image) &&
          gimp_drawable_get_height (GIMP_DRAWABLE (target)) == gimp_image_get_height (image) &&
          (img->width  != gimp_image_get_width (image) ||
           img->height != gimp_image_get_height (image)))
        gimp_image_resize (image, img->width, img->height, 0, 0);

      gimp_image_remove_layer (image, target);
    }
  else
    {
      gchar *base = g_path_get_basename (path);

      name  = g_strdup_printf (_("%s (developed again)"), base);
      layer = gimp_layer_new (image, name, img->width, img->height,
                              GIMP_RGB_IMAGE, 100.0,
                              gimp_image_get_default_new_layer_mode (image));
      gimp_image_insert_layer (image, layer, NULL, 0);
      g_free (base);
    }
  g_free (name);

  if (! rawheic_write_layer (GIMP_DRAWABLE (layer), img, s.color_space, &error))
    {
      gimp_image_undo_group_end (image);
      raw_image_free (img);
      g_free (path);
      return gimp_procedure_new_return_values (procedure,
                                               GIMP_PDB_EXECUTION_ERROR, error);
    }

  rawheic_mark_layer (layer);
  rawheic_attach_source (image, &s, path);

  gimp_image_undo_group_end (image);
  gimp_progress_update (1.0);

  if (run_mode != GIMP_RUN_NONINTERACTIVE)
    gimp_displays_flush ();

  raw_image_free (img);
  g_free (path);

  return gimp_procedure_new_return_values (procedure, GIMP_PDB_SUCCESS, NULL);
}
