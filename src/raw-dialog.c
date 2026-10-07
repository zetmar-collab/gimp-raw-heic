/* gimp-raw-heic -- RAW import / re-develop dialog with live preview
 * Copyright (C) 2026 Marek Zettel
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <string.h>

#include <libgimp/gimpui.h>

#include "rawheic.h"

#define PREVIEW_W     640
#define PREVIEW_H     480
#define PREVIEW_SIZE  640
#define SPOT_SIZE     0.04

typedef struct
{
  GimpProcedureConfig *config;
  gchar               *path;
  GtkWidget           *area;
  GtkWidget           *status;

  RawSession          *session;
  gboolean             session_failed;
  gboolean             busy;
  gboolean             pending;
  gboolean             closing;
  guint                timeout_id;

  /* last rendered preview, sRGB u8 */
  guchar              *rgb;
  gint                 rgb_w;
  gint                 rgb_h;
  ColorSpace           rgb_cs;
} PreviewUI;

typedef struct
{
  RawSettings  settings;
  RawImage    *image;
  gchar       *error;
  gint64       usecs;
} RenderJob;


static void render_start (PreviewUI *ui);


static void
render_job_free (RenderJob *job)
{
  raw_image_free (job->image);
  g_free (job->error);
  g_free (job);
}

static void
render_thread (GTask        *task,
               gpointer      source,
               gpointer      task_data,
               GCancellable *cancellable)
{
  PreviewUI *ui;
  RenderJob *job = task_data;
  GError    *error = NULL;
  gint64     t0 = g_get_monotonic_time ();

  ui = g_object_get_data (G_OBJECT (task), "ui");

  if (! ui->session && ! ui->session_failed)
    {
      ui->session = raw_session_open (ui->path, &error);
      if (! ui->session)
        ui->session_failed = TRUE;
    }

  if (ui->session)
    job->image = raw_session_develop (ui->session, &job->settings,
                                      PREVIEW_SIZE, NULL, NULL, &error);

  if (error)
    {
      job->error = g_strdup (error->message);
      g_error_free (error);
    }

  job->usecs = g_get_monotonic_time () - t0;
}

static void
draw_preview (PreviewUI *ui)
{
  GimpPreviewArea *area = GIMP_PREVIEW_AREA (ui->area);
  RawSettings      s;
  gint             ox, oy;

  gimp_preview_area_fill (area, 0, 0, PREVIEW_W, PREVIEW_H, 128, 128, 128);

  if (! ui->rgb)
    return;

  ox = (PREVIEW_W - ui->rgb_w) / 2;
  oy = (PREVIEW_H - ui->rgb_h) / 2;

  gimp_preview_area_draw (area, ox, oy, ui->rgb_w, ui->rgb_h,
                          GIMP_RGB_IMAGE, ui->rgb, ui->rgb_w * 3);

  raw_settings_from_config (&s, ui->config);
  if (s.wb_mode == WB_SPOT)
    {
      gint x = ox + (gint) (s.spot[0] * ui->rgb_w);
      gint y = oy + (gint) (s.spot[1] * ui->rgb_h);
      gint w = MAX (3, (gint) (s.spot[2] * ui->rgb_w));
      gint h = MAX (3, (gint) (s.spot[3] * ui->rgb_h));

      gimp_preview_area_fill (area, x, y, w, 1, 255, 255, 0);
      gimp_preview_area_fill (area, x, y + h - 1, w, 1, 255, 255, 0);
      gimp_preview_area_fill (area, x, y, 1, h, 255, 255, 0);
      gimp_preview_area_fill (area, x + w - 1, y, 1, h, 255, 255, 0);
    }
}

