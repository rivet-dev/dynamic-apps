/*
 * Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
 * Copyright (c) 2008-2009 Mike Massonnet <mmassonnet@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>

#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <glib/gi18n-lib.h>

#include <xfconf/xfconf.h>

#include "twp-provider.h"


#define XFDESKTOP_SELECTION_FMT "XFDESKTOP_SELECTION_%d"
#define NAUTILUS_SELECTION_FMT  "NAUTILUS_DESKTOP_WINDOW_ID"



static void   twp_menu_provider_init            (ThunarxMenuProviderIface *iface);
static void   twp_provider_finalize             (GObject                  *object);
static GList *twp_provider_get_file_menu_items  (ThunarxMenuProvider      *menu_provider,
                                                 GtkWidget                *window,
                                                 GList                    *files);
static void   twp_action_set_wallpaper          (ThunarxMenuItem          *item,
                                                 gpointer                  user_data);
static gint   twp_get_active_workspace_number   (GdkScreen *screen);

static gboolean    _has_gsettings = FALSE;
static GtkWidget   *main_window = NULL;

struct _TwpProviderClass
{
  GObjectClass __parent__;
};

struct _TwpProvider
{
  GObject         __parent__;

  gchar          *child_watch_path;
  gint            child_watch_id;
};



THUNARX_DEFINE_TYPE_WITH_CODE (TwpProvider, twp_provider, G_TYPE_OBJECT,
                               THUNARX_IMPLEMENT_INTERFACE (THUNARX_TYPE_MENU_PROVIDER,
                                                            twp_menu_provider_init));

static void
twp_menu_provider_init (ThunarxMenuProviderIface *iface)
{
  iface->get_file_menu_items = twp_provider_get_file_menu_items;
}



static void
twp_provider_class_init (TwpProviderClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = twp_provider_finalize;
}

static void
twp_provider_init (TwpProvider *twp_provider)
{
  gchar *program;

  program = g_find_program_in_path ("gsettings");
  if (G_LIKELY (program != NULL))
    {
      _has_gsettings = TRUE;
      g_free (program);
    }
}



static void
twp_provider_finalize (GObject *object)
{
  G_OBJECT_CLASS (twp_provider_parent_class)->finalize (object);
}



static GList*
twp_provider_get_file_menu_items (ThunarxMenuProvider *menu_provider,
                                  GtkWidget           *window,
                                  GList               *files)
{
  ThunarxMenuItem *item = NULL;
  GFile           *location;
  GList           *items = NULL;
  gchar           *mime_type;

  main_window = window;

  /* we can only set a single wallpaper */
  if (files->next == NULL)
    {
      /* get the location of the file */
      location = thunarx_file_info_get_location (files->data);

      /* unable to handle non-local files */
      if (G_UNLIKELY (!g_file_has_uri_scheme (location, "file")))
        {
          g_object_unref (location);
          return NULL;
        }

      /* release the location */
      g_object_unref (location);

      if (!thunarx_file_info_is_directory (files->data))
        {
          /* get the mime type of the file */
          mime_type = thunarx_file_info_get_mime_type (files->data);

          if (g_str_has_prefix (mime_type, "image/")
              && (thunarx_file_info_has_mime_type (files->data, "image/jpeg")
                  || thunarx_file_info_has_mime_type (files->data, "image/png")
                  || thunarx_file_info_has_mime_type (files->data, "image/svg+xml")
                  || thunarx_file_info_has_mime_type (files->data, "image/svg+xml-compressed")))
            {
              item = thunarx_menu_item_new ("Twp::setwallpaper", _("Set as wallpaper"), NULL, "preferences-desktop-wallpaper");
              g_signal_connect (item, "activate", G_CALLBACK (twp_action_set_wallpaper), files->data);

              items = g_list_append (items, item);
            }
          g_free(mime_type);
        }
    }

  return items;
}

