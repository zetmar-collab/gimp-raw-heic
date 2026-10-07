/* gimp-raw-heic -- File > Open several RAW files
 * Copyright (C) 2026 Marek Zettel
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Choose settings once (with a preview of the first file), then open a
 * queue of RAW files with a progress bar.  Every file goes through GIMP's
 * normal file loading (see raw_batch_session_* in settings.c), so names,
 * metadata, profile and re-develop data are identical to File > Open.
 * Cancelling stops before the next file.
 */

#include "config.h"

#include <libgimp/gimpui.h>

#include "rawheic.h"

static GimpValueArray * batch_run (GimpProcedure       *procedure,
                                   GimpProcedureConfig *config,
                                   gpointer             run_data);

static const gchar *raw_patterns[] =
{
  "cr2", "cr3", "crw", "nef", "nrw", "arw", "srf", "sr2", "dng", "raf", "orf",
  "rw2", "rwl", "raw", "pef", "srw", "3fr", "fff", "iiq", "erf", "mrw", "kdc",
  "dcr", "mef", "mos", "x3f", "gpr", NULL
};

GimpProcedure *
batch_procedure_new (GimpPlugIn  *plug_in,
                     const gchar *name)
{
  GimpProcedure *procedure;

  procedure = gimp_procedure_new (plug_in, name, GIMP_PDB_PROC_TYPE_PLUGIN,
                                  batch_run, NULL, NULL);

  gimp_procedure_set_menu_label (procedure, _("Open several RAW files..."));
  gimp_procedure_add_menu_path (procedure, "<Image>/File/Open");
  gimp_procedure_set_sensitivity_mask (procedure,
                                       GIMP_PROCEDURE_SENSITIVE_NO_IMAGE      |
                                       GIMP_PROCEDURE_SENSITIVE_NO_DRAWABLES  |
                                       GIMP_PROCEDURE_SENSITIVE_DRAWABLE      |
                                       GIMP_PROCEDURE_SENSITIVE_DRAWABLES);
  gimp_procedure_set_documentation (procedure,
                                    _("Develop several RAW files with the same settings"),
                                    _("Pick RAW files, choose development settings once "
                                      "and open them all; the progress can be cancelled."),
                                    name);
  gimp_procedure_set_attribution (procedure, "Marek Zettel", "Marek Zettel", "2026");

  gimp_procedure_add_enum_argument (procedure, "run-mode", "Run mode", "The run mode",
                                    GIMP_TYPE_RUN_MODE, GIMP_RUN_INTERACTIVE,
                                    G_PARAM_READWRITE);
  gimp_procedure_add_string_array_argument (procedure, "files", _("Files"),
                                            _("Paths of the RAW files to open"),
                                            G_PARAM_READWRITE);
  raw_settings_add_arguments (procedure);

  return procedure;
}

static gchar **
choose_files (void)
{
  GtkWidget     *chooser;
  GtkFileFilter *filter;
  GPtrArray     *paths = g_ptr_array_new ();
  gint           i;

  chooser = gtk_file_chooser_dialog_new (_("Open several RAW files"), NULL,
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         _("_Cancel"), GTK_RESPONSE_CANCEL,
                                         _("_Open"),   GTK_RESPONSE_ACCEPT,
                                         NULL);
  gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (chooser), TRUE);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Camera RAW files"));
  for (i = 0; raw_patterns[i]; i++)
    {
      gchar *lo = g_strdup_printf ("*.%s", raw_patterns[i]);
      gchar *up = g_ascii_strup (lo, -1);

      gtk_file_filter_add_pattern (filter, lo);
      gtk_file_filter_add_pattern (filter, up);
      g_free (lo);
      g_free (up);
    }
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT)
    {
      GSList *list = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER (chooser));
      GSList *l;

      for (l = list; l; l = l->next)
        g_ptr_array_add (paths, l->data);   /* takes ownership */
      g_slist_free (list);
    }

  gtk_widget_destroy (chooser);
  g_ptr_array_add (paths, NULL);

  return (gchar **) g_ptr_array_free (paths, FALSE);
}

