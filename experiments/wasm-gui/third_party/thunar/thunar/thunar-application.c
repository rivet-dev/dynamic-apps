/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2005      Jeff Franks <jcfranks@xfce.org>
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include <stdlib.h>

#ifdef HAVE_GUDEV
#include <gudev/gudev.h>
#endif

#include <libxfce4ui/libxfce4ui.h>

#include <thunar/thunar-application.h>
#include <thunar/thunar-browser.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-gdk-extensions.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-io-jobs.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-progress-dialog.h>
#include <thunar/thunar-renamer-dialog.h>
#include <thunar/thunar-thumbnail-cache.h>
#include <thunar/thunar-thumbnailer.h>
#include <thunar/thunar-transfer-job.h>
#include <thunar/thunar-util.h>
#include <thunar/thunar-view.h>
#include <thunar/thunar-session-client.h>
#include <thunar/thunar-dbus-service.h>

#define ACCEL_MAP_PATH "Thunar/accels.scm"



/* option values */
static gboolean opt_version = FALSE;
/* the sm-client-id option is only honored while starting the primary instance,
 * and will be silently ignored on every other invocation */
static gchar   *opt_sm_client_id = NULL;

/* option entries */
static const GOptionEntry option_entries[] =
{
  { "bulk-rename", 'B', 0, G_OPTION_ARG_NONE, NULL, N_ ("Open the bulk rename dialog"), NULL, },
  { "daemon", 0, 0, G_OPTION_ARG_NONE, NULL, N_ ("Run in daemon mode"), NULL, },
  { "sm-client-id", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &opt_sm_client_id, NULL, NULL, },
  { "quit", 'q', 0, G_OPTION_ARG_NONE, NULL, N_ ("Quit a running Thunar instance"), NULL, },
  { "version", 'V', 0, G_OPTION_ARG_NONE, &opt_version, N_ ("Print version information and exit"), NULL, },
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, NULL, NULL, NULL, },
  { NULL, },
};



/* Prototype for the Thunar job launchers */
typedef ThunarJob *(*Launcher) (GList *source_path_list,
                                GList *target_path_list);



/* Property identifiers */
enum
{
  PROP_0,
  PROP_DAEMON,
};



static void           thunar_application_finalize               (GObject                *object);
static void           thunar_application_get_property           (GObject                *object,
                                                                 guint                   prop_id,
                                                                 GValue                 *value,
                                                                 GParamSpec             *pspec);
static void           thunar_application_set_property           (GObject                *object,
                                                                 guint                   prop_id,
                                                                 const GValue           *value,
                                                                 GParamSpec             *pspec);
static void           thunar_application_dbus_acquired_cb       (GDBusConnection        *conn,
                                                                 const gchar            *name,
                                                                 gpointer                user_data);
static void           thunar_application_name_acquired_cb       (GDBusConnection        *connection,
                                                                 const gchar            *name,
                                                                 gpointer                user_data);
static void           thunar_application_dbus_name_lost_cb      (GDBusConnection        *connection,
                                                                 const gchar            *name,
                                                                 gpointer                user_data);
static void           thunar_application_dbus_init              (ThunarApplication      *application);
static void           thunar_application_startup                (GApplication           *application);
static void           thunar_application_shutdown               (GApplication           *application);
static void           thunar_application_activate               (GApplication           *application);
static int            thunar_application_handle_local_options   (GApplication           *application,
                                                                 GVariantDict           *options);
static int            thunar_application_command_line           (GApplication           *application,
                                                                 GApplicationCommandLine *command_line);
static gboolean       thunar_application_dbus_register          (GApplication           *application,
                                                                 GDBusConnection        *connection,
                                                                 const gchar            *object_path,
                                                                 GError                **error);
static void           thunar_application_load_css               (void);
static void           thunar_application_accel_map_changed      (ThunarApplication      *application);
static gboolean       thunar_application_accel_map_save         (gpointer                user_data);
static gboolean       thunar_application_accel_map_load         (gpointer                user_data);
static void           thunar_application_collect_and_launch     (ThunarApplication      *application,
                                                                 gpointer                parent,
                                                                 const gchar            *icon_name,
                                                                 const gchar            *title,
                                                                 Launcher                launcher,
                                                                 GList                  *source_file_list,
                                                                 GFile                  *target_file,
                                                                 gboolean                update_source_folders,
                                                                 gboolean                update_target_folders,
                                                                 ThunarOperationLogMode  log_mode,
                                                                 GClosure               *new_files_closure);
static void           thunar_application_launch_finished        (ThunarJob              *job,
                                                                 GList                  *containing_folders);
static void           thunar_application_launch                 (ThunarApplication      *application,
                                                                 gpointer                parent,
                                                                 const gchar            *icon_name,
                                                                 const gchar            *title,
                                                                 Launcher                launcher,
                                                                 GList                  *source_path_list,
                                                                 GList                  *target_path_list,
                                                                 gboolean                update_source_folders,
                                                                 gboolean                update_target_folders,
                                                                 ThunarOperationLogMode  log_mode,
                                                                 GClosure               *new_files_closure);
#ifdef HAVE_GUDEV
static void           thunar_application_uevent                 (GUdevClient            *client,
                                                                 const gchar            *action,
                                                                 GUdevDevice            *device,
                                                                 ThunarApplication      *application);
static gboolean       thunar_application_volman_idle            (gpointer                user_data);
static void           thunar_application_volman_idle_destroy    (gpointer                user_data);
static void           thunar_application_volman_watch           (GPid                    pid,
                                                                 gint                    status,
                                                                 gpointer                user_data);
static void           thunar_application_volman_watch_destroy   (gpointer                user_data);
#endif
static gboolean       thunar_application_show_dialogs           (gpointer                user_data);
static void           thunar_application_show_dialogs_destroy   (gpointer                user_data);
static GtkWidget     *thunar_application_get_progress_dialog    (ThunarApplication      *application);
static void           thunar_application_process_files          (ThunarApplication      *application);



struct _ThunarApplicationClass
{
  GtkApplicationClass __parent__;
};

struct _ThunarApplication
{
  GtkApplication                 __parent__;

  ThunarSessionClient            *session_client;

  ThunarPreferences              *preferences;
  GtkWidget                      *progress_dialog;

  ThunarThumbnailCache           *thumbnail_cache;
  ThunarThumbnailer              *thumbnailer;

  ThunarDBusService              *dbus_service;

  gboolean                        daemon;

  guint                           accel_map_load_id;
  guint                           accel_map_save_id;
  GtkAccelMap                    *accel_map;

  guint                           show_dialogs_timer_id;

#ifdef HAVE_GUDEV
  GUdevClient                    *udev_client;

  GSList                         *volman_udis;
  guint                           volman_idle_id;
  guint                           volman_watch_id;
#endif

  GList                          *files_to_launch;
  ThunarApplicationProcessAction  process_file_action;

  guint                           dbus_owner_id_xfce;
  guint                           dbus_owner_id_fdo;
};



static GQuark thunar_application_screen_quark;
static GQuark thunar_application_startup_id_quark;
static GQuark thunar_application_file_quark;



G_DEFINE_TYPE_EXTENDED (ThunarApplication, thunar_application, GTK_TYPE_APPLICATION, 0,
                        G_IMPLEMENT_INTERFACE (THUNAR_TYPE_BROWSER, NULL))



static void
thunar_application_class_init (ThunarApplicationClass *klass)
{
  GObjectClass        *gobject_class = G_OBJECT_CLASS (klass);
  GApplicationClass   *gapplication_class = G_APPLICATION_CLASS (klass);

  /* pre-allocate the required quarks */
  thunar_application_screen_quark =
    g_quark_from_static_string ("thunar-application-screen");
  thunar_application_startup_id_quark =
    g_quark_from_static_string ("thunar-application-startup-id");
  thunar_application_file_quark =
    g_quark_from_static_string ("thunar-application-file");

  gobject_class->finalize = thunar_application_finalize;
  gobject_class->get_property = thunar_application_get_property;
  gobject_class->set_property = thunar_application_set_property;

  gapplication_class->startup              = thunar_application_startup;
  gapplication_class->activate             = thunar_application_activate;
  gapplication_class->shutdown             = thunar_application_shutdown;
  gapplication_class->handle_local_options = thunar_application_handle_local_options;
  gapplication_class->command_line         = thunar_application_command_line;
  gapplication_class->dbus_register        = thunar_application_dbus_register;

  /**
   * ThunarApplication:daemon:
   *
   * %TRUE if the application should be run in daemon mode,
   * in which case it will never terminate. %FALSE if the
   * application should terminate once the last window is
   * closed.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_DAEMON,
                                   g_param_spec_boolean ("daemon",
                                                         "daemon",
                                                         "daemon",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));
}



static void
thunar_application_init (ThunarApplication *application)
{
  /* we do most initialization in GApplication::startup since it is only needed
   * in the primary instance anyways */

  application->files_to_launch = NULL;
  application->process_file_action = THUNAR_APPLICATION_SELECT_FILES;
  application->progress_dialog = NULL;
  application->preferences     = NULL;

  g_application_set_flags (G_APPLICATION (application), G_APPLICATION_HANDLES_COMMAND_LINE);
  g_application_add_main_option_entries (G_APPLICATION (application), option_entries);
}


static void
thunar_application_dbus_acquired_cb (GDBusConnection *conn,
                                     const gchar     *name,
                                     gpointer         user_data)
{
  g_debug (_("Acquired the session message bus '%s'\n"), name);
}



static void
thunar_application_name_acquired_cb (GDBusConnection *connection,
                                     const gchar     *name,
                                     gpointer         user_data)
{
    g_debug (_("Acquired the name '%s' on the session message bus\n"), name);
}



static void
thunar_application_dbus_name_lost_cb (GDBusConnection *connection,
                                      const gchar     *name,
                                      gpointer         user_data)
{
  g_warning (_("Name '%s' lost on the message dbus."), name);
}



