/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2006      Brian Tarricone, <bjt23@cornell.edu>
 *  Copyright(c) 2010-2011 Jannis Pohlmann, <jannis@xfce.org>
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
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <gio/gio.h>
#ifdef HAVE_GIO_UNIX
#include <gio/gunixmounts.h>
#endif

#include <gtk/gtk.h>

#include <libxfce4ui/libxfce4ui.h>

#include <exo/exo.h>

#ifdef HAVE_THUNARX
#include <thunarx/thunarx.h>
#endif

#include "xfdesktop-common.h"
#include "xfdesktop-file-icon.h"
#include "xfdesktop-file-manager-fdo-proxy.h"
#include "xfdesktop-file-manager-proxy.h"
#include "xfdesktop-file-utils.h"
#include "xfdesktop-trash-proxy.h"
#include "xfdesktop-thunar-proxy.h"

typedef struct {
    GtkWindow *parent;
    GFile *file;
} ExecuteData;

static void xfdesktop_file_utils_add_emblems(GdkPixbuf *pix, GList *emblems);

static XfdesktopTrash       *xfdesktop_file_utils_peek_trash_proxy(void);
static XfdesktopFileManager *xfdesktop_file_utils_peek_filemanager_proxy(void);
static XfdesktopFileManager1 *xfdesktop_file_utils_peek_filemanager_fdo_proxy(void);

static void xfdesktop_file_utils_trash_proxy_new_cb (GObject *source_object,
                                                     GAsyncResult *res,
                                                     gpointer user_data);

static void xfdesktop_file_utils_file_manager_proxy_new_cb (GObject *source_object,
                                                            GAsyncResult *res,
                                                            gpointer user_data);

static void xfdesktop_file_utils_file_manager_fdo_proxy_new_cb(GObject *source_object,
                                                               GAsyncResult *res,
                                                               gpointer user_data);

static void xfdesktop_file_utils_thunar_proxy_new_cb (GObject *source_object,
                                                      GAsyncResult *res,
                                                      gpointer user_data);

#ifdef HAVE_THUNARX
static XfdesktopThunar *xfdesktop_file_utils_peek_thunar_proxy(void);
#else
static gpointer xfdesktop_file_utils_peek_thunar_proxy(void);
#endif

gboolean
xfdesktop_file_utils_is_desktop_file(GFileInfo *info)
{
    const gchar *content_type;
    gboolean is_desktop_file = FALSE;

    content_type = g_file_info_get_content_type(info);
    if(content_type)
        is_desktop_file = g_content_type_equals(content_type, "application/x-desktop");

    return is_desktop_file
        && !g_str_has_suffix(g_file_info_get_name(info), ".directory");
}

gboolean
xfdesktop_file_utils_file_is_executable(GFileInfo *info)
{
    const gchar *content_type;
    gboolean can_execute = FALSE;

    g_return_val_if_fail(G_IS_FILE_INFO(info), FALSE);

    if(g_file_info_get_attribute_boolean(info, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE)) {
        /* get the content type of the file */
        content_type = g_file_info_get_content_type(info);
        if(content_type != NULL) {
#ifdef G_OS_WIN32
            /* check for .exe, .bar or .com */
            can_execute = g_content_type_can_be_executable(content_type);
#else
            /* check if the content type is save to execute, we don't use
             * g_content_type_can_be_executable() for unix because it also returns
             * true for "text/plain" and we don't want that */
            if(g_content_type_is_a(content_type, "application/x-executable")
               || g_content_type_is_a(content_type, "application/x-shellscript"))
            {
                can_execute = TRUE;
            }
#endif
        }
    }

    return can_execute || xfdesktop_file_utils_is_desktop_file(info);
}

gchar *
xfdesktop_file_utils_format_time_for_display(guint64 file_time)
{
    const gchar *date_format;
    struct tm *tfile;
    time_t ftime;
    GDate dfile;
    GDate dnow;
    gchar buffer[128];
    gint diff;

    /* check if the file_time is valid */
    if(file_time != 0) {
        ftime = (time_t) file_time;

        /* determine the local file time */
        tfile = localtime(&ftime);

        /* setup the dates for the time values */
        g_date_set_time_t(&dfile, (time_t) ftime);
        g_date_set_time_t(&dnow, time(NULL));

        /* determine the difference in days */
        diff = g_date_get_julian(&dnow) - g_date_get_julian(&dfile);
        if(diff == 0) {
            /* TRANSLATORS: file was modified less than one day ago */
            strftime(buffer, 128, _("Today at %X"), tfile);
            return g_strdup(buffer);
        } else if(diff == 1) {
            /* TRANSLATORS: file was modified less than two days ago */
            strftime(buffer, 128, _("Yesterday at %X"), tfile);
            return g_strdup(buffer);
        } else {
            if (diff > 1 && diff < 7) {
                /* Days from last week */
                date_format = _("%A at %X");
            } else {
                /* Any other date */
                date_format = _("%x at %X");
            }

            /* format the date string accordingly */
            strftime(buffer, 128, date_format, tfile);
            return g_strdup(buffer);
        }
    }

    /* the file_time is invalid */
    return g_strdup(_("Unknown"));
}

