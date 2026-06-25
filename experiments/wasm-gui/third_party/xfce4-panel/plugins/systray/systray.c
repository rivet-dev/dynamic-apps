/*
 * Copyright (c) 2005-2007 Jasper Huijsmans <jasper@xfce.org>
 * Copyright (c) 2007-2010 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <common/panel-private.h>
#include <common/panel-xfconf.h>
#include <common/panel-utils.h>
#include <common/panel-debug.h>

#include "systray.h"
#include "systray-box.h"
#include "systray-socket.h"
#include "systray-manager.h"

#define BUTTON_SIZE   (16)


static void     systray_plugin_names_update                 (SnPlugin              *plugin);
static gboolean systray_plugin_names_get_hidden             (SnPlugin              *plugin,
                                                             const gchar           *name);
static void     systray_plugin_icon_added                   (SystrayManager        *manager,
                                                             GtkWidget             *icon,
                                                             SnPlugin              *plugin);
static void     systray_plugin_icon_removed                 (SystrayManager        *manager,
                                                             GtkWidget             *icon,
                                                             SnPlugin              *plugin);
static void     systray_plugin_lost_selection               (SystrayManager        *manager,
                                                             SnPlugin              *plugin);



void  systray_plugin_configuration_changed  (SnConfig           *config,
                                             SnPlugin           *plugin)
{
  gint icon_size, n_rows, row_size, padding;
  gboolean square_icons;
  gboolean single_row;
  GList *list;
  GList *l;
  gchar *name;

  /* icon-size */
  sn_config_get_dimensions (config, &icon_size, &n_rows, &row_size, &padding);
  systray_box_set_dimensions (XFCE_SYSTRAY_BOX (plugin->systray_box),
                              icon_size, n_rows, row_size, padding);

  /* square-icons */
  square_icons = sn_config_get_square_icons (config);
  systray_box_set_squared (XFCE_SYSTRAY_BOX (plugin->systray_box), square_icons);

  /* single-row */
  single_row = sn_config_get_single_row (config);
  systray_box_set_single_row (XFCE_SYSTRAY_BOX (plugin->systray_box), single_row);

  /* known-legacy-items */
  {
    g_slist_free_full (plugin->names_ordered, g_free);
    plugin->names_ordered = NULL;

    /* add new values */
    list = sn_config_get_known_legacy_items (config);
    for (l = list; l != NULL; l = l->next)
      {
        name = g_strdup (l->data);
        plugin->names_ordered = g_slist_prepend (plugin->names_ordered, name);
      }
    plugin->names_ordered = g_slist_reverse (plugin->names_ordered);
  }

  /* hidden-legacy-items */
  {
    g_hash_table_remove_all (plugin->names_hidden);

    /* add new values */
    list = sn_config_get_hidden_legacy_items (config);
    for (l = list; l != NULL; l = l->next)
      {
        name = g_strdup (l->data);
        g_hash_table_replace (plugin->names_hidden, name, NULL);
      }

    if (list != NULL)
      g_list_free (list);
  }

  /* update icons in the box */
  systray_plugin_names_update (plugin);

  systray_plugin_size_changed (XFCE_PANEL_PLUGIN (plugin),
                               xfce_panel_plugin_get_size (XFCE_PANEL_PLUGIN (plugin)));
}



static gboolean
systray_plugin_screen_changed_idle (gpointer user_data)
{
  SnPlugin      *plugin = XFCE_SN_PLUGIN (user_data);
  GdkScreen     *screen;
  GError        *error = NULL;

  /* create a new manager and register this screen */
  plugin->manager = systray_manager_new ();
  g_signal_connect (G_OBJECT (plugin->manager), "icon-added",
      G_CALLBACK (systray_plugin_icon_added), plugin);
  g_signal_connect (G_OBJECT (plugin->manager), "icon-removed",
      G_CALLBACK (systray_plugin_icon_removed), plugin);
  g_signal_connect (G_OBJECT (plugin->manager), "lost-selection",
      G_CALLBACK (systray_plugin_lost_selection), plugin);

  /* try to register the systray */
  screen = gtk_widget_get_screen (GTK_WIDGET (plugin));
  if (systray_manager_register (plugin->manager, screen, &error))
    {
      /* send the plugin orientation */
      systray_plugin_orientation_changed (XFCE_PANEL_PLUGIN (plugin),
         xfce_panel_plugin_get_orientation (XFCE_PANEL_PLUGIN (plugin)));
    }
  else
    {
      xfce_dialog_show_error (NULL, error, _("Unable to start the notification area"));
      g_error_free (error);
    }

  return FALSE;
}



