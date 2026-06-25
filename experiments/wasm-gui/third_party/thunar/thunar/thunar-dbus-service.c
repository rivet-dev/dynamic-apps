/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006      Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2010 Jannis Pohlmann <jannis@xfce.org>
 * Copyright (c) 2010      Daniel Morales <daniel@daniel.com.uy>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib/gstdio.h>

#include <libxfce4util/libxfce4util.h>

#include <thunar/thunar-application.h>
#include <thunar/thunar-chooser-dialog.h>
#include <thunar/thunar-dbus-service.h>
#include <thunar/thunar-file.h>
#include <thunar/thunar-gdk-extensions.h>
#include <thunar/thunar-preferences-dialog.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-properties-dialog.h>
#include <thunar/thunar-util.h>



/* include generate dbus infos */
#include <thunar/thunar-dbus-freedesktop-interfaces.h>
#include <thunar/thunar-dbus-service-infos.h>



typedef enum
{
  THUNAR_DBUS_TRANSFER_MODE_COPY_TO,
  THUNAR_DBUS_TRANSFER_MODE_COPY_INTO,
  THUNAR_DBUS_TRANSFER_MODE_MOVE_INTO,
  THUNAR_DBUS_TRANSFER_MODE_LINK_INTO,
} ThunarDBusTransferMode;


static void     thunar_dbus_service_finalize                    (GObject                *object);
static gboolean thunar_dbus_service_connect_trash_bin           (ThunarDBusService      *dbus_service,
                                                                 GError                **error);
static gboolean thunar_dbus_service_parse_uri_and_display       (ThunarDBusService      *dbus_service,
                                                                 const gchar            *uri,
                                                                 const gchar            *display,
                                                                 ThunarFile            **file_return,
                                                                 GdkScreen             **screen_return,
                                                                 GError                **error);
static gboolean thunar_dbus_service_transfer_files              (ThunarDBusTransferMode  transfer_mode,
                                                                 const gchar            *working_directory,
                                                                 const gchar * const    *source_filenames,
                                                                 const gchar * const    *target_filenames,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 GError                **error);
static void     thunar_dbus_service_trash_bin_changed           (ThunarDBusService      *dbus_service,
                                                                 ThunarFile             *trash_bin);
static gboolean thunar_dbus_service_display_chooser_dialog      (ThunarDBusFileManager  *object,
                                                                 GDBusMethodInvocation  *invocation,
                                                                 const gchar            *uri,
                                                                 gboolean                open,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 ThunarDBusService      *dbus_service);
static gboolean thunar_dbus_service_display_app_chooser_dialog  (ThunarDBusFileManager  *object,
                                                                 GDBusMethodInvocation  *invocation,
                                                                 const gchar            *uri,
                                                                 gboolean                open,
                                                                 gboolean                preselect_default_checkbox,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 ThunarDBusService      *dbus_service);
static gboolean thunar_dbus_service_display_folder              (ThunarDBusFileManager  *object,
                                                                 GDBusMethodInvocation  *invocation,
                                                                 const gchar            *uri,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 ThunarDBusService      *dbus_service);
static gboolean thunar_dbus_service_display_folder_and_select   (ThunarDBusFileManager  *object,
                                                                 GDBusMethodInvocation  *invocation,
                                                                 const gchar            *uri,
                                                                 const gchar            *filename,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 ThunarDBusService      *dbus_service);
static gboolean thunar_dbus_service_display_file_properties     (ThunarDBusFileManager  *object,
                                                                 GDBusMethodInvocation  *invocation,
                                                                 const gchar            *uri,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 ThunarDBusService      *dbus_service);
static gboolean thunar_dbus_service_launch                      (ThunarDBusFileManager  *object,
                                                                 GDBusMethodInvocation  *invocation,
                                                                 const gchar            *uri,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 ThunarDBusService      *dbus_service);
static gboolean thunar_dbus_service_execute                     (ThunarDBusFileManager  *object,
                                                                 GDBusMethodInvocation  *invocation,
                                                                 const gchar            *working_directory,
                                                                 const gchar            *uri,
                                                                 const gchar           **files,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 ThunarDBusService      *dbus_service);
static gboolean thunar_dbus_service_display_preferences_dialog  (ThunarDBusFileManager  *object,
                                                                 GDBusMethodInvocation  *invocation,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 ThunarDBusService      *dbus_service);
static gboolean thunar_dbus_service_display_trash               (ThunarDBusTrash        *object,
                                                                 GDBusMethodInvocation  *invocation,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 ThunarDBusService      *dbus_service);
static gboolean thunar_dbus_service_empty_trash                 (ThunarDBusTrash        *object,
                                                                 GDBusMethodInvocation  *invocation,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 ThunarDBusService      *dbus_service);
static gboolean thunar_dbus_service_move_to_trash               (ThunarDBusTrash        *object,
                                                                 GDBusMethodInvocation  *invocation,
                                                                 gchar                 **filenames,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 ThunarDBusService      *dbus_service);
static gboolean thunar_dbus_service_query_trash                 (ThunarDBusTrash        *object,
                                                                 GDBusMethodInvocation  *invocation,
                                                                 ThunarDBusService      *dbus_service);
static gboolean thunar_dbus_service_bulk_rename                 (ThunarDBusThunar  *object,
                                                                 GDBusMethodInvocation  *invocation,
                                                                 const gchar            *working_directory,
                                                                 gchar                 **filenames,
                                                                 gboolean                standalone,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 ThunarDBusService      *dbus_service);
static gboolean thunar_dbus_service_launch_files                (ThunarDBusFileManager  *object,
                                                                 GDBusMethodInvocation  *invocation,
                                                                 const gchar            *working_directory,
                                                                 gchar                 **filenames,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 ThunarDBusService      *dbus_service);
static gboolean thunar_dbus_service_rename_file                 (ThunarDBusFileManager  *object,
                                                                 GDBusMethodInvocation  *invocation,
                                                                 const gchar            *uri,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 ThunarDBusService      *dbus_service);
static gboolean thunar_dbus_service_create_file                 (ThunarDBusFileManager  *object,
                                                                 GDBusMethodInvocation  *invocation,
                                                                 const gchar            *parent_directory,
                                                                 const gchar            *content_type,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 ThunarDBusService      *dbus_service);
static gboolean thunar_dbus_service_create_file_from_template   (ThunarDBusFileManager  *object,
                                                                 GDBusMethodInvocation  *invocation,
                                                                 const gchar            *parent_directory,
                                                                 const gchar            *template_uri,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 ThunarDBusService      *dbus_service);
static gboolean thunar_dbus_service_copy_to                     (ThunarDBusFileManager  *object,
                                                                 GDBusMethodInvocation  *invocation,
                                                                 const gchar            *working_directory,
                                                                 gchar                 **source_filenames,
                                                                 gchar                 **target_filenames,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 ThunarDBusService      *dbus_service);