GKeyFile *
xfdesktop_file_utils_query_key_file(GFile *file,
                                    GCancellable *cancellable,
                                    GError **error)
{
    GKeyFile *key_file;
    gchar *contents = NULL;
    gsize length;

    g_return_val_if_fail(G_IS_FILE(file), NULL);
    g_return_val_if_fail(cancellable == NULL || G_IS_CANCELLABLE(cancellable), NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    /* try to load the entire file into memory */
    if (!g_file_load_contents(file, cancellable, &contents, &length, NULL, error))
        return NULL;

    /* allocate a new key file */
    key_file = g_key_file_new();

    /* try to parse the key file from the contents of the file */
    if (length == 0
        || g_key_file_load_from_data(key_file, contents, length,
                                     G_KEY_FILE_KEEP_COMMENTS
                                     | G_KEY_FILE_KEEP_TRANSLATIONS,
                                     error))
    {
        g_free(contents);
        return key_file;
    }
    else
    {
        g_free(contents);
        g_key_file_free(key_file);
        return NULL;
    }
}

gchar *
xfdesktop_file_utils_get_display_name(GFile *file,
                                      GFileInfo *info)
{
    GKeyFile *key_file;
    gchar *display_name = NULL;

    g_return_val_if_fail(G_IS_FILE_INFO(info), NULL);

    /* check if we have a desktop entry */
    if(xfdesktop_file_utils_is_desktop_file(info)) {
        /* try to load its data into a GKeyFile */
        key_file = xfdesktop_file_utils_query_key_file(file, NULL, NULL);
        if(key_file) {
            /* try to parse the display name */
            display_name = g_key_file_get_locale_string(key_file,
                                                        G_KEY_FILE_DESKTOP_GROUP,
                                                        G_KEY_FILE_DESKTOP_KEY_NAME,
                                                        NULL,
                                                        NULL);

            /* free the key file */
            g_key_file_free (key_file);
        }
    }

    /* use the default display name as a fallback */
    if(!display_name
       || *display_name == '\0'
       || !g_utf8_validate(display_name, -1, NULL))
    {
        display_name = g_strdup(g_file_info_get_display_name(info));
    }

    return display_name;
}

/**
 * xfdesktop_file_utils_next_new_file_name
 * @filename : the filename which will be used as the basis/default
 * @folder : the directory to search for a free filename
 *
 * Returns a filename that is like @filename with the possible addition of
 * a number to differentiate it from other similarly named files. In other words
 * it searches @folder for incrementally named files starting from @file_name
 * and returns the first available increment.
 *
 * e.g. in a folder with the following files:
 * - file
 * - empty
 * - file_copy
 *
 * Calling this functions with the above folder and @filename equal to 'file' the returned
 * filename will be 'file (copy 1)'.
 *
 * The caller is responsible to free the returned string using g_free() when no longer needed.
 *
 * Code extracted and adapted from on thunar_util_next_new_file_name.
 *
 * Return value: pointer to the new filename.
 **/
gchar*
xfdesktop_file_utils_next_new_file_name(const gchar *filename,
                                        const gchar *folder)
{
  unsigned long   file_name_size  = strlen(filename);
  unsigned        count           = 0;
  gboolean        found_duplicate = FALSE;
  gchar          *extension       = NULL;
  gchar          *new_name        = g_strdup(filename);

  extension = strrchr(filename, '.');
  if (!extension || extension == filename)
    extension = "";
  else
    file_name_size -= strlen(extension);

  /* loop until new_name is unique */
  while(TRUE)
    {
      GFile *file = g_file_new_build_filename(folder, new_name, NULL);
      found_duplicate = g_file_query_exists(file, NULL);
      g_object_unref(file);

      if (!found_duplicate)
        break;
      g_free(new_name);
      new_name = g_strdup_printf(_("%.*s (copy %u)%s"), (int) file_name_size, filename, ++count, extension ? extension : "");
    }

  return new_name;
}

GList *
xfdesktop_file_utils_file_icon_list_to_file_list(GList *icon_list)
{
    GList *file_list = NULL, *l;
    XfdesktopFileIcon *icon;
    GFile *file;

    for(l = icon_list; l; l = l->next) {
        icon = XFDESKTOP_FILE_ICON(l->data);
        file = xfdesktop_file_icon_peek_file(icon);
        if(file)
            file_list = g_list_prepend(file_list, g_object_ref(file));
    }

    return g_list_reverse(file_list);
}

GList *
xfdesktop_file_utils_file_list_from_string(const gchar *string)
{
    GList *list = NULL;
    gchar **uris;
    gsize n;

    uris = g_uri_list_extract_uris(string);

    for (n = 0; uris != NULL && uris[n] != NULL; ++n)
      list = g_list_append(list, g_file_new_for_uri(uris[n]));

    g_strfreev (uris);

    return list;
}

gchar *
xfdesktop_file_utils_file_list_to_string(GList *list)
{
    GString *string;
    GList *lp;
    gchar *uri;

    /* allocate initial string */
    string = g_string_new(NULL);

    for (lp = list; lp != NULL; lp = lp->next) {
        uri = g_file_get_uri(lp->data);
        string = g_string_append(string, uri);
        g_free(uri);

        string = g_string_append(string, "\r\n");
      }

    return g_string_free(string, FALSE);
}

gchar **
xfdesktop_file_utils_file_list_to_uri_array(GList *file_list)
{
    GList *lp;
    gchar **uris = NULL;
    guint list_length, n;

    list_length = g_list_length(file_list);

    uris = g_new0(gchar *, list_length + 1);
    for (n = 0, lp = file_list; lp != NULL; ++n, lp = lp->next)
        uris[n] = g_file_get_uri(lp->data);
    uris[n] = NULL;

    return uris;
}

void
xfdesktop_file_utils_file_list_free(GList *file_list)
{
    g_list_free_full(file_list, g_object_unref);
}

static GdkPixbuf *xfdesktop_fallback_icon = NULL;
static gint xfdesktop_fallback_icon_size = -1;

GdkPixbuf *
xfdesktop_file_utils_get_fallback_icon(gint size)
{
    g_return_val_if_fail(size > 0, NULL);

    if(size != xfdesktop_fallback_icon_size && xfdesktop_fallback_icon) {
        g_object_unref(G_OBJECT(xfdesktop_fallback_icon));
        xfdesktop_fallback_icon = NULL;
    }

    if(!xfdesktop_fallback_icon) {
        xfdesktop_fallback_icon = gdk_pixbuf_new_from_file_at_size(DATADIR "/pixmaps/xfdesktop/xfdesktop-fallback-icon.png",
                                                                   size,
                                                                   size,
                                                                   NULL);
    }

    if(G_UNLIKELY(!xfdesktop_fallback_icon)) {
        /* this is kinda crappy, but hopefully should never happen */
        xfdesktop_fallback_icon = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(),
                                                           "image-missing",
                                                           size,
                                                           GTK_ICON_LOOKUP_USE_BUILTIN,
                                                           NULL);
        if(gdk_pixbuf_get_width(xfdesktop_fallback_icon) != size
           || gdk_pixbuf_get_height(xfdesktop_fallback_icon) != size)
        {
            GdkPixbuf *tmp = gdk_pixbuf_scale_simple(xfdesktop_fallback_icon,
                                                     size, size,
                                                     GDK_INTERP_BILINEAR);
            g_object_unref(G_OBJECT(xfdesktop_fallback_icon));
            xfdesktop_fallback_icon = tmp;
        }
    }

    xfdesktop_fallback_icon_size = size;

    return GDK_PIXBUF(g_object_ref(G_OBJECT(xfdesktop_fallback_icon)));
}

