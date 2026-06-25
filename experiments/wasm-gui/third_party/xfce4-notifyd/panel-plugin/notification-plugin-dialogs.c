/*  xfce4-notification-plugin
 *
 *  Copyright (C) 2017 Simon Steinbeiß <simon@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>

#include <xfconf/xfconf.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4panel/libxfce4panel.h>

#include "notification-plugin.h"
#include "notification-plugin-log.h"
#include "notification-plugin-dialogs.h"

/* the website url */
#define PLUGIN_WEBSITE "https://docs.xfce.org/apps/notifyd/start"



static void
notification_plugin_configure_response (GtkWidget    *dialog,
                           gint          response,
                           NotificationPlugin *notification_plugin)
{
  gboolean result;

  if (response == GTK_RESPONSE_HELP)
    {
      result = g_spawn_command_line_async ("exo-open --launch WebBrowser " PLUGIN_WEBSITE, NULL);

      if (G_UNLIKELY (result == FALSE))
        g_warning (_("Unable to open the following url: %s"), PLUGIN_WEBSITE);
    }
  else
    {
      g_object_set_data (G_OBJECT (notification_plugin->plugin), "dialog", NULL);
      xfce_panel_plugin_unblock_menu (notification_plugin->plugin);
      gtk_widget_destroy (dialog);
    }
}



void
notification_plugin_configure (XfcePanelPlugin      *plugin,
                               NotificationPlugin   *notification_plugin)
{
  GtkBuilder *builder;
  GtkWidget *dialog;
  gint log_icon_size;
  gdouble log_display_limit;

  builder = gtk_builder_new_from_resource("/org/xfce/notifyd/panel-plugin/notification-plugin-settings.glade");
  if (G_UNLIKELY(builder == NULL)) {
      g_critical("Unable to read settings UI description");
      return;
  }

  /* block the plugin menu */
  xfce_panel_plugin_block_menu(plugin);

  dialog = GTK_WIDGET(gtk_builder_get_object(builder, "settings_dialog"));
  g_signal_connect(G_OBJECT(dialog), "response",
                   G_CALLBACK(notification_plugin_configure_response), notification_plugin);

  log_icon_size = xfconf_channel_get_int(notification_plugin->channel, SETTING_LOG_ICON_SIZE, DEFAULT_LOG_ICON_SIZE);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "log_icon_size_adj")), log_icon_size);
  xfconf_g_property_bind(notification_plugin->channel, SETTING_LOG_ICON_SIZE, G_TYPE_INT,
                         G_OBJECT(gtk_builder_get_object(builder, "log_icon_size")), "value");

  log_display_limit = xfconf_channel_get_int(notification_plugin->channel,
                                             SETTING_LOG_DISPLAY_LIMIT, DEFAULT_LOG_DISPLAY_LIMIT);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "log_display_limit_adj")), log_display_limit);
  xfconf_g_property_bind(notification_plugin->channel, SETTING_LOG_DISPLAY_LIMIT, G_TYPE_INT,
                         G_OBJECT(gtk_builder_get_object(builder, "log_display_limit")), "value");

  xfconf_g_property_bind(notification_plugin->channel, SETTING_LOG_ONLY_TODAY, G_TYPE_BOOLEAN,
                         G_OBJECT(gtk_builder_get_object(builder, "show_only_today")), "active");

  xfconf_g_property_bind(notification_plugin->channel, SETTING_HIDE_CLEAR_PROMPT, G_TYPE_BOOLEAN,
                         G_OBJECT(gtk_builder_get_object(builder, "hide_clear_prompt")), "active");

  xfconf_g_property_bind(notification_plugin->channel, SETTING_HIDE_ON_READ, G_TYPE_BOOLEAN,
                         G_OBJECT(gtk_builder_get_object(builder, "hide_on_read")), "active");

  xfconf_g_property_bind(notification_plugin->channel, SETTING_SHOW_IN_MENU, G_TYPE_STRING,
                         G_OBJECT(gtk_builder_get_object(builder, "show_in_menu")), "active-id");

  xfconf_g_property_bind(notification_plugin->channel, SETTING_AFTER_MENU_SHOWN, G_TYPE_STRING,
                         G_OBJECT(gtk_builder_get_object(builder, "after_menu_shown")), "active-id");

  /* link the dialog to the plugin, so we can destroy it when the plugin
   * is closed, but the dialog is still open */
  g_object_set_data(G_OBJECT(plugin), "dialog", dialog);

  gtk_widget_show_all(dialog);
}



void
notification_plugin_about (XfcePanelPlugin *plugin)
{
  const gchar *auth[] =
    {
      "Simon Steinbeiss <simon@xfce.org>",
      NULL
    };
  gtk_show_about_dialog (NULL,
                         "logo-icon-name",         ICON_NAME,
                         "license",                xfce_get_license_text (XFCE_LICENSE_TEXT_GPL),
                         "version",                PACKAGE_VERSION,
                         "program-name",           PACKAGE_NAME,
                         "comments",               _("This is the notification plugin"),
                         "website",                PLUGIN_WEBSITE,
                         "copyright",              _("Copyright \xc2\xa9 2017 Simon Steinbeiß\n"),
                         "authors",                auth,
                         NULL);
}
