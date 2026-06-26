/*  $Id$
 *
 *  Copyright © 2008-2010 Jérôme Guelfucci <jeromeg@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "screenshooter-utils.h"

#include <glib/gstdio.h>
#include <libxfce4ui/libxfce4ui.h>

#ifdef ENABLE_WAYLAND
#include <gdk/gdkwayland.h>
#endif

/* Internals */



/**
 * @format - a date format string
 *
 * Builds a timestamp using local time.
 * Returned string should be released with g_free()
 **/
static gchar *
get_datetime (const gchar *format)
{
  gchar *timestamp;
  GDateTime *now = g_date_time_new_now_local ();
  timestamp = g_date_time_format (now, format);

  g_date_time_unref (now);
  /* TODO: strip potential : and / if the format is configurable */
  DBG ("datetime is %s", timestamp);
  return timestamp;
}



/* Check whether specified path is a writable directory
 * @path: URI to the directory
 */
static gboolean
screenshooter_is_directory_writable (const gchar *path)
{
  GFile *dir;
  GFileInfo *info;
  GError *error = NULL;
  gboolean result;

  dir = g_file_new_for_uri (path);
  info = g_file_query_info (dir, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE ","
                               G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE ","
                               G_FILE_ATTRIBUTE_STANDARD_TYPE,
                               G_FILE_QUERY_INFO_NONE, NULL, &error);

  result = (g_file_query_exists (dir, NULL) &&
            g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY &&
            g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE) &&
            g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE));

  if (G_UNLIKELY (info == NULL))
    {
      g_warning ("Failed to query file info: %s", path);
      g_error_free (error);
      return FALSE;
    }

  g_object_unref (dir);
  g_object_unref (info);

  return result;
}



/* Public */



/* Copy the screenshot to the Clipboard.
* Code is from gnome-screenshooter.
* @screenshot: the screenshot
*/
void
screenshooter_copy_to_clipboard (GdkPixbuf *screenshot)
{
  GtkClipboard *clipboard;

  TRACE ("Adding the image to the clipboard...");

  clipboard =
    gtk_clipboard_get_for_display (gdk_display_get_default(), GDK_SELECTION_CLIPBOARD);

  gtk_clipboard_set_image (clipboard, screenshot);

  gtk_clipboard_store (clipboard);
}