GdkPixbuf *
xfdesktop_file_utils_get_icon(GIcon *icon,
                              gint width,
                              gint height,
                              guint opacity)
{
    GtkIconTheme *itheme = gtk_icon_theme_get_default();
    GdkPixbuf *pix = NULL;
    GIcon *base_icon = NULL;
    gint size = MIN(width, height);

    g_return_val_if_fail(width > 0 && height > 0 && icon != NULL, NULL);

    /* Extract the base icon if available */
    if(G_IS_EMBLEMED_ICON(icon))
        base_icon = g_emblemed_icon_get_icon(G_EMBLEMED_ICON(icon));
    else
        base_icon = icon;

    if(!base_icon)
        return NULL;

    if(G_IS_THEMED_ICON(base_icon)) {
      GtkIconInfo *icon_info = gtk_icon_theme_lookup_by_gicon(itheme,
                                                              base_icon, size,
                                                              ITHEME_FLAGS);
      if(icon_info) {
          GdkPixbuf *pix_theme = gtk_icon_info_load_icon(icon_info, NULL);
          // these icons are owned by GtkIconTheme and shouldn't be modified
          pix = gdk_pixbuf_copy(pix_theme);
          g_object_unref(pix_theme);
          g_object_unref(icon_info);
      }
    } else if(G_IS_LOADABLE_ICON(base_icon)) {
        GInputStream *stream = g_loadable_icon_load(G_LOADABLE_ICON(base_icon),
                                                    size, NULL, NULL, NULL);
        if(stream) {
            pix = gdk_pixbuf_new_from_stream_at_scale(stream, width, height, TRUE, NULL, NULL);
            g_object_unref(stream);
        }
    } else if(G_IS_FILE_ICON(base_icon)) {
        GFile *file = g_file_icon_get_file(G_FILE_ICON(icon));
        gchar *path = g_file_get_path(file);

        pix = gdk_pixbuf_new_from_file_at_size(path, width, height, NULL);

        g_free(path);
        g_object_unref(file);
    }

    if (G_LIKELY(pix != NULL)) {
        gint pix_width = gdk_pixbuf_get_width(pix);
        gint pix_height = gdk_pixbuf_get_height(pix);

        if (pix_width > width || pix_height > height) {
            GdkPixbuf *scaled = exo_gdk_pixbuf_scale_down(pix, TRUE, width, height);
            g_object_unref(pix);
            pix = scaled;
        }
    } else {
        pix = xfdesktop_file_utils_get_fallback_icon(size);
        if (G_UNLIKELY(pix == NULL)) {
            g_warning("Unable to find fallback icon");
            return NULL;
        }
    }

    /* Add the emblems */
    if(G_IS_EMBLEMED_ICON(icon))
        xfdesktop_file_utils_add_emblems(pix, g_emblemed_icon_get_emblems(G_EMBLEMED_ICON(icon)));

    if(opacity != 100) {
        GdkPixbuf *tmp = exo_gdk_pixbuf_lucent(pix, opacity);
        g_object_unref(G_OBJECT(pix));
        pix = tmp;
    }

    return pix;
}

static void
xfdesktop_file_utils_add_emblems(GdkPixbuf *pix, GList *emblems)
{
    GdkPixbuf *emblem_pix = NULL;
    gint max_emblems;
    gint pix_width, pix_height;
    gint emblem_size;
    gint dest_x, dest_y, dest_width, dest_height;
    gint position;
    GList *iter;
    GtkIconTheme *itheme = gtk_icon_theme_get_default();

    g_return_if_fail(pix != NULL);

    pix_width = gdk_pixbuf_get_width(pix);
    pix_height = gdk_pixbuf_get_height(pix);

    emblem_size = MIN(pix_width, pix_height) / 2;

    /* render up to four emblems for sizes from 48 onwards, else up to 2 emblems */
    max_emblems = (pix_height < 48 && pix_width < 48) ? 2 : 4;

    for(iter = g_list_last(emblems), position = 0;
        iter != NULL && position < max_emblems; iter = iter->prev) {
        /* extract the icon from the emblem and load it */
        GIcon *emblem = g_emblem_get_icon(iter->data);
        GtkIconInfo *icon_info = gtk_icon_theme_lookup_by_gicon(itheme,
                                                                emblem,
                                                                emblem_size,
                                                                ITHEME_FLAGS);
        if(icon_info) {
            emblem_pix = gtk_icon_info_load_icon(icon_info, NULL);
            g_object_unref(icon_info);
        }

        if(emblem_pix) {
            if(gdk_pixbuf_get_width(emblem_pix) != emblem_size
               || gdk_pixbuf_get_height(emblem_pix) != emblem_size)
            {
                GdkPixbuf *tmp = gdk_pixbuf_scale_simple(emblem_pix,
                                                         emblem_size,
                                                         emblem_size,
                                                         GDK_INTERP_BILINEAR);
                g_object_unref(emblem_pix);
                emblem_pix = tmp;
            }

            dest_width = pix_width - emblem_size;
            dest_height = pix_height - emblem_size;

            switch(position) {
                case 0: /* bottom right */
                    dest_x = dest_width;
                    dest_y = dest_height;
                    break;
                case 1: /* bottom left */
                    dest_x = 0;
                    dest_y = dest_height;
                    break;
                case 2: /* upper left */
                    dest_x = dest_y = 0;
                    break;
                case 3: /* upper right */
                    dest_x = dest_width;
                    dest_y = 0;
                    break;
                default:
                    g_warning("Invalid emblem position in xfdesktop_file_utils_add_emblems");
            }

            DBG("calling gdk_pixbuf_composite(%p, %p, %d, %d, %d, %d, %d, %d, %.1f, %.1f, %d, %d) pixbuf w: %d h: %d",
                emblem_pix, pix,
                dest_x, dest_y,
                emblem_size, emblem_size,
                dest_x, dest_y,
                1.0, 1.0, GDK_INTERP_BILINEAR, 255, pix_width, pix_height);

            /* Add the emblem */
            gdk_pixbuf_composite(emblem_pix, pix,
                                 dest_x, dest_y,
                                 emblem_size, emblem_size,
                                 dest_x, dest_y,
                                 1.0, 1.0, GDK_INTERP_BILINEAR, 255);

            g_object_unref(emblem_pix);
            emblem_pix = NULL;

            position++;
        }
    }
}

void
xfdesktop_file_utils_set_window_cursor(GtkWindow *window,
                                       GdkCursorType cursor_type)
{
    GdkCursor *cursor;

    if(!window || !gtk_widget_get_window(GTK_WIDGET(window)))
        return;

    cursor = gdk_cursor_new_for_display(gtk_widget_get_display(GTK_WIDGET(window)), cursor_type);
    if(G_LIKELY(cursor)) {
        gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(window)), cursor);
        g_object_unref(cursor);
    }
}

static gchar *
xfdesktop_file_utils_change_working_directory (const gchar *new_directory)
{
  gchar *old_directory;

  g_return_val_if_fail(new_directory && *new_directory != '\0', NULL);

  /* allocate a path buffer for the old working directory */
  old_directory = g_malloc0(sizeof(gchar) * MAXPATHLEN);

  /* try to determine the current working directory */
#ifdef G_PLATFORM_WIN32
  if(!_getcwd(old_directory, MAXPATHLEN))
#else
  if(!getcwd (old_directory, MAXPATHLEN))
#endif
  {
      /* working directory couldn't be determined, reset the buffer */
      g_free(old_directory);
      old_directory = NULL;
  }

  /* try switching to the new working directory */
#ifdef G_PLATFORM_WIN32
  if(_chdir (new_directory))
#else
  if(chdir (new_directory))
#endif
  {
      /* switching failed, we don't need to return the old directory */
      g_free(old_directory);
      old_directory = NULL;
  }

  return old_directory;
}

