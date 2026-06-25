/*
 * Copyright (C) 2007-2010 Nick Schermer <nick@xfce.org>
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

#ifndef __CLOCK_ANALOG_H__
#define __CLOCK_ANALOG_H__

G_BEGIN_DECLS

typedef struct _XfceClockAnalogClass XfceClockAnalogClass;
typedef struct _XfceClockAnalog      XfceClockAnalog;

#define XFCE_CLOCK_TYPE_ANALOG            (xfce_clock_analog_get_type ())
#define XFCE_CLOCK_ANALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_CLOCK_TYPE_ANALOG, XfceClockAnalog))
#define XFCE_CLOCK_ANALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_CLOCK_TYPE_ANALOG, XfceClockAnalogClass))
#define XFCE_CLOCK_IS_ANALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_CLOCK_TYPE_ANALOG))
#define XFCE_CLOCK_IS_ANALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_CLOCK_TYPE_ANALOG))
#define XFCE_CLOCK_ANALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_CLOCK_TYPE_ANALOG, XfceClockAnalogClass))

GType      xfce_clock_analog_get_type      (void) G_GNUC_CONST;

void       xfce_clock_analog_register_type (XfcePanelTypeModule *type_module);

GtkWidget *xfce_clock_analog_new           (ClockTime           *time,
                                            ClockSleepMonitor   *sleep_monitor) G_GNUC_MALLOC;

G_END_DECLS

#endif /* !__CLOCK_ANALOG_H__ */
