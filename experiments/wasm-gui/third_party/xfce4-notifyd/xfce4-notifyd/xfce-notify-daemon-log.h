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

#ifndef __XFCE_NOTIFY_DAEMON_LOG_H__
#define __XFCE_NOTIFY_DAEMON_LOG_H__

#include <glib-object.h>

#include <common/xfce-notify-log-gbus.h>

#include "xfce-notify-log.h"

G_DECLARE_FINAL_TYPE(XfceNotifyDaemonLog, xfce_notify_daemon_log, XFCE, NOTIFY_DAEMON_LOG, XfceNotifyLogGBusSkeleton)
#define XFCE_TYPE_NOTIFY_DAEMON_LOG (xfce_notify_daemon_log_get_type())

XfceNotifyDaemonLog *xfce_notify_daemon_log_new(GDBusConnection *bus,
                                                XfceNotifyLog *log,
                                                GError **error);

G_BEGIN_DECLS

G_END_DECLS

#endif  /* __XFCE_NOTIFY_DAEMON_LOG_H__ */