gboolean
xfdesktop_file_utils_app_info_launch(GAppInfo *app_info,
                                     GFile *working_directory,
                                     GList *files,
                                     GAppLaunchContext *context,
                                     GError **error)
{
    gboolean result = FALSE;
    gchar *new_path = NULL;
    gchar *old_path = NULL;

    g_return_val_if_fail(G_IS_APP_INFO(app_info), FALSE);
    g_return_val_if_fail(working_directory == NULL || G_IS_FILE(working_directory), FALSE);
    g_return_val_if_fail(files != NULL && files->data != NULL, FALSE);
    g_return_val_if_fail(G_IS_APP_LAUNCH_CONTEXT(context), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    /* check if we want to set the working directory of the spawned app */
    if(working_directory) {
        /* determine the working directory path */
        new_path = g_file_get_path(working_directory);
        if(new_path) {
            /* switch to the desired working directory, remember that
             * of xfdesktop itself */
            old_path = xfdesktop_file_utils_change_working_directory(new_path);

            /* forget about the new working directory path */
            g_free(new_path);
        }
    }

    /* launch the paths with the specified app info */
    result = g_app_info_launch(app_info, files, context, error);

    /* check if we need to reset the working directory to the one xfdesktop was
     * opened from */
    if(old_path) {
        /* switch to xfdesktop's original working directory */
        new_path = xfdesktop_file_utils_change_working_directory(old_path);

        /* clean up */
        g_free (new_path);
        g_free (old_path);
    }

    return result;
}

void
xfdesktop_file_utils_open_folder(GFile *file,
                                 GdkScreen *screen,
                                 GtkWindow *parent)
{
    gchar *uri = NULL;
    GError *error = NULL;

    g_return_if_fail(G_IS_FILE(file));
    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));

    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));

    uri = g_file_get_uri(file);

    if(!exo_execute_preferred_application_on_screen("FileManager",
                                                    uri,
                                                    NULL,
                                                    NULL,
                                                    screen,
                                                    &error))
    {
        xfce_message_dialog(parent,
                            _("Launch Error"), "dialog-error",
                            _("The folder could not be opened"),
                            error->message,
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);

        g_clear_error(&error);
    }

    g_free(uri);
}

static void
xfdesktop_file_utils_async_handle_error(GError *error, gpointer userdata)
{
    GtkWindow *parent = GTK_WINDOW(userdata);

    if(error != NULL) {
        if(error->domain != G_IO_ERROR || error->code != G_IO_ERROR_TIMED_OUT) {
            xfce_message_dialog(parent,
                                _("Error"), "dialog-error",
                                _("The requested operation could not be completed"),
                                error->message,
                                XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                                NULL);
        }

        g_clear_error(&error);
    }
}

static void
rename_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    if (!xfdesktop_file_manager_call_rename_file_finish(XFDESKTOP_FILE_MANAGER(source_object), res, &error))
        xfdesktop_file_utils_async_handle_error(error, user_data);
}

void
xfdesktop_file_utils_rename_file(GFile *file,
                                 GdkScreen *screen,
                                 GtkWindow *parent)
{
    XfdesktopFileManager *fileman_proxy;

    g_return_if_fail(G_IS_FILE(file));
    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));

    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));

    fileman_proxy = xfdesktop_file_utils_peek_filemanager_proxy();
    if(fileman_proxy) {
        gchar *uri = g_file_get_uri(file);
        gchar *display_name = g_strdup(gdk_display_get_name(gdk_screen_get_display(screen)));
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());

        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);


        xfdesktop_file_manager_call_rename_file(fileman_proxy,
                                                uri, display_name, startup_id,
                                                NULL,
                                                rename_cb,
                                                parent);

        xfdesktop_file_utils_set_window_cursor(parent, GDK_LEFT_PTR);

        g_free(startup_id);
        g_free(uri);
        g_free(display_name);
    } else {
        xfce_message_dialog(parent,
                            _("Rename Error"), "dialog-error",
                            _("The file could not be renamed"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
    }
}

static void
bulk_rename_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    if (!xfdesktop_thunar_call_bulk_rename_finish(XFDESKTOP_THUNAR(source_object), res, &error))
        xfdesktop_file_utils_async_handle_error(error, user_data);
}

void
xfdesktop_file_utils_bulk_rename(GFile *working_directory,
                                 GList *files,
                                 GdkScreen *screen,
                                 GtkWindow *parent)
{
    XfdesktopThunar *thunar_proxy;

    g_return_if_fail(G_IS_FILE(working_directory));
    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));

    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));

    thunar_proxy = xfdesktop_file_utils_peek_thunar_proxy();
    if(thunar_proxy) {
        gchar *directory = g_file_get_path(working_directory);
        guint nfiles = g_list_length(files);
        gchar **filenames = g_new0(gchar *, nfiles+1);
        gchar *display_name = g_strdup(gdk_display_get_name(gdk_screen_get_display(screen)));
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());
        GList *lp;
        gint n;

        /* convert GFile list into an array of filenames */
        for(n = 0, lp = files; lp != NULL; ++n, lp = lp->next)
            filenames[n] = g_file_get_basename(lp->data);
        filenames[n] = NULL;

        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);


        xfdesktop_thunar_call_bulk_rename(thunar_proxy,
                                          directory, (const gchar **)filenames,
                                          FALSE, display_name, startup_id,
                                          NULL,
                                          bulk_rename_cb,
                                          parent);

        xfdesktop_file_utils_set_window_cursor(parent, GDK_LEFT_PTR);

        g_free(directory);
        g_free(startup_id);
        g_strfreev(filenames);
        g_free(display_name);
    } else {
        xfce_message_dialog(parent,
                            _("Rename Error"), "dialog-error",
                            _("The files could not be renamed"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
    }
}

static void
unlink_files_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    if (!xfdesktop_file_manager_call_unlink_files_finish(XFDESKTOP_FILE_MANAGER(source_object), res, &error))
        xfdesktop_file_utils_async_handle_error(error, user_data);
}

void
xfdesktop_file_utils_unlink_files(GList *files,
                                  GdkScreen *screen,
                                  GtkWindow *parent)
{
    XfdesktopFileManager *fileman_proxy;

    g_return_if_fail(files != NULL && G_IS_FILE(files->data));
    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));

    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));

    fileman_proxy = xfdesktop_file_utils_peek_filemanager_proxy();
    if(fileman_proxy) {
        guint nfiles = g_list_length(files);
        gchar **uris = g_new0(gchar *, nfiles+1);
        gchar *display_name = g_strdup(gdk_display_get_name(gdk_screen_get_display(screen)));
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());
        GList *lp;
        gint n;

        /* convert GFile list into an array of URIs */
        for(n = 0, lp = files; lp != NULL; ++n, lp = lp->next)
            uris[n] = g_file_get_uri(lp->data);
        uris[n] = NULL;

        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);


        xfdesktop_file_manager_call_unlink_files(fileman_proxy,
                                                 "", (const gchar **)uris,
                                                 display_name, startup_id,
                                                 NULL,
                                                 unlink_files_cb,
                                                 parent);

        xfdesktop_file_utils_set_window_cursor(parent, GDK_LEFT_PTR);

        g_free(startup_id);
        g_strfreev(uris);
        g_free(display_name);
    } else {
        xfce_message_dialog(parent,
                            _("Delete Error"), "dialog-error",
                            _("The selected files could not be deleted"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
    }
}

static void
trash_files_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    if (!xfdesktop_trash_call_move_to_trash_finish(XFDESKTOP_TRASH(source_object), res, &error))
        xfdesktop_file_utils_async_handle_error(error, user_data);
}

