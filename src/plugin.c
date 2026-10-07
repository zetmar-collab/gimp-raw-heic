/* gimp-raw-heic -- RAW and HEIC/HEIF loader for GIMP 3.x
 * Copyright (C) 2026 Marek Zettel
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include "config.h"

#include <libraw/libraw.h>
#include <libheif/heif.h>

#include "rawheic.h"


typedef struct _RawHeic      RawHeic;
typedef struct _RawHeicClass RawHeicClass;

struct _RawHeic
{
  GimpPlugIn parent_instance;
};

struct _RawHeicClass
{
  GimpPlugInClass parent_class;
};

#define RAW_HEIC_TYPE (raw_heic_get_type ())

GType                   raw_heic_get_type         (void);

static GList          * raw_heic_query_procedures (GimpPlugIn          *plug_in);
static GimpProcedure  * raw_heic_create_procedure (GimpPlugIn          *plug_in,
                                                   const gchar         *name);
static gboolean         raw_heic_set_i18n         (GimpPlugIn          *plug_in,
                                                   const gchar         *procedure_name,
                                                   gchar              **gettext_domain,
                                                   gchar              **catalog_dir);
static GimpValueArray * about_run                 (GimpProcedure       *procedure,
                                                   GimpProcedureConfig *config,
                                                   gpointer             run_data);


G_DEFINE_TYPE (RawHeic, raw_heic, GIMP_TYPE_PLUG_IN)

GIMP_MAIN (RAW_HEIC_TYPE)


static void
raw_heic_class_init (RawHeicClass *klass)
{
  GimpPlugInClass *plug_in_class = GIMP_PLUG_IN_CLASS (klass);

  plug_in_class->query_procedures = raw_heic_query_procedures;
  plug_in_class->create_procedure = raw_heic_create_procedure;
  plug_in_class->set_i18n         = raw_heic_set_i18n;
}

static void
raw_heic_init (RawHeic *self)
{
}

/* Translations live in <plug-in folder>/locale/<lang>/LC_MESSAGES. */
static gboolean
raw_heic_set_i18n (GimpPlugIn   *plug_in,
                   const gchar  *procedure_name,
                   gchar       **gettext_domain,
                   gchar       **catalog_dir)
{
  *gettext_domain = g_strdup (GETTEXT_PACKAGE);
  *catalog_dir    = g_strdup ("locale");

  return TRUE;
}

static GList *
raw_heic_query_procedures (GimpPlugIn *plug_in)
{
  GList *list = NULL;

  list = g_list_append (list, g_strdup (RAW_THUMB_PROC));
  list = g_list_append (list, g_strdup (RAW_LOAD_PROC));
  list = g_list_append (list, g_strdup (HEIF_THUMB_PROC));
  list = g_list_append (list, g_strdup (HEIF_LOAD_PROC));
  list = g_list_append (list, g_strdup (REDEVELOP_PROC));
  list = g_list_append (list, g_strdup (ADJUST_PROC));
  list = g_list_append (list, g_strdup (BATCH_PROC));
  list = g_list_append (list, g_strdup (ABOUT_PROC));

  return list;
}

static GimpProcedure *
raw_heic_create_procedure (GimpPlugIn  *plug_in,
                           const gchar *name)
{
  GimpProcedure *procedure = NULL;

  if (! g_strcmp0 (name, RAW_LOAD_PROC))
    {
      procedure = raw_load_procedure_new (plug_in, name);
    }
  else if (! g_strcmp0 (name, RAW_THUMB_PROC))
    {
      procedure = raw_thumb_procedure_new (plug_in, name);
    }
  else if (! g_strcmp0 (name, HEIF_LOAD_PROC) ||
           ! g_strcmp0 (name, HEIF_THUMB_PROC))
    {
      procedure = heif_load_procedure_new (plug_in, name);
    }
  else if (! g_strcmp0 (name, REDEVELOP_PROC))
    {
      procedure = redevelop_procedure_new (plug_in, name);
    }
  else if (! g_strcmp0 (name, BATCH_PROC))
    {
      procedure = batch_procedure_new (plug_in, name);
    }
  else if (! g_strcmp0 (name, ADJUST_PROC))
    {
      procedure = adjust_procedure_new (plug_in, name);
    }
  else if (! g_strcmp0 (name, ABOUT_PROC))
    {
      procedure = gimp_procedure_new (plug_in, name,
                                      GIMP_PDB_PROC_TYPE_PLUGIN,
                                      about_run, NULL, NULL);
      gimp_procedure_set_menu_label (procedure, _("About RAW / HEIC loader"));
      gimp_procedure_add_menu_path (procedure, "<Image>/Help");
      gimp_procedure_set_documentation (procedure,
                                        _("Show gimp-raw-heic version"),
                                        _("Shows plug-in and library versions."),
                                        name);
      gimp_procedure_set_attribution (procedure, "Marek Zettel",
                                      "Marek Zettel", "2026");
      gimp_procedure_add_enum_argument (procedure, "run-mode", "Run mode",
                                        "The run mode", GIMP_TYPE_RUN_MODE,
                                        GIMP_RUN_INTERACTIVE,
                                        G_PARAM_READWRITE);
    }

  return procedure;
}

static GimpValueArray *
about_run (GimpProcedure       *procedure,
           GimpProcedureConfig *config,
           gpointer             run_data)
{
  gchar *msg = g_strdup_printf (_("gimp-raw-heic %s\n"
                                  "LibRaw %s (%d cameras)\n"
                                  "libheif %s\n"
                                  "License: GPL-3.0-or-later"),
                                PLUGIN_VERSION,
                                libraw_version (),
                                libraw_cameraCount (),
                                heif_get_version ());
  gimp_message (msg);
  g_free (msg);

  return gimp_procedure_new_return_values (procedure, GIMP_PDB_SUCCESS, NULL);
}