static void
systray_plugin_screen_changed_idle_destroyed (gpointer user_data)
{
  XFCE_SN_PLUGIN (user_data)->idle_startup = 0;
}



void
systray_plugin_screen_changed (GtkWidget *widget,
                               GdkScreen *previous_screen)
{
  SnPlugin *plugin = XFCE_SN_PLUGIN (widget);

  if (G_UNLIKELY (plugin->manager != NULL))
    {
      /* unregister this screen screen */
      systray_manager_unregister (plugin->manager);
      g_object_unref (G_OBJECT (plugin->manager));
      plugin->manager = NULL;
    }

  /* schedule a delayed startup */
  if (plugin->idle_startup == 0)
    plugin->idle_startup = gdk_threads_add_idle_full (G_PRIORITY_LOW, systray_plugin_screen_changed_idle,
                                                      plugin, systray_plugin_screen_changed_idle_destroyed);
}


void
systray_plugin_composited_changed (GtkWidget *widget)
{
  /* restart the manager to add the sockets again */
  systray_plugin_screen_changed (widget, gtk_widget_get_screen (widget));
}



void
systray_plugin_orientation_changed (XfcePanelPlugin *panel_plugin,
                                    GtkOrientation   orientation)
{
  SnPlugin *plugin = XFCE_SN_PLUGIN (panel_plugin);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (plugin->box), orientation);
  systray_box_set_orientation (XFCE_SYSTRAY_BOX (plugin->systray_box), orientation);

  if (G_LIKELY (plugin->manager != NULL))
    systray_manager_set_orientation (plugin->manager, orientation);

  /* apply symbolic colors */
  if (G_LIKELY (plugin->manager != NULL)) {
    GtkStyleContext *context;
    GdkRGBA rgba, fg, error, warning, success;

    context = gtk_widget_get_style_context (GTK_WIDGET (plugin->systray_box));
    gtk_style_context_get_color (context, GTK_STATE_FLAG_NORMAL, &rgba);

    rgba.red *= G_MAXUSHORT;
    rgba.green *= G_MAXUSHORT;
    rgba.blue *= G_MAXUSHORT;

    fg = error = warning = success = rgba;

    systray_manager_set_colors (plugin->manager, &fg, &error, &warning, &success);
  }

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_widget_set_size_request (plugin->button, BUTTON_SIZE, -1);
  else
    gtk_widget_set_size_request (plugin->button, -1, BUTTON_SIZE);
}



gboolean
systray_plugin_size_changed (XfcePanelPlugin *panel_plugin,
                             gint             size)
{
  SnPlugin         *plugin = XFCE_SN_PLUGIN (panel_plugin);
  GtkStyleContext  *context;
  GtkBorder         padding;
  gint              border = 0;

  /* because the allocated size, used in size_requested is always 1 step
   * behind the allocated size when resizing and during startup, we
   * correct the maximum size set by the user with the size the panel
   * will most likely allocated */
  context = gtk_widget_get_style_context (plugin->box);
  gtk_style_context_get_padding (context, gtk_widget_get_state_flags (plugin->box), &padding);

  border += MAX (padding.left + padding.right, padding.top + padding.bottom);
  systray_box_set_size_alloc (XFCE_SYSTRAY_BOX (plugin->systray_box), size - 2 * border);

  return TRUE;
}



static void
systray_plugin_box_draw_icon (GtkWidget *child,
                              gpointer   user_data)
{
  cairo_t       *cr = user_data;
  GtkAllocation  alloc;

  if (systray_socket_is_composited (XFCE_SYSTRAY_SOCKET (child)))
    {
      gtk_widget_get_allocation (child, &alloc);

      /* skip hidden (see offscreen in box widget) icons */
      if (alloc.x > -1 && alloc.y > -1)
        {
          // FIXME
          gdk_cairo_set_source_window (cr, gtk_widget_get_window (child),
                                       alloc.x, alloc.y);
          cairo_paint (cr);
        }
    }
}