/* TODO: [GTK3 Port] Check if there's a cleaner way to register */
/* these extra dbus names (besides org.xfce.Thunar) */
static void
thunar_application_dbus_init (ThunarApplication *application)
{
    application->dbus_owner_id_xfce = g_bus_own_name (G_BUS_TYPE_SESSION,
                                      "org.xfce.FileManager",
                                      G_BUS_NAME_OWNER_FLAGS_NONE,
                                      thunar_application_dbus_acquired_cb,
                                      thunar_application_name_acquired_cb,
                                      thunar_application_dbus_name_lost_cb,
                                      application,
                                      NULL);

    application->dbus_owner_id_fdo = g_bus_own_name (G_BUS_TYPE_SESSION,
                                     "org.freedesktop.FileManager1",
                                     G_BUS_NAME_OWNER_FLAGS_NONE,
                                     thunar_application_dbus_acquired_cb,
                                     thunar_application_name_acquired_cb,
                                     thunar_application_dbus_name_lost_cb,
                                     application,
                                     NULL);
}



static void
thunar_application_startup (GApplication *gapp)
{
  ThunarApplication *application = THUNAR_APPLICATION (gapp);

#ifdef HAVE_GUDEV
  static const gchar *subsystems[] = { "block", "input", "usb", NULL };
#endif

  /* initialize the application */
  application->preferences = thunar_preferences_get ();

#ifdef HAVE_GUDEV
  /* establish connection with udev */
  application->udev_client = g_udev_client_new (subsystems);

  /* connect to the client in order to be notified when devices are plugged in
   * or disconnected from the computer */
  g_signal_connect (application->udev_client, "uevent",
                    G_CALLBACK (thunar_application_uevent), application);
#endif

  thunar_application_dbus_init (application);

  G_APPLICATION_CLASS (thunar_application_parent_class)->startup (gapp);

  /* connect to the session manager */
  application->session_client = thunar_session_client_new (opt_sm_client_id);

  /* schedule accel map load and update windows when finished, this way empty but active accelerators are preserved */
  application->accel_map_load_id = gdk_threads_add_idle_full (G_PRIORITY_LOW, thunar_application_accel_map_load, application, NULL);

  thunar_application_load_css ();
}



static void
thunar_application_shutdown (GApplication *gapp)
{
  ThunarApplication *application = THUNAR_APPLICATION (gapp);

  /* unqueue all files waiting to be processed */
  thunar_g_list_free_full (application->files_to_launch);

  /* save the current accel map */
  if (G_UNLIKELY (application->accel_map_save_id != 0))
    {
      g_source_remove (application->accel_map_save_id);
      thunar_application_accel_map_save (application);
    }

  if (G_UNLIKELY (application->accel_map_load_id != 0))
    g_source_remove (application->accel_map_load_id);

  if (application->accel_map != NULL)
    g_object_unref (G_OBJECT (application->accel_map));

#ifdef HAVE_GUDEV
  /* cancel any pending volman watch source */
  if (G_UNLIKELY (application->volman_watch_id != 0))
    g_source_remove (application->volman_watch_id);

  /* cancel any pending volman idle source */
  if (G_UNLIKELY (application->volman_idle_id != 0))
    g_source_remove (application->volman_idle_id);

  /* drop all pending volume manager UDIs */
  g_slist_free_full (application->volman_udis, g_free);

  /* disconnect from the udev client */
  g_object_unref (application->udev_client);
#endif

  /* drop any running "show dialogs" timer */
  if (G_UNLIKELY (application->show_dialogs_timer_id != 0))
    g_source_remove (application->show_dialogs_timer_id);

  /* drop ref on the thumbnailer */
  if (application->thumbnailer != NULL)
    g_object_unref (application->thumbnailer);

  /* release the thumbnail cache */
  if (application->thumbnail_cache != NULL)
    g_object_unref (G_OBJECT (application->thumbnail_cache));

  /* disconnect from the preferences */
  g_object_unref (G_OBJECT (application->preferences));

  /* disconnect from the session manager */
  g_object_unref (G_OBJECT (application->session_client));

  /* remove the dbus service */
  g_clear_pointer (&application->dbus_service, g_object_unref);

  G_APPLICATION_CLASS (thunar_application_parent_class)->shutdown (gapp);
}



static void
thunar_application_finalize (GObject *object)
{
  /* rule of thumb: what gets initialized in GApplication::startup is cleaned up
   * in GApplication::shutdown. Therefore, this method doesn't do very much */

  (*G_OBJECT_CLASS (thunar_application_parent_class)->finalize) (object);
}



static int
thunar_application_handle_local_options (GApplication *gapp,
                                         GVariantDict *options)
{
  /* check if we should print version information */
  if (G_UNLIKELY (opt_version))
    {
      g_print ("%s %s (Xfce %s)\n\n", PACKAGE_NAME, PACKAGE_VERSION, xfce_version_string ());
      g_print ("%s\n", "Copyright (c) 2004-2022");
      g_print ("\t%s\n\n", _("The Thunar development team. All rights reserved."));
      g_print ("%s\n\n", _("Written by Benedikt Meurer <benny@xfce.org>."));
      g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
      g_print ("\n");
      return EXIT_SUCCESS;
    }

  /* continue processing on the primary instance */
  return -1;
}



static int
thunar_application_command_line (GApplication            *gapp,
                                 GApplicationCommandLine *command_line)
{
  ThunarApplication *application  = THUNAR_APPLICATION (gapp);
  gboolean           bulk_rename  = FALSE;
  gboolean           daemon       = FALSE;
  gboolean           quit         = FALSE;
  GStrv              filenames    = NULL;
  const char        *cwd          = g_application_command_line_get_cwd (command_line);
  GVariantDict      *options_dict = g_application_command_line_get_options_dict (command_line);
  GError            *error        = NULL;
  gchar             *cwd_list[]   = { (gchar *)".", NULL };

  /* retrieve arguments */
  g_variant_dict_lookup (options_dict, "bulk-rename", "b", &bulk_rename);
  g_variant_dict_lookup (options_dict, "quit", "b", &quit);
  g_variant_dict_lookup (options_dict, "daemon", "b", &daemon);
  g_variant_dict_lookup (options_dict, G_OPTION_REMAINING, "^aay", &filenames);

  /* FIXME: --quit should be named --suicide */
  if (G_UNLIKELY (quit))
    {
        g_debug ("quitting");
        g_application_quit (gapp);
        goto out;
    }

  /* check whether we should daemonize */
  if (G_UNLIKELY (daemon))
    {
        thunar_application_set_daemon (application, TRUE);
    }

  /* check if we should open the bulk rename dialog */
  if (G_UNLIKELY (bulk_rename))
    {
      /* try to open the bulk rename dialog */
      if (!thunar_application_bulk_rename (application, cwd, filenames, TRUE, NULL, NULL, &error))
        {
          /* FIXME? */
          g_application_command_line_printerr (command_line, "Thunar: Failed to open bulk rename: %s\n", error->message);
        }
    }
  else if (filenames != NULL)
    {
      if (!thunar_application_process_filenames (application, cwd, filenames, NULL, NULL, &error, THUNAR_APPLICATION_SELECT_FILES))
        {
          /* we failed to process the filenames or the bulk rename failed */
          g_application_command_line_printerr (command_line, "Thunar: %s\n", error->message);
        }
    }
  else if (!daemon)
    {
      GList        *window_list;
      ThunarWindow *window;
      gchar       **tabs_left;
      gchar       **tabs_right;
      gboolean      restore_tabs;
      gboolean      has_left_tabs; /* used to check whether the split-view should be enabled */
      gint          last_focused_tab;

      if (!thunar_application_process_filenames (application, cwd, cwd_list, NULL, NULL, &error, THUNAR_APPLICATION_SELECT_FILES))
        {
          /* we failed to process the filenames or the bulk rename failed */
          g_application_command_line_printerr (command_line, "Thunar: %s\n", error->message);
        }

      /* reopen tabs */
      g_object_get (G_OBJECT (application->preferences), "last-restore-tabs", &restore_tabs, NULL);
      if (restore_tabs)
        {
          /* get ThunarWindow */
          window_list = thunar_application_get_windows (application);
          window_list = g_list_last (window_list); /* this will be the topmost Window */
          window = THUNAR_WINDOW (window_list->data);

          /* restore left tabs */
          has_left_tabs = FALSE;
          g_object_get (G_OBJECT (application->preferences), "last-tabs-left", &tabs_left, "last-focused-tab-left", &last_focused_tab, NULL);
          if (tabs_left != NULL && g_strv_length (tabs_left) > 0)
            {
              gint n_tabs = 0;
              for (guint i = 0; i < g_strv_length (tabs_left); i++)
                {
                  ThunarFile *directory = thunar_file_get_for_uri (tabs_left[i], NULL);
                  if (G_LIKELY (directory != NULL) && thunar_file_is_directory (directory))
                    {
                      thunar_window_notebook_add_new_tab (window, directory, TRUE);
                      n_tabs++;
                    }
                }

              if (n_tabs > 0)
                thunar_window_notebook_remove_tab (window, 0); /* remove automatically opened tab */
              thunar_window_notebook_set_current_tab (window, last_focused_tab);
              has_left_tabs = TRUE;
            }

          /* restore right tabs */
          /* (will be restored on the left if no left tabs are found) */
          g_object_get (G_OBJECT (application->preferences), "last-tabs-right", &tabs_right, "last-focused-tab-right", &last_focused_tab, NULL);
          if (tabs_right != NULL && g_strv_length (tabs_right) > 0)
            {
              gint n_tabs = 0;

              if (has_left_tabs)
                thunar_window_notebook_toggle_split_view (window); /* enabling the split view selects the new notebook */
              for (guint i = 0; i < g_strv_length (tabs_right); i++)
                {
                  ThunarFile *directory = thunar_file_get_for_uri (tabs_right[i], NULL);
                  if (G_LIKELY (directory != NULL) && thunar_file_is_directory (directory))
                    {
                      thunar_window_notebook_add_new_tab (window, directory, TRUE);
                      n_tabs++;
                    }
                }

              if (n_tabs > 0)
                thunar_window_notebook_remove_tab (window, 0); /* remove automatically opened tab */
              thunar_window_notebook_set_current_tab (window, last_focused_tab);
            }

          /* free memory */
          g_list_free (window_list);
          g_strfreev (tabs_left);
          g_strfreev (tabs_right);
        }
    }

out:
  /* cleanup */
  g_strfreev (filenames);

  if (error)
    {
      g_error_free (error);
      return EXIT_FAILURE;
    }
  else
    return EXIT_SUCCESS;
}



