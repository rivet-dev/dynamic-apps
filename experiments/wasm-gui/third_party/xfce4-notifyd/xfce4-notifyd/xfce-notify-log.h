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

#ifndef __XFCE_NOTIFY_LOG_H__
#define __XFCE_NOTIFY_LOG_H__

#include <glib-object.h>

#include <common/xfce-notify-log-types.h>

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(XfceNotifyLog, xfce_notify_log, XFCE, NOTIFY_LOG, GObject)
#define XFCE_TYPE_NOTIFY_LOG (xfce_notify_log_get_type())

XfceNotifyLog *xfce_notify_log_open(GError **error);

XfceNotifyLogEntry *xfce_notify_log_get(XfceNotifyLog *log,
                                        const gchar *id);
GList *xfce_notify_log_read(XfceNotifyLog *log,
                            const gchar *start_after_id,
                            guint count);
GList *xfce_notify_log_read_unread(XfceNotifyLog *log,
                                   const gchar *start_after_id,
                                   guint count);

gboolean xfce_notify_log_has_unread_messages(XfceNotifyLog *log);
guint xfce_notify_log_count_unread_messages(XfceNotifyLog *log);
GHashTable *xfce_notify_log_get_app_id_counts(XfceNotifyLog *log);

void xfce_notify_log_write(XfceNotifyLog *log,
                           XfceNotifyLogEntry *entry);
void xfce_notify_log_mark_read(XfceNotifyLog *log,
                               const gchar *id);
void xfce_notify_log_mark_all_read(XfceNotifyLog *log);

void xfce_notify_log_delete(XfceNotifyLog *log,
                            const gchar *id);
void xfce_notify_log_truncate(XfceNotifyLog *log,
                              guint n_entries_to_keep);

void xfce_notify_log_clear(XfceNotifyLog *log);

G_END_DECLS

#endif  /* __XFCE_NOTIFY_LOG_H__ */