static gboolean thunar_dbus_service_copy_into                   (ThunarDBusFileManager  *object,
                                                                 GDBusMethodInvocation  *invocation,
                                                                 const gchar            *working_directory,
                                                                 gchar                 **source_filenames,
                                                                 const gchar            *target_filename,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 ThunarDBusService      *dbus_service);
static gboolean thunar_dbus_service_move_into                   (ThunarDBusFileManager  *object,
                                                                 GDBusMethodInvocation  *invocation,
                                                                 const gchar            *working_directory,
                                                                 gchar                 **source_filenames,
                                                                 const gchar            *target_filenames,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 ThunarDBusService      *dbus_service);
static gboolean thunar_dbus_service_link_into                   (ThunarDBusFileManager  *object,
                                                                 GDBusMethodInvocation  *invocation,
                                                                 const gchar            *working_directory,
                                                                 gchar                 **source_filenames,
                                                                 const gchar            *target_filename,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 ThunarDBusService      *dbus_service);
static gboolean thunar_dbus_service_unlink_files                (ThunarDBusFileManager  *object,
                                                                 GDBusMethodInvocation  *invocation,
                                                                 const gchar            *working_directory,
                                                                 gchar                 **filenames,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 ThunarDBusService      *dbus_service);
static gboolean thunar_dbus_service_terminate                   (ThunarDBusThunar       *object,
                                                                 GDBusMethodInvocation  *invocation,
                                                                 ThunarDBusService      *dbus_service);

static gboolean thunar_dbus_freedesktop_show_folders            (ThunarOrgFreedesktopFileManager1 *object,
                                                                 GDBusMethodInvocation            *invocation,
                                                                 gchar                           **uris,
                                                                 const gchar                      *startup_id,
                                                                 ThunarDBusService                *dbus_service);
static gboolean thunar_dbus_freedesktop_show_items              (ThunarOrgFreedesktopFileManager1 *object,
                                                                 GDBusMethodInvocation            *invocation,
                                                                 gchar                           **uris,
                                                                 const gchar                      *startup_id,
                                                                 ThunarDBusService                *dbus_service);
static gboolean thunar_dbus_freedesktop_show_item_properties    (ThunarOrgFreedesktopFileManager1 *object,
                                                                 GDBusMethodInvocation            *invocation,
                                                                 gchar                           **uris,
                                                                 const gchar                      *startup_id,
                                                                 ThunarDBusService                *dbus_service);


struct _ThunarDBusServiceClass
{
  GObjectClass __parent__;
};

struct _ThunarDBusService
{
  GObject __parent__;

  ThunarDBusFileManager            *file_manager;
  ThunarDBusTrash                  *trash;
  ThunarDBusThunar                 *thunar;
  ThunarOrgFreedesktopFileManager1 *file_manager_fdo;

  ThunarFile      *trash_bin;
};



G_DEFINE_TYPE (ThunarDBusService, thunar_dbus_service, G_TYPE_OBJECT)



static void
thunar_dbus_service_class_init (ThunarDBusServiceClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_dbus_service_finalize;
}



static void connect_signals_multiple (gpointer object,
                                      gpointer data,
                                      ...)
{
  va_list ap;

  const char *signal;
  gpointer   *handler;

  va_start (ap, data);

  while ((signal = va_arg (ap, const char *)))
    {
      handler = va_arg (ap, gpointer);

      g_signal_connect (object, signal, G_CALLBACK (handler), data);
    }

  va_end (ap);
}



static void
thunar_dbus_service_init (ThunarDBusService *dbus_service)
{
  dbus_service->file_manager      = thunar_dbus_file_manager_skeleton_new ();
  dbus_service->trash             = thunar_dbus_trash_skeleton_new ();
  dbus_service->thunar            = thunar_dbus_thunar_skeleton_new ();
  dbus_service->file_manager_fdo  = thunar_org_freedesktop_file_manager1_skeleton_new ();

  connect_signals_multiple (dbus_service->file_manager, dbus_service,
                            "handle-display-chooser-dialog", thunar_dbus_service_display_chooser_dialog,
                            "handle-display-application-chooser-dialog", thunar_dbus_service_display_app_chooser_dialog,
                            "handle-display-folder", thunar_dbus_service_display_folder,
                            "handle-display-folder-and-select", thunar_dbus_service_display_folder_and_select,
                            "handle-display-file-properties", thunar_dbus_service_display_file_properties,
                            "handle-launch", thunar_dbus_service_launch,
                            "handle-execute", thunar_dbus_service_execute,
                            "handle-display-preferences-dialog", thunar_dbus_service_display_preferences_dialog,
                            "handle-copy-to", thunar_dbus_service_copy_to,
                            "handle-copy-into", thunar_dbus_service_copy_into,
                            "handle-move-into", thunar_dbus_service_move_into,
                            "handle-link-into", thunar_dbus_service_link_into,
                            "handle-unlink-files", thunar_dbus_service_unlink_files,
                            "handle-launch-files", thunar_dbus_service_launch_files,
                            "handle-rename-file", thunar_dbus_service_rename_file,
                            "handle-create-file", thunar_dbus_service_create_file,
                            "handle-create-file-from-template", thunar_dbus_service_create_file_from_template,
                            NULL);

  connect_signals_multiple (dbus_service->trash, dbus_service,
                            "handle-display-trash", thunar_dbus_service_display_trash,
                            "handle-empty-trash", thunar_dbus_service_empty_trash,
                            "handle-move-to-trash", thunar_dbus_service_move_to_trash,
                            "handle-query-trash", thunar_dbus_service_query_trash,
                            NULL);

  connect_signals_multiple (dbus_service->thunar, dbus_service,
                            "handle-bulk-rename", thunar_dbus_service_bulk_rename,
                            "handle-terminate", thunar_dbus_service_terminate,
                            NULL);

  connect_signals_multiple (dbus_service->file_manager_fdo, dbus_service,
                            "handle-show-folders", thunar_dbus_freedesktop_show_folders,
                            "handle-show-items", thunar_dbus_freedesktop_show_items,
                            "handle-show-item-properties", thunar_dbus_freedesktop_show_item_properties,
                            NULL);
}



static void
thunar_dbus_service_finalize (GObject *object)
{
  ThunarDBusService *dbus_service = THUNAR_DBUS_SERVICE (object);

  g_object_unref (dbus_service->file_manager);
  g_object_unref (dbus_service->trash);
  g_object_unref (dbus_service->thunar);
  g_object_unref (dbus_service->file_manager_fdo);

  if (dbus_service->trash_bin)
    g_object_unref (dbus_service->trash_bin);

  (*G_OBJECT_CLASS (thunar_dbus_service_parent_class)->finalize) (object);
}



