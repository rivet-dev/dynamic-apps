/*
 *  xfce4-notifyd
 *
 *  Copyright (c) 2008,2023 Brian Tarricone <brian@tarricone.org>
 *  Copyright (c) Simon Steinbeiß <simon@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __XFCE_NOTIFY_LOG_VIEWER_H__
#define __XFCE_NOTIFY_LOG_VIEWER_H__

#include <gtk/gtk.h>

#include <xfconf/xfconf.h>

#include <common/xfce-notify-log-gbus.h>

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(XfceNotifyLogViewer, xfce_notify_log_viewer, XFCE, NOTIFY_LOG_VIEWER, GtkBox)
#define XFCE_TYPE_NOTIFY_LOG_VIEWER (xfce_notify_log_viewer_get_type())

GtkWidget *xfce_notify_log_viewer_new(XfconfChannel *channel,
                                      XfceNotifyLogGBus *log);

G_END_DECLS

#endif  /* __XFCE_NOTIFY_LOG_VIEWER_H__ */
