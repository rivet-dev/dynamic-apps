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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/libxfce4panel.h>

#include <common/xfce-notify-common.h>
#include <common/xfce-notify-enum-types.h>
#include <common/xfce-notify-log-gbus.h>
#include <common/xfce-notify-log-types.h>
#include <common/xfce-notify-log-util.h>

#include "notification-plugin.h"
#include "notification-plugin-log.h"



static void
notification_plugin_menu_item_activate (GtkWidget      *menuitem,
                                        gpointer        user_data)
{
  NotificationPlugin *notification_plugin = user_data;
  gboolean            muted;

  muted = !gtk_switch_get_active (GTK_SWITCH (notification_plugin->do_not_disturb_switch));
  gtk_switch_set_active (GTK_SWITCH (notification_plugin->do_not_disturb_switch), muted);
}



static void
notification_plugin_settings_activate_cb (GtkMenuItem *menuitem,
                                          gpointer     user_data)
{
  GAppInfo *app_info;
  GError   *error = NULL;

  app_info = g_app_info_create_from_commandline ("xfce4-notifyd-config", "Notification Settings",
                                                 G_APP_INFO_CREATE_NONE, NULL);
  if (!g_app_info_launch (app_info, NULL, NULL, &error)) {
    if (error != NULL) {
      g_warning ("xfce4-notifyd-config could not be launched. %s", error->message);
      g_error_free (error);
    }
  }
}



static void
notification_plugin_clear_log_dialog (GtkWidget *widget, gpointer user_data)
{
  NotificationPlugin* notification_plugin = user_data;
  GtkWidget *dialog;
	
  if (xfconf_channel_get_bool (notification_plugin->channel, SETTING_HIDE_CLEAR_PROMPT, FALSE)) {
    xfce_notify_log_gbus_call_clear(notification_plugin->log, NULL, NULL, NULL);
  } else {
    dialog = xfce_notify_clear_log_dialog(notification_plugin->log, NULL);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
  }
}


static void
notification_plugin_mark_all_read(GtkWidget *widget, NotificationPlugin *notification_plugin) {
  xfce_notify_log_gbus_call_mark_all_read(notification_plugin->log, NULL, NULL, NULL);
}