static GimpImage *
load_one (const gchar  *path,
          GimpRunMode   run_mode,
          gboolean     *cancelled,
          gchar       **error_msg)
{
  GFile     *file  = g_file_new_for_path (path);
  GimpImage *image = gimp_file_load (run_mode, file);

  if (! image)
    {
      const gchar *err = gimp_pdb_get_last_error (gimp_get_pdb ());

      if (gimp_pdb_get_last_status (gimp_get_pdb ()) == GIMP_PDB_CANCEL)
        *cancelled = TRUE;
      else
        *error_msg = g_strdup (err ? err : "");
    }

  g_object_unref (file);

  return image;
}

static GimpValueArray *
batch_run (GimpProcedure       *procedure,
           GimpProcedureConfig *config,
           gpointer             run_data)
{
  GimpRunMode     run_mode;
  RawSettings     s;
  gchar         **files = NULL;
  GString        *errors = g_string_new (NULL);
  gint            n, i, loaded = 0;
  gboolean        cancelled = FALSE;

  g_object_get (config, "run-mode", &run_mode, "files", &files, NULL);

  if (run_mode == GIMP_RUN_INTERACTIVE && (! files || ! files[0]))
    {
      gimp_ui_init (PLUGIN_NAME);
      g_strfreev (files);
      files = choose_files ();

      if (! files || ! files[0])
        {
          g_strfreev (files);
          g_string_free (errors, TRUE);
          return gimp_procedure_new_return_values (procedure, GIMP_PDB_CANCEL, NULL);
        }
    }

  if (! files || ! files[0])
    {
      GError *error = g_error_new_literal (G_FILE_ERROR, G_FILE_ERROR_INVAL,
                                           _("No files were given."));
      g_strfreev (files);
      g_string_free (errors, TRUE);
      return gimp_procedure_new_return_values (procedure, GIMP_PDB_CALLING_ERROR, error);
    }

  n = g_strv_length (files);

  if (run_mode == GIMP_RUN_INTERACTIVE)
    {
      gchar    *title = g_strdup_printf (ngettext ("Develop %d RAW file",
                                                   "Develop %d RAW files", n), n);
      gboolean  ok    = raw_dialog_run (procedure, config, files[0], title);

      g_free (title);
      if (! ok)
        {
          g_strfreev (files);
          g_string_free (errors, TRUE);
          return gimp_procedure_new_return_values (procedure, GIMP_PDB_CANCEL, NULL);
        }
    }

  raw_settings_from_config (&s, config);
  raw_batch_session_begin (&s, files);

  for (i = 0; i < n && ! cancelled; i++)
    {
      gchar       *base = g_path_get_basename (files[i]);
      gchar       *err  = NULL;
      GimpImage   *image;

      gimp_progress_init_printf (_("(%d/%d) %s"), i + 1, n, base);
      if (! gimp_progress_update ((gdouble) i / n))
        {
          cancelled = TRUE;
          g_free (base);
          break;
        }

      image = load_one (files[i], GIMP_RUN_NONINTERACTIVE, &cancelled, &err);
      if (image)
        {
          loaded++;
          if (run_mode == GIMP_RUN_INTERACTIVE)
            {
              gimp_display_new (image);
              gimp_displays_flush ();
            }
        }
      else if (err)
        {
          g_string_append_printf (errors, "%s: %s\n", base, err);
        }

      g_free (err);
      g_free (base);
    }

  raw_batch_session_end ();
  gimp_progress_end ();

  if (run_mode == GIMP_RUN_INTERACTIVE)
    {
      if (errors->len)
        {
          gchar *msg = g_strdup_printf (_("Some files could not be opened:\n%s"),
                                        errors->str);
          gimp_message (msg);
          g_free (msg);
        }
      if (cancelled && loaded < n)
        gimp_message (_("Opening was cancelled."));
    }

  g_string_free (errors, TRUE);
  g_strfreev (files);

  return gimp_procedure_new_return_values (procedure,
                                           cancelled && loaded == 0 ?
                                           GIMP_PDB_CANCEL : GIMP_PDB_SUCCESS,
                                           NULL);
}
