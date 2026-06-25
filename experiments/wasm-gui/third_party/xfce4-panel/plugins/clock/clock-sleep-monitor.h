/*
 * Copyright (C) 2022 Christian Henz <chrhenz@gmx.de>
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
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#pragma once

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _ClockSleepMonitor ClockSleepMonitor;

GType clock_sleep_monitor_get_type (void) G_GNUC_CONST;

#define XFCE_TYPE_CLOCK_SLEEP_MONITOR (clock_sleep_monitor_get_type ())
#define XFCE_CLOCK_SLEEP_MONITOR(object) (G_TYPE_CHECK_INSTANCE_CAST((object), XFCE_TYPE_CLOCK_SLEEP_MONITOR, ClockSleepMonitor))
#define XFCE_IS_CLOCK_SLEEP_MONITOR(object) (G_TYPE_CHECK_INSTANCE_TYPE((object), XFCE_TYPE_CLOCK_SLEEP_MONITOR))

/* Factory function that tries to instantiate a sleep monitor. Returns
 * NULL if no implementation could be found or instantiated.
 *
 * The sleep monitor emits a signal `woke-up()` when it detects wakeup
 * from a sleep state.
 */
ClockSleepMonitor* clock_sleep_monitor_create (void);

G_END_DECLS