/* Read the options from file and sets the sd values.
@file: the path to the rc file.
@sd: the ScreenshotData to be set.
@dir_only: if true, only read the screenshot_dir.
*/
void
screenshooter_read_rc_file (const gchar *file, ScreenshotData *sd)
{
  gchar *default_uri = screenshooter_get_xdg_image_dir_uri ();

  XfceRc *rc;
  gint delay = 0;
  gint region = FULLSCREEN;
  gint action = SAVE;
  gint show_mouse = 1;
  gint show_border = 1;
  gboolean timestamp = TRUE;
  gboolean show_in_folder = FALSE;
  gchar *screenshot_dir = g_strdup (default_uri);
  gchar *title = g_strdup (_("Screenshot"));
  gchar *app = g_strdup ("none");
  gchar *last_user = g_strdup ("");
  gchar *last_extension = g_strdup ("png");
  gchar *last_custom_action_command = g_strdup ("none");

  if (G_LIKELY (file != NULL))
    {
      TRACE ("Open the rc file");

      rc = xfce_rc_simple_open (file, TRUE);

      if (G_LIKELY (rc != NULL))
        {
          TRACE ("Read the entries");

          delay = xfce_rc_read_int_entry (rc, "delay", 0);
          region = xfce_rc_read_int_entry (rc, "region", FULLSCREEN);
          action = xfce_rc_read_int_entry (rc, "action", SAVE);
          show_mouse = xfce_rc_read_int_entry (rc, "show_mouse", 1);
          show_border = xfce_rc_read_int_entry (rc, "show_border", 1);
          timestamp = xfce_rc_read_bool_entry (rc, "timestamp", TRUE);
          show_in_folder = xfce_rc_read_bool_entry (rc, "show_in_folder", FALSE);

          g_free (app);
          app = g_strdup (xfce_rc_read_entry (rc, "app", "none"));

          g_free (last_custom_action_command);
          last_custom_action_command = g_strdup (xfce_rc_read_entry (rc, "custom_action_command", "none"));

          g_free (last_user);
          last_user = g_strdup (xfce_rc_read_entry (rc, "last_user", ""));

          g_free (last_extension);
          last_extension = g_strdup (xfce_rc_read_entry (rc, "last_extension", "png"));

          g_free (screenshot_dir);
          screenshot_dir =
            g_strdup (xfce_rc_read_entry (rc, "screenshot_dir", default_uri));

          g_free (title);
          title =
            g_strdup (xfce_rc_read_entry (rc, "title", _("Screenshot")));

          TRACE ("Close the rc file");

          xfce_rc_close (rc);
        }
    }

  /* And set the sd values */
  TRACE ("Set the values of the struct");

  sd->delay = delay;
  sd->region = region;
  sd->action = action;
  sd->show_mouse = show_mouse;
  sd->show_border = show_border;
  sd->timestamp = timestamp;
  sd->screenshot_dir = screenshot_dir;
  sd->title = title;
  sd->app = app;
  sd->app_info = NULL;
  sd->last_user = last_user;
  sd->last_extension = last_extension;
  sd->show_in_folder = show_in_folder;
  sd->custom_action_command = last_custom_action_command;

  /* Under Wayland force FULLSCREEN region */
#ifdef ENABLE_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ()))
    {
      sd->region = FULLSCREEN;
    }
#endif

  /* Check if the screenshot directory read from the preferences is valid */
  if (G_UNLIKELY (!screenshooter_is_directory_writable (sd->screenshot_dir)))
    {
      g_warning ("Invalid directory or permissions: %s", sd->screenshot_dir);
      g_free (sd->screenshot_dir);
      sd->screenshot_dir = g_strdup (default_uri);
    }

  g_free (default_uri);
}



/* Writes the options from sd to file.
@file: the path to the rc file.
@sd: a ScreenshotData.
*/
void
screenshooter_write_rc_file (const gchar *file, ScreenshotData *sd)
{
  XfceRc *rc;

  g_return_if_fail (file != NULL);

  TRACE ("Open the rc file");

  rc = xfce_rc_simple_open (file, FALSE);

  g_return_if_fail (rc != NULL);

  TRACE ("Write the entries.");

  xfce_rc_write_entry (rc, "app", sd->app);
  xfce_rc_write_entry (rc, "custom_action_command", sd->custom_action_command);
  xfce_rc_write_entry (rc, "last_user", sd->last_user);
  xfce_rc_write_entry (rc, "last_extension", sd->last_extension);
  xfce_rc_write_bool_entry (rc, "show_in_folder", sd->show_in_folder);

  /* do not save if screenshot_dir is not path, i.e. specified from cli */
  if (sd->path_is_dir)
  {
    xfce_rc_write_entry (rc, "screenshot_dir", sd->screenshot_dir);
  }

  /* do not save if action was specified from cli */
  if (!sd->action_specified)
  {
    xfce_rc_write_int_entry (rc, "action", sd->action);
  }

  /* do not save if region was specified from cli */
  if (!sd->region_specified)
  {
    xfce_rc_write_int_entry (rc, "delay", sd->delay);
    xfce_rc_write_int_entry (rc, "region", sd->region);
    xfce_rc_write_int_entry (rc, "show_mouse", sd->show_mouse);
    xfce_rc_write_int_entry (rc, "show_border", sd->show_border);
  }

  /* clean up rc, remove this after some releases */
  xfce_rc_delete_entry (rc, "enable_imgur_upload", TRUE);

  TRACE ("Flush and close the rc file");
  xfce_rc_close (rc);
}