static gboolean
thunar_dbus_service_connect_trash_bin (ThunarDBusService *dbus_service,
                                       GError           **error)
{
  GFile *trash_bin_path;

  /* check if we're not already connected to the trash bin */
  if (G_UNLIKELY (dbus_service->trash_bin == NULL))
    {
      /* try to connect to the trash bin */
      trash_bin_path = thunar_g_file_new_for_trash ();
      dbus_service->trash_bin = thunar_file_get (trash_bin_path, error);
      if (G_LIKELY (dbus_service->trash_bin != NULL))
        {
          /* watch the trash bin for changes */
          thunar_file_watch (dbus_service->trash_bin);

          /* stay informed about changes to the trash bin */
          g_signal_connect_swapped (G_OBJECT (dbus_service->trash_bin), "changed",
                                    G_CALLBACK (thunar_dbus_service_trash_bin_changed),
                                    dbus_service);
          thunar_file_reload_idle (dbus_service->trash_bin);
        }
      g_object_unref (trash_bin_path);
    }

  return (dbus_service->trash_bin != NULL);
}



static gboolean
thunar_dbus_service_parse_uri_and_display (ThunarDBusService *dbus_service,
                                           const gchar       *uri,
                                           const gchar       *display,
                                           ThunarFile       **file_return,
                                           GdkScreen        **screen_return,
                                           GError           **error)
{
  /* try to open the display */
  *screen_return = thunar_gdk_screen_open (display, error);
  if (G_UNLIKELY (*screen_return == NULL))
    return FALSE;

  /* try to determine the file for the URI */
  *file_return = thunar_file_get_for_uri (uri, error);
  if (G_UNLIKELY (*file_return == NULL))
    {
      g_object_unref (G_OBJECT (*screen_return));
      return FALSE;
    }

  return TRUE;
}



static void
thunar_dbus_service_trash_bin_changed (ThunarDBusService *dbus_service,
                                       ThunarFile        *trash_bin)
{
  _thunar_return_if_fail (THUNAR_IS_DBUS_SERVICE (dbus_service));
  _thunar_return_if_fail (dbus_service->trash_bin == trash_bin);
  _thunar_return_if_fail (THUNAR_IS_FILE (trash_bin));

  /* emit the "trash-changed" signal with the new state */
  thunar_dbus_trash_emit_trash_changed (dbus_service->trash);
}



/**
 * TODO: Method to be removed after thunar 4.18 got released (dbus call is marked as deprecated). Use thunar_dbus_service_display_app_chooser_dialog instead.
 */
static gboolean
thunar_dbus_service_display_chooser_dialog (ThunarDBusFileManager  *object,
                                            GDBusMethodInvocation  *invocation,
                                            const gchar            *uri,
                                            gboolean                open,
                                            const gchar            *display,
                                            const gchar            *startup_id,
                                            ThunarDBusService      *dbus_service)
{
  ThunarFile *file;
  GdkScreen  *screen;
  GError     *error = NULL;

  /* parse uri and display parameters */
  if (!thunar_dbus_service_parse_uri_and_display (dbus_service, uri, display, &file, &screen, &error))
    goto out;

  /* popup the chooser dialog */
  /* TODO use the startup id! */
  thunar_show_chooser_dialog (screen, file, open, FALSE);

  /* cleanup */
  g_object_unref (G_OBJECT (screen));
  g_object_unref (G_OBJECT (file));

out:
  if (error)
    g_dbus_method_invocation_take_error (invocation, error);
  else
    thunar_dbus_file_manager_complete_display_chooser_dialog (object, invocation);

  return TRUE;
}



static gboolean
thunar_dbus_service_display_app_chooser_dialog (ThunarDBusFileManager  *object,
                                                GDBusMethodInvocation  *invocation,
                                                const gchar            *uri,
                                                gboolean                open,
                                                gboolean                preselect_default_checkbox,
                                                const gchar            *display,
                                                const gchar            *startup_id,
                                                ThunarDBusService      *dbus_service)
{
  ThunarFile *file;
  GdkScreen  *screen;
  GError     *error = NULL;

  /* parse uri and display parameters */
  if (!thunar_dbus_service_parse_uri_and_display (dbus_service, uri, display, &file, &screen, &error))
    goto out;

  /* popup the chooser dialog */
  /* TODO use the startup id! */
  thunar_show_chooser_dialog (screen, file, open, preselect_default_checkbox);

  /* cleanup */
  g_object_unref (G_OBJECT (screen));
  g_object_unref (G_OBJECT (file));

out:
  if (error)
    g_dbus_method_invocation_take_error (invocation, error);
  else
    thunar_dbus_file_manager_complete_display_chooser_dialog (object, invocation);

  return TRUE;
}



static gboolean
thunar_dbus_service_display_folder (ThunarDBusFileManager  *object,
                                    GDBusMethodInvocation  *invocation,
                                    const gchar            *uri,
                                    const gchar            *display,
                                    const gchar            *startup_id,
                                    ThunarDBusService      *dbus_service)
{
  ThunarApplication *application;
  ThunarFile        *file;
  GdkScreen         *screen;
  GError            *error = NULL;

  /* parse uri and display parameters */
  if (!thunar_dbus_service_parse_uri_and_display (dbus_service, uri, display, &file, &screen, &error))
    return FALSE;

  /* popup a new window for the folder */
  application = thunar_application_get ();
  thunar_application_open_window (application, file, screen, startup_id, FALSE);
  g_object_unref (G_OBJECT (application));

  /* cleanup */
  g_object_unref (G_OBJECT (screen));
  g_object_unref (G_OBJECT (file));

  if (error)
    g_dbus_method_invocation_take_error (invocation, error);
  else
    thunar_dbus_file_manager_complete_display_folder (object, invocation);

  return TRUE;
}



static gboolean
thunar_dbus_service_display_folder_and_select (ThunarDBusFileManager  *object,
                                               GDBusMethodInvocation  *invocation,
                                               const gchar            *uri,
                                               const gchar            *filename,
                                               const gchar            *display,
                                               const gchar            *startup_id,
                                               ThunarDBusService      *dbus_service)
{
  ThunarApplication *application;
  ThunarFile        *folder;
  GdkScreen         *screen;
  GtkWidget         *window;
  GFile             *gfile;
  GList             *gfiles = NULL;
  GError            *error = NULL;

  /* verify that filename is valid */
  if (G_UNLIKELY (filename == NULL || *filename == '\0' || strchr (filename, '/') != NULL))
    {
      g_set_error (&error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Invalid filename \"%s\""), filename);
      goto out;
    }

  /* parse uri and display parameters */
  if (!thunar_dbus_service_parse_uri_and_display (dbus_service, uri, display, &folder, &screen, &error))
    goto out;

  /* popup a new window for the folder */
  application = thunar_application_get ();
  window = thunar_application_open_window (application, folder, screen, startup_id, FALSE);
  g_object_unref (application);

  /* determine the path for the filename relative to the folder */
  gfile = g_file_resolve_relative_path (thunar_file_get_file (folder), filename);
  if (G_LIKELY (gfile != NULL))
    {
      gfiles = g_list_append (gfiles, gfile);
      thunar_window_show_and_select_files (THUNAR_WINDOW (window), gfiles);

      g_list_free_full (gfiles, g_object_unref);
    }

  /* cleanup */
  g_object_unref (screen);
  g_object_unref (folder);