static gboolean
thunar_application_dbus_register (GApplication           *gapp,
                                  GDBusConnection        *connection,
                                  const gchar            *object_path,
                                  GError                **error)
{
    ThunarApplication *application = THUNAR_APPLICATION (gapp);

    if (application->dbus_service) /* WTF? */
        return TRUE;

    application->dbus_service = g_object_new (THUNAR_TYPE_DBUS_SERVICE, NULL);

    return thunar_dbus_service_export_on_connection (application->dbus_service, connection, error);
}



static void
thunar_application_load_css (void)
{
  GtkCssProvider *css_provider;
  GdkScreen *screen;

  css_provider = gtk_css_provider_new ();

  gtk_css_provider_load_from_data (css_provider,
    /* for the location-buttons any margin looks ugly */
    ".location-button { margin-right: 0; }"
    /* remove extra border between side pane and view */
    ".shortcuts-pane { border-right-width: 0px; }"
    /* add missing top border to side pane */
    ".shortcuts-pane { border-top-style: solid; }"
    /* make border thicker during DnD */
    ".standard-view { border-left-width: 0px; border-right-width: 0px; }"
    /* for the example box in properties dialog > highlight tab */ 
    "#example { border-radius: 10px; }"
    ".standard-view:drop(active) { border-width: 2px; }", -1, NULL);
  screen = gdk_screen_get_default ();
  gtk_style_context_add_provider_for_screen (screen, GTK_STYLE_PROVIDER (css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (css_provider);
}



static void
thunar_application_activate (GApplication *gapp)
{
  /* TODO */

  G_APPLICATION_CLASS (thunar_application_parent_class)->activate (gapp);
}



static void
thunar_application_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ThunarApplication *application = THUNAR_APPLICATION (object);

  switch (prop_id)
    {
    case PROP_DAEMON:
      g_value_set_boolean (value, thunar_application_get_daemon (application));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_application_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ThunarApplication *application = THUNAR_APPLICATION (object);

  switch (prop_id)
    {
    case PROP_DAEMON:
      thunar_application_set_daemon (application, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gboolean
thunar_application_accel_map_save (gpointer user_data)
{
  ThunarApplication *application = THUNAR_APPLICATION (user_data);
  gchar             *path;

  _thunar_return_val_if_fail (THUNAR_IS_APPLICATION (application), FALSE);

  application->accel_map_save_id = 0;

  /* save the current accel map */
  path = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, ACCEL_MAP_PATH, TRUE);
  if (G_LIKELY (path != NULL))
    {
      /* save the accel map */
      gtk_accel_map_save (path);
      g_free (path);
    }

  return FALSE;
}



static gboolean
thunar_application_accel_map_load (gpointer user_data)
{
  ThunarApplication *application = user_data;
  gchar             *path;

  /* watch for changes */
  application->accel_map = gtk_accel_map_get ();
  g_signal_connect_swapped (G_OBJECT (application->accel_map), "changed",
                            G_CALLBACK (thunar_application_accel_map_changed), application);

  /* check if we have a saved accel map */
  path = xfce_resource_lookup (XFCE_RESOURCE_CONFIG, ACCEL_MAP_PATH);
  if (G_LIKELY (path != NULL))
    {
      /* load the accel map */
      gtk_accel_map_load (path);
      g_free (path);
    }

  application->accel_map_load_id = 0;

  return FALSE;
}



static void
thunar_application_accel_map_changed (ThunarApplication *application)
{
  GList *windows;

  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));

  /* stop pending save */
  if (application->accel_map_save_id != 0)
    {
      g_source_remove (application->accel_map_save_id);
      application->accel_map_save_id = 0;
    }

  /* schedule new save */
  application->accel_map_save_id =
      g_timeout_add_seconds (10, thunar_application_accel_map_save, application);

  /* update the accelerators for each window */
  windows = thunar_application_get_windows (application);
  for (GList *lp = windows; lp != NULL; lp = lp->next)
    {
      ThunarWindow *window = lp->data;
      thunar_window_reconnect_accelerators (window);
    }
  g_list_free (windows);
}



static void
thunar_application_collect_and_launch (ThunarApplication      *application,
                                       gpointer                parent,
                                       const gchar            *icon_name,
                                       const gchar            *title,
                                       Launcher                launcher,
                                       GList                  *source_file_list,
                                       GFile                  *target_file,
                                       gboolean                update_source_folders,
                                       gboolean                update_target_folders,
                                       ThunarOperationLogMode  log_mode,
                                       GClosure               *new_files_closure)
{
  GFile  *file;
  GError *err = NULL;
  GList  *target_file_list = NULL;
  GList  *lp;
  gchar  *base_name;

  /* check if we have anything to operate on */
  if (G_UNLIKELY (source_file_list == NULL))
    return;

  /* generate the target path list */
  for (lp = g_list_last (source_file_list); err == NULL && lp != NULL; lp = lp->prev)
    {
      /* verify that we're not trying to collect a root node */
      if (G_UNLIKELY (thunar_g_file_is_root (lp->data)))
        {
          /* tell the user that we cannot perform the requested operation */
          g_set_error (&err, G_FILE_ERROR, G_FILE_ERROR_INVAL, "%s", g_strerror (EINVAL));
        }
      else
        {
          base_name = g_file_get_basename (lp->data);
          file = g_file_resolve_relative_path (target_file, base_name);
          g_free (base_name);

          /* add to the target file list */
          target_file_list = thunar_g_list_prepend_deep (target_file_list, file);
          g_object_unref (file);
        }
    }

  /* check if we failed */
  if (G_UNLIKELY (err != NULL))
    {
      /* display an error message to the user */
      thunar_dialogs_show_error (parent, err, _("Failed to launch operation"));

      /* release the error */
      g_error_free (err);
    }
  else
    {
      /* launch the operation */
      thunar_application_launch (application, parent, icon_name, title, launcher,
                                 source_file_list, target_file_list, update_source_folders, update_target_folders, log_mode, new_files_closure);
    }

  /* release the target path list */
  thunar_g_list_free_full (target_file_list);
}



static void
thunar_application_launch_finished (ThunarJob  *job,
                                    GList      *containing_folders)
{
  GList        *lp;
  ThunarFile   *file;
  ThunarFolder *folder;

  _thunar_return_if_fail (THUNAR_IS_JOB (job));

  for (lp = containing_folders; lp != NULL; lp = lp->next)
    {
      if (lp->data == NULL)
        continue;
      file = thunar_file_get (lp->data, NULL);
      if (file != NULL)
        {
          if (thunar_file_is_directory (file))
            {
              folder = thunar_folder_get_for_file (file);
              if (folder != NULL)
                {
                  /* If the folder is connected to a folder monitor, we dont need to trigger the reload manually */
                  if (!thunar_folder_has_folder_monitor (folder))
                  {
                    thunar_folder_reload (folder, FALSE);
                  }
                  g_object_unref (folder);
                }
            }
          g_object_unref (file);
        }
      /* Unref all containing_folders (refs obtained by g_file_get_parent in thunar_g_file_list_get_parents ) */
      g_object_unref (lp->data);
    }
  g_list_free (containing_folders);
}



static void
thunar_application_launch (ThunarApplication     *application,
                           gpointer               parent,
                           const gchar           *icon_name,
                           const gchar           *title,
                           Launcher               launcher,
                           GList                 *source_file_list,
                           GList                 *target_file_list,
                           gboolean               update_source_folders,
                           gboolean               update_target_folders,
                           ThunarOperationLogMode log_mode,
                           GClosure              *new_files_closure)
{
  GtkWidget *dialog;
  GdkScreen *screen;
  ThunarJob *job;
  GList     *parent_folder_list = NULL;
  gboolean   has_jobs;

  _thunar_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));

  /* parse the parent pointer */
  screen = thunar_util_parse_parent (parent, NULL);

  /* try to allocate a new job for the operation */
  job = (*launcher) (source_file_list, target_file_list);

  if (update_source_folders)
    parent_folder_list = g_list_concat (parent_folder_list, thunar_g_file_list_get_parents (source_file_list));
  if (update_target_folders)
    parent_folder_list = g_list_concat (parent_folder_list, thunar_g_file_list_get_parents (target_file_list));

  thunar_job_set_log_mode (job, log_mode);

  /* connect a callback to instantly refresh the parent folders after the operation finished */
  g_signal_connect (G_OBJECT (job), "finished",
                    G_CALLBACK (thunar_application_launch_finished),
                    parent_folder_list);

  /* connect the "new-files" closure (if any) */
  if (G_LIKELY (new_files_closure != NULL))
    g_signal_connect_closure (job, "new-files", new_files_closure, FALSE);

  /* get the shared progress dialog */
  dialog = thunar_application_get_progress_dialog (application);

  /* place the dialog on the given screen */
  if (screen != NULL)
    gtk_window_set_screen (GTK_WINDOW (dialog), screen);

  has_jobs = thunar_progress_dialog_has_jobs (THUNAR_PROGRESS_DIALOG (dialog));

  /* add the job to the dialog */
  thunar_progress_dialog_add_job (THUNAR_PROGRESS_DIALOG (dialog),
                                  job, icon_name, title);

  if (has_jobs)
    {
      /* show the dialog immediately */
      thunar_application_show_dialogs (application);
    }
  else
    {
      /* Set up a timer to show the dialog, to make sure we don't
       * just popup and destroy a dialog for a very short job.
       */
      if (G_LIKELY (application->show_dialogs_timer_id == 0))
        {
          application->show_dialogs_timer_id =
            gdk_threads_add_timeout_full (G_PRIORITY_DEFAULT, 750, thunar_application_show_dialogs,
                                          application, thunar_application_show_dialogs_destroy);
        }
    }

  /* drop our reference on the job */
  g_object_unref (job);
}