/* Opens the screenshot using application.
@screenshot_path: the path to the saved screenshot.
@application: the command to run the application.
@app_info: GAppInfo object associated with application.
*/
void
screenshooter_open_screenshot (const gchar *screenshot_path, const gchar *application, GAppInfo *const app_info)
{
  gpointer screenshot_file = NULL;
  gboolean success = TRUE;
  gchar   *command;
  GError  *error = NULL;
  GList   *files = NULL;

  g_return_if_fail (screenshot_path != NULL);

  TRACE ("Path was != NULL");

  if (g_str_equal (application, "none"))
    return;

  TRACE ("Application was not none");

  if (app_info != NULL)
    {
      TRACE ("Launch the app");

      screenshot_file = g_file_new_for_path (screenshot_path);
      files = g_list_append (NULL, screenshot_file);
      success = g_app_info_launch (app_info, files, NULL, &error);
      g_list_free_full (files, g_object_unref);
    }
  else if (application != NULL)
    {
      TRACE ("Launch the command");

      command = g_strconcat (application, " ", "\"", screenshot_path, "\"", NULL);
      success = g_spawn_command_line_async (command, &error);
      g_free (command);
    }

  /* report any error */
  if (!success && error != NULL)
    {
      TRACE ("An error occured");
      screenshooter_error (_("<b>The application could not be launched.</b>\n%s"), error->message);
      g_error_free (error);
    }
}



gchar *screenshooter_get_home_uri (void)
{
  gchar *result = NULL;
  const gchar *home_path = g_getenv ("HOME");

  if (G_UNLIKELY (!home_path))
    home_path = g_get_home_dir ();

  result = g_strconcat ("file://", home_path, NULL);

  return result;
}



gchar *screenshooter_get_xdg_image_dir_uri (void)
{
  gchar *result, *tmp;

  tmp = g_strdup (g_get_user_special_dir (G_USER_DIRECTORY_PICTURES));

  if (tmp == NULL)
    return screenshooter_get_home_uri ();

  result = g_strconcat ("file://", tmp, NULL);
  g_free (tmp);

  return result;
}


gboolean screenshooter_is_remote_uri (const gchar *uri)
{
  g_return_val_if_fail(uri != NULL, FALSE);

  /* if the URI doesn't start  with "file://", we take it as remote */

  if (G_UNLIKELY (!g_str_has_prefix (uri, "file:")))
    return TRUE;

  return FALSE;
}



/**
 * screenshooter_error:
 * @format: printf()-style format string
 * @...: arguments for @format
 *
 * Shows a modal error dialog with the given error text. Blocks until the user
 * clicks the OK button.
 **/
void screenshooter_error (const gchar *format, ...)
{
  va_list va_args;
  gchar *message = NULL;
  GtkWidget *dialog;

  g_return_if_fail (format != NULL);

  va_start (va_args, format);
  message = g_strdup_vprintf (format, va_args);
  va_end (va_args);

  g_fprintf (stderr, "Error: %s\n", message);

  dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                                   NULL);
  gtk_window_set_title (GTK_WINDOW (dialog), _("Error"));
  gtk_window_set_icon_name (GTK_WINDOW (dialog), "dialog-error-symbolic");
  gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), message);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy  (dialog);

  g_free (message);
}



/* If @timestamp is true, generates a file name @title - date - hour - n.png,
 * where n is the lowest integer such as this file does not exist in the @uri
 * folder.
 * Else, generates a file name @title-n.png, where n is the lowest integer
 * such as this file does not exist in the @uri folder.
 *
 * @uri: uri of the folder for which the filename should be generated.
 * @title: the main title of the file name.
 * @timestamp: whether the date and the hour should be appended to the file name.
 *
 * returns: the filename or NULL if *uri == NULL.
*/
gchar *
screenshooter_get_filename_for_uri (const gchar *uri,
                                    const gchar *title,
                                    const gchar *extension,
                                    gboolean     timestamp)
{
  gboolean exists = TRUE;
  GFile *directory;
  GFile *file;
  gchar *base_name;
  gchar *datetime;
  const gchar *strftime_format = "%Y-%m-%d_%H-%M-%S";

  gint i;

  if (G_UNLIKELY (uri == NULL))
    {
      TRACE ("URI was NULL");
      return NULL;
    }

  TRACE ("Get the folder corresponding to the URI");
  datetime = get_datetime (strftime_format);
  directory = g_file_new_for_uri (uri);
  if (!timestamp)
    base_name = g_strconcat (title, ".", extension, NULL);
  else
    base_name = g_strconcat (title, "_", datetime, ".", extension, NULL);

  file = g_file_get_child (directory, base_name);

  if (!g_file_query_exists (file, NULL))
    {
      g_object_unref (file);
      g_object_unref (directory);

      return base_name;
    }

  g_object_unref (file);
  g_free (base_name);

  for (i = 1; exists; ++i)
    {
      const gchar *suffix = g_strdup_printf ("-%d.%s", i, extension);

      if (!timestamp)
         base_name = g_strconcat (title, suffix, NULL);
       else
         base_name = g_strconcat (title, "_", datetime, suffix, NULL);

      file = g_file_get_child (directory, base_name);
      exists = g_file_query_exists (file, NULL);

      if (exists)
        g_free (base_name);

      g_object_unref (file);
    }

  g_free (datetime);
  g_object_unref (directory);

  return base_name;
}