out:
  if (error)
    g_dbus_method_invocation_take_error (invocation, error);
  else
    thunar_dbus_file_manager_complete_display_folder_and_select (object, invocation);

  return TRUE;
}



static gboolean
thunar_dbus_service_display_file_properties (ThunarDBusFileManager  *object,
                                             GDBusMethodInvocation  *invocation,
                                             const gchar            *uri,
                                             const gchar            *display,
                                             const gchar            *startup_id,
                                             ThunarDBusService      *dbus_service)
{
  ThunarApplication *application;
  ThunarFile        *file;
  GdkScreen         *screen;
  GtkWidget         *dialog;
  GError            *error = NULL;

  /* parse uri and display parameters */
  if (!thunar_dbus_service_parse_uri_and_display (dbus_service, uri, display, &file, &screen, &error))
    goto out;

  /* popup the file properties dialog */
  dialog = thunar_properties_dialog_new (NULL, THUNAR_PROPERTIES_DIALOG_NONE);
  gtk_window_set_screen (GTK_WINDOW (dialog), screen);
  gtk_window_set_startup_id (GTK_WINDOW (dialog), startup_id);
  thunar_properties_dialog_set_file (THUNAR_PROPERTIES_DIALOG (dialog), file);
  gtk_window_present (GTK_WINDOW (dialog));

  /* let the application take control over the dialog */
  application = thunar_application_get ();
  thunar_application_take_window (application, GTK_WINDOW (dialog));
  g_object_unref (G_OBJECT (application));

  /* cleanup */
  g_object_unref (G_OBJECT (screen));
  g_object_unref (G_OBJECT (file));

out:
  if (error)
    g_dbus_method_invocation_take_error (invocation, error);
  else
    thunar_dbus_file_manager_complete_display_file_properties (object, invocation);

  return TRUE;
}



static gboolean
thunar_dbus_service_launch (ThunarDBusFileManager  *object,
                            GDBusMethodInvocation  *invocation,
                            const gchar            *uri,
                            const gchar            *display,
                            const gchar            *startup_id,
                            ThunarDBusService      *dbus_service)
{
  ThunarFile *file;
  GdkScreen  *screen;
  GError     *error = NULL;

  /* parse uri and display parameters */
  if (thunar_dbus_service_parse_uri_and_display (dbus_service, uri, display, &file, &screen, &error))
    {
      /* try to launch the file on the given screen */
      thunar_file_launch (file, screen, startup_id, &error);

      /* cleanup */
      g_object_unref (G_OBJECT (screen));
      g_object_unref (G_OBJECT (file));
    }

  if (error)
    g_dbus_method_invocation_take_error (invocation, error);
  else
    thunar_dbus_file_manager_complete_launch (object, invocation);

  return TRUE;
}



static gboolean
thunar_dbus_service_execute (ThunarDBusFileManager  *object,
                             GDBusMethodInvocation  *invocation,
                             const gchar            *working_directory,
                             const gchar            *uri,
                             const gchar           **files,
                             const gchar            *display,
                             const gchar            *startup_id,
                             ThunarDBusService      *dbus_service)
{
  ThunarFile *file;
  GdkScreen  *screen;
  GError     *error = NULL;
  GFile      *working_dir;
  GList      *file_list = NULL;
  gchar      *tmp_working_dir = NULL;
  gchar      *old_working_dir = NULL;
  guint       n;

  /* parse uri and display parameters */
  if (thunar_dbus_service_parse_uri_and_display (dbus_service, uri, display, &file, &screen, &error))
    {
      if (working_directory != NULL && *working_directory != '\0')
        old_working_dir = thunar_util_change_working_directory (working_directory);

      for (n = 0; files != NULL && files[n] != NULL; ++n)
        file_list = g_list_prepend (file_list, g_file_new_for_commandline_arg (files[n]));

      file_list = g_list_reverse (file_list);

      if (old_working_dir != NULL)
        {
          tmp_working_dir = thunar_util_change_working_directory (old_working_dir);
          g_free (tmp_working_dir);
          g_free (old_working_dir);
        }

      /* try to launch the file on the given screen */
      working_dir = g_file_new_for_commandline_arg (working_directory);
      thunar_file_execute (file, working_dir, screen, file_list, startup_id, &error);
      g_object_unref (working_dir);

      /* cleanup */
      g_list_free_full (file_list, g_object_unref);
      g_object_unref (screen);
      g_object_unref (file);
    }

  if (error)
    g_dbus_method_invocation_take_error (invocation, error);
  else
    thunar_dbus_file_manager_complete_launch (object, invocation);

  return TRUE;
}



static gboolean
thunar_dbus_service_display_preferences_dialog (ThunarDBusFileManager  *object,
                                                GDBusMethodInvocation  *invocation,
                                                const gchar            *display,
                                                const gchar            *startup_id,
                                                ThunarDBusService      *dbus_service)
{
  ThunarApplication *application;
  GdkScreen         *screen;
  GtkWidget         *dialog;
  GError            *error = NULL;

  /* try to open the screen for the display name */
  screen = thunar_gdk_screen_open (display, &error);
  if (G_UNLIKELY (screen == NULL))
    goto out;

  /* popup the preferences dialog... */
  dialog = thunar_preferences_dialog_new (NULL);
  gtk_window_set_screen (GTK_WINDOW (dialog), screen);
  gtk_window_set_startup_id (GTK_WINDOW (dialog), startup_id);
  gtk_widget_show (GTK_WIDGET (dialog));

  /* ...and let the application take care of it */
  application = thunar_application_get ();
  thunar_application_take_window (application, GTK_WINDOW (dialog));
  g_object_unref (G_OBJECT (application));

  /* cleanup */
  g_object_unref (G_OBJECT (screen));

out:
  if (error)
    g_dbus_method_invocation_take_error (invocation, error);
  else
    thunar_dbus_file_manager_complete_display_preferences_dialog (object, invocation);

  return TRUE;
}



static gboolean
thunar_dbus_service_display_trash (ThunarDBusTrash        *object,
                                   GDBusMethodInvocation  *invocation,
                                   const gchar            *display,
                                   const gchar            *startup_id,
                                   ThunarDBusService      *dbus_service)
{
  ThunarApplication *application;
  GdkScreen         *screen;
  GError            *error = NULL;

  /* connect to the trash bin on-demand */
  if (!thunar_dbus_service_connect_trash_bin (dbus_service, &error))
    goto out;

  /* try to open the screen for the display name */
  screen = thunar_gdk_screen_open (display, &error);
  if (G_LIKELY (screen != NULL))
    {
      /* tell the application to display the trash bin */
      application = thunar_application_get ();
      thunar_application_open_window (application, dbus_service->trash_bin, screen, startup_id, FALSE);
      g_object_unref (G_OBJECT (application));

      /* release the screen */
      g_object_unref (G_OBJECT (screen));
    }

out:
  if (error)
    g_dbus_method_invocation_take_error (invocation, error);
  else
    thunar_dbus_trash_complete_display_trash (object, invocation);

  return TRUE;
}