void
xfdesktop_file_utils_trash_files(GList *files,
                                 GdkScreen *screen,
                                 GtkWindow *parent)
{
    XfdesktopTrash *trash_proxy;

    g_return_if_fail(files != NULL && G_IS_FILE(files->data));
    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));

    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));

    trash_proxy = xfdesktop_file_utils_peek_trash_proxy();
    if(trash_proxy) {
        guint nfiles = g_list_length(files);
        gchar **uris = g_new0(gchar *, nfiles+1);
        gchar *display_name = g_strdup(gdk_display_get_name(gdk_screen_get_display(screen)));
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());
        GList *lp;
        gint n;

        /* convert GFile list into an array of URIs */
        for(n = 0, lp = files; lp != NULL; ++n, lp = lp->next)
            uris[n] = g_file_get_uri(lp->data);
        uris[n] = NULL;

        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);


        xfdesktop_trash_call_move_to_trash(trash_proxy,
                                           (const gchar **)uris,
                                           display_name, startup_id,
                                           NULL,
                                           trash_files_cb,
                                           parent);

        xfdesktop_file_utils_set_window_cursor(parent, GDK_LEFT_PTR);

        g_free(startup_id);
        g_strfreev(uris);
        g_free(display_name);
    } else {
        xfce_message_dialog(parent,
                            _("Trash Error"), "dialog-error",
                            _("The selected files could not be moved to the trash"),
                            _("This feature requires a trash service to "
                              "be present (such as the one supplied by Thunar)."),
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
    }
}

static void
empty_trash_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    if (!xfdesktop_trash_call_empty_trash_finish(XFDESKTOP_TRASH(source_object), res, &error))
        xfdesktop_file_utils_async_handle_error(error, user_data);
}

void
xfdesktop_file_utils_empty_trash(GdkScreen *screen,
                                 GtkWindow *parent)
{
    XfdesktopTrash *trash_proxy;

    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));

    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));

    trash_proxy = xfdesktop_file_utils_peek_trash_proxy();
    if(trash_proxy) {
        gchar *display_name = g_strdup(gdk_display_get_name(gdk_screen_get_display(screen)));
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());

        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);


        xfdesktop_trash_call_empty_trash(trash_proxy,
                                         display_name, startup_id,
                                         NULL,
                                         empty_trash_cb,
                                         parent);

        xfdesktop_file_utils_set_window_cursor(parent, GDK_LEFT_PTR);

        g_free(startup_id);
        g_free(display_name);
    } else {
        xfce_message_dialog(parent,
                            _("Trash Error"), "dialog-error",
                            _("Could not empty the trash"),
                            _("This feature requires a trash service to "
                              "be present (such as the one supplied by Thunar)."),
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
    }
}

static void
create_file_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    if (!xfdesktop_file_manager_call_create_file_finish(XFDESKTOP_FILE_MANAGER(source_object), res, &error))
        xfdesktop_file_utils_async_handle_error(error, user_data);
}

void
xfdesktop_file_utils_create_file(GFile *parent_folder,
                                 const gchar *content_type,
                                 GdkScreen *screen,
                                 GtkWindow *parent)
{
    XfdesktopFileManager *fileman_proxy;

    g_return_if_fail(G_IS_FILE(parent_folder));
    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));

    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));

    fileman_proxy = xfdesktop_file_utils_peek_filemanager_proxy();
    if(fileman_proxy) {
        gchar *parent_directory = g_file_get_uri(parent_folder);
        gchar *display_name = g_strdup(gdk_display_get_name(gdk_screen_get_display(screen)));
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());

        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);


        xfdesktop_file_manager_call_create_file(fileman_proxy,
                                                parent_directory,
                                                content_type, display_name,
                                                startup_id,
                                                NULL,
                                                create_file_cb,
                                                parent);

        xfdesktop_file_utils_set_window_cursor(parent, GDK_LEFT_PTR);

        g_free(startup_id);
        g_free(parent_directory);
        g_free(display_name);
    } else {
        xfce_message_dialog(parent,
                            _("Create File Error"), "dialog-error",
                            _("Could not create a new file"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
    }
}

static void
create_file_from_template_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    if (!xfdesktop_file_manager_call_create_file_from_template_finish(XFDESKTOP_FILE_MANAGER(source_object), res, &error))
        xfdesktop_file_utils_async_handle_error(error, user_data);
}

void
xfdesktop_file_utils_create_file_from_template(GFile *parent_folder,
                                               GFile *template_file,
                                               GdkScreen *screen,
                                               GtkWindow *parent)
{
    XfdesktopFileManager *fileman_proxy;

    g_return_if_fail(G_IS_FILE(parent_folder));
    g_return_if_fail(G_IS_FILE(template_file));
    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));

    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));

    fileman_proxy = xfdesktop_file_utils_peek_filemanager_proxy();
    if(fileman_proxy) {
        gchar *parent_directory = g_file_get_uri(parent_folder);
        gchar *template_uri = g_file_get_uri(template_file);
        gchar *display_name = g_strdup(gdk_display_get_name(gdk_screen_get_display(screen)));
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());

        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);


        xfdesktop_file_manager_call_create_file_from_template(fileman_proxy,
                                                              parent_directory,
                                                              template_uri,
                                                              display_name,
                                                              startup_id,
                                                              NULL,
                                                              create_file_from_template_cb,
                                                              parent);

        xfdesktop_file_utils_set_window_cursor(parent, GDK_LEFT_PTR);

        g_free(startup_id);
        g_free(display_name);
        g_free(parent_directory);
    } else {
        xfce_message_dialog(parent,
                            _("Create Document Error"), "dialog-error",
                            _("Could not create a new document from the template"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
    }
}

static void
show_properties_fdo_cb(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    if (!xfdesktop_file_manager1_call_show_item_properties_finish(XFDESKTOP_FILE_MANAGER1(source_object), res, &error)) {
        xfdesktop_file_utils_async_handle_error(error, user_data);
    }
}


static void
show_properties_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    if (!xfdesktop_file_manager_call_display_file_properties_finish(XFDESKTOP_FILE_MANAGER(source_object), res, &error))
        xfdesktop_file_utils_async_handle_error(error, user_data);
}

