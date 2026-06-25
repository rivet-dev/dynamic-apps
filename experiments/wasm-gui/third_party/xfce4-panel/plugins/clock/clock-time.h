/*
 * Copyright (C) 2013      Andrzej <ndrwrdck@gmail.com>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __CLOCK_TIME_H__
#define __CLOCK_TIME_H__

#include <glib.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/libxfce4panel.h>

#include "clock-sleep-monitor.h"

G_BEGIN_DECLS

#define CLOCK_INTERVAL_SECOND (1)
#define CLOCK_INTERVAL_MINUTE (60)

typedef struct _ClockTime          ClockTime;
typedef struct _ClockTimeClass     ClockTimeClass;
typedef struct _ClockTimeTimeout   ClockTimeTimeout;

#define XFCE_TYPE_CLOCK_TIME              (clock_time_get_type ())
#define XFCE_CLOCK_TIME(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_CLOCK_TIME, ClockTime))
#define XFCE_CLOCK_TIME_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_CLOCK_TIME, ClockTimeClass))
#define XFCE_IS_CLOCK_TIME(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_CLOCK_TIME))
#define XFCE_IS_CLOCK_TIME_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_CLOCK_TIME))
#define XFCE_CLOCK_TIME_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_CLOCK_TIME, ClockTimeClass))



GType               clock_time_get_type               (void) G_GNUC_CONST;

void                clock_time_register_type          (XfcePanelTypeModule *type_module);

ClockTime          *clock_time_new                    (void);

ClockTimeTimeout   *clock_time_timeout_new            (guint                interval,
                                                       ClockTime           *time,
                                                       ClockSleepMonitor   *sleep_monitor,
                                                       GCallback            c_handler,
                                                       gpointer             gobject);

void                clock_time_timeout_set_interval   (ClockTimeTimeout    *timeout,
                                                       guint                interval);

void                clock_time_timeout_restart        (ClockTimeTimeout    *timeout);

void                clock_time_timeout_free           (ClockTimeTimeout    *timeout);

GDateTime          *clock_time_get_time               (ClockTime           *time);

gchar              *clock_time_strdup_strftime        (ClockTime           *time,
                                                       const gchar         *format);

guint               clock_time_interval_from_format   (const gchar         *format);

G_END_DECLS

#endif /* !__CLOCK_TIME_H__ */