static void
render_done (GObject      *source,
             GAsyncResult *result,
             gpointer      data)
{
  PreviewUI *ui  = data;
  RenderJob *job = g_task_get_task_data (G_TASK (result));

  ui->busy = FALSE;

  if (ui->closing)
    return;

  if (job->image)
    {
      GimpColorProfile *profile = rawheic_profile_new (job->settings.color_space, TRUE);
      const Babl       *src;
      gsize             n = (gsize) job->image->width * job->image->height;

      src = gimp_color_profile_get_format (profile, babl_format ("RGB float"),
                                           GIMP_COLOR_RENDERING_INTENT_RELATIVE_COLORIMETRIC,
                                           NULL);
      g_object_unref (profile);

      g_free (ui->rgb);
      ui->rgb   = g_malloc (n * 3);
      ui->rgb_w = job->image->width;
      ui->rgb_h = job->image->height;
      babl_process (babl_fish (src, babl_format ("R'G'B' u8")),
                    job->image->pixels, ui->rgb, n);

      gtk_label_set_text (GTK_LABEL (ui->status), "");
      {
        gchar *msg = g_strdup_printf (_("%s — preview %d ms"), job->image->camera,
                                      (gint) (job->usecs / 1000));
        gtk_label_set_text (GTK_LABEL (ui->status), msg);
        g_free (msg);
      }
    }
  else if (job->error)
    {
      gtk_label_set_text (GTK_LABEL (ui->status), job->error);
    }

  draw_preview (ui);

  if (ui->pending)
    {
      ui->pending = FALSE;
      render_start (ui);
    }
}

static void
render_start (PreviewUI *ui)
{
  GTask     *task;
  RenderJob *job;

  if (ui->busy)
    {
      ui->pending = TRUE;
      return;
    }

  job = g_new0 (RenderJob, 1);
  raw_settings_from_config (&job->settings, ui->config);

  ui->busy = TRUE;
  gtk_label_set_text (GTK_LABEL (ui->status), _("Developing preview…"));

  task = g_task_new (NULL, NULL, render_done, ui);
  g_object_set_data (G_OBJECT (task), "ui", ui);
  g_task_set_task_data (task, job, (GDestroyNotify) render_job_free);
  g_task_run_in_thread (task, render_thread);
  g_object_unref (task);
}

static gboolean
render_timeout (gpointer data)
{
  PreviewUI *ui = data;

  ui->timeout_id = 0;
  render_start (ui);

  return G_SOURCE_REMOVE;
}

static void
config_changed (GObject    *config,
                GParamSpec *pspec,
                PreviewUI  *ui)
{
  /* properties that do not change the preview */
  if (! strcmp (pspec->name, "ask-dialog") ||
      ! strcmp (pspec->name, "depth")      ||
      ! strcmp (pspec->name, "half-size")  ||
      ! strcmp (pspec->name, "linear")     ||
      ! strcmp (pspec->name, "demosaic")   ||
      ! strcmp (pspec->name, "median-passes"))
    return;

  if (ui->timeout_id)
    g_source_remove (ui->timeout_id);

  ui->timeout_id = g_timeout_add (250, render_timeout, ui);
}

static gboolean
area_clicked (GtkWidget      *widget,
              GdkEventButton *event,
              PreviewUI      *ui)
{
  gdouble fx, fy;

  if (event->button != 1 || ! ui->rgb)
    return FALSE;

  fx = (event->x - (PREVIEW_W - ui->rgb_w) / 2) / ui->rgb_w;
  fy = (event->y - (PREVIEW_H - ui->rgb_h) / 2) / ui->rgb_h;

  if (fx < 0 || fy < 0 || fx > 1 || fy > 1)
    return FALSE;

  g_object_set (ui->config,
                "spot-x",      CLAMP (fx - SPOT_SIZE / 2, 0.0, 1.0 - SPOT_SIZE),
                "spot-y",      CLAMP (fy - SPOT_SIZE / 2, 0.0, 1.0 - SPOT_SIZE),
                "spot-width",  SPOT_SIZE,
                "spot-height", SPOT_SIZE,
                "wb-mode",     "spot",
                NULL);

  return TRUE;
}

static void
set_sensitive_if (GimpProcedureDialog *dialog,
                  const gchar         *property,
                  const gchar         *choice_property,
                  const gchar         *value)
{
  GimpValueArray *values = gimp_value_array_new (1);
  GValue          v      = G_VALUE_INIT;

  g_value_init (&v, G_TYPE_STRING);
  g_value_set_string (&v, value);
  gimp_value_array_append (values, &v);
  g_value_unset (&v);

  gimp_procedure_dialog_set_sensitive_if_in (dialog, property, NULL,
                                             choice_property, values, TRUE);
}