void
xfdesktop_file_utils_show_properties_dialog(GList *files,
                                            GdkScreen *screen,
                                            GtkWindow *parent)
{
    XfdesktopFileManager *fileman_proxy;
    XfdesktopFileManager1 *fileman_fdo_proxy;

    g_return_if_fail(files != NULL);
    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));

    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));

    fileman_proxy = xfdesktop_file_utils_peek_filemanager_proxy();
    fileman_fdo_proxy = xfdesktop_file_utils_peek_filemanager_fdo_proxy();

    if ((files->next != NULL || fileman_proxy == NULL) && fileman_fdo_proxy != NULL) {  // multiple files or no xfce fileman proxy
        gchar **uris = g_new0(gchar *, g_list_length(files) + 1);
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());
        gint i = 0;

        for (GList *l = files; l != NULL; l = l->next, ++i) {
            uris[i] = g_file_get_uri(G_FILE(l->data));
        }

        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);

        xfdesktop_file_manager1_call_show_item_properties(fileman_fdo_proxy,
                                                          (const gchar *const *)uris,
                                                          startup_id,
                                                          NULL,
                                                          show_properties_fdo_cb,
                                                          parent);

        xfdesktop_file_utils_set_window_cursor(parent, GDK_LEFT_PTR);

        g_strfreev(uris);
        g_free(startup_id);
    } else if (files->next == NULL && fileman_proxy) {
        GFile *file = files->data;
        gchar *uri = g_file_get_uri(file);
        gchar *display_name = g_strdup(gdk_display_get_name(gdk_screen_get_display(screen)));
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());

        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);

        xfdesktop_file_manager_call_display_file_properties(fileman_proxy,
                                                            uri, display_name, startup_id,
                                                            NULL,
                                                            show_properties_cb,
                                                            parent);

        xfdesktop_file_utils_set_window_cursor(parent, GDK_LEFT_PTR);

        g_free(startup_id);
        g_free(uri);
        g_free(display_name);
    } else {
        xfce_message_dialog(parent,
                            _("File Properties Error"), "dialog-error",
                            _("The file properties dialog could not be opened"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
    }
}

static void
launch_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    if (!xfdesktop_file_manager_call_launch_files_finish(XFDESKTOP_FILE_MANAGER(source_object), res, &error))
        xfdesktop_file_utils_async_handle_error(error, user_data);
}

void
xfdesktop_file_utils_launch(GFile *file,
                            GdkScreen *screen,
                            GtkWindow *parent)
{
    XfdesktopFileManager *fileman_proxy;

    g_return_if_fail(G_IS_FILE(file));
    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));

    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));

    fileman_proxy = xfdesktop_file_utils_peek_filemanager_proxy();
    if(fileman_proxy) {
        gchar **uris;
        GFile  *parent_file = g_file_get_parent(file);
        gchar  *parent_path = g_file_get_path(parent_file);
        gchar *display_name = g_strdup(gdk_display_get_name(gdk_screen_get_display(screen)));
        gchar  *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());

        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);

        uris = g_new0(gchar *, 2);
        uris[0] = g_file_get_uri(file);
        uris[1] = NULL;

        xfdesktop_file_manager_call_launch_files(fileman_proxy, parent_path,
                                                 (const gchar * const*)uris,
                                                 display_name, startup_id,
                                                 NULL,
                                                 launch_cb,
                                                 parent);

        xfdesktop_file_utils_set_window_cursor(parent, GDK_LEFT_PTR);

        g_free(startup_id);
        g_free(uris[0]);
        g_free(uris);
        g_free(parent_path);
        g_object_unref(parent_file);
        g_free(display_name);
    } else {
        xfce_message_dialog(parent,
                            _("Launch Error"), "dialog-error",
                            _("The file could not be opened"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
    }
}

static void
execute_finished_cb(GObject *source,
                    GAsyncResult *res,
                    gpointer user_data)
{
    ExecuteData *edata = (ExecuteData *)user_data;
    gboolean ret;
    GError *error = NULL;

    ret = xfdesktop_file_manager_call_execute_finish(XFDESKTOP_FILE_MANAGER(source), res, &error);
    if (!ret) {
        gchar *filename = g_file_get_uri(edata->file);
        gchar *name = g_filename_display_basename(filename);
        gchar *primary = g_markup_printf_escaped(_("Failed to run \"%s\""), name);

        xfce_message_dialog(edata->parent,
                            _("Launch Error"), "dialog-error",
                            primary, error->message,
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);

        g_free(primary);
        g_free(name);
        g_free(filename);
        g_error_free(error);
    }

    if (edata->parent != NULL) {
        g_object_unref(edata->parent);
    }
    g_object_unref(edata->file);
    g_free(edata);
}

gboolean
xfdesktop_file_utils_execute(GFile *working_directory,
                             GFile *file,
                             GList *files,
                             GdkScreen *screen,
                             GtkWindow *parent)
{
    XfdesktopFileManager *fileman_proxy;
    gboolean success = TRUE;

    g_return_val_if_fail(working_directory == NULL || G_IS_FILE(working_directory), FALSE);
    g_return_val_if_fail(G_IS_FILE(file), FALSE);
    g_return_val_if_fail(screen == NULL || GDK_IS_SCREEN(screen), FALSE);
    g_return_val_if_fail(parent == NULL || GTK_IS_WINDOW(parent), FALSE);

    if(!screen)
        screen = gdk_display_get_default_screen(gdk_display_get_default());

    fileman_proxy = xfdesktop_file_utils_peek_filemanager_proxy();
    if(fileman_proxy) {
        ExecuteData *edata;
        gchar *working_dir = working_directory != NULL ? g_file_get_uri(working_directory) : NULL;
        const gchar *path_prop;
        gchar *uri = g_file_get_uri(file);
        gchar *display_name = g_strdup(gdk_display_get_name(gdk_screen_get_display(screen)));
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());
        GList *lp;
        guint n = g_list_length (files);
        gchar **uris = g_new0 (gchar *, n + 1);

        for (n = 0, lp = files; lp != NULL; ++n, lp = lp->next)
            uris[n] = g_file_get_uri(lp->data);
        uris[n] = NULL;

        /* If the working_dir wasn't set check if this is a .desktop file
         * we can parse a working dir from */
        if(working_dir == NULL) {
            GFileInfo *info = g_file_query_info(file,
                                                XFDESKTOP_FILE_INFO_NAMESPACE,
                                                G_FILE_QUERY_INFO_NONE,
                                                NULL, NULL);

            if(xfdesktop_file_utils_is_desktop_file(info)) {
                XfceRc *rc;
                gchar *path = g_file_get_path(file);
                if(path != NULL) {
                    rc = xfce_rc_simple_open(path, TRUE);
                    if(rc != NULL) {
                        path_prop = xfce_rc_read_entry(rc, "Path", NULL);
                        if(xfce_str_is_empty(path_prop))
                            working_dir = g_strdup(g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP));
                        else
                            working_dir = g_strdup(path_prop);
                        xfce_rc_close(rc);
                    }
                    g_free(path);
                }
            }

            if(info)
                g_object_unref(info);
        }

        edata = g_new0(ExecuteData, 1);
        if (parent != NULL) {
            edata->parent = g_object_ref(parent);
        }
        edata->file = g_object_ref(file);
        xfdesktop_file_manager_call_execute(fileman_proxy,
                                            working_dir,
                                            uri,
                                            (const gchar **)uris,
                                            display_name,
                                            startup_id,
                                            NULL,
                                            execute_finished_cb,
                                            edata);

        g_free(startup_id);
        g_strfreev(uris);
        g_free(uri);
        g_free(working_dir);
        g_free(display_name);
    } else {
        gchar *filename = g_file_get_uri(file);
        gchar *name = g_filename_display_basename(filename);
        gchar *primary = g_markup_printf_escaped(_("Failed to run \"%s\""), name);

        xfce_message_dialog(parent,
                            _("Launch Error"), "dialog-error",
                            primary,
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);

        g_free(primary);
        g_free(name);
        g_free(filename);

        success = FALSE;
    }

    return success;
}

