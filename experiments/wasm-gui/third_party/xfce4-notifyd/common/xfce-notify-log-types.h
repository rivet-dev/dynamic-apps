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

#ifndef __XFCE_NOTIFY_LOG_TYPES_H__
#define __XFCE_NOTIFY_LOG_TYPES_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _XfceNotifyLogEntryAction {
    gchar *id;
    gchar *label;
} XfceNotifyLogEntryAction;

typedef struct _XfceNotifyLogEntry {
    gchar *id;
    GDateTime *timestamp;
    gchar *app_id;
    gchar *app_name;
    gchar *icon_id;
    gchar *summary;
    gchar *body;
    GList *actions;
    gint expire_timeout;
    gboolean is_read;

    /*< private >*/
    gatomicrefcount ref_count;
} XfceNotifyLogEntry;

XfceNotifyLogEntry *xfce_notify_log_entry_new_empty(void);
XfceNotifyLogEntry *xfce_notify_log_entry_ref(XfceNotifyLogEntry *entry);
void xfce_notify_log_entry_unref(XfceNotifyLogEntry *entry);

G_END_DECLS

#endif  /* __XFCE_NOTIFY_LOG_TYPES_H__ */