static gboolean
thunar_dbus_service_empty_trash (ThunarDBusTrash        *object,
                                 GDBusMethodInvocation  *invocation,
                                 const gchar            *display,
                                 const gchar            *startup_id,
                                 ThunarDBusService      *dbus_service)
{
  ThunarApplication *application;
  GdkScreen         *screen;
  GError            *error = NULL;

  /* try to open the screen for the display name */
  screen = thunar_gdk_screen_open (display, &error);
  if (G_LIKELY (screen != NULL))
    {
      /* tell the application to empty the trash bin */
      application = thunar_application_get ();
      thunar_application_empty_trash (application, screen, startup_id);
      g_object_unref (G_OBJECT (application));

      /* release the screen */
      g_object_unref (G_OBJECT (screen));
    }

  if (error)
    g_dbus_method_invocation_take_error (invocation, error);
  else
    thunar_dbus_trash_complete_empty_trash (object, invocation);

  return TRUE;
}



static gboolean
thunar_dbus_service_move_to_trash (ThunarDBusTrash        *object,
                                   GDBusMethodInvocation  *invocation,
                                   gchar                 **filenames,
                                   const gchar            *display,
                                   const gchar            *startup_id,
                                   ThunarDBusService      *dbus_service)
{
  ThunarApplication *application;
  GFile             *file;
  GdkScreen         *screen;
  GError            *err = NULL;
  GList             *file_list = NULL;
  gchar             *filename;
  guint              n;

  /* try to open the screen for the display name */
  screen = thunar_gdk_screen_open (display, &err);
  if (G_LIKELY (screen != NULL))
    {
      /* try to parse the specified filenames */
      for (n = 0; err == NULL && filenames[n] != NULL; ++n)
        {
          /* decode the filename (D-BUS uses UTF-8) */
          filename = g_filename_from_utf8 (filenames[n], -1, NULL, NULL, &err);
          if (G_LIKELY (err == NULL))
            {
              /* determine the path for the filename */
              /* TODO Not sure this will work as expected */
              file = g_file_new_for_commandline_arg (filename);
              file_list = thunar_g_list_append_deep (file_list, file);
              g_object_unref (file);
            }

          /* cleanup */
          g_free (filename);
        }

      /* check if we succeed */
      if (G_LIKELY (err == NULL))
        {
          /* tell the application to move the specified files to the trash */
          application = thunar_application_get ();
          thunar_application_trash (application, screen, file_list, THUNAR_OPERATION_LOG_NO_OPERATIONS);
          g_object_unref (application);
        }

      /* cleanup */
      thunar_g_list_free_full (file_list);
      g_object_unref (screen);
    }

  /* check if we failed */
  if (err)
    g_dbus_method_invocation_take_error (invocation, err);
  else
    thunar_dbus_trash_complete_move_to_trash (object, invocation);

  return TRUE;
}



static gboolean
thunar_dbus_service_query_trash (ThunarDBusTrash        *object,
                                 GDBusMethodInvocation  *invocation,
                                 ThunarDBusService      *dbus_service)
{
  GError *error = NULL;
  gboolean full = FALSE;

  /* connect to the trash bin on-demand */
  if (thunar_dbus_service_connect_trash_bin (dbus_service, &error))
    {
      /* check whether the trash bin is not empty */
      full = (thunar_file_get_item_count (dbus_service->trash_bin) > 0);
    }

  if (error)
    g_dbus_method_invocation_take_error (invocation, error);
  else
    thunar_dbus_trash_complete_query_trash (object, invocation, full);

  return TRUE;
}



static gboolean
thunar_dbus_service_bulk_rename (ThunarDBusThunar  *object,
                                 GDBusMethodInvocation  *invocation,
                                 const gchar            *working_directory,
                                 gchar                 **filenames,
                                 gboolean                standalone,
                                 const gchar            *display,
                                 const gchar            *startup_id,
                                 ThunarDBusService      *dbus_service)
{
  ThunarApplication *application;
  GdkScreen         *screen;
  gchar             *cwd;
  GError            *error = NULL;

  /* determine a proper working directory */
  cwd = (working_directory != NULL && *working_directory != '\0')
      ? g_strdup (working_directory)
      : g_get_current_dir ();

  /* try to open the screen for the display name */
  screen = thunar_gdk_screen_open (display, &error);
  if (G_LIKELY (screen != NULL))
    {
      /* tell the application to display the bulk rename dialog */
      application = thunar_application_get ();
      thunar_application_bulk_rename (application, cwd, filenames, standalone, screen, startup_id, &error);
      g_object_unref (G_OBJECT (application));

      /* release the screen */
      g_object_unref (G_OBJECT (screen));
    }

  /* release the cwd */
  g_free (cwd);

  if (error)
    g_dbus_method_invocation_take_error (invocation, error);
  else
    thunar_dbus_thunar_complete_bulk_rename (object, invocation);

  return TRUE;
}



static gboolean
thunar_dbus_service_launch_files (ThunarDBusFileManager  *object,
                                  GDBusMethodInvocation  *invocation,
                                  const gchar            *working_directory,
                                  gchar                 **filenames,
                                  const gchar            *display,
                                  const gchar            *startup_id,
                                  ThunarDBusService      *dbus_service)
{
  ThunarApplication *application;
  GdkScreen         *screen;
  GError            *error = NULL;

  /* verify that a valid working directory is given */
  if (G_UNLIKELY (!g_path_is_absolute (working_directory)))
    {
      /* LaunchFiles() invoked without a valid working directory */
      g_set_error (&error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("The working directory must be an absolute path"));
      goto out;
    }

  /* verify that at least one filename is given */
  if (G_UNLIKELY (filenames == NULL || *filenames == NULL))
    {
      /* LaunchFiles() invoked with an empty filename list */
      g_set_error (&error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("At least one filename must be specified"));
      goto out;
    }

  /* try to open the screen for the display name */
  screen = thunar_gdk_screen_open (display, &error);
  if (G_LIKELY (screen != NULL))
    {
      /* let the application process the filenames */
      application = thunar_application_get ();
      thunar_application_process_filenames (application, working_directory, filenames, screen, startup_id, &error, THUNAR_APPLICATION_LAUNCH_FILES);
      g_object_unref (G_OBJECT (application));

      /* release the screen */
      g_object_unref (G_OBJECT (screen));
    }

out:
  if (error)
    g_dbus_method_invocation_take_error (invocation, error);
  else
    thunar_dbus_file_manager_complete_launch_files (object, invocation);

  return TRUE;
}