#ifdef HAVE_GUDEV
static void
thunar_application_uevent (GUdevClient       *client,
                           const gchar       *action,
                           GUdevDevice       *device,
                           ThunarApplication *application)
{
  const gchar *sysfs_path;
  gboolean     is_cdrom = FALSE;
  gboolean     has_media = FALSE;
  GSList      *lp;

  _thunar_return_if_fail (G_UDEV_IS_CLIENT (client));
  _thunar_return_if_fail (action != NULL && *action != '\0');
  _thunar_return_if_fail (G_UDEV_IS_DEVICE (device));
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));
  _thunar_return_if_fail (client == application->udev_client);

  /* determine the sysfs path of the device */
  sysfs_path = g_udev_device_get_sysfs_path (device);

  /* check if the device is a CD drive */
  is_cdrom = g_udev_device_get_property_as_boolean (device, "ID_CDROM");
  has_media = g_udev_device_get_property_as_boolean (device, "ID_CDROM_MEDIA");

  /* distinguish between "add", "change" and "remove" actions, ignore "move" */
  if (g_strcmp0 (action, "add") == 0
      || (is_cdrom && has_media && g_strcmp0 (action, "change") == 0))
    {
      /* only insert the path if we don't have it already */
      if (g_slist_find_custom (application->volman_udis, sysfs_path,
                               (GCompareFunc) (void (*)(void)) g_utf8_collate) == NULL)
        {
          application->volman_udis = g_slist_prepend (application->volman_udis,
                                                      g_strdup (sysfs_path));

          /* check if there's currently no active or scheduled handler */
          if (G_LIKELY (application->volman_idle_id == 0
                        && application->volman_watch_id == 0))
            {
              /* schedule a new handler using the idle source, which invokes the handler */
              application->volman_idle_id =
                g_idle_add_full (G_PRIORITY_LOW, thunar_application_volman_idle,
                                 application, thunar_application_volman_idle_destroy);
            }
        }
    }
  else if (g_strcmp0 (action, "remove") == 0)
    {
      /* look for the sysfs path in the list of pending paths */
      lp = g_slist_find_custom (application->volman_udis, sysfs_path,
                                (GCompareFunc) (void (*)(void)) g_utf8_collate);

      if (G_LIKELY (lp != NULL))
        {
          /* free the sysfs path string */
          g_free (lp->data);

          /* drop the sysfs path from the list of pending device paths */
          application->volman_udis = g_slist_delete_link (application->volman_udis, lp);
        }
    }
}



static gboolean
thunar_application_volman_idle (gpointer user_data)
{
  ThunarApplication *application = THUNAR_APPLICATION (user_data);
  GdkScreen         *screen;
  gboolean           misc_volume_management;
  GError            *err = NULL;
  gchar            **argv;
  GPid               pid;
  char              *display = NULL;

  /* check if volume management is enabled (otherwise, we don't spawn anything, but clear the list here) */
  g_object_get (G_OBJECT (application->preferences), "misc-volume-management", &misc_volume_management, NULL);
  if (G_LIKELY (misc_volume_management))
    {
      /* check if we don't already have a handler, and we have a pending UDI */
      if (application->volman_watch_id == 0 && application->volman_udis != NULL)
        {
          /* generate the argument list for the volman */
          argv = g_new (gchar *, 4);
          argv[0] = g_strdup ("thunar-volman");
          argv[1] = g_strdup ("--device-added");
          argv[2] = application->volman_udis->data;
          argv[3] = NULL;

          /* remove the first list item from the pending list */
          application->volman_udis = g_slist_delete_link (application->volman_udis, application->volman_udis);

          /* locate the currently active screen (the one with the pointer) */
          screen = xfce_gdk_screen_get_active (NULL);

          if (screen != NULL)
            display = g_strdup (gdk_display_get_name (gdk_screen_get_display (screen)));

          /* try to spawn the volman on the active screen */
          if (g_spawn_async (NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH, thunar_setup_display_cb, display, &pid, &err))
            {
              /* add a child watch for the volman handler */
              application->volman_watch_id = g_child_watch_add_full (G_PRIORITY_LOW, pid, thunar_application_volman_watch,
                                                                     application, thunar_application_volman_watch_destroy);
            }
          else
            {
              /* failed to spawn, tell the user, giving a hint to install the thunar-volman package */
              g_warning ("Failed to launch the volume manager (%s), make sure you have the \"thunar-volman\" package installed.", err->message);
              g_error_free (err);
            }

          /* cleanup */
          g_free (display);
          g_strfreev (argv);
        }

    }
  else
    {
      /* drop all pending HAL device UDIs */
      g_slist_free_full (application->volman_udis, g_free);
      application->volman_udis = NULL;
    }

  /* keep the idle source alive as long as no handler is
   * active and we have pending UDIs that must be handled
   */
  return (application->volman_watch_id == 0
       && application->volman_udis != NULL);
}



static void
thunar_application_volman_idle_destroy (gpointer user_data)
{
  THUNAR_APPLICATION (user_data)->volman_idle_id = 0;
}



static void
thunar_application_volman_watch (GPid     pid,
                                 gint     status,
                                 gpointer user_data)
{
  ThunarApplication *application = THUNAR_APPLICATION (user_data);

  /* check if the idle source isn't active, but we have pending UDIs */
  if (application->volman_idle_id == 0 && application->volman_udis != NULL)
    {
      /* schedule a new handler using the idle source, which invokes the handler */
      application->volman_idle_id = gdk_threads_add_idle_full (G_PRIORITY_LOW, thunar_application_volman_idle,
                                                               application, thunar_application_volman_idle_destroy);
    }

  /* be sure to close the pid handle */
  g_spawn_close_pid (pid);
}



static void
thunar_application_volman_watch_destroy (gpointer user_data)
{
  THUNAR_APPLICATION (user_data)->volman_watch_id = 0;
}
#endif /* HAVE_GUDEV */



static gboolean
thunar_application_show_dialogs (gpointer user_data)
{
  ThunarApplication *application = THUNAR_APPLICATION (user_data);

  /* show the progress dialog */
  if (application->progress_dialog != NULL)
    gtk_window_present (GTK_WINDOW (application->progress_dialog));

  return FALSE;
}



static void
thunar_application_show_dialogs_destroy (gpointer user_data)
{
  THUNAR_APPLICATION (user_data)->show_dialogs_timer_id = 0;
}



/**
 * thunar_application_get:
 *
 * Returns the global shared #ThunarApplication instance.
 * This method takes a reference on the global instance
 * for the caller, so you must call g_object_unref()
 * on it when done.
 *
 * Return value: the shared #ThunarApplication instance.
 **/
ThunarApplication*
thunar_application_get (void)
{
  GApplication *default_app = g_application_get_default ();

  if (default_app)
    return THUNAR_APPLICATION (g_object_ref (default_app));
  else
    return g_object_ref_sink (g_object_new (THUNAR_TYPE_APPLICATION, "application-id", "org.xfce.Thunar", NULL));
}



/**
 * thunar_application_quit:
 * @application : a #ThunarApplication.
 *
 * Attempts to exit daemon mode(required if application is on hold) and leaves the gtk main loop
 **/
void
thunar_application_quit (ThunarApplication *application)
{
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));

  thunar_application_set_daemon(application, FALSE);

  /* For some unknown reason we need to close all open thunar windows before quit */
  /* Otherwise thunar will hangup on quit and thunar_application_shutdown will not be called */
  thunar_application_close_all_windows (application);

  g_application_quit (G_APPLICATION (application));
}



/**
 * thunar_application_get_daemon:
 * @application : a #ThunarApplication.
 *
 * Returns %TRUE if @application is in daemon mode.
 *
 * Return value: %TRUE if @application is in daemon mode.
 **/
gboolean
thunar_application_get_daemon (ThunarApplication *application)
{
  _thunar_return_val_if_fail (THUNAR_IS_APPLICATION (application), FALSE);
  return application->daemon;
}



/**
 * thunar_application_set_daemon:
 * @application : a #ThunarApplication.
 * @daemonize   : %TRUE to set @application into daemon mode.
 *
 * If @daemon is %TRUE, @application will be set into daemon mode.
 **/
void
thunar_application_set_daemon (ThunarApplication *application,
                               gboolean           daemonize)
{
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));

  if (application->daemon != daemonize)
    {
      application->daemon = daemonize;
      g_object_notify (G_OBJECT (application), "daemon");

      if (daemonize)
        g_application_hold (G_APPLICATION (application));
      else
        g_application_release (G_APPLICATION (application));
    }
}



/**
 * thunar_application_close_all_windows:
 * @application : a #ThunarApplication.
 *
 * Closes all #ThunarWindows currently handled by @application.
 **/
void
thunar_application_close_all_windows (ThunarApplication *application)
{
  GList *gtk_windows, *lp;

  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));

  gtk_windows = gtk_application_get_windows (GTK_APPLICATION (application));

  for (lp = gtk_windows; lp != NULL; lp = lp->next)
    if (G_LIKELY (THUNAR_IS_WINDOW (lp->data)))
      gtk_window_close (lp->data);
}



/**
 * thunar_application_get_windows:
 * @application : a #ThunarApplication.
 *
 * Returns the list of regular #ThunarWindows currently handled by
 * @application. The returned list is owned by the caller and
 * must be freed using g_list_free().
 *
 * Return value: the list of regular #ThunarWindows in @application.
 **/
GList*
thunar_application_get_windows (ThunarApplication *application)
{
  GList *gtk_windows, *thunar_windows = NULL, *lp;

  _thunar_return_val_if_fail (THUNAR_IS_APPLICATION (application), NULL);

  gtk_windows = gtk_application_get_windows (GTK_APPLICATION (application));

  for (lp = gtk_windows; lp != NULL; lp = lp->next)
    if (G_LIKELY (THUNAR_IS_WINDOW (lp->data)))
      thunar_windows = g_list_prepend (thunar_windows, lp->data);

  return thunar_windows;
}


/**
 * thunar_application_has_windows:
 * @application : a #ThunarApplication.
 *
 * Returns %TRUE if @application controls atleast one window.
 *
 * Return value: %TRUE if @application controls atleast one window.
 **/
gboolean
thunar_application_has_windows (ThunarApplication *application)
{
  _thunar_return_val_if_fail (THUNAR_IS_APPLICATION (application), FALSE);

  /* FIXME: this is possible inconsisitent with thunar_application_get_windows() */
  return gtk_application_get_windows (GTK_APPLICATION (application)) != NULL;
}



/**
 * thunar_application_take_window:
 * @application : a #ThunarApplication.
 * @window      : a #GtkWindow.
 *
 * Lets @application take over control of the specified @window.
 * @application will not exit until the last controlled #GtkWindow
 * is closed by the user.
 *
 * If the @window has no transient window, it will also create a
 * new #GtkWindowGroup for this window. This will make different
 * windows work independant (think gtk_dialog_run).
 **/
void
thunar_application_take_window (ThunarApplication *application,
                                GtkWindow         *window)
{
  GtkWindowGroup *group;

  _thunar_return_if_fail (GTK_IS_WINDOW (window));
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));

  /* only windows without a parent get a new window group */
  if (gtk_window_get_transient_for (window) == NULL && !gtk_window_has_group (window))
    {
      group = gtk_window_group_new ();
      gtk_window_group_add_window (group, window);
      g_object_weak_ref (G_OBJECT (window), (GWeakNotify) (void (*)(void)) g_object_unref, group);
    }

  /* add the ourselves to the window */
  gtk_window_set_application (window, GTK_APPLICATION (application));
}



