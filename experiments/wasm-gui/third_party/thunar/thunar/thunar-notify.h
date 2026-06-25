/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2010 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __THUNAR_NOTIFY_H__
#define __THUNAR_NOTIFY_H__

#include <glib.h>
#include <thunar/thunar-device.h>
#include <thunar/thunar-job-operation.h>

G_BEGIN_DECLS

void     thunar_notify_unmount     (ThunarDevice *device);

void     thunar_notify_eject       (ThunarDevice *device);

void     thunar_notify_progress    (ThunarDevice *device, const gchar *message_to_show);

void     thunar_notify_finish      (ThunarDevice *device);

void     thunar_notify_undo        (ThunarJobOperation *operation);

void     thunar_notify_redo        (ThunarJobOperation *operation);

void     thunar_notify_uninit      (void);

G_END_DECLS

#endif /* !__THUNAR_NOTIFY_H__ */