static gboolean
thunar_dbus_service_rename_file (ThunarDBusFileManager  *object,
                                 GDBusMethodInvocation  *invocation,
                                 const gchar            *uri,
                                 const gchar            *display,
                                 const gchar            *startup_id,
                                 ThunarDBusService      *dbus_service)
{
  ThunarApplication *application;
  ThunarFile        *file;
  GdkScreen         *screen;
  GError            *error = NULL;

  /* parse uri and display parameters */
  if (!thunar_dbus_service_parse_uri_and_display (dbus_service, uri, display, &file, &screen, &error))
    goto out;

  /* popup a new window for the folder */
  application = thunar_application_get ();
  thunar_application_rename_file (application, file, screen, startup_id, THUNAR_OPERATION_LOG_NO_OPERATIONS);
  g_object_unref (G_OBJECT (application));

  /* cleanup */
  g_object_unref (G_OBJECT (screen));
  g_object_unref (G_OBJECT (file));

out:
  if (error)
    g_dbus_method_invocation_take_error (invocation, error);
  else
    thunar_dbus_file_manager_complete_rename_file (object, invocation);

  return TRUE;
}



static gboolean
thunar_dbus_service_create_file (ThunarDBusFileManager  *object,
                                 GDBusMethodInvocation  *invocation,
                                 const gchar            *parent_directory,
                                 const gchar            *content_type,
                                 const gchar            *display,
                                 const gchar            *startup_id,
                                 ThunarDBusService      *dbus_service)
{
  ThunarApplication *application;
  ThunarFile        *file;
  GdkScreen         *screen;
  GError            *error = NULL;

  /* parse uri and display parameters */
  if (!thunar_dbus_service_parse_uri_and_display (dbus_service, parent_directory, display, &file, &screen, &error))
    goto out;

  /* fall back to plain text file if no content type is provided */
  if (content_type == NULL || *content_type == '\0')
    content_type = "text/plain";

  /* popup a new window for the folder */
  application = thunar_application_get ();
  thunar_application_create_file (application, file, content_type, screen, startup_id, THUNAR_OPERATION_LOG_NO_OPERATIONS);
  g_object_unref (G_OBJECT (application));

  /* cleanup */
  g_object_unref (G_OBJECT (screen));
  g_object_unref (G_OBJECT (file));

out:
  if (error)
    g_dbus_method_invocation_take_error (invocation, error);
  else
    thunar_dbus_file_manager_complete_create_file (object, invocation);

  return TRUE;
}



static gboolean
thunar_dbus_service_create_file_from_template (ThunarDBusFileManager  *object,
                                               GDBusMethodInvocation  *invocation,
                                               const gchar            *parent_directory,
                                               const gchar            *template_uri,
                                               const gchar            *display,
                                               const gchar            *startup_id,
                                               ThunarDBusService      *dbus_service)
{
  ThunarApplication *application;
  ThunarFile        *file;
  ThunarFile        *template_file;
  GdkScreen         *screen;
  GError            *error = NULL;

  /* parse uri and display parameters */
  if (!thunar_dbus_service_parse_uri_and_display (dbus_service, parent_directory, display, &file, &screen, &error))
    goto out;

  /* try to determine the file for the template URI */
  template_file = thunar_file_get_for_uri (template_uri, &error);
  if(template_file == NULL)
    goto out;

  /* popup a new window for the folder */
  application = thunar_application_get ();
  thunar_application_create_file_from_template (application, file, template_file, screen, startup_id, THUNAR_OPERATION_LOG_NO_OPERATIONS);
  g_object_unref (G_OBJECT (application));

  /* cleanup */
  g_object_unref (G_OBJECT (screen));
  g_object_unref (G_OBJECT (file));

out:
  if (error)
    g_dbus_method_invocation_take_error (invocation, error);
  else
    thunar_dbus_file_manager_complete_create_file_from_template (object, invocation);

  return TRUE;
}



static gboolean
thunar_dbus_service_transfer_files (ThunarDBusTransferMode transfer_mode,
                                    const gchar           *working_directory,
                                    const gchar * const   *source_filenames,
                                    const gchar * const   *target_filenames,
                                    const gchar           *display,
                                    const gchar           *startup_id,
                                    GError               **error)
{
  ThunarApplication *application;
  GdkScreen         *screen;
  GError            *err = NULL;
  GFile             *file;
  GList             *source_file_list = NULL;
  GList             *target_file_list = NULL;
  gchar             *filename;
  gchar             *new_working_dir = NULL;
  gchar             *old_working_dir = NULL;
  guint              n;

  /* verify that at least one file to transfer is given */
  if (source_filenames == NULL || *source_filenames == NULL)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                   _("At least one source filename must be specified"));
      return FALSE;
    }

  /* verify that the target filename is set / enough target filenames are given */
  if (transfer_mode == THUNAR_DBUS_TRANSFER_MODE_COPY_TO)
    {
      if (g_strv_length ((gchar **)source_filenames) != g_strv_length ((gchar **)target_filenames))
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                       _("The number of source and target filenames must be the same"));
          return FALSE;
        }
    }
  else
    {
      if (target_filenames == NULL || *target_filenames == NULL)
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                       _("A destination directory must be specified"));
          return FALSE;
        }
    }

  /* try to open the screen for the display name */
  screen = thunar_gdk_screen_open (display, &err);
  if (screen != NULL)
    {
      /* change the working directory if necessary */
      if (!xfce_str_is_empty (working_directory))
        old_working_dir = thunar_util_change_working_directory (working_directory);

      /* transform the source filenames into GFile objects */
      for (n = 0; err == NULL && source_filenames[n] != NULL; ++n)
        {
          filename = g_filename_from_utf8 (source_filenames[n], -1, NULL, NULL, &err);
          if (filename != NULL)
            {
              file = g_file_new_for_commandline_arg (filename);
              source_file_list = thunar_g_list_append_deep (source_file_list, file);
              g_object_unref (file);
              g_free (filename);
            }
        }

      /* transform the target filename(s) into (a) GFile object(s) */
      for (n = 0; err == NULL && target_filenames[n] != NULL; ++n)
        {
          filename = g_filename_from_utf8 (target_filenames[n], -1, NULL, NULL, &err);
          if (filename != NULL)
            {
              file = g_file_new_for_commandline_arg (filename);
              target_file_list = thunar_g_list_append_deep (target_file_list, file);
              g_object_unref (file);
              g_free (filename);
            }
        }

      /* switch back to the previous working directory */
      if (!xfce_str_is_empty (working_directory))
        {
          new_working_dir = thunar_util_change_working_directory (old_working_dir);
          g_free (old_working_dir);
          g_free (new_working_dir);
        }

      if (err == NULL)
        {
          /* let the application process the filenames */
          application = thunar_application_get ();
          switch (transfer_mode)
            {
            case THUNAR_DBUS_TRANSFER_MODE_COPY_TO:
              thunar_application_copy_to (application, screen,
                                          source_file_list, target_file_list,
                                          THUNAR_OPERATION_LOG_NO_OPERATIONS,
                                          NULL);
              break;
            case THUNAR_DBUS_TRANSFER_MODE_COPY_INTO:
              thunar_application_copy_into (application, screen,
                                            source_file_list, target_file_list->data,
                                            THUNAR_OPERATION_LOG_NO_OPERATIONS,
                                            NULL);
              break;
            case THUNAR_DBUS_TRANSFER_MODE_MOVE_INTO:
              thunar_application_move_into (application, screen,
                                            source_file_list, target_file_list->data,
                                            THUNAR_OPERATION_LOG_NO_OPERATIONS,
                                            NULL);
              break;
            case THUNAR_DBUS_TRANSFER_MODE_LINK_INTO:
              thunar_application_link_into (application, screen,
                                            source_file_list, target_file_list->data,
                                            THUNAR_OPERATION_LOG_NO_OPERATIONS,
                                            NULL);
              break;
            }
          g_object_unref (application);
        }

      /* free the file lists */
      thunar_g_list_free_full (source_file_list);
      thunar_g_list_free_full (target_file_list);

      /* release the screen */
      g_object_unref (screen);
    }

  if (err != NULL)
    {
      g_propagate_error (error, err);
      return FALSE;
    }

  return TRUE;
}