/**
 * thunar_application_open_window:
 * @application      : a #ThunarApplication.
 * @directory        : the directory to open.
 * @screen           : the #GdkScreen on which to open the window or %NULL
 *                     to open on the default screen.
 * @startup_id       : startup id from startup notification passed along
 *                     with dbus to make focus stealing work properly.
 * @force_new_window : set this flag to force a new window, even if misc-open-new-window-as-tab is set
 *
 * Opens a new #ThunarWindow (or tab if preferred) for @application, displaying the
 * given @directory.
 *
 * Return value: the newly allocated #ThunarWindow.
 **/
GtkWidget*
thunar_application_open_window (ThunarApplication *application,
                                ThunarFile        *directory,
                                GdkScreen         *screen,
                                const gchar       *startup_id,
                                gboolean           force_new_window)
{
  GList*     list;
  GtkWidget *window;
  gchar     *role;
  gboolean   open_new_window_as_tab;
  gboolean   misc_open_new_windows_in_split_view;
  gboolean   restore_tabs;

  _thunar_return_val_if_fail (THUNAR_IS_APPLICATION (application), NULL);
  _thunar_return_val_if_fail (directory == NULL || THUNAR_IS_FILE (directory), NULL);
  _thunar_return_val_if_fail (screen == NULL || GDK_IS_SCREEN (screen), NULL);

  if (G_UNLIKELY (screen == NULL))
    screen = gdk_screen_get_default ();

  /* open as tab instead, if preferred */
  g_object_get (G_OBJECT (application->preferences), "misc-open-new-window-as-tab", &open_new_window_as_tab, NULL);
  if (G_UNLIKELY (!force_new_window && open_new_window_as_tab))
    {
      list = thunar_application_get_windows (application);
      if (list != NULL)  
        {
          GList     *lp = list;
          GtkWidget *data;

          /* this will be the topmost Window */
          list = g_list_last (list);
          data = list->data;

          if (directory != NULL)
              thunar_window_notebook_add_new_tab (THUNAR_WINDOW (data), directory, THUNAR_NEW_TAB_BEHAVIOR_SWITCH);
          
          /* bring the window to front */
          gtk_window_present (GTK_WINDOW (data));

          g_list_free (lp);

          return data;
        }
    }

  /* generate a unique role for the new window (for session management) */
  role = g_strdup_printf ("Thunar-%u-%u", (guint) time (NULL), (guint) g_random_int ());

  /* allocate the window */
  window = g_object_new (THUNAR_TYPE_WINDOW,
                         "role", role,
                         "screen", screen,
                         NULL);

  /* cleanup */
  g_free (role);

  /* set the startup id */
  if (startup_id != NULL)
    gtk_window_set_startup_id (GTK_WINDOW (window), startup_id);

  /* hook up the window */
  thunar_application_take_window (application, GTK_WINDOW (window));

  /* show the new window */
  gtk_widget_show (window);

  /* change the directory */
  if (directory != NULL)
    thunar_window_set_current_directory (THUNAR_WINDOW (window), directory);

  /* enable split view, if preferred */
  g_object_get (G_OBJECT (application->preferences),
                "misc-open-new-windows-in-split-view", &misc_open_new_windows_in_split_view,
                "last-restore-tabs", &restore_tabs, NULL);
  if (misc_open_new_windows_in_split_view && !restore_tabs)
    thunar_window_notebook_toggle_split_view (THUNAR_WINDOW (window));

  return window;
}



/**
 * thunar_application_bulk_rename:
 * @application       : a #ThunarApplication.
 * @working_directory : the default working directory for the bulk rename dialog.
 * @filenames         : the list of file names that should be renamed or the empty
 *                      list to start with an empty rename dialog. The file names
 *                      can either be absolute paths, supported URIs or relative file
 *                      names to @working_directory.
 * @standalone        : %TRUE to display the bulk rename dialog like a standalone
 *                      application.
 * @screen            : the #GdkScreen on which to rename the @filenames or %NULL
 *                      to use the default #GdkScreen.
 * @startup_id        : startup notification id to properly finish startup notification
 *                      and focus the window when focus stealing is enabled or %NULL.
 * @error             : return location for errors or %NULL.
 *
 * Tries to popup the bulk rename dialog.
 *
 * Return value: %TRUE if the dialog was opened successfully, otherwise %FALSE.
 **/
gboolean
thunar_application_bulk_rename (ThunarApplication *application,
                                const gchar       *working_directory,
                                gchar            **filenames,
                                gboolean           standalone,
                                GdkScreen         *screen,
                                const gchar       *startup_id,
                                GError           **error)
{
  ThunarFile *current_directory = NULL;
  ThunarFile *file;
  gboolean    result = FALSE;
  GList      *file_list = NULL;
  gchar      *filename;
  gchar      *empty_strv[] = { NULL };
  gint        n;

  _thunar_return_val_if_fail (screen == NULL || GDK_IS_SCREEN (screen), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_APPLICATION (application), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _thunar_return_val_if_fail (working_directory != NULL, FALSE);

  if (!filenames)
    filenames = empty_strv;

  /* determine the file for the working directory */
  current_directory = thunar_file_get_for_uri (working_directory, error);
  if (G_UNLIKELY (current_directory == NULL))
    return FALSE;

  /* check if we should use the default screen */
  if (G_LIKELY (screen == NULL))
    screen = gdk_screen_get_default ();

  /* try to process all filenames and convert them to the appropriate file objects */
  for (n = 0; filenames[n] != NULL; ++n)
    {
      /* check if the filename is an absolute path or looks like an URI */
      if (g_path_is_absolute (filenames[n]) || g_uri_is_valid (filenames[n], G_URI_FLAGS_NONE, NULL))
        {
          /* determine the file for the filename directly */
          file = thunar_file_get_for_uri (filenames[n], error);
        }
      else
        {
          /* translate the filename into an absolute path first */
          filename = g_build_filename (working_directory, filenames[n], NULL);
          file = thunar_file_get_for_uri (filename, error);
          g_free (filename);
        }

      /* verify that we have a valid file */
      if (G_LIKELY (file != NULL))
        file_list = g_list_append (file_list, file);
      else
        break;
    }

  /* check if the filenames where resolved successfully */
  if (G_LIKELY (filenames[n] == NULL))
    {
      /* popup the bulk rename dialog */
      thunar_show_renamer_dialog (screen, current_directory, file_list, standalone, startup_id);

      /* we succeed */
      result = TRUE;
    }

  /* cleanup */
  g_object_unref (G_OBJECT (current_directory));
  thunar_g_list_free_full (file_list);

  return result;
}



static GtkWidget *
thunar_application_get_progress_dialog (ThunarApplication *application)
{
  _thunar_return_val_if_fail (THUNAR_IS_APPLICATION (application), NULL);

  if (application->progress_dialog == NULL)
    {
      application->progress_dialog = thunar_progress_dialog_new ();

      g_object_add_weak_pointer (G_OBJECT (application->progress_dialog),
                                 (gpointer) &application->progress_dialog);

      thunar_application_take_window (application,
                                      GTK_WINDOW (application->progress_dialog));
    }

  return application->progress_dialog;
}



static void
thunar_application_process_files_finish (ThunarBrowser *browser,
                                         ThunarFile    *file,
                                         ThunarFile    *target_file,
                                         GError        *error,
                                         gpointer       unused)
{
  ThunarApplication *application = THUNAR_APPLICATION (browser);
  GdkScreen         *screen;
  const gchar       *startup_id;

  _thunar_return_if_fail (THUNAR_IS_BROWSER (browser));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));

  /* determine and reset the screen of the file */
  screen = g_object_get_qdata (G_OBJECT (file), thunar_application_screen_quark);
  g_object_set_qdata (G_OBJECT (file), thunar_application_screen_quark, NULL);

  /* determine and the startup id of the file */
  startup_id = g_object_get_qdata (G_OBJECT (file), thunar_application_startup_id_quark);

  /* check if resolving/mounting failed */
  if (error != NULL)
    {
      /* don't display cancel errors */
      if (error->domain != G_IO_ERROR || error->code != G_IO_ERROR_CANCELLED)
        {
          /* tell the user that we were unable to launch the file specified */
          thunar_dialogs_show_error (screen, error, _("Failed to open \"%s\""),
                                     thunar_file_get_display_name (file));
        }

      /* stop processing files */
      thunar_g_list_free_full (application->files_to_launch);
      application->files_to_launch = NULL;
    }
  else
    {
      if (application->process_file_action == THUNAR_APPLICATION_LAUNCH_FILES)
        {
          /* try to launch the file / open the directory */
          thunar_file_launch (target_file, screen, startup_id, &error);
        }
      else if (thunar_file_is_directory (file))
        {
          thunar_application_open_window (application, file, screen, startup_id, FALSE);
        }
      else
        {
          /* Note that for security reasons we do not execute files passed via command line */
          /* Lets rather open the containing directory and select the file */
          ThunarFile *parent = thunar_file_get_parent (file, NULL);

          if (G_LIKELY (parent != NULL))
            {
              GList* files = NULL;
              GtkWidget *window;

              window = thunar_application_open_window (application, parent, screen, startup_id, FALSE);
              g_object_unref (parent);

              files = g_list_append (files, thunar_file_get_file (file));
              thunar_window_show_and_select_files (THUNAR_WINDOW (window), files);
              g_list_free (files);
            }
        }

      /* remove the file from the list */
      application->files_to_launch = g_list_delete_link (application->files_to_launch,
                                                         application->files_to_launch);

      /* check if we have more files to process */
      if (application->files_to_launch != NULL)
        {
          /* continue processing the next file */
          thunar_application_process_files (application);
        }
    }

  /* unset the startup id */
  if (startup_id != NULL)
    g_object_set_qdata (G_OBJECT (file), thunar_application_startup_id_quark, NULL);

  /* release the file */
  g_object_unref (file);

  /* release the application */
  g_application_release (G_APPLICATION (application));
}