static void
twp_action_set_wallpaper (ThunarxMenuItem *item,
                          gpointer         user_data)
{
  ThunarxFileInfo *file_info = user_data;
  GdkDisplay      *display = gdk_display_get_default();
  gint             screen_nr = 0;
  gint             n_monitors;
  gint             monitor_nr = 0;
  gint             workspace;
  GdkScreen       *screen;
  GdkMonitor      *monitor;
  gchar           *image_path_prop;
  gchar           *image_show_prop;
  gchar           *image_style_prop;
  const char      *monitor_name;
  gchar           *file_uri;
  gchar           *escaped_file_name;
  gchar           *file_name = NULL;
  gchar           *hostname = NULL;
  gchar           *command;
  XfconfChannel   *channel;
  gboolean         is_single_workspace;
  gint             current_image_style;
  const gchar     *desktop_type = NULL;

  screen = gdk_display_get_default_screen (display);

  desktop_type = g_getenv ("XDG_CURRENT_DESKTOP");
  if (desktop_type == NULL)
    {
      g_warning ("Failed to set wallpaper: $XDG_CURRENT_DESKTOP is not defined");
      return;
    }

  file_uri = thunarx_file_info_get_uri (file_info);
  file_name = g_filename_from_uri (file_uri, &hostname, NULL);
  if (hostname != NULL)
    {
      g_free (hostname);
      g_free (file_uri);
      g_free (file_name);

      return;
    }

  workspace = twp_get_active_workspace_number (screen);
  n_monitors = gdk_display_get_n_monitors (display);

  if (n_monitors > 1 && main_window)
    {
      monitor = gdk_display_get_monitor_at_window (display, gtk_widget_get_window (main_window));
    }
  else
    {
      monitor = gdk_display_get_monitor (display, monitor_nr);
    }

  monitor_name = gdk_monitor_get_model (monitor);
  escaped_file_name = g_shell_quote (file_name);

  if (g_strcmp0 (desktop_type, "XFCE") == 0)
    {
      g_debug ("set on xfce");

      channel = xfconf_channel_get ("xfce4-desktop");

      /* This is the format for xfdesktop before 4.11 */
      image_path_prop = g_strdup_printf("/backdrop/screen%d/monitor%d/image-path", screen_nr, monitor_nr);
      image_show_prop = g_strdup_printf("/backdrop/screen%d/monitor%d/image-show", screen_nr, monitor_nr);
      image_style_prop = g_strdup_printf("/backdrop/screen%d/monitor%d/image-style", screen_nr, monitor_nr);

      /* Set the wallpaper and ensure that it's set to show */
      xfconf_channel_set_string (channel, image_path_prop, file_name);
      xfconf_channel_set_bool (channel, image_show_prop, TRUE);

      /* If there isn't a wallpaper style set (-1), or it is set to 'None' (which is 0) then set one */
      current_image_style = xfconf_channel_get_int (channel, image_style_prop, -1);
      if (current_image_style <= 0 )
        {
          /* Lets hope that 5 = 'Zoomed' works fine for the selected picture */
          xfconf_channel_set_int (channel, image_style_prop, 5);
        }

      g_free(image_path_prop);
      g_free(image_show_prop);
      g_free(image_style_prop);


      /* Xfdesktop 4.11+ has a concept of a single-workspace-mode where
       * the same workspace is used for everything but additionally allows
       * the user to use any current workspace as the single active
       * workspace, we'll need to check if it is enabled (which by default
       * it is) and use that. */
      is_single_workspace = xfconf_channel_get_bool (channel, "/backdrop/single-workspace-mode", TRUE);
      if (is_single_workspace)
        {
          workspace = xfconf_channel_get_int (channel, "/backdrop/single-workspace-number", 0);
        }

      /* This is the format for xfdesktop post 4.11. A workspace number is
       * added and the monitor is referred to name. We set both formats so
       * that it works as the user expects. */
      if (monitor_name)
        {
          image_path_prop = g_strdup_printf("/backdrop/screen%d/monitor%s/workspace%d/last-image", screen_nr, monitor_name, workspace);
          image_style_prop = g_strdup_printf("/backdrop/screen%d/monitor%s/workspace%d/image-style", screen_nr, monitor_name, workspace);
        }
      else
        {
          /* gdk_screen_get_monitor_plug_name can return NULL, in those
           * instances we fallback to monitor number but still include the
           * workspace number */
          image_path_prop = g_strdup_printf("/backdrop/screen%d/monitor%d/workspace%d/last-image", screen_nr, monitor_nr, workspace);
          image_style_prop = g_strdup_printf("/backdrop/screen%d/monitor%d/workspace%d/image-style", screen_nr, monitor_nr, workspace);
        }

      xfconf_channel_set_string (channel, image_path_prop, file_name);

      /* If there isn't a wallpaper style set (-1), or it is set to 'None' (which is 0) then set one */
      current_image_style = xfconf_channel_get_int (channel, image_style_prop, -1);
      if (current_image_style <= 0)
        {
          /* Lets hope that 5 = 'Zoomed' works fine for the selected picture */
          xfconf_channel_set_int (channel, image_style_prop, 5);
        }

      g_free(image_path_prop);
      g_free(image_style_prop);
    }
  else if (g_strcmp0 (desktop_type, "GNOME") == 0)
    {
      if (_has_gsettings)
        {
          g_debug ("set on gnome");

          command = g_strdup_printf ("gsettings set "
                                     "org.gnome.desktop.background picture-uri "
                                     "'%s'", file_uri);
          g_spawn_command_line_async (command, NULL);
          g_free (command);
        }
      else
        {
          g_warning ("Failed to set wallpaper: Missing executable 'gsettings'");
        }
    }
  else
    {
      g_warning (("Failed to set wallpaper: $XDG_CURRENT_DESKTOP Desktop type '%s' not supported by thunar wallpaper plugin."), desktop_type);
    }

  g_free (escaped_file_name);
  g_free (file_name);
  g_free (file_uri);
}