static void
display_chooser_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    if (!xfdesktop_file_manager_call_display_application_chooser_dialog_finish(XFDESKTOP_FILE_MANAGER(source_object), res, &error))
        xfdesktop_file_utils_async_handle_error(error, user_data);
}

void
xfdesktop_file_utils_display_app_chooser_dialog(GFile *file,
                                                gboolean open,
                                                gboolean preselect_default_checkbox,
                                                GdkScreen *screen,
                                                GtkWindow *parent)
{
    XfdesktopFileManager *fileman_proxy;

    g_return_if_fail(G_IS_FILE(file));
    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));

    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));

    fileman_proxy = xfdesktop_file_utils_peek_filemanager_proxy();
    if(fileman_proxy) {
        gchar *uri = g_file_get_uri(file);
        gchar *display_name = g_strdup(gdk_display_get_name(gdk_screen_get_display(screen)));
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());

        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);

        xfdesktop_file_manager_call_display_application_chooser_dialog(fileman_proxy,
                                                                       uri, open,
                                                                       preselect_default_checkbox,
                                                                       display_name,
                                                                       startup_id,
                                                                       NULL,
                                                                       display_chooser_cb,
                                                                       parent);

        xfdesktop_file_utils_set_window_cursor(parent, GDK_LEFT_PTR);

        g_free(startup_id);
        g_free(uri);
        g_free(display_name);
    } else {
        xfce_message_dialog(parent,
                            _("Launch Error"), "dialog-error",
                            _("The application chooser could not be opened"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
    }
}

/**
 * 'out_source_files' will hold a list owned by caller with contents owned by callee
 * 'dest_source_files' will hold a list and contents owned by caller
 */
void
xfdesktop_file_utils_build_transfer_file_lists(GdkDragAction action,
                                               GList *source_icons,
                                               XfdesktopFileIcon *dest_icon,
                                               GList **out_source_files,
                                               GList **out_dest_files)
{
    g_return_if_fail(source_icons != NULL);
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON(dest_icon));
    g_return_if_fail(out_source_files != NULL && out_dest_files != NULL);

    switch (action) {
        case GDK_ACTION_MOVE:
        case GDK_ACTION_LINK:
            *out_dest_files = g_list_append(NULL, g_object_ref(xfdesktop_file_icon_peek_file(dest_icon)));
            for (GList *l = source_icons; l != NULL; l = l->next) {
                GFile *source_file = xfdesktop_file_icon_peek_file(XFDESKTOP_FILE_ICON(l->data));
                if (source_file != NULL) {
                    *out_source_files = g_list_prepend(*out_source_files, source_file);
                }
            }
            *out_source_files = g_list_reverse(*out_source_files);
            break;

        case GDK_ACTION_COPY:
            for (GList *l = source_icons; l != NULL; l = l->next) {
                GFile *source_file = xfdesktop_file_icon_peek_file(XFDESKTOP_FILE_ICON(l->data));
                if (source_file != NULL) {
                    gchar *name = g_file_get_basename(source_file);
                    if (name != NULL) {
                        GFile *dest_file = g_file_get_child(xfdesktop_file_icon_peek_file(dest_icon), name);
                        *out_dest_files = g_list_prepend(*out_dest_files, dest_file);
                        *out_source_files = g_list_prepend(*out_source_files, source_file);
                        g_free(name);
                    }
                }
            }
            *out_source_files = g_list_reverse(*out_source_files);
            *out_dest_files = g_list_reverse(*out_dest_files);
            break;

        default:
            g_warning("Unsupported drag action: %d", action);
    }
}

static void
transfer_files_cb(GObject *source_object,
                  GAsyncResult *result,
                  gpointer user_data)
{
    gboolean (*finish_func)(XfdesktopFileManager *, GAsyncResult *, GError **) = user_data;
    GError *error = NULL;
    gboolean success;

    success = finish_func(XFDESKTOP_FILE_MANAGER(source_object), result, &error);
    if (!success) {
        gchar *message = error != NULL ? error->message : _("Unknown");
        xfce_message_dialog(NULL,
                            _("Transfer Error"), "dialog-error",
                            _("The file transfer could not be performed"),
                            message,
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);

        g_clear_error(&error);
    }
}