gboolean
raw_dialog_run (GimpProcedure       *procedure,
                GimpProcedureConfig *config,
                const gchar         *path,
                const gchar         *title)
{
  GimpProcedureDialog *dialog;
  PreviewUI            ui = { 0 };
  GtkWidget           *hbox, *vbox, *frame, *right;
  gboolean             run;
  gulong               handler;

  gimp_ui_init (PLUGIN_NAME);

  ui.config = config;
  ui.path   = g_strdup (path);

  dialog = GIMP_PROCEDURE_DIALOG (gimp_procedure_dialog_new (procedure, config, title));
  gimp_procedure_dialog_set_ok_label (dialog, _("_Develop"));

  /* settings pages */
  gimp_procedure_dialog_get_label (dialog, "wb-label",  _("White balance"), FALSE, FALSE);
  gimp_procedure_dialog_get_label (dialog, "exp-label", _("Exposure"),      FALSE, FALSE);
  gimp_procedure_dialog_get_label (dialog, "det-label", _("Detail"),        FALSE, FALSE);
  gimp_procedure_dialog_get_label (dialog, "out-label", _("Output"),        FALSE, FALSE);

  gimp_procedure_dialog_get_label (dialog, "wb-hint",
                                   _("Tip: click the preview to pick a neutral (grey) area."),
                                   FALSE, FALSE);

  gimp_procedure_dialog_fill_box (dialog, "wb-box",
                                  "wb-mode", "wb-temperature", "wb-tint", "wb-hint", NULL);
  gimp_procedure_dialog_fill_box (dialog, "exp-box",
                                  "exposure", "exposure-preserve", "highlight",
                                  "rebuild-level", "brightness", "auto-bright", NULL);
  gimp_procedure_dialog_fill_box (dialog, "det-box",
                                  "demosaic", "denoise", "fbdd", "ca-red", "ca-blue",
                                  "hot-pixels", "median-passes", NULL);
  gimp_procedure_dialog_fill_box (dialog, "out-box",
                                  "color-space", "linear", "depth", "half-size",
                                  "auto-rotate", "camera-crop", NULL);

  gimp_procedure_dialog_fill_notebook (dialog, "settings",
                                       "wb-label",  "wb-box",
                                       "exp-label", "exp-box",
                                       "det-label", "det-box",
                                       "out-label", "out-box",
                                       NULL);

  set_sensitive_if (dialog, "wb-temperature", "wb-mode", "manual");
  set_sensitive_if (dialog, "wb-tint",        "wb-mode", "manual");
  set_sensitive_if (dialog, "rebuild-level",  "highlight", "rebuild");

  if (g_object_class_find_property (G_OBJECT_GET_CLASS (config), "ask-dialog"))
    right = gimp_procedure_dialog_fill_box (dialog, "right", "settings", "ask-dialog", NULL);
  else if (g_object_class_find_property (G_OBJECT_GET_CLASS (config), "redevelop-mode"))
    right = gimp_procedure_dialog_fill_box (dialog, "right", "settings", "redevelop-mode", NULL);
  else
    right = gimp_procedure_dialog_fill_box (dialog, "right", "settings", NULL);

  /* preview */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 12);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                      hbox, TRUE, TRUE, 0);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

  ui.area = gimp_preview_area_new ();
  gtk_widget_set_size_request (ui.area, PREVIEW_W, PREVIEW_H);
  gtk_widget_add_events (ui.area, GDK_BUTTON_PRESS_MASK);
  g_signal_connect (ui.area, "button-press-event", G_CALLBACK (area_clicked), &ui);
  gtk_container_add (GTK_CONTAINER (frame), ui.area);

  ui.status = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (ui.status), 0.0);
  gtk_label_set_ellipsize (GTK_LABEL (ui.status), PANGO_ELLIPSIZE_END);
  gtk_box_pack_start (GTK_BOX (vbox), ui.status, FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (hbox), right, TRUE, TRUE, 0);
  gtk_widget_show_all (hbox);

  handler = g_signal_connect (config, "notify", G_CALLBACK (config_changed), &ui);
  render_start (&ui);

  run = gimp_procedure_dialog_run (dialog);

  g_signal_handler_disconnect (config, handler);
  if (ui.timeout_id)
    g_source_remove (ui.timeout_id);

  /* the worker thread uses the session: wait for it */
  ui.closing = TRUE;
  while (ui.busy)
    g_main_context_iteration (NULL, TRUE);

  gtk_widget_destroy (GTK_WIDGET (dialog));

  raw_session_free (ui.session);
  g_free (ui.rgb);
  g_free (ui.path);

  return run;
}
