/*
 *  xfce4-notifyd
 *
 *  Copyright (c) 2008-2009 Brian Tarricone <bjt23@cornell.edu>
 *  Copyright (c) 2009 Jérôme Guelfucci <jeromeg@xfce.org>
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

#ifndef __XFCE_NOTIFY_WINDOW_H__
#define __XFCE_NOTIFY_WINDOW_H__

#include <gtk/gtk.h>

#include <common/xfce-notify-common.h>

#define XFCE_TYPE_NOTIFY_WINDOW     (xfce_notify_window_get_type())
#define XFCE_NOTIFY_WINDOW(obj)     (G_TYPE_CHECK_INSTANCE_CAST((obj), XFCE_TYPE_NOTIFY_WINDOW, XfceNotifyWindow))
#define XFCE_IS_NOTIFY_WINDOW(obj)  (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFCE_TYPE_NOTIFY_WINDOW))

G_BEGIN_DECLS

typedef struct _XfceNotifyWindow  XfceNotifyWindow;

GType xfce_notify_window_get_type(void) G_GNUC_CONST;

GtkWidget *xfce_notify_window_new(guint id,
                                  GdkMonitor *monitor,
                                  gboolean override_redirect,
                                  XfceNotifyPosition location,
                                  gdouble normal_opacity,
                                  gboolean show_text_with_gauge);

guint xfce_notify_window_get_id(XfceNotifyWindow *window);

GdkMonitor *xfce_notify_window_get_monitor(XfceNotifyWindow *window);
void xfce_notify_window_update_monitor(XfceNotifyWindow *window,
                                       GdkMonitor *monitor);

void xfce_notify_window_set_geometry(XfceNotifyWindow *window,
                                     GdkRectangle *rectangle,
                                     GdkRectangle *monitor_workarea);
GdkRectangle *xfce_notify_window_get_geometry(XfceNotifyWindow *window);

G_END_DECLS

#endif  /* __XFCE_NOTIFY_WINDOW_H__ */