static void
thunar_application_process_files (ThunarApplication *application)
{
  ThunarFile *file;
  GdkScreen  *screen;

  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));

  /* don't do anything if no files are to be processed */
  if (application->files_to_launch == NULL)
    return;

  /* take the next file from the queue */
  file = THUNAR_FILE (application->files_to_launch->data);

  /* retrieve the screen we need to launch the file on */
  screen = g_object_get_qdata (G_OBJECT (file), thunar_application_screen_quark);

  /* make sure to hold a reference to the application while file processing is going on */
  g_application_hold (G_APPLICATION (application));

  /* resolve the file and/or mount its enclosing volume
   * before handling it in the callback */
  thunar_browser_poke_file (THUNAR_BROWSER (application), file, screen,
                            thunar_application_process_files_finish, NULL);
}



/**
 * thunar_application_process_filenames:
 * @application       : a #ThunarApplication.
 * @working_directory : the working directory relative to which the @filenames should
 *                      be interpreted.
 * @filenames         : a list of supported URIs or filenames. If a filename is specified
 *                      it can be either an absolute path or a path relative to the
 *                      @working_directory.
 * @screen            : the #GdkScreen on which to process the @filenames, or %NULL to
 *                      use the default screen.
 * @startup_id        : startup id to finish startup notification and properly focus the
 *                      window when focus stealing is enabled or %NULL.
 * @error             : return location for errors or %NULL.
 * @action            : action to invoke on the files
 *
 * Tells @application to process the given @filenames and launch them appropriately.
 *
 * Return value: %TRUE on success, %FALSE if @error is set.
 **/
gboolean
thunar_application_process_filenames (ThunarApplication               *application,
                                      const gchar                     *working_directory,
                                      gchar                          **filenames,
                                      GdkScreen                       *screen,
                                      const gchar                     *startup_id,
                                      GError                         **error,
                                      ThunarApplicationProcessAction   action)
{
  ThunarFile *file;
  GError     *derror = NULL;
  gchar      *filename;
  GList      *file_list = NULL;
  GList      *lp;
  gint        n;

  _thunar_return_val_if_fail (THUNAR_IS_APPLICATION (application), FALSE);
  _thunar_return_val_if_fail (working_directory != NULL, FALSE);
  _thunar_return_val_if_fail (filenames != NULL, FALSE);
  _thunar_return_val_if_fail (*filenames != NULL, FALSE);
  _thunar_return_val_if_fail (screen == NULL || GDK_IS_SCREEN (screen), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* try to process all filenames and convert them to the appropriate file objects */
  for (n = 0; filenames[n] != NULL; ++n)
    {
      /* check if the filename is an absolute path or looks like an URI */
      if (g_path_is_absolute (filenames[n]) || g_uri_is_valid (filenames[n], G_URI_FLAGS_NONE, NULL))
        {
          /* determine the file for the filename directly */
          file = thunar_file_get_for_uri (filenames[n], &derror);
        }
      else
        {
          /* translate the filename into an absolute path first */
          filename = g_build_filename (working_directory, filenames[n], NULL);
          file = thunar_file_get_for_uri (filename, &derror);
          g_free (filename);
        }

      /* verify that we have a valid file */
      if (G_LIKELY (file != NULL))
        {
          file_list = g_list_append (file_list, file);
        }
      else
        {
          /* tell the user that we were unable to launch the file specified */
          thunar_dialogs_show_error (screen, derror, _("Failed to open \"%s\""),
                                     filenames[n]);

          g_set_error (error, derror->domain, derror->code,
                       _("Failed to open \"%s\": %s"), filenames[n], derror->message);
          g_error_free (derror);

          thunar_g_list_free_full (file_list);

          return FALSE;
        }
    }

  /* loop over all files */
  for (lp = file_list; lp != NULL; lp = lp->next)
    {
      /* remember the screen to launch the file on */
      g_object_set_qdata (G_OBJECT (lp->data), thunar_application_screen_quark, screen);

      /* remember the startup id to set on the window */
      if (G_LIKELY (startup_id != NULL && *startup_id != '\0'))
        g_object_set_qdata_full (G_OBJECT (lp->data), thunar_application_startup_id_quark,
                                 g_strdup (startup_id), (GDestroyNotify) g_free);

      /* append the file to the list of files we need to launch */
      application->files_to_launch = g_list_append (application->files_to_launch,
                                                    lp->data);
    }

  /* start processing files if we have any to launch */
  if (application->files_to_launch != NULL)
    {
      application->process_file_action = action;
      thunar_application_process_files (application);
    }

  /* free the file list */
  g_list_free (file_list);

  return TRUE;
}



static void
thunar_application_rename_file_error (ExoJob            *job,
                                      GError            *error,
                                      ThunarApplication *application)
{
  ThunarFile *file;
  GdkScreen  *screen;

  _thunar_return_if_fail (EXO_IS_JOB (job));
  _thunar_return_if_fail (error != NULL);
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));

  screen = g_object_get_qdata (G_OBJECT (job), thunar_application_screen_quark);
  file = g_object_get_qdata (G_OBJECT (job), thunar_application_file_quark);

  g_assert (screen != NULL);
  g_assert (file != NULL);

  thunar_dialogs_show_error (screen, error, _("Failed to rename \"%s\""),
                             thunar_file_get_display_name (file));
}



static void
thunar_application_rename_file_finished (ExoJob  *job,
                                         gpointer user_data)
{
  _thunar_return_if_fail (EXO_IS_JOB (job));

  /* destroy the job object */
  g_object_unref (job);
}



/**
 * thunar_application_rename_file:
 * @application : a #ThunarApplication.
 * @file        : a #ThunarFile to be renamed.
 * @screen      : the #GdkScreen on which to open the window or %NULL
 *                to open on the default screen.
 * @startup_id  : startup id from startup notification passed along
 *                with dbus to make focus stealing work properly.
 * @log_mode    : a #ThunarOperationLogMode to control logging of the operation
 *
 * Prompts the user to rename the @file.
 **/
void
thunar_application_rename_file (ThunarApplication      *application,
                                ThunarFile             *file,
                                GdkScreen              *screen,
                                const gchar            *startup_id,
                                ThunarOperationLogMode  log_mode)
{
  ThunarJob *job;

  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));
  _thunar_return_if_fail (GDK_IS_SCREEN (screen));
  _thunar_return_if_fail (startup_id != NULL);

  /* TODO pass the startup ID to the rename dialog */

  /* run the rename dialog */
  job = thunar_dialogs_show_rename_file (screen, file, log_mode);
  if (G_LIKELY (job != NULL))
    {
      /* remember the screen and file */
      g_object_set_qdata (G_OBJECT (job), thunar_application_screen_quark, screen);
      g_object_set_qdata_full (G_OBJECT (job), thunar_application_file_quark,
                               g_object_ref (file), g_object_unref);

      /* handle rename errors */
      g_signal_connect (job, "error",
                        G_CALLBACK (thunar_application_rename_file_error), application);

      /* destroy the job when it has finished */
      g_signal_connect (job, "finished",
                        G_CALLBACK (thunar_application_rename_file_finished), NULL);
    }
}



/**
 * thunar_application_create_file:
 * @application      : a #ThunarApplication.
 * @parent_directory : the #ThunarFile of the parent directory.
 * @content_type     : the content type of the new file.
 * @screen           : the #GdkScreen on which to open the window or %NULL
 *                     to open on the default screen.
 * @startup_id       : startup id from startup notification passed along
 *                     with dbus to make focus stealing work properly.
 * @log_mode          : a #ThunarOperationLogMode to control logging
 *
 * Prompts the user to create a new file or directory in @parent_directory.
 * The @content_type defines the icon and other elements in the filename
 * prompt dialog.
 **/
void
thunar_application_create_file (ThunarApplication      *application,
                                ThunarFile             *parent_directory,
                                const gchar            *content_type,
                                GdkScreen              *screen,
                                const gchar            *startup_id,
                                ThunarOperationLogMode  log_mode)
{
  const gchar *dialog_title;
  const gchar *title;
  gboolean     is_directory;
  GList        path_list;
  gchar       *name;

  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));
  _thunar_return_if_fail (THUNAR_IS_FILE (parent_directory));
  _thunar_return_if_fail (content_type != NULL && *content_type != '\0');
  _thunar_return_if_fail (GDK_IS_SCREEN (screen));
  _thunar_return_if_fail (startup_id != NULL);

  is_directory = (g_strcmp0 (content_type, "inode/directory") == 0);

  if (is_directory)
    {
      dialog_title = _("New Folder");
      title = _("Create New Folder");
    }
  else
    {
      dialog_title = _("New File");
      title = _("Create New File");
    }

  /* TODO pass the startup ID to the rename dialog */

  /* ask the user to enter a name for the new folder */
  name = thunar_dialogs_show_create (screen, content_type, dialog_title, title);
  if (G_LIKELY (name != NULL))
    {
      path_list.data = g_file_get_child (thunar_file_get_file (parent_directory), name);
      path_list.next = path_list.prev = NULL;

      /* launch the operation */
      if (is_directory)
        thunar_application_mkdir (application, screen, &path_list, NULL, log_mode);
      else
        thunar_application_creat (application, screen, &path_list, NULL, NULL, log_mode);

      g_object_unref (path_list.data);
      g_free (name);
    }
}



/**
 * thunar_application_create_file_from_template:
 * @application      : a #ThunarApplication.
 * @parent_directory : the #ThunarFile of the parent directory.
 * @template_file    : the #ThunarFile of the template.
 * @screen           : the #GdkScreen on which to open the window or %NULL
 *                     to open on the default screen.
 * @startup_id       : startup id from startup notification passed along
 *                     with dbus to make focus stealing work properly.
 * @log_mode          : a #ThunarOperationLogMode to control logging
 *
 * Prompts the user to create a new file or directory in @parent_directory
 * from an existing @template_file which predefines the name and extension
 * in the create dialog.
 **/
