/* vi:set et ai sw=4 sts=4 ts=4: */
/*
 *  xfce4-notifyd
 *
 *  Copyright (c) 2023 Brian Tarricone <brian@tarricone.org>
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

#ifndef __XFCE_NOTIFICATION_H__
#define __XFCE_NOTIFICATION_H__

#include <gtk/gtk.h>

#ifdef ENABLE_SOUND
#include <canberra.h>
#endif

#include <common/xfce-notify-common.h>

#include "xfce-notify-types.h"

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(XfceNotification, xfce_notification, XFCE, NOTIFICATION, GObject)
#define XFCE_TYPE_NOTIFICATION (xfce_notification_get_type())

XfceNotification *xfce_notification_new(guint id,
                                        const gchar *log_id,
                                        const gchar *summary,
                                        const gchar *body,
                                        gboolean icon_only,
                                        const gchar *icon_name,
                                        GdkPixbuf *icon_pixbuf,
                                        const gchar *icon_id,
                                        guint gauge_value,
                                        gboolean gauge_value_set,
                                        XfceNotificationActions *actions,
                                        guint expire_timeout,
                                        XfceNotifyUrgency urgency,
                                        gboolean do_fadeout,
                                        gboolean do_slideout,
#ifdef ENABLE_SOUND
                                        ca_proplist *sound_props,
#endif
                                        GtkCssProvider *css_provider);

void xfce_notification_update(XfceNotification *notification,
                              const gchar *summary,
                              const gchar *body,
                              gboolean icon_only,
                              const gchar *icon_name,
                              GdkPixbuf *icon_pixbuf,
                              const gchar *icon_id,
                              guint gauge_value,
                              gboolean gauge_value_set,
                              XfceNotificationActions *actions,
                              guint expire_timeout,
                              XfceNotifyUrgency urgency
#ifdef ENABLE_SOUND
                              , ca_proplist *sound_props
#endif
                              );

GList *xfce_notification_create_windows(XfceNotification *notification,
                                        GList *monitors,
                                        gboolean override_redirect,
                                        XfceNotifyPosition location,
                                        gdouble normal_opacity,
                                        gboolean show_text_with_gauge);
void xfce_notification_realize(XfceNotification *notification);

void xfce_notification_set_do_fadeout(XfceNotification *notification,
                                      gboolean do_fadeout);
void xfce_notification_set_do_slideout(XfceNotification *notification,
                                       gboolean do_slideout);

guint xfce_notification_get_id(XfceNotification *notification);
const gchar *xfce_notification_get_log_id(XfceNotification *notification);
const gchar *xfce_notification_get_summary(XfceNotification *notification);
const gchar *xfce_notification_get_body(XfceNotification *notification);
const gchar *xfce_notification_get_icon_id(XfceNotification *notification);
XfceNotifyUrgency xfce_notification_get_urgency(XfceNotification *notification);
GList *xfce_notification_get_windows(XfceNotification *notification);

void xfce_notification_closed(XfceNotification *notification,
                              XfceNotifyCloseReason reason);

G_END_DECLS

#endif  /* __XFCE_NOTIFICATION_H__ */
