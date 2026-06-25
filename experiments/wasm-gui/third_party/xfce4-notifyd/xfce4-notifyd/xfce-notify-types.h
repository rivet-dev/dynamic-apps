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

#ifndef __XFCE_NOTIFY_TYPES_H__
#define __XFCE_NOTIFY_TYPES_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct {
    gchar *id;
    gchar *label;
} XfceNotificationAction;

typedef struct {
    gboolean ids_are_icon_names;
    XfceNotificationAction *actions;
    gsize n_actions;
} XfceNotificationActions;

void xfce_notification_actions_free(XfceNotificationActions *actions);

G_END_DECLS

#endif  /* __XFCE_NOTIFY_TYPES_H__ */
