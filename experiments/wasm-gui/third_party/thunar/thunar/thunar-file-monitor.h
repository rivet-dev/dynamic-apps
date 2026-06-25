/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_FILE_MONITOR_H__
#define __THUNAR_FILE_MONITOR_H__

#include <thunar/thunar-file.h>

G_BEGIN_DECLS;

typedef struct _ThunarFileMonitorClass ThunarFileMonitorClass;
typedef struct _ThunarFileMonitor      ThunarFileMonitor;

#define THUNAR_TYPE_FILE_MONITOR            (thunar_file_monitor_get_type ())
#define THUNAR_FILE_MONITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_FILE_MONITOR, ThunarFileMonitor))
#define THUNAR_FILE_MONITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_FILE_MONITOR, ThunarFileMonitorClass))
#define THUNAR_IS_FILE_MONITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_FILE_MONITOR))
#define THUNAR_IS_FILE_MONITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_FILE_MONITOR))
#define THUNAR_FILE_MONITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_FILE_MONITOR, ThunarFileMonitorClass))

GType              thunar_file_monitor_get_type       (void) G_GNUC_CONST;

ThunarFileMonitor *thunar_file_monitor_get_default    (void);

void               thunar_file_monitor_file_changed   (ThunarFile *file);
void               thunar_file_monitor_file_destroyed (ThunarFile *file);

G_END_DECLS;

#endif /* !__THUNAR_FILE_MONITOR_H__ */
