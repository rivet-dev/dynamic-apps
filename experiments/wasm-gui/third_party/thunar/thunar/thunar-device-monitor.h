/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2012 Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __THUNAR_DEVICE_MONITOR_H__
#define __THUNAR_DEVICE_MONITOR_H__

#include <thunar/thunar-device.h>

G_BEGIN_DECLS

typedef struct _ThunarDeviceMonitorClass ThunarDeviceMonitorClass;
typedef struct _ThunarDeviceMonitor      ThunarDeviceMonitor;

#define THUNAR_TYPE_DEVICE_MONITOR             (thunar_device_monitor_get_type ())
#define THUNAR_DEVICE_MONITOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_DEVICE_MONITOR, ThunarDeviceMonitor))
#define THUNAR_DEVICE_MONITOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_DEVICE_MONITOR, ThunarDeviceMonitorClass))
#define THUNAR_IS_DEVICE_MONITOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_DEVICE_MONITOR))
#define THUNAR_IS_DEVICE_MONITOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((obj), THUNAR_TYPE_DEVICE_MONITOR))
#define THUNAR_DEVICE_MONITOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_DEVICE_MONITOR, ThunarDeviceMonitorClass))

GType                thunar_device_monitor_get_type    (void) G_GNUC_CONST;

ThunarDeviceMonitor *thunar_device_monitor_get         (void);

GList               *thunar_device_monitor_get_devices (ThunarDeviceMonitor *monitor);

void                 thunar_device_monitor_set_hidden  (ThunarDeviceMonitor *monitor,
                                                        ThunarDevice        *device,
                                                        gboolean             hidden);

G_END_DECLS

#endif /* !__THUNAR_DEVICE_MONITOR_H__ */