GtkWidget *
notification_plugin_menu_new(NotificationPlugin *notification_plugin) {
  GtkWidget *menu;
  GtkWidget *mi, *image, *label, *box;
  GDateTime *today;
  gint today_year, today_day;
  gchar *notify_log_icon_folder;
  int log_icon_size;
  XfceNotifyDatetimeFormat dt_format;
  gchar *custom_dt_format;
  const gchar *show_in_menu;
  gboolean only_unread;
  const gchar *after_menu_shown;
  gboolean mark_shown_read;
  gboolean mark_all_read;
  gboolean has_error = FALSE;
  gboolean no_notifications = FALSE;
  GtkStyleContext *style_context = gtk_widget_get_style_context(notification_plugin->button);
  gint scale_factor = gtk_widget_get_scale_factor(notification_plugin->button);

  today = g_date_time_new_now_local();
  today_year = g_date_time_get_year(today);
  today_day = g_date_time_get_day_of_year(today);

  menu = gtk_menu_new();

  notify_log_icon_folder = xfce_resource_save_location (XFCE_RESOURCE_CACHE,
                                                        XFCE_NOTIFY_ICON_PATH, TRUE);
  log_icon_size = xfconf_channel_get_int (notification_plugin->channel,
                                          SETTING_LOG_ICON_SIZE, -1);
  if (log_icon_size == -1)
    log_icon_size = DEFAULT_LOG_ICON_SIZE;
  dt_format = xfce_notify_xfconf_channel_get_enum(notification_plugin->channel,
                                                  DATETIME_FORMAT_PROP,
                                                  XFCE_NOTIFY_DATETIME_LOCALE_DEFAULT,
                                                  XFCE_TYPE_NOTIFY_DATETIME_FORMAT);
  custom_dt_format = xfconf_channel_get_string(notification_plugin->channel, DATETIME_CUSTOM_FORMAT_PROP, DATETIME_CUSTOM_FORMAT_DEFAULT);

  show_in_menu = xfconf_channel_get_string(notification_plugin->channel, SETTING_SHOW_IN_MENU, VALUE_SHOW_ALL);
  only_unread = g_strcmp0(show_in_menu, VALUE_SHOW_UNREAD) == 0;

  after_menu_shown = xfconf_channel_get_string(notification_plugin->channel, SETTING_AFTER_MENU_SHOWN, VALUE_MARK_ALL_READ);
  mark_shown_read = g_strcmp0(after_menu_shown, VALUE_MARK_SHOWN_READ) == 0;
  mark_all_read = g_strcmp0(after_menu_shown, VALUE_MARK_ALL_READ) == 0;

  /* switch for the do not disturb mode of xfce4-notifyd */
  mi = gtk_menu_item_new ();
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  label = gtk_label_new (NULL);
  gtk_label_set_markup_with_mnemonic (GTK_LABEL (label), _("<b>_Do not disturb</b>"));
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  notification_plugin->do_not_disturb_switch = gtk_switch_new ();
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), notification_plugin->do_not_disturb_switch, FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (mi), box);
  xfconf_g_property_bind (notification_plugin->channel, "/do-not-disturb", G_TYPE_BOOLEAN,
                          G_OBJECT (notification_plugin->do_not_disturb_switch), "active");
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
  gtk_widget_show_all (mi);
  g_signal_connect (mi, "activate",
                    G_CALLBACK (notification_plugin_menu_item_activate), notification_plugin);

  /* separator before the log */
  mi = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
  gtk_widget_show (mi);

  has_error = notification_plugin->log == NULL;

  if (!has_error) {
    GVariant *entriesv = NULL;
    GList *entries = NULL;
    GList *entries_shown = NULL;
    int log_display_limit;
    int numberof_notifications_shown = 0;
    gboolean log_only_today;
    GError *error = NULL;

    log_display_limit = xfconf_channel_get_int (notification_plugin->channel,
                                                SETTING_LOG_DISPLAY_LIMIT, -1);
    log_only_today = xfconf_channel_get_bool (notification_plugin->channel,
                                              SETTING_LOG_ONLY_TODAY, FALSE);
    if (log_display_limit == -1)
      log_display_limit = DEFAULT_LOG_DISPLAY_LIMIT;

    if (!xfce_notify_log_gbus_call_list_sync(notification_plugin->log,
                                             "",
                                             log_display_limit,
                                             only_unread,
                                             &entriesv,
                                             NULL,
                                             &error))
    {
        g_warning("Failed to fetch log entries: %s", error != NULL ? error->message : "(unknown)");
        if (error != NULL) {
            g_error_free(error);
        }
        has_error = TRUE;
    } else {
        entries = notify_log_variant_to_entries(entriesv);
        g_variant_unref(entriesv);
    }

    /* Check if the menu is going to be empty despite there being a log file, e.g.
       when showing only the notifications of today but the log only contains entries
       from yesterday and before. In this case show the placeholder. */
    if (entries == NULL)
      no_notifications = TRUE;

    for (GList *l = entries; l != NULL; l = l->next) {
      XfceNotifyLogEntry *entry = l->data;
      GtkWidget *hbox;
      GtkWidget *timestamp, *summary, *body = NULL, *app_icon = NULL;
      const gchar *app_name;
      gchar *timestamp_text;
      gchar *summary_text;
      gchar *body_text;
      cairo_surface_t *icon;
      gchar *tooltip_timestamp_text;
      gchar *tooltip_text;

      /* optionally only show notifications from today */
      if (log_only_today == TRUE) {
        GDateTime *entry_local = g_date_time_to_local(entry->timestamp);
        gint entry_year = g_date_time_get_year(entry_local);
        gint entry_day = g_date_time_get_day_of_year(entry_local);
        g_date_time_unref(entry_local);

        if (entry_year != today_year || entry_day != today_day) {
          no_notifications = TRUE;
          continue;
        } else {
          numberof_notifications_shown++;
        }
      }

      app_name = entry->app_name != NULL && entry->app_name[0] != '\0' ? entry->app_name : entry->app_id;
      timestamp_text = notify_log_format_timestamp(entry->timestamp, dt_format, custom_dt_format);
      summary_text = notify_log_format_summary(entry->summary);
      body_text = notify_log_format_body(entry->body);
      icon = notify_log_load_icon(notify_log_icon_folder, entry->icon_id, entry->app_id, log_icon_size, scale_factor);
      tooltip_timestamp_text = notify_log_format_timestamp(entry->timestamp, XFCE_NOTIFY_DATETIME_LOCALE_DEFAULT, NULL);
      tooltip_text = notify_log_format_tooltip(app_name, tooltip_timestamp_text, body_text);

      summary = g_object_new(GTK_TYPE_LABEL,
                             "use-markup", TRUE,
                             "label", summary_text,
                             "max-width-chars", 30,
                             "ellipsize", PANGO_ELLIPSIZE_END,
                             "xalign", 0.0,
                             NULL);
      timestamp = g_object_new(GTK_TYPE_LABEL,
                               "label", timestamp_text,
                               "xalign", 1.0,
                               NULL);
      if (body_text != NULL) {
        gchar *p = strchr(body_text, '\n');
        if (p != NULL) {
          p = strchr(p + 1, '\n');
          if (p != NULL) {
            *p = '\0';
          }
        }

        body = g_object_new(GTK_TYPE_LABEL,
                            "use-markup", TRUE,
                            "label", body_text,
                            "lines", 1,
                            "max-width-chars", 60,
                            "ellipsize", PANGO_ELLIPSIZE_END,
                            "wrap", TRUE,
                            "wrap-mode", PANGO_WRAP_WORD_CHAR,
                            "xalign", 0.0,
                            NULL);
      }
      if (icon != NULL) {
        if (!entry->is_read) {
            notify_log_icon_add_unread_emblem(icon, style_context, log_icon_size, scale_factor, 1.0);
        }
        app_icon = gtk_image_new_from_surface(icon);
      }

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      mi = gtk_image_menu_item_new ();
      gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), app_icon);