void
thunar_application_create_file_from_template (ThunarApplication      *application,
                                              ThunarFile             *parent_directory,
                                              ThunarFile             *template_file,
                                              GdkScreen              *screen,
                                              const gchar            *startup_id,
                                              ThunarOperationLogMode log_mode)
{
  GList  target_path_list;
  gchar *name;
  gchar *title;

  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));
  _thunar_return_if_fail (THUNAR_IS_FILE (parent_directory));
  _thunar_return_if_fail (THUNAR_IS_FILE (template_file));
  _thunar_return_if_fail (GDK_IS_SCREEN (screen));
  _thunar_return_if_fail (startup_id != NULL);

  /* generate a title for the create dialog */
  title = g_strdup_printf (_("Create Document from template \"%s\""),
                           thunar_file_get_display_name (template_file));

  /* TODO pass the startup ID to the rename dialog */

  /* ask the user to enter a name for the new document */
  name = thunar_dialogs_show_create (screen,
                                     thunar_file_get_content_type (template_file),
                                     thunar_file_get_display_name (template_file),
                                     title);
  if (G_LIKELY (name != NULL))
    {
      /* fake the target path list */
      target_path_list.data = g_file_get_child (thunar_file_get_file (parent_directory), name);
      target_path_list.next = target_path_list.prev = NULL;

      /* launch the operation */
      thunar_application_creat (application, screen,
                                &target_path_list,
                                thunar_file_get_file (template_file),
                                NULL, log_mode);

      /* release the target path */
      g_object_unref (target_path_list.data);

      /* release the file name */
      g_free (name);
    }

  /* clean up */
  g_free (title);
}



/**
 * thunar_application_copy_to:
 * @application       : a #ThunarApplication.
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
 * @source_file_list  : the lst of #GFile<!---->s that should be copied.
 * @target_file_list  : the list of #GFile<!---->s where files should be copied to.
 * @log_mode          : a #ThunarOperationLogMode controlling the logging of the operation.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #GFile<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 *
 * Copies all files from @source_file_list to their locations specified in
 * @target_file_list.
 *
 * @source_file_list and @target_file_list must be of the same length.
 **/
void
thunar_application_copy_to (ThunarApplication      *application,
                            gpointer                parent,
                            GList                  *source_file_list,
                            GList                  *target_file_list,
                            ThunarOperationLogMode  log_mode,
                            GClosure               *new_files_closure)
{
  _thunar_return_if_fail (g_list_length (source_file_list) == g_list_length (target_file_list));
  _thunar_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));

  /* launch the operation */
  thunar_application_launch (application, parent, "edit-copy",
                             _("Copying files..."), thunar_io_jobs_copy_files,
                             source_file_list, target_file_list, FALSE, TRUE, log_mode, new_files_closure);
}



/**
 * thunar_application_copy_into:
 * @application       : a #ThunarApplication.
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
 * @source_file_list  : the list of #GFile<!---->s that should be copied.
 * @target_file       : the #GFile to the target directory.
 * @log_mode          : a #ThunarOperationLogMode controlling the logging of the operation.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #GFile<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 *
 * Copies all files referenced by the @source_file_list to the directory
 * referenced by @target_file. This method takes care of all user interaction.
 **/
void
thunar_application_copy_into (ThunarApplication       *application,
                              gpointer                 parent,
                              GList                   *source_file_list,
                              GFile                   *target_file,
                              ThunarOperationLogMode   log_mode,
                              GClosure                *new_files_closure)
{
  ThunarFile *target_folder;
  GVolume    *volume         = NULL;
  gchar      *display_name   = NULL;
  gchar      *title          = NULL;
  gchar      *volume_name    = NULL;
  gchar      *volume_uuid    = NULL;
  gboolean    use_display_name;

  _thunar_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));
  _thunar_return_if_fail (G_IS_FILE (target_file));

  target_folder = thunar_file_get (target_file , NULL);
  if (target_folder != NULL)
    {
      volume = thunar_file_get_volume (target_folder);
      g_object_unref (target_folder);
    }

  if (volume != NULL)
    {
      volume_name = g_volume_get_name (volume);
      volume_uuid = g_volume_get_uuid (volume);
    }

  /* generate a title for the progress dialog */
  display_name = thunar_file_cached_display_name (target_file);

  /* if the display_name is equal to the volume_uuid display the volume_name */
  use_display_name = g_strcmp0 (display_name, volume_uuid) != 0;

  title = g_strdup_printf (_("Copying files to \"%s\"..."), use_display_name ? display_name : volume_name);
  g_free (display_name);

  /* collect the target files and launch the job */
  thunar_application_collect_and_launch (application, parent, "edit-copy",
                                         title, thunar_io_jobs_copy_files,
                                         source_file_list, target_file,
                                         FALSE, TRUE,
                                         log_mode,
                                         new_files_closure);

  /* free */
  g_free (title);
  if (volume != NULL)
    {
      g_free (volume_name);
      g_free (volume_uuid);
      g_object_unref (G_OBJECT (volume));
    }
}



/**
 * thunar_application_link_into:
 * @application       : a #ThunarApplication.
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
 * @source_file_list  : the list of #GFile<!---->s that should be symlinked.
 * @target_file       : the target directory.
 * @log_mode          : A #ThunarOperationLogMode enum to control logging.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #GFile<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 *
 * Symlinks all files referenced by the @source_file_list to the directory
 * referenced by @target_file. This method takes care of all user
 * interaction.
 **/
void
thunar_application_link_into (ThunarApplication      *application,
                              gpointer                parent,
                              GList                  *source_file_list,
                              GFile                  *target_file,
                              ThunarOperationLogMode  log_mode,
                              GClosure                *new_files_closure)
{
  gchar *display_name;
  gchar *title;

  _thunar_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));
  _thunar_return_if_fail (G_IS_FILE (target_file));

  /* generate a title for the progress dialog */
  display_name = thunar_file_cached_display_name (target_file);
  title = g_strdup_printf (_("Creating symbolic links in \"%s\"..."), display_name);
  g_free (display_name);

  /* collect the target files and launch the job */
  thunar_application_collect_and_launch (application, parent, "insert-link",
                                         title, thunar_io_jobs_link_files,
                                         source_file_list, target_file,
                                         FALSE, TRUE,
                                         log_mode,
                                         new_files_closure);

  /* free the title */
  g_free (title);
}



/**
 * thunar_application_move_into:
 * @application       : a #ThunarApplication.
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
 * @source_file_list  : the list of #GFile<!---->s that should be moved.
 * @target_file       : the target directory.
 * @log_mode          : A #ThunarOperationLogMode enum to control logging.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #GFile<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 *
 * Moves all files referenced by the @source_file_list to the directory
 * referenced by @target_file. This method takes care of all user
 * interaction.
 **/
void
thunar_application_move_into (ThunarApplication      *application,
                              gpointer                parent,
                              GList                  *source_file_list,
                              GFile                  *target_file,
                              ThunarOperationLogMode  log_mode,
                              GClosure               *new_files_closure)
{
  gchar *display_name;
  gchar *title;

  _thunar_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));
  _thunar_return_if_fail (target_file != NULL);

  /* Source folder cannot be moved into its subdirectory */
  for (GList* lp = source_file_list; lp != NULL; lp = lp->next)
    {
      if (thunar_g_file_is_descendant (target_file, G_FILE (lp->data)))
        {
          gchar *file_name = g_file_get_basename (G_FILE (lp->data));
          thunar_dialogs_show_error (NULL, NULL, "The folder (%s) cannot be moved into its own subdirectory", file_name);
          g_free (file_name);
          return;
        }
    }

  /* launch the appropriate operation depending on the target file */
  if (thunar_g_file_is_trash (target_file))
    {
      thunar_application_trash (application, parent, source_file_list, THUNAR_OPERATION_LOG_OPERATIONS);
    }
  else
    {
      /* generate a title for the progress dialog */
      display_name = thunar_file_cached_display_name (target_file);
      title = g_strdup_printf (_("Moving files into \"%s\"..."), display_name);
      g_free (display_name);

      /* collect the target files and launch the job */
      thunar_application_collect_and_launch (application, parent,
                                             "stock_folder-move", title,
                                             thunar_io_jobs_move_files,
                                             source_file_list, target_file,
                                             TRUE, TRUE,
                                             log_mode,
                                             new_files_closure);

      /* free the title */
      g_free (title);
    }
}



/**
 * thunar_application_move_files:
 * @application       : a #ThunarApplication.
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
 * @source_file_list  : the list of #GFile<!---->s that should be moved.
 * @target_file_list  : the list of #GFile<!---->s that the files should be moved to
 * @log_mode          : a #ThunarOperationLogMode controlling the logging of the operation.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #GFile<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 *
 * Moves all the files in the source file list to the files given in the target file list.
 **/
void
thunar_application_move_files (ThunarApplication       *application,
                               gpointer                 parent,
                               GList                   *source_file_list,
                               GList                   *target_file_list,
                               ThunarOperationLogMode   log_mode,
                               GClosure                *new_files_closure)
{
  _thunar_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));
 
  thunar_application_launch (application, parent,
                             "stock_folder-move", _("Moving files ..."),
                             thunar_io_jobs_move_files,
                             source_file_list, target_file_list,
                             TRUE, TRUE,
                             log_mode,
                             new_files_closure);
}



static ThunarJob *
unlink_stub (GList *source_path_list,
             GList *target_path_list)
{
  return thunar_io_jobs_unlink_files (source_path_list);
}



/**
 * thunar_application_unlink_files:
 * @application : a #ThunarApplication.
 * @parent      : a #GdkScreen, a #GtkWidget or %NULL.
 * @file_list   : the list of #ThunarFile<!---->s that should be deleted.
 * @permanently : whether to unlink the files permanently.
 * @warn        : whether to warn the user if deleting permanently.
 *
 * Deletes all files in the @file_list and takes care of all user interaction.
 *
 * If the user pressed the shift key while triggering the delete action,
 * the files will be deleted permanently (after confirming the action),
 * otherwise the files will be moved to the trash.
 *
 * Return value: TRUE if the trash/delete operation was canceled, FALSE otehrwise
 **/