void
systray_plugin_box_draw (GtkWidget *box,
                         cairo_t   *cr,
                         gpointer   user_data)
{
  SnPlugin *plugin = XFCE_SN_PLUGIN (user_data);
  panel_return_if_fail (XFCE_IS_SN_PLUGIN (plugin));
  panel_return_if_fail (cr != NULL);

  /* separately draw all the composed tray icons after gtk
   * handled the draw event */
  gtk_container_foreach (GTK_CONTAINER (box),
                         (GtkCallback) (void (*)(void)) systray_plugin_box_draw_icon, cr);
}



static void
systray_plugin_names_update_icon (GtkWidget *icon,
                                  gpointer   data)
{
  SnPlugin *plugin = XFCE_SN_PLUGIN (data);
  SystraySocket *socket = XFCE_SYSTRAY_SOCKET (icon);
  const gchar   *name;

  panel_return_if_fail (XFCE_IS_SN_PLUGIN (plugin));
  panel_return_if_fail (XFCE_IS_SYSTRAY_SOCKET (icon));

  name = systray_socket_get_name (socket);
  systray_socket_set_hidden (socket,
      systray_plugin_names_get_hidden (plugin, name));
}



static void
systray_plugin_names_update (SnPlugin *plugin)
{
  panel_return_if_fail (XFCE_IS_SN_PLUGIN (plugin));

  gtk_container_foreach (GTK_CONTAINER (plugin->systray_box),
    systray_plugin_names_update_icon, plugin);
  systray_box_update (XFCE_SYSTRAY_BOX (plugin->systray_box),
    plugin->names_ordered);
}



static gboolean
systray_plugin_names_get_hidden (SnPlugin      *plugin,
                                 const gchar   *name)
{
  if (panel_str_is_empty (name))
    return FALSE;

  /* lookup the name in the list */
  if (g_slist_find_custom (plugin->names_ordered, name, (GCompareFunc)g_strcmp0) == NULL)
    {
      /* add the new name */
      plugin->names_ordered = g_slist_prepend (plugin->names_ordered, g_strdup (name));

      if (sn_plugin_legacy_item_added (plugin, name))
        {
          g_hash_table_replace (plugin->names_hidden, g_strdup (name), NULL);
          return TRUE;
        }

      /* do not hide the icon */
      return FALSE;
    }
  else
    {
      return g_hash_table_contains (plugin->names_hidden, name);
    }
}



static void
systray_plugin_icon_added (SystrayManager *manager,
                           GtkWidget      *icon,
                           SnPlugin       *plugin)
{
  panel_return_if_fail (XFCE_IS_SYSTRAY_MANAGER (manager));
  panel_return_if_fail (XFCE_IS_SN_PLUGIN (plugin));
  panel_return_if_fail (XFCE_IS_SYSTRAY_SOCKET (icon));
  panel_return_if_fail (plugin->manager == manager);
  panel_return_if_fail (GTK_IS_WIDGET (icon));

  systray_plugin_names_update_icon (icon, plugin);
  gtk_container_add (GTK_CONTAINER (plugin->systray_box), icon);
  gtk_widget_show (icon);

  panel_debug_filtered (PANEL_DEBUG_SYSTRAY, "added %s[%p] icon",
      systray_socket_get_name (XFCE_SYSTRAY_SOCKET (icon)), icon);
}



static void
systray_plugin_icon_removed (SystrayManager *manager,
                             GtkWidget      *icon,
                             SnPlugin       *plugin)
{
  panel_return_if_fail (XFCE_IS_SYSTRAY_MANAGER (manager));
  panel_return_if_fail (XFCE_IS_SN_PLUGIN (plugin));
  panel_return_if_fail (plugin->manager == manager);
  panel_return_if_fail (GTK_IS_WIDGET (icon));

  /* remove the icon from the box */
  gtk_container_remove (GTK_CONTAINER (plugin->systray_box), icon);

  panel_debug_filtered (PANEL_DEBUG_SYSTRAY, "removed %s[%p] icon",
      systray_socket_get_name (XFCE_SYSTRAY_SOCKET (icon)), icon);
}



static void
systray_plugin_lost_selection (SystrayManager *manager,
                               SnPlugin       *plugin)
{
  GError error;

  panel_return_if_fail (XFCE_IS_SYSTRAY_MANAGER (manager));
  panel_return_if_fail (XFCE_IS_SN_PLUGIN (plugin));
  panel_return_if_fail (plugin->manager == manager);

  /* create fake error and show it */
  error.message = _("Most likely another widget took over the function "
                    "of a notification area. This area will be unused.");
  xfce_dialog_show_error (NULL, &error, _("The notification area lost selection"));
}
