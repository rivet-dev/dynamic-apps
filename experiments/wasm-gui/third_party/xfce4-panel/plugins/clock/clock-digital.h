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

#ifndef __CLOCK_DIGITAL_H__
#define __CLOCK_DIGITAL_H__

G_BEGIN_DECLS

/* TRANSLATORS: adjust this accordingly for your locale format */
#define DEFAULT_DIGITAL_TIME_FORMAT NC_("Time", "%R")
#define DEFAULT_DIGITAL_DATE_FORMAT NC_("Date", "%Y-%m-%d")
#define DEFAULT_DIGITAL_FORMAT NC_("Time", "%a %_d %b, %R")

typedef enum
{

  CLOCK_PLUGIN_DIGITAL_FORMAT_DATE_TIME = 0,
  CLOCK_PLUGIN_DIGITAL_FORMAT_TIME_DATE,
  CLOCK_PLUGIN_DIGITAL_FORMAT_DATE,
  CLOCK_PLUGIN_DIGITAL_FORMAT_TIME,


  /* defines */
  CLOCK_PLUGIN_DIGITAL_FORMAT_MIN = CLOCK_PLUGIN_DIGITAL_FORMAT_DATE_TIME,
  CLOCK_PLUGIN_DIGITAL_FORMAT_MAX = CLOCK_PLUGIN_DIGITAL_FORMAT_TIME,
  CLOCK_PLUGIN_DIGITAL_FORMAT_DEFAULT = CLOCK_PLUGIN_DIGITAL_FORMAT_DATE_TIME,
}
ClockPluginDigitalFormat;

typedef struct _XfceClockDigitalClass XfceClockDigitalClass;
typedef struct _XfceClockDigital      XfceClockDigital;

#define XFCE_CLOCK_TYPE_DIGITAL            (xfce_clock_digital_get_type ())
#define XFCE_CLOCK_DIGITAL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_CLOCK_TYPE_DIGITAL, XfceClockDigital))
#define XFCE_CLOCK_DIGITAL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_CLOCK_TYPE_DIGITAL, XfceClockDigitalClass))
#define XFCE_CLOCK_IS_DIGITAL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_CLOCK_TYPE_DIGITAL))
#define XFCE_CLOCK_IS_DIGITAL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_CLOCK_TYPE_DIGITAL))
#define XFCE_CLOCK_DIGITAL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_CLOCK_TYPE_DIGITAL, XfceClockDigitalClass))

GType      xfce_clock_digital_get_type      (void) G_GNUC_CONST;

void       xfce_clock_digital_register_type (XfcePanelTypeModule *type_module);

GtkWidget *xfce_clock_digital_new           (ClockTime           *time,
                                             ClockSleepMonitor   *sleep_monitor) G_GNUC_MALLOC;

G_END_DECLS

#endif /* !__CLOCK_DIGITAL_H__ */