/* Taken from xfce_spawn_get_active_workspace_number in xfce-spawn.c apart of
 * the libxfce4ui library.
 * https://git.xfce.org/xfce/libxfce4ui/tree/libxfce4ui/xfce-spawn.c#n193
 */
static gint
twp_get_active_workspace_number (GdkScreen *screen)
{
  GdkWindow *root;
  gulong     bytes_after_ret = 0;
  gulong     nitems_ret = 0;
  guint     *prop_ret = NULL;
  Atom       _NET_CURRENT_DESKTOP;
  Atom       _WIN_WORKSPACE;
  Atom       type_ret = None;
  gint       format_ret;
  gint       ws_num = 0;

  gdk_x11_display_error_trap_push (gdk_display_get_default ());

  root = gdk_screen_get_root_window (screen);

  /* determine the X atom values */
  _NET_CURRENT_DESKTOP = XInternAtom (GDK_WINDOW_XDISPLAY (root), "_NET_CURRENT_DESKTOP", False);
  _WIN_WORKSPACE = XInternAtom (GDK_WINDOW_XDISPLAY (root), "_WIN_WORKSPACE", False);

  if (XGetWindowProperty (GDK_WINDOW_XDISPLAY (root),
                          gdk_x11_get_default_root_xwindow(),
                          _NET_CURRENT_DESKTOP, 0, 32, False, XA_CARDINAL,
                          &type_ret, &format_ret, &nitems_ret, &bytes_after_ret,
                          (gpointer) &prop_ret) != Success)
    {
      if (XGetWindowProperty (GDK_WINDOW_XDISPLAY (root),
                              gdk_x11_get_default_root_xwindow(),
                              _WIN_WORKSPACE, 0, 32, False, XA_CARDINAL,
                              &type_ret, &format_ret, &nitems_ret, &bytes_after_ret,
                              (gpointer) &prop_ret) != Success)
        {
          if (G_UNLIKELY (prop_ret != NULL))
            {
              XFree (prop_ret);
              prop_ret = NULL;
            }
        }
    }

  if (G_LIKELY (prop_ret != NULL))
    {
      if (G_LIKELY (type_ret != None && format_ret != 0))
        ws_num = *prop_ret;
      XFree (prop_ret);
    }

  gdk_x11_display_error_trap_pop_ignored (gdk_display_get_default ());

  return ws_num;
}