void
xfdesktop_file_utils_transfer_files(GdkDragAction action,
                                    GList *source_files,
                                    GList *target_files,
                                    GdkScreen *screen)
{
    XfdesktopFileManager *fileman_proxy;

    g_return_if_fail(action == GDK_ACTION_MOVE || action == GDK_ACTION_COPY || action == GDK_ACTION_LINK);
    g_return_if_fail(source_files != NULL && G_IS_FILE(source_files->data));
    g_return_if_fail(target_files != NULL && G_IS_FILE(target_files->data));
    g_return_if_fail(screen == NULL || GDK_IS_SCREEN(screen));

    if(!screen)
        screen = gdk_display_get_default_screen(gdk_display_get_default());

    fileman_proxy = xfdesktop_file_utils_peek_filemanager_proxy();
    if(fileman_proxy) {
        gchar **source_uris = xfdesktop_file_utils_file_list_to_uri_array(source_files);
        gchar **target_uris = xfdesktop_file_utils_file_list_to_uri_array(target_files);
        gchar *display_name = g_strdup(gdk_display_get_name(gdk_screen_get_display(screen)));
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());

        switch(action) {
            case GDK_ACTION_MOVE:
                xfdesktop_file_manager_call_move_into(fileman_proxy, "",
                                                      (const gchar **)source_uris,
                                                      (const gchar *)target_uris[0],
                                                      display_name, startup_id,
                                                      NULL,
                                                      transfer_files_cb, xfdesktop_file_manager_call_move_into_finish);
                break;
            case GDK_ACTION_COPY:
                xfdesktop_file_manager_call_copy_to(fileman_proxy, "",
                                                    (const gchar **)source_uris,
                                                    (const gchar **)target_uris,
                                                    display_name, startup_id,
                                                    NULL,
                                                    transfer_files_cb, xfdesktop_file_manager_call_copy_to_finish);
                break;
            case GDK_ACTION_LINK:
                xfdesktop_file_manager_call_link_into(fileman_proxy, "",
                                                      (const gchar **)source_uris,
                                                      (const gchar *)target_uris[0],
                                                      display_name, startup_id,
                                                      NULL,
                                                      transfer_files_cb, xfdesktop_file_manager_call_link_into_finish);
                break;
            default:
                g_assert_not_reached();
                break;
        }

        g_free(startup_id);
        g_free(display_name);
        g_strfreev(target_uris);
        g_strfreev(source_uris);
    } else {
        xfce_message_dialog(NULL,
                            _("Transfer Error"), "dialog-error",
                            _("The file transfer could not be performed"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
    }
}

static gint dbus_ref_cnt = 0;
static GDBusConnection *dbus_gconn = NULL;
static XfdesktopTrash *dbus_trash_proxy = NULL;
static XfdesktopFileManager *dbus_filemanager_proxy = NULL;
static XfdesktopFileManager1 *dbus_filemanager_fdo_proxy = NULL;
#ifdef HAVE_THUNARX
static XfdesktopThunar *dbus_thunar_proxy = NULL;
#else
static GDBusProxy *dbus_thunar_proxy = NULL;
#endif
gboolean
xfdesktop_file_utils_dbus_init(void)
{
    gboolean ret = TRUE;

    if(dbus_ref_cnt++)
        return TRUE;

    if(!dbus_gconn) {
        dbus_gconn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
    }

    if(dbus_gconn) {
        xfdesktop_trash_proxy_new(dbus_gconn,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  "org.xfce.FileManager",
                                  "/org/xfce/FileManager",
                                  NULL,
                                  xfdesktop_file_utils_trash_proxy_new_cb,
                                  NULL);

        xfdesktop_file_manager_proxy_new(dbus_gconn,
                                         G_DBUS_PROXY_FLAGS_NONE,
                                         "org.xfce.FileManager",
                                         "/org/xfce/FileManager",
                                         NULL,
                                         xfdesktop_file_utils_file_manager_proxy_new_cb,
                                         NULL);

        xfdesktop_file_manager1_proxy_new(dbus_gconn,
                                         G_DBUS_PROXY_FLAGS_NONE,
                                         "org.freedesktop.FileManager1",
                                         "/org/freedesktop/FileManager1",
                                         NULL,
                                         xfdesktop_file_utils_file_manager_fdo_proxy_new_cb,
                                         NULL);

#ifdef HAVE_THUNARX
        xfdesktop_thunar_proxy_new(dbus_gconn,
                                   G_DBUS_PROXY_FLAGS_NONE,
                                   "org.xfce.FileManager",
                                   "/org/xfce/FileManager",
                                   NULL,
                                   xfdesktop_file_utils_thunar_proxy_new_cb,
                                   NULL);
#else
        dbus_thunar_proxy = NULL;
#endif

    } else {
        ret = FALSE;
        dbus_ref_cnt = 0;
    }

    return ret;
}

static XfdesktopTrash *
xfdesktop_file_utils_peek_trash_proxy(void)
{
    return dbus_trash_proxy;
}

static XfdesktopFileManager *
xfdesktop_file_utils_peek_filemanager_proxy(void)
{
    return dbus_filemanager_proxy;
}

static XfdesktopFileManager1 *
xfdesktop_file_utils_peek_filemanager_fdo_proxy(void)
{
    return dbus_filemanager_fdo_proxy;
}

#ifdef HAVE_THUNARX
static XfdesktopThunar *
xfdesktop_file_utils_peek_thunar_proxy(void)
{
    return dbus_thunar_proxy;
}
#else
static gpointer
xfdesktop_file_utils_peek_thunar_proxy(void)
{
    return NULL;
}
#endif

static void
xfdesktop_file_utils_trash_proxy_new_cb (GObject *source_object,
                                         GAsyncResult *res,
                                         gpointer user_data) {
    dbus_trash_proxy = xfdesktop_trash_proxy_new_finish (res, NULL);
}

static void
xfdesktop_file_utils_file_manager_proxy_new_cb (GObject *source_object,
                                                GAsyncResult *res,
                                                gpointer user_data) {
    dbus_filemanager_proxy = xfdesktop_file_manager_proxy_new_finish (res, NULL);
}

static void
xfdesktop_file_utils_file_manager_fdo_proxy_new_cb(GObject *source_object,
                                                   GAsyncResult *res,
                                                   gpointer user_data)
{
    dbus_filemanager_fdo_proxy = xfdesktop_file_manager1_proxy_new_finish(res, NULL);
}

static void
xfdesktop_file_utils_thunar_proxy_new_cb (GObject *source_object,
                                          GAsyncResult *res,
                                          gpointer user_data) {
#ifdef HAVE_THUNARX
    dbus_thunar_proxy = xfdesktop_thunar_proxy_new_finish (res, NULL);
#endif
}

void
xfdesktop_file_utils_dbus_cleanup(void)
{
    if(dbus_ref_cnt == 0 || --dbus_ref_cnt > 0)
        return;

    if(dbus_trash_proxy)
        g_object_unref(G_OBJECT(dbus_trash_proxy));
    if(dbus_filemanager_proxy)
        g_object_unref(G_OBJECT(dbus_filemanager_proxy));
    if (dbus_filemanager_fdo_proxy) {
        g_object_unref(G_OBJECT(dbus_filemanager_fdo_proxy));
    }
    if(dbus_thunar_proxy)
        g_object_unref(G_OBJECT(dbus_thunar_proxy));
    if(dbus_gconn)
        g_object_unref(G_OBJECT(dbus_gconn));
}



#ifdef HAVE_THUNARX

/* thunar extension interface stuff: ThunarxFileInfo implementation */

gchar *
xfdesktop_thunarx_file_info_get_name(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    GFile *file = xfdesktop_file_icon_peek_file(icon);

    return file ? g_file_get_basename(file) : NULL;
}

gchar *
xfdesktop_thunarx_file_info_get_uri(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    GFile *file = xfdesktop_file_icon_peek_file(icon);

    return file ? g_file_get_uri(file) : NULL;
}

gchar *
xfdesktop_thunarx_file_info_get_parent_uri(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    GFile *file = xfdesktop_file_icon_peek_file(icon);
    gchar *uri = NULL;

    if(file) {
        GFile *parent = g_file_get_parent(file);
        if(parent) {
            uri = g_file_get_uri(parent);
            g_object_unref(parent);
        }
    }

    return uri;
}

gchar *
xfdesktop_thunarx_file_info_get_uri_scheme_file(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    GFile *file = xfdesktop_file_icon_peek_file(icon);

    return file ? g_file_get_uri_scheme(file) : NULL;
}

gchar *
xfdesktop_thunarx_file_info_get_mime_type(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    GFileInfo *info = xfdesktop_file_icon_peek_file_info(icon);

    return info ? g_strdup(g_file_info_get_content_type(info)) : NULL;
}

gboolean
xfdesktop_thunarx_file_info_has_mime_type(ThunarxFileInfo *file_info,
                                          const gchar *mime_type)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    GFileInfo *info = xfdesktop_file_icon_peek_file_info(icon);
    const gchar *content_type;

    if(!info)
        return FALSE;

    content_type = g_file_info_get_content_type(info);
    return g_content_type_is_a(content_type, mime_type);
}

gboolean
xfdesktop_thunarx_file_info_is_directory(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    GFileInfo *info = xfdesktop_file_icon_peek_file_info(icon);

    return (info && g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY);
}

GFileInfo *
xfdesktop_thunarx_file_info_get_file_info(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    GFileInfo *info = xfdesktop_file_icon_peek_file_info(icon);
    return info ? g_object_ref (info) : NULL;
}

GFileInfo *
xfdesktop_thunarx_file_info_get_filesystem_info(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    GFileInfo *info = xfdesktop_file_icon_peek_filesystem_info(icon);
    return info ? g_object_ref (info) : NULL;
}

GFile *
xfdesktop_thunarx_file_info_get_location(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    GFile *file = xfdesktop_file_icon_peek_file(icon);
    return g_object_ref (file);
}

#endif  /* HAVE_THUNARX */
