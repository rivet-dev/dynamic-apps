/*
 *  xfce4-notifyd
 *
 *  Copyright (c) 2016 Simon Steinbeiß <ochosi@xfce.org>
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

#ifndef __XFCE_NOTIFY_LOG_UTIL_H__
#define __XFCE_NOTIFY_LOG_UTIL_H__

#include <gtk/gtk.h>

#include "common/xfce-notify-log-gbus.h"

#include "xfce-notify-common.h"
#include "xfce-notify-log-types.h"

#define XFCE_NOTIFY_ICON_PATH "xfce4/notifyd/icons/"

G_BEGIN_DECLS

GdkPixbuf *notify_pixbuf_from_image_data (GVariant *image_data);

const gchar *xfce_notify_log_get_icon_folder(void);
gchar *xfce_notify_log_cache_icon(GVariant *v_image_data,
                                  const gchar *image_path,
                                  const gchar *app_icon,
                                  const gchar *desktop_id);

gchar *notify_get_from_desktop_file (const gchar *desktop_file,
                                     const gchar *key);

GtkWidget *xfce_notify_clear_log_dialog(XfceNotifyLogGBus *log,
                                        GtkWindow *parent);

cairo_surface_t *notify_log_load_icon(const gchar *notify_log_icon_folder,
                                      const gchar *icon_id,
                                      const gchar *app_id,
                                      gint size,
                                      gint scale);
void notify_log_icon_add_unread_emblem(cairo_surface_t *surface,
                                       GtkStyleContext *style_context,
                                       gint size,
                                       gint scale_factor,
                                       gdouble alpha);

gchar *notify_log_format_timestamp(GDateTime *timestamp,
                                   XfceNotifyDatetimeFormat format,
                                   const gchar *custom_format);
gchar *notify_log_format_summary(const gchar *summary);
gchar *notify_log_format_body(const gchar *body);
gchar *notify_log_format_tooltip(const gchar *app_name,
                                 const gchar *timestamp,
                                 const gchar *body_text);

GList *notify_log_variant_to_entries(GVariant *variant);
XfceNotifyLogEntry *notify_log_variant_to_entry(GVariant *variant);

G_END_DECLS

#endif /* __XFCE_NOTIFY_LOG_UTIL_H__ */