G_GNUC_END_IGNORE_DEPRECATIONS
      gtk_widget_set_tooltip_markup(mi, tooltip_text);

      hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
      gtk_widget_show(hbox);
      gtk_box_pack_start(GTK_BOX(hbox), summary, TRUE, TRUE, 0);
      gtk_box_pack_end(GTK_BOX(hbox), timestamp, FALSE, FALSE, 0);

      if (body == NULL) {
        gtk_container_add(GTK_CONTAINER(mi), hbox);
      } else {
        GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_widget_show(vbox);
        gtk_container_add(GTK_CONTAINER(mi), vbox);

        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), body, FALSE, FALSE, 0);
      }

      gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
      gtk_widget_show_all(mi);

      entries_shown = g_list_prepend(entries_shown, entry);

      g_free(timestamp_text);
      g_free(summary_text);
      g_free(body_text);
      g_free(tooltip_timestamp_text);
      g_free(tooltip_text);
      if (icon != NULL) {
          cairo_surface_destroy(icon);
      }
    }

    if (numberof_notifications_shown > 0)
      no_notifications = FALSE;

    if (mark_all_read) {
      xfce_notify_log_gbus_call_mark_all_read(notification_plugin->log, NULL, NULL, NULL);
    } else if (mark_shown_read && entries_shown != NULL) {
      GStrvBuilder *builder = g_strv_builder_new();
      gchar **ids;

      for (GList *l = entries_shown; l != NULL; l = l->next) {
        g_strv_builder_add(builder, ((XfceNotifyLogEntry *)l->data)->id);
      }
      ids = g_strv_builder_end(builder);

      xfce_notify_log_gbus_call_mark_read(notification_plugin->log, (const gchar *const *)ids, NULL, NULL, NULL);

      g_strfreev(ids);
      g_strv_builder_unref(builder);
    }

    g_list_free(entries_shown);
    g_list_free_full(entries, (GDestroyNotify)xfce_notify_log_entry_unref);
  }

  g_date_time_unref (today);
  g_free(custom_dt_format);

  /* Show a placeholder label when there are no notifications */
  if (has_error || no_notifications) {
    GtkStyleContext *context;
    GtkBorder padding;
    mi = gtk_menu_item_new ();
    if (has_error) {
      label = gtk_label_new(_("Unable to open notification log"));
    } else if (only_unread) {
      label = gtk_label_new(_("No unread notifications"));
    } else {
      label = gtk_label_new (_("No notifications"));
    }
    gtk_widget_set_sensitive (mi, FALSE);
    gtk_container_add (GTK_CONTAINER (mi), label);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
    gtk_widget_show_all (mi);
    /* Center the text and add top and bottom padding */
    context = gtk_widget_get_style_context (GTK_WIDGET (mi));
    gtk_style_context_get_padding (context, gtk_widget_get_state_flags (GTK_WIDGET (mi)), &padding);
    gtk_widget_set_margin_end (label, log_icon_size + padding.left);
    gtk_widget_set_margin_top (label, padding.top * 2);
    gtk_widget_set_margin_bottom (label, padding.top * 2);
  }

  /* footer items */
  mi = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
  gtk_widget_show (mi);

  /* checkmenuitem for the do not disturb mode of xfce4-notifyd */
  image = gtk_image_new_from_icon_name ("edit-clear-symbolic", GTK_ICON_SIZE_MENU);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  mi = gtk_image_menu_item_new_with_mnemonic (_("_Clear log"));
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi), image);
G_GNUC_END_IGNORE_DEPRECATIONS
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
  gtk_widget_set_sensitive(mi, notification_plugin->log != NULL);
  gtk_widget_show (mi);
  g_signal_connect (mi, "activate", G_CALLBACK (notification_plugin_clear_log_dialog),
                    notification_plugin);

  image = gtk_image_new_from_icon_name("checkbox-checked-symbolic", GTK_ICON_SIZE_MENU);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  mi = gtk_image_menu_item_new_with_mnemonic(_("_Mark all read"));
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), image);
G_GNUC_END_IGNORE_DEPRECATIONS
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
  gtk_widget_set_sensitive(mi, notification_plugin->log != NULL && !mark_all_read && notification_plugin->new_notifications);
  gtk_widget_show(mi);
  g_signal_connect(mi, "activate",
                   G_CALLBACK(notification_plugin_mark_all_read), notification_plugin);

  mi = gtk_menu_item_new_with_mnemonic (_("_Notification settings…"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
  gtk_widget_show (mi);
  g_signal_connect (mi, "activate", G_CALLBACK (notification_plugin_settings_activate_cb),
                    notification_plugin);

  return menu;
}
