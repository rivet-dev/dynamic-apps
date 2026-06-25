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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "xfce-notify-log-types.h"

static inline void
xfce_notify_log_entry_action_free(XfceNotifyLogEntryAction *action) {
    g_free(action->id);
    g_free(action->label);
    g_free(action);
}

XfceNotifyLogEntry *
xfce_notify_log_entry_new_empty(void) {
    XfceNotifyLogEntry *entry = g_new0(XfceNotifyLogEntry, 1);
    g_atomic_ref_count_init(&entry->ref_count);
    return entry;
}

XfceNotifyLogEntry *
xfce_notify_log_entry_ref(XfceNotifyLogEntry *entry) {
    g_return_val_if_fail(entry != NULL, NULL);
    g_atomic_ref_count_inc(&entry->ref_count);
    return entry;
}

void
xfce_notify_log_entry_unref(XfceNotifyLogEntry *entry) {
    g_return_if_fail(entry != NULL);

    if (g_atomic_ref_count_dec(&entry->ref_count)) {
        g_free(entry->id);
        if (G_LIKELY(entry->timestamp != NULL)) {
            g_date_time_unref(entry->timestamp);
        }
        g_free(entry->app_id);
        g_free(entry->app_name);
        g_free(entry->icon_id);
        g_free(entry->summary);
        g_free(entry->body);
        g_list_free_full(entry->actions, (GDestroyNotify)xfce_notify_log_entry_action_free);
        g_free(entry);
    }
}
