/*
 * Copyright (C) 2009-2010 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include <libxfce4ui/libxfce4ui.h>

#include <common/panel-private.h>
#include <common/panel-utils.h>



void
_panel_utils_weak_notify (gpointer  data,
                          GObject  *where_the_object_was)
{
  if (XFCE_IS_PANEL_PLUGIN (data))
    xfce_panel_plugin_unblock_menu (data);
  else
    g_object_unref (data);
}



static void
panel_utils_help_button_clicked (GtkWidget       *button,
                                 XfcePanelPlugin *panel_plugin)
{
  GtkWidget *toplevel;

  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN (panel_plugin));
  panel_return_if_fail (GTK_IS_WIDGET (button));

  toplevel = gtk_widget_get_toplevel (button);
  panel_utils_show_help (GTK_WINDOW (toplevel),
      xfce_panel_plugin_get_name (panel_plugin),
      NULL);
}



static void
panel_utils_block_autohide (XfcePanelPlugin *panel_plugin)
{
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN (panel_plugin));

  xfce_panel_plugin_block_autohide (panel_plugin, TRUE);
}



static void
panel_utils_unblock_autohide (XfcePanelPlugin *panel_plugin)
{
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN (panel_plugin));

  xfce_panel_plugin_block_autohide (panel_plugin, FALSE);
}



GtkBuilder *
panel_utils_builder_new (XfcePanelPlugin  *panel_plugin,
                         const gchar      *buffer,
                         gsize             length,
                         GObject         **dialog_return)
{
  GError     *error = NULL;
  GtkBuilder *builder;
  GObject    *dialog, *button;

  panel_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (panel_plugin), NULL);

  builder = gtk_builder_new ();
  if (gtk_builder_add_from_string (builder, buffer, length, &error))
    {
      dialog = gtk_builder_get_object (builder, "dialog");
      if (G_LIKELY (dialog != NULL))
        {
          g_object_weak_ref (G_OBJECT (dialog), _panel_utils_weak_notify, builder);
          xfce_panel_plugin_take_window (panel_plugin, GTK_WINDOW (dialog));

          xfce_panel_plugin_block_menu (panel_plugin);
          g_object_weak_ref (G_OBJECT (dialog), _panel_utils_weak_notify, panel_plugin);

          g_signal_connect_swapped (dialog, "show",
              G_CALLBACK (panel_utils_block_autohide), panel_plugin);
          g_signal_connect_swapped (dialog, "hide",
              G_CALLBACK (panel_utils_unblock_autohide), panel_plugin);

          button = gtk_builder_get_object (builder, "close-button");
          if (G_LIKELY (button != NULL))
            g_signal_connect_swapped (G_OBJECT (button), "clicked",
                G_CALLBACK (gtk_widget_destroy), dialog);

          button = gtk_builder_get_object (builder, "help-button");
          if (G_LIKELY (button != NULL))
            g_signal_connect (G_OBJECT (button), "clicked",
                G_CALLBACK (panel_utils_help_button_clicked), panel_plugin);

          if (G_LIKELY (dialog_return != NULL))
            *dialog_return = dialog;

          return builder;
        }
      else
        {
          g_set_error_literal (&error, 0, 0, "No widget with the name \"dialog\" found");
        }
    }

  g_critical ("Failed to construct the builder for plugin %s-%d: %s.",
              xfce_panel_plugin_get_name (panel_plugin),
              xfce_panel_plugin_get_unique_id (panel_plugin),
              error->message);
  g_error_free (error);
  g_object_unref (G_OBJECT (builder));

  return NULL;
}



void
panel_utils_show_help (GtkWindow   *parent,
                       const gchar *page,
                       const gchar *offset)
{
  xfce_dialog_show_help (parent, "xfce4-panel", page, offset);
}



gboolean
panel_utils_device_grab (GtkWidget *widget)
{
  GdkScreen  *screen = gtk_widget_get_screen (widget);
  GdkDisplay *display = gdk_screen_get_display (screen);
  GdkSeat    *seat = gdk_display_get_default_seat (display);
  GdkWindow  *window = gdk_window_get_effective_toplevel (gtk_widget_get_window (widget));

  return xfce_gdk_device_grab (seat, window, GDK_SEAT_CAPABILITY_ALL, NULL);
}



void
panel_utils_set_atk_info (GtkWidget   *widget,
                          const gchar *name,
                          const gchar *description)
{
  static gboolean  initialized = FALSE;
  static gboolean  atk_enabled = TRUE;
  AtkObject       *object;

  panel_return_if_fail (GTK_IS_WIDGET (widget));

  if (atk_enabled)
    {
      object = gtk_widget_get_accessible (widget);

      if (!initialized)
        {
          initialized = TRUE;
          atk_enabled = GTK_IS_ACCESSIBLE (object);

          if (!atk_enabled)
            return;
        }

      if (name != NULL)
        atk_object_set_name (object, name);

      if (description != NULL)
        atk_object_set_description (object, description);
    }
}



static gboolean
destroy_later (gpointer widget)
{
  gtk_widget_destroy (GTK_WIDGET (widget));
  g_object_unref (G_OBJECT (widget));
  return FALSE;
}



void
panel_utils_destroy_later (GtkWidget *widget)
{
  panel_return_if_fail (GTK_IS_WIDGET (widget));

  g_idle_add_full (G_PRIORITY_HIGH, destroy_later, widget, NULL);
  g_object_ref_sink (G_OBJECT (widget));
}