void screenshooter_open_help (GtkWindow *parent)
{
  xfce_dialog_show_help (parent, "screenshooter", "start", "");
}



gboolean
screenshooter_f1_key (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
  GtkWidget *window;

  if (event->keyval == GDK_KEY_F1)
    {
      window = gtk_widget_get_toplevel (widget);
      screenshooter_open_help (GTK_WINDOW (window));
      return TRUE;
    }

  return FALSE;
}



void
screenshooter_show_file_in_folder (const gchar *save_location)
{
  GDBusProxy *proxy;
  GVariantBuilder *builder;
  gchar *uri, *startup_id;

  if (save_location == NULL)
    return;

  uri = g_filename_to_uri (save_location, NULL, NULL);
  startup_id = g_strdup_printf ("%s-%ld", "xfce4-screenshooter",
                                g_get_monotonic_time () / G_TIME_SPAN_SECOND);
  proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                         G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                         NULL,
                                         "org.freedesktop.FileManager1",
                                         "/org/freedesktop/FileManager1",
                                         "org.freedesktop.FileManager1",
                                         NULL, NULL);
  builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
  g_variant_builder_add (builder, "s", uri);

  g_dbus_proxy_call_sync (proxy, "ShowItems",
                          g_variant_new ("(ass)", builder, startup_id),
                          G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);

  g_variant_builder_unref (builder);
  g_free (startup_id);
  g_free (uri);
}



gboolean
screenshooter_is_format_supported (const gchar *format)
{
  gboolean result = FALSE;
  GSList *supported_formats;
  gchar *name;

  supported_formats = gdk_pixbuf_get_formats ();

  for (GSList *lp = supported_formats; lp != NULL; lp = lp->next)
    {
      name = gdk_pixbuf_format_get_name (lp->data);
      if (G_UNLIKELY (g_strcmp0 (name, format) == 0 && gdk_pixbuf_format_is_writable (lp->data)))
        {
          result = TRUE;
          g_free (name);
          break;
        }
      g_free (name);
    }

  g_slist_free_1 (supported_formats);

  return result;
}



/* Changes the file permission to 0600 if its parent is not owned by the current user
 * @file: file to be checked
 */
void
screenshooter_restrict_file_permission (GFile *file)
{
  GFileInfo *info;
  GFile *parent;
  gchar *path;
  GError *error = NULL;

  parent = g_file_get_parent (file);
  path = g_file_get_path (file);
  info = g_file_query_info (parent, G_FILE_ATTRIBUTE_OWNER_USER, G_FILE_QUERY_INFO_NONE, NULL, &error);
  g_object_unref (parent);

  if (G_UNLIKELY (info == NULL))
    {
      g_warning ("Failed to query file info: %s", path);
      g_free (path);
      g_error_free (error);
      return;
    }

  if (g_strcmp0 (g_get_user_name (), g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_OWNER_USER)) != 0)
    {
      FILE *f = g_fopen (path, "w");
      g_chmod (path, S_IRUSR | S_IWUSR); /* 0600 */
      fclose (f);
    }

  g_free (path);
  g_object_unref (info);
}