gboolean
thunar_application_unlink_files (ThunarApplication *application,
                                 gpointer           parent,
                                 GList             *file_list,
                                 gboolean           permanently,
                                 gboolean           warn)
{
  GtkWidget *dialog;
  GtkWindow *window;
  GdkScreen *screen;
  GList     *path_list = NULL;
  GList     *lp;
  gchar     *message;
  guint      n_path_list = 0;
  gint       response;
  gboolean   operation_canceled = FALSE;

  _thunar_return_val_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent), TRUE);
  _thunar_return_val_if_fail (THUNAR_IS_APPLICATION (application), TRUE);

  /* determine the paths for the files */
  for (lp = g_list_last (file_list); lp != NULL; lp = lp->prev, ++n_path_list)
    {
      /* prepend the path to the path list */
      path_list = thunar_g_list_prepend_deep (path_list, thunar_file_get_file (lp->data));

      /* permanently delete if at least one of the file is not a local
       * file (e.g. resides in the trash) or cannot be trashed */
      if (!thunar_file_is_local (lp->data) || !thunar_file_can_be_trashed (lp->data))
        permanently = TRUE;
    }

  /* nothing to do if we don't have any paths */
  if (G_UNLIKELY (n_path_list == 0))
    return FALSE;

  /* ask the user to confirm if deleting permanently */
  if (G_UNLIKELY (permanently) && warn)
    {
      /* parse the parent pointer */
      screen = thunar_util_parse_parent (parent, &window);

      /* generate the question to confirm the delete operation */
      if (G_LIKELY (n_path_list == 1))
        {
          message = g_strdup_printf (_("Are you sure that you want to\npermanently delete \"%s\"?"),
                                     thunar_file_get_display_name (THUNAR_FILE (file_list->data)));
        }
      else
        {
          message = g_strdup_printf (ngettext ("Are you sure that you want to permanently\ndelete the selected file?",
                                               "Are you sure that you want to permanently\ndelete the %u selected files?",
                                               n_path_list),
                                     n_path_list);
        }

      /* ask the user to confirm the delete operation */
      dialog = gtk_message_dialog_new (window,
                                       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_QUESTION,
                                       GTK_BUTTONS_NONE,
                                       "%s", message);
      if (G_UNLIKELY (window == NULL && screen != NULL))
        gtk_window_set_screen (GTK_WINDOW (dialog), screen);
      gtk_window_set_title (GTK_WINDOW (dialog), _("Attention"));
      gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                              _("_Cancel"), GTK_RESPONSE_CANCEL,
                              _("_Delete"), GTK_RESPONSE_YES,
                              NULL);
      gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                _("If you delete a file, it is permanently lost."));
      response = gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      g_free (message);

      /* perform the delete operation */
      if (G_LIKELY (response == GTK_RESPONSE_YES))
        {
          /* launch the "Delete" operation */
          thunar_application_launch (application, parent, "edit-delete",
                                     _("Deleting files..."), unlink_stub,
                                     path_list, path_list, TRUE, FALSE, THUNAR_OPERATION_LOG_OPERATIONS, NULL);
        }
      else
        operation_canceled = TRUE;
    }
  else if (G_UNLIKELY (permanently))
    {
      thunar_application_launch (application, parent, "edit-delete",
                                 _("Deleting files..."), unlink_stub,
                                 path_list, path_list, TRUE, FALSE, THUNAR_OPERATION_LOG_OPERATIONS, NULL);
    }
  else
    {
      /* launch the "Move to Trash" operation */
      thunar_application_trash (application, parent, path_list, THUNAR_OPERATION_LOG_OPERATIONS);
    }

  /* release the path list */
  thunar_g_list_free_full (path_list);

  return operation_canceled;
}



static ThunarJob *
trash_stub (GList *source_file_list,
            GList *target_file_list)
{
  return thunar_io_jobs_trash_files (source_file_list);
}



/* thunar_application_trash:
 * @application       : a #ThunarApplication.
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
 * @file_list         : the list of #GFile<!---->s to trash.
 * @log_mode          : a #ThunarOperationLogMode to control logging
 *
 * Trashes the files specified by @file_list
 **/
void
thunar_application_trash (ThunarApplication     *application,
                          gpointer               parent,
                          GList                 *file_list,
                          ThunarOperationLogMode log_mode)
{
  _thunar_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));
  _thunar_return_if_fail (file_list != NULL);

  thunar_application_launch (application, parent, "user-trash-full",
                             _("Moving files into the trash..."), trash_stub,
                             file_list, NULL, TRUE, FALSE, log_mode, NULL);
}



static ThunarJob *
creat_stub (GList *template_file,
            GList *target_path_list)
{
   _thunar_return_val_if_fail (template_file->data == NULL || G_IS_FILE (template_file->data), NULL);
  return thunar_io_jobs_create_files (target_path_list, template_file->data);
}



/**
 * thunar_application_creat:
 * @application       : a #ThunarApplication.
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
 * @file_list         : the list of files to create.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #GFile<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 * @log_mode          : a #ThunarOperationLogMode to control logging
 *
 * Creates empty files for all #GFile<!---->s listed in @file_list. This
 * method takes care of all user interaction.
 **/
void
thunar_application_creat (ThunarApplication      *application,
                          gpointer                parent,
                          GList                  *file_list,
                          GFile                  *template_file,
                          GClosure               *new_files_closure,
                          ThunarOperationLogMode  log_mode)
{
  GList template_list;

  _thunar_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));

  template_list.next = template_list.prev = NULL;
  template_list.data = template_file;

  /* launch the operation */
  thunar_application_launch (application, parent, "document-new",
                             _("Creating files..."), creat_stub,
                             &template_list, file_list, FALSE, TRUE, log_mode, new_files_closure);
}



static ThunarJob *
mkdir_stub (GList *source_path_list,
            GList *target_path_list)
{
  return thunar_io_jobs_make_directories (source_path_list);
}



/**
 * thunar_application_mkdir:
 * @application       : a #ThunarApplication.
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
 * @file_list         : the list of directories to create.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #GFile<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 * @log_mode          : a #ThunarOperationLogMode to control logging
 *
 * Creates all directories referenced by the @file_list. This method takes care of all user
 * interaction.
 **/
void
thunar_application_mkdir (ThunarApplication      *application,
                          gpointer                parent,
                          GList                  *file_list,
                          GClosure               *new_files_closure,
                          ThunarOperationLogMode  log_mode)
{
  _thunar_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));

  /* launch the operation */
  thunar_application_launch (application, parent, "folder-new",
                             _("Creating directories..."), mkdir_stub,
                             file_list, file_list, TRUE, FALSE, log_mode, new_files_closure);
}



/**
 * thunar_application_empty_trash:
 * @application : a #ThunarApplication.
 * @parent      : the parent, which can either be the associated
 *                #GtkWidget, the screen on which display the
 *                progress and the confirmation, or %NULL.
 *
 * Deletes all files and folders in the Trash after asking
 * the user to confirm the operation.
 **/
void
thunar_application_empty_trash (ThunarApplication *application,
                                gpointer           parent,
                                const gchar       *startup_id)
{
  GtkWidget *dialog;
  GtkWindow *window;
  GdkScreen *screen;
  GList      file_list;
  gint       response;

  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));
  _thunar_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));

  /* parse the parent pointer */
  screen = thunar_util_parse_parent (parent, &window);

  /* ask the user to confirm the operation */
  dialog = gtk_message_dialog_new (window,
                                   GTK_DIALOG_MODAL
                                   | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   _("Remove all files and folders from the Trash?"));
  if (G_UNLIKELY (window == NULL && screen != NULL))
    gtk_window_set_screen (GTK_WINDOW (dialog), screen);
  gtk_window_set_startup_id (GTK_WINDOW (dialog), startup_id);
  gtk_window_set_title (GTK_WINDOW (dialog), _("Empty Trash"));
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("_Cancel"), GTK_RESPONSE_CANCEL,
                          _("_Empty Trash"), GTK_RESPONSE_YES,
                          NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            _("If you choose to empty the Trash, all items in it will be permanently lost. "
                                              "Please note that you can also delete them separately."));
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  /* check if the user confirmed */
  if (G_LIKELY (response == GTK_RESPONSE_YES))
    {
      /* fake a path list with only the trash root (the root
       * folder itself will never be unlinked, so this is safe)
       */
      file_list.data = thunar_g_file_new_for_trash ();
      file_list.next = NULL;
      file_list.prev = NULL;

      /* launch the operation */
      thunar_application_launch (application, parent, "user-trash",
                                 _("Emptying the Trash..."),
                                 unlink_stub, &file_list, NULL, TRUE, FALSE, THUNAR_OPERATION_LOG_OPERATIONS, NULL);

      /* cleanup */
      g_object_unref (file_list.data);
    }
}



/**
 * thunar_application_restore_files:
 * @application       : a #ThunarApplication.
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
 * @trash_file_list   : a #GList of #ThunarFile<!---->s in the trash.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #GFile<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 *
 * Restores all #ThunarFile<!---->s in the @trash_file_list to their original
 * location.
 **/
void
thunar_application_restore_files (ThunarApplication *application,
                                  gpointer           parent,
                                  GList             *trash_file_list,
                                  GClosure          *new_files_closure)
{
  const gchar *original_uri;
  GError      *err = NULL;
  GFile       *target_path;
  GList       *source_path_list = NULL;
  GList       *target_path_list = NULL;
  GList       *lp;

  _thunar_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));

  for (lp = trash_file_list; lp != NULL; lp = lp->next)
    {
      original_uri = thunar_file_get_original_path (lp->data);
      if (G_UNLIKELY (original_uri == NULL))
        {
          /* no OriginalPath, impossible to continue */
          g_set_error (&err, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                       _("Failed to determine the original path for \"%s\""),
                       thunar_file_get_display_name (lp->data));
          break;
        }

      /* TODO we might have to distinguish between URIs and paths here */
      target_path = g_file_new_for_commandline_arg (original_uri);

      source_path_list = thunar_g_list_append_deep (source_path_list, thunar_file_get_file (lp->data));
      target_path_list = thunar_g_list_append_deep (target_path_list, target_path);

      g_object_unref (target_path);
    }

  if (G_UNLIKELY (err != NULL))
    {
      /* display an error dialog */
      thunar_dialogs_show_error (parent, err, _("Could not restore \"%s\""),
                                 thunar_file_get_display_name (lp->data));
      g_error_free (err);
    }
  else
    {
      /* launch the operation */
      thunar_application_launch (application, parent, "stock_folder-move",
                                 _("Restoring files..."), thunar_io_jobs_restore_files,
                                 source_path_list, target_path_list, TRUE, TRUE, THUNAR_OPERATION_LOG_OPERATIONS, new_files_closure);
    }

  /* free path lists */
  thunar_g_list_free_full (source_path_list);
  thunar_g_list_free_full (target_path_list);
}



ThunarThumbnailCache *
thunar_application_get_thumbnail_cache (ThunarApplication *application)
{
  _thunar_return_val_if_fail (THUNAR_IS_APPLICATION (application), NULL);

  if (application->thumbnail_cache == NULL)
    application->thumbnail_cache = thunar_thumbnail_cache_new ();

  return g_object_ref (application->thumbnail_cache);
}