static gboolean
thunar_dbus_service_copy_to (ThunarDBusFileManager  *object,
                             GDBusMethodInvocation  *invocation,
                             const gchar            *working_directory,
                             gchar                 **source_filenames,
                             gchar                 **target_filenames,
                             const gchar            *display,
                             const gchar            *startup_id,
                             ThunarDBusService      *dbus_service)
{
  GError *error = NULL;

  thunar_dbus_service_transfer_files (THUNAR_DBUS_TRANSFER_MODE_COPY_TO,
                                      working_directory,
                                      (const gchar * const *)source_filenames,
                                      (const gchar * const *)target_filenames,
                                      display,
                                      startup_id,
                                      &error);

  if (error)
    g_dbus_method_invocation_take_error (invocation, error);
  else
    thunar_dbus_file_manager_complete_copy_to (object, invocation);

  return TRUE;
}



static gboolean
thunar_dbus_service_copy_into (ThunarDBusFileManager  *object,
                               GDBusMethodInvocation  *invocation,
                               const gchar            *working_directory,
                               gchar                 **source_filenames,
                               const gchar            *target_filename,
                               const gchar            *display,
                               const gchar            *startup_id,
                               ThunarDBusService      *dbus_service)
{
  const gchar *target_filenames[2] = { target_filename, NULL };
  GError *error = NULL;

  thunar_dbus_service_transfer_files (THUNAR_DBUS_TRANSFER_MODE_COPY_INTO,
                                      working_directory,
                                      (const gchar * const *)source_filenames,
                                      target_filenames,
                                      display,
                                      startup_id,
                                      &error);

  if (error)
    g_dbus_method_invocation_take_error (invocation, error);
  else
    thunar_dbus_file_manager_complete_copy_into (object, invocation);

  return TRUE;
}



static gboolean
thunar_dbus_service_move_into (ThunarDBusFileManager  *object,
                               GDBusMethodInvocation  *invocation,
                               const gchar            *working_directory,
                               gchar                 **source_filenames,
                               const gchar            *target_filename,
                               const gchar            *display,
                               const gchar            *startup_id,
                               ThunarDBusService      *dbus_service)
{
  const gchar *target_filenames[2] = { target_filename, NULL };
  GError *error = NULL;

  thunar_dbus_service_transfer_files (THUNAR_DBUS_TRANSFER_MODE_MOVE_INTO,
                                      working_directory,
                                      (const gchar * const *)source_filenames,
                                      target_filenames,
                                      display,
                                      startup_id,
                                      &error);

  if (error)
    g_dbus_method_invocation_take_error (invocation, error);
  else
    thunar_dbus_file_manager_complete_move_into (object, invocation);

  return TRUE;
}



static gboolean
thunar_dbus_service_link_into (ThunarDBusFileManager  *object,
                               GDBusMethodInvocation  *invocation,
                               const gchar            *working_directory,
                               gchar                 **source_filenames,
                               const gchar            *target_filename,
                               const gchar            *display,
                               const gchar            *startup_id,
                               ThunarDBusService      *dbus_service)
{
  const gchar *target_filenames[2] = { target_filename, NULL };
  GError *error = NULL;

  thunar_dbus_service_transfer_files (THUNAR_DBUS_TRANSFER_MODE_LINK_INTO,
                                      working_directory,
                                      (const gchar * const *)source_filenames,
                                      target_filenames,
                                      display,
                                      startup_id,
                                      &error);

  if (error)
    g_dbus_method_invocation_take_error (invocation, error);
  else
    thunar_dbus_file_manager_complete_link_into (object, invocation);

  return TRUE;
}


static gboolean
thunar_dbus_service_unlink_files (ThunarDBusFileManager  *object,
                                  GDBusMethodInvocation  *invocation,
                                  const gchar            *working_directory,
                                  gchar                 **filenames,
                                  const gchar            *display,
                                  const gchar            *startup_id,
                                  ThunarDBusService      *dbus_service)
{
  ThunarApplication *application;
  ThunarFile        *thunar_file;
  GFile             *file;
  GdkScreen         *screen;
  GError            *err = NULL;
  GList             *file_list = NULL;
  gchar             *filename;
  gchar             *new_working_dir = NULL;
  gchar             *old_working_dir = NULL;
  guint              n;

  /* verify that atleast one filename is given */
  if (filenames == NULL || *filenames == NULL)
    {
      g_set_error (&err, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("At least one filename must be specified"));
      goto out;
    }

  /* try to open the screen for the display name */
  screen = thunar_gdk_screen_open (display, &err);
  if (screen != NULL)
    {
      /* change the working directory if necessary */
      if (!xfce_str_is_empty (working_directory))
        old_working_dir = thunar_util_change_working_directory (working_directory);

      /* try to parse the specified filenames */
      for (n = 0; err == NULL && filenames[n] != NULL; ++n)
        {
          /* decode the filename (D-BUS uses UTF-8) */
          filename = g_filename_from_utf8 (filenames[n], -1, NULL, NULL, &err);
          if (filename != NULL)
            {
              /* determine the path for the filename */
              file = g_file_new_for_commandline_arg (filename);
              thunar_file = thunar_file_get (file, &err);

              if (thunar_file != NULL)
                file_list = g_list_append (file_list, thunar_file);

              g_object_unref (file);
            }

          /* cleanup */
          g_free (filename);
        }

      /* switch back to the previous working directory */
      if (!xfce_str_is_empty (working_directory))
        {
          new_working_dir = thunar_util_change_working_directory (old_working_dir);
          g_free (old_working_dir);
          g_free (new_working_dir);
        }

      /* check if we succeeded */
      if (err == NULL && file_list != NULL)
        {
          /* tell the application to move the specified files to the trash */
          application = thunar_application_get ();
          thunar_application_unlink_files (application, screen, file_list, TRUE, TRUE);
          g_object_unref (application);
        }

      /* cleanup */
      thunar_g_list_free_full (file_list);
      g_object_unref (screen);
    }

out:
  if (err)
    g_dbus_method_invocation_take_error (invocation, err);
  else
    thunar_dbus_file_manager_complete_unlink_files (object, invocation);

  return TRUE;
}



