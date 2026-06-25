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

#ifndef __NOTIFICATION_PLUGIN_H__
#define __NOTIFICATION_PLUGIN_H__

#include <xfconf/xfconf.h>
#include <libxfce4panel/libxfce4panel.h>

#include <common/xfce-notify-log-gbus.h>

G_BEGIN_DECLS

#define ICON_NAME                 "org.xfce.notification"
#define XFCE_NOTIFY_ICON_PATH     "xfce4/notifyd/icons/"
#define SETTING_LOG_DISPLAY_LIMIT "/plugin/log-display-limit"
#define DEFAULT_LOG_DISPLAY_LIMIT 10
#define SETTING_LOG_ONLY_TODAY    "/plugin/log-only-today"
#define SETTING_HIDE_CLEAR_PROMPT "/plugin/hide-clear-prompt"
#define DEFAULT_LOG_ICON_SIZE     16
#define SETTING_LOG_ICON_SIZE     "/plugin/log-icon-size"
#define SETTING_HIDE_ON_READ      "/plugin/hide-on-read"

#define SETTING_SHOW_IN_MENU      "/plugin/show-in-menu"
#define VALUE_SHOW_ALL            "show-all"
#define VALUE_SHOW_UNREAD         "show-unread"

#define SETTING_AFTER_MENU_SHOWN  "/plugin/after-menu-shown"
#define VALUE_MARK_ALL_READ       "mark-all-read"
#define VALUE_MARK_SHOWN_READ     "mark-shown-read"
#define VALUE_DO_NOTHING          "do-nothing"

/* plugin structure */
typedef struct
{
    XfcePanelPlugin *plugin;
    XfconfChannel   *channel;

    /* log */
    gint log_proxy_connect_id;
    XfceNotifyLogGBus *log;

    /* state */
    gboolean         new_notifications;

    /* panel widgets */
    GtkWidget       *button;
    GtkWidget       *image;

    /* menu widgets */
    GtkWidget       *do_not_disturb_switch;

    /* handlers */
    guint            menu_size_allocate_next_handler;

    gboolean         hide_on_read;
    gint             icon_size;
}
NotificationPlugin;

GtkWidget *notification_plugin_menu_new(NotificationPlugin *notification_plugin);
void notification_plugin_update_icon (NotificationPlugin *notification_plugin);

G_END_DECLS

#endif /* !__NOTIFICATION_PLUGIN_H__ */