static gboolean
thunar_dbus_service_terminate (ThunarDBusThunar       *object,
                               GDBusMethodInvocation  *invocation,
                               ThunarDBusService      *dbus_service)
{
  /* leave the Gtk main loop as soon as possible */
  ThunarApplication *app = thunar_application_get ();
  g_application_quit (G_APPLICATION (app));
  g_object_unref (app);

  /* we cannot fail */
  thunar_dbus_thunar_complete_terminate (object, invocation);

  return TRUE;
}



static gboolean
thunar_dbus_freedesktop_show_folders (ThunarOrgFreedesktopFileManager1 *object,
                                      GDBusMethodInvocation            *invocation,
                                      gchar                           **uris,
                                      const gchar                      *startup_id,
                                      ThunarDBusService                *dbus_service)
{
  ThunarApplication *application;
  GdkScreen         *screen;
  gint               n;
  GFile             *file;
  ThunarFile        *thunar_file;

  screen = gdk_screen_get_default ();
  application = thunar_application_get ();

  for (n = 0; uris[n] != NULL; ++n)
    {
      file = g_file_new_for_uri (uris[n]);
      thunar_file = thunar_file_get (file, NULL);

      g_object_unref (G_OBJECT (file));
      if (thunar_file == NULL)
        continue;

      if (thunar_file_is_directory (thunar_file))
        thunar_application_open_window (application, thunar_file, screen,
                                        startup_id, FALSE);

      g_object_unref (G_OBJECT (thunar_file));
    }

  g_object_unref (G_OBJECT (application));

  thunar_org_freedesktop_file_manager1_complete_show_folders (object, invocation);

  return TRUE;
}



static gboolean
thunar_dbus_freedesktop_show_items (ThunarOrgFreedesktopFileManager1 *object,
                                    GDBusMethodInvocation            *invocation,
                                    gchar                           **uris,
                                    const gchar                      *startup_id,
                                    ThunarDBusService                *dbus_service)
{
  ThunarApplication *application;
  GtkWidget         *window;
  GdkScreen         *screen;
  gint               n;
  GList             *gfiles = NULL;
  ThunarFile        *thunar_file = NULL;
  ThunarFile        *thunar_folder = NULL;
  GError            *error = NULL;

  screen = gdk_screen_get_default ();
  application = thunar_application_get ();

  for (n = 0; uris[n] != NULL; ++n)
    {
      thunar_file = thunar_file_get_for_uri (uris[n], &error);
      if (error)
        {
          g_dbus_method_invocation_take_error (invocation, error);
          continue;
        }

      thunar_folder = NULL;
      if (G_LIKELY (thunar_file_has_parent (thunar_file)))
        thunar_folder = thunar_file_get_parent (thunar_file, NULL);

      if (thunar_folder == NULL)
        {
          g_object_unref (G_OBJECT (thunar_file));
          continue;
        }

      window = thunar_application_open_window (application, thunar_folder,
                                               screen, startup_id, FALSE);

      gfiles = g_list_append (gfiles, thunar_file_get_file (thunar_file));
      thunar_window_show_and_select_files (THUNAR_WINDOW (window), gfiles);

      g_list_free (gfiles);
      gfiles = NULL;
      g_object_unref (G_OBJECT (thunar_file));
      g_object_unref (G_OBJECT (thunar_folder));
    }

  g_object_unref (G_OBJECT (application));

  thunar_org_freedesktop_file_manager1_complete_show_items (object, invocation);
  return TRUE;
}



static gboolean
thunar_dbus_freedesktop_show_item_properties (ThunarOrgFreedesktopFileManager1 *object,
                                              GDBusMethodInvocation            *invocation,
                                              gchar                           **uris,
                                              const gchar                      *startup_id,
                                              ThunarDBusService                *dbus_service)
{
  ThunarApplication *application;
  GdkScreen         *screen;
  gint               n;
  GList             *thunar_files = NULL;
  GFile             *file;
  GtkWidget         *dialog;
  ThunarFile        *thunar_file;

  screen = gdk_screen_get_default ();
  application = thunar_application_get ();

  for (n = 0; uris[n] != NULL; ++n)
    {
      file = g_file_new_for_uri (uris[n]);
      thunar_file = thunar_file_get (file, NULL);

      g_object_unref (G_OBJECT (file));
      if (thunar_file == NULL)
        continue;

      thunar_files = g_list_prepend (thunar_files, thunar_file);
    }

  if (thunar_files != NULL)
    {
      thunar_files = g_list_reverse (thunar_files);

      dialog = thunar_properties_dialog_new (NULL, THUNAR_PROPERTIES_DIALOG_NONE);
      gtk_window_set_screen (GTK_WINDOW (dialog), screen);
      gtk_window_set_startup_id (GTK_WINDOW (dialog), startup_id);
      thunar_properties_dialog_set_files (THUNAR_PROPERTIES_DIALOG (dialog),
                                          thunar_files);
      gtk_window_present (GTK_WINDOW (dialog));
      thunar_application_take_window (application, GTK_WINDOW (dialog));

      g_list_free_full (thunar_files, g_object_unref);
    }

  g_object_unref (G_OBJECT (application));

  thunar_org_freedesktop_file_manager1_complete_show_item_properties (object, invocation);
  return TRUE;
}



gboolean thunar_dbus_service_export_on_connection (ThunarDBusService *service,
                                                   GDBusConnection   *connection,
                                                   GError           **error)
{
  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (service->file_manager),
                                         connection,
                                         "/org/xfce/FileManager",
                                         error))
    goto fail;

  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (service->trash),
                                         connection,
                                         "/org/xfce/FileManager",
                                         error))
    goto fail;

  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (service->thunar),
                                         connection,
                                         "/org/xfce/FileManager",
                                         error))
    goto fail;

  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (service->file_manager_fdo),
                                         connection,
                                         "/org/freedesktop/FileManager1",
                                         error))
    goto fail;

  return TRUE;

fail:
  /* export either all or nothing */
  g_dbus_interface_skeleton_unexport_from_connection (G_DBUS_INTERFACE_SKELETON (service->file_manager), connection);
  g_dbus_interface_skeleton_unexport_from_connection (G_DBUS_INTERFACE_SKELETON (service->trash), connection);
  g_dbus_interface_skeleton_unexport_from_connection (G_DBUS_INTERFACE_SKELETON (service->thunar), connection);
  g_dbus_interface_skeleton_unexport_from_connection (G_DBUS_INTERFACE_SKELETON (service->file_manager_fdo), connection);
  return FALSE;
}
