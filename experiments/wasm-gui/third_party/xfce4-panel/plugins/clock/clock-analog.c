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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include <gtk/gtk.h>
#include <cairo/cairo.h>
#include <common/panel-private.h>

#include "clock.h"
#include "clock-time.h"
#include "clock-analog.h"

#define CLOCK_SCALE 0.08
#define TICKS_TO_RADIANS(x)   (G_PI - (G_PI / 30.0) * (x))
#define HOURS_TO_RADIANS(x,y) (G_PI - (G_PI / 6.0) * (((x) > 12 ? (x) - 12 : (x)) + (y) / 60.0))



static void      xfce_clock_analog_set_property  (GObject              *object,
                                                  guint                 prop_id,
                                                  const GValue         *value,
                                                  GParamSpec           *pspec);
static void      xfce_clock_analog_get_property  (GObject              *object,
                                                  guint                 prop_id,
                                                  GValue               *value,
                                                  GParamSpec           *pspec);
static void      xfce_clock_analog_finalize      (GObject              *object);
static gboolean  xfce_clock_analog_draw          (GtkWidget            *widget,
                                                  cairo_t              *cr);
static void      xfce_clock_analog_draw_ticks    (cairo_t              *cr,
                                                  gdouble               xc,
                                                  gdouble               yc,
                                                  gdouble               radius);
static void      xfce_clock_analog_draw_pointer  (cairo_t              *cr,
                                                  gdouble               xc,
                                                  gdouble               yc,
                                                  gdouble               radius,
                                                  gdouble               angle,
                                                  gdouble               scale,
                                                  gboolean              line);
static gboolean  xfce_clock_analog_update        (XfceClockAnalog      *analog,
                                                  ClockTime            *time);




enum
{
  PROP_0,
  PROP_SHOW_SECONDS,
  PROP_SIZE_RATIO,
  PROP_ORIENTATION
};

struct _XfceClockAnalogClass
{
  GtkImageClass __parent__;
};

struct _XfceClockAnalog
{
  GtkImage __parent__;

  ClockTimeTimeout   *timeout;

  guint               show_seconds : 1;
  ClockTime          *time;
};



XFCE_PANEL_DEFINE_TYPE (XfceClockAnalog, xfce_clock_analog, GTK_TYPE_IMAGE)



static void
xfce_clock_analog_class_init (XfceClockAnalogClass *klass)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *gtkwidget_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->set_property = xfce_clock_analog_set_property;
  gobject_class->get_property = xfce_clock_analog_get_property;
  gobject_class->finalize = xfce_clock_analog_finalize;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->draw = xfce_clock_analog_draw;

  g_object_class_install_property (gobject_class,
                                   PROP_SIZE_RATIO,
                                   g_param_spec_double ("size-ratio", NULL, NULL,
                                                        -1, G_MAXDOUBLE, 1.0,
                                                        G_PARAM_READABLE
                                                        | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_ORIENTATION,
                                   g_param_spec_enum ("orientation", NULL, NULL,
                                                      GTK_TYPE_ORIENTATION,
                                                      GTK_ORIENTATION_HORIZONTAL,
                                                      G_PARAM_WRITABLE
                                                      | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_SECONDS,
                                   g_param_spec_boolean ("show-seconds", NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE
                                                         | G_PARAM_STATIC_STRINGS));
}



static void
xfce_clock_analog_init (XfceClockAnalog *analog)
{
  analog->show_seconds = FALSE;
}



static void
xfce_clock_analog_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  XfceClockAnalog *analog = XFCE_CLOCK_ANALOG (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      break;

    case PROP_SHOW_SECONDS:
      analog->show_seconds = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

  /* reschedule the timeout and redraw */
  clock_time_timeout_set_interval (analog->timeout,
      analog->show_seconds ? CLOCK_INTERVAL_SECOND : CLOCK_INTERVAL_MINUTE);
  xfce_clock_analog_update (analog, analog->time);
}



static void
xfce_clock_analog_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  XfceClockAnalog *analog = XFCE_CLOCK_ANALOG (object);

  switch (prop_id)
    {
    case PROP_SHOW_SECONDS:
      g_value_set_boolean (value, analog->show_seconds);
      break;

    case PROP_SIZE_RATIO:
      g_value_set_double (value, 1.0);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
xfce_clock_analog_finalize (GObject *object)
{
  /* stop the timeout */
  clock_time_timeout_free (XFCE_CLOCK_ANALOG (object)->timeout);

  (*G_OBJECT_CLASS (xfce_clock_analog_parent_class)->finalize) (object);
}



static gboolean
xfce_clock_analog_draw (GtkWidget *widget,
                        cairo_t   *cr)
{
  XfceClockAnalog *analog = XFCE_CLOCK_ANALOG (widget);
  gdouble          xc, yc;
  gdouble          angle, radius;
  GDateTime       *time;
  GtkAllocation    allocation;
  GtkStyleContext *ctx;
  GdkRGBA          fg_rgba;

  panel_return_val_if_fail (XFCE_CLOCK_IS_ANALOG (analog), FALSE);
  panel_return_val_if_fail (cr != NULL, FALSE);

  /* get center of the widget and the radius */
  gtk_widget_get_allocation (widget, &allocation);
  xc = (allocation.width / 2.0);
  yc = (allocation.height / 2.0);
  radius = MIN (xc, yc);

  /* get the local time */
  time = clock_time_get_time (analog->time);

  /* set the line properties */
  cairo_set_line_width (cr, 1);
  ctx = gtk_widget_get_style_context (widget);
  gtk_style_context_get_color (ctx, gtk_widget_get_state_flags (widget), &fg_rgba);
  gdk_cairo_set_source_rgba (cr, &fg_rgba);

  /* draw the ticks */
  xfce_clock_analog_draw_ticks (cr, xc, yc, radius);

  if (analog->show_seconds)
    {
      /* second pointer */
      angle = TICKS_TO_RADIANS (g_date_time_get_second (time));
      xfce_clock_analog_draw_pointer (cr, xc, yc, radius, angle, 0.7, TRUE);
    }

  /* minute pointer */
  angle = TICKS_TO_RADIANS (g_date_time_get_minute (time) + g_date_time_get_second (time) / 60.0);
  xfce_clock_analog_draw_pointer (cr, xc, yc, radius, angle, 0.8, FALSE);

  /* hour pointer */
  angle = HOURS_TO_RADIANS (g_date_time_get_hour (time), g_date_time_get_minute (time));
  xfce_clock_analog_draw_pointer (cr, xc, yc, radius, angle, 0.5, FALSE);

  /* cleanup */
  g_date_time_unref (time);

  return FALSE;
}



static void
xfce_clock_analog_draw_ticks (cairo_t *cr,
                              gdouble  xc,
                              gdouble  yc,
                              gdouble  radius)
{
  gint    i;
  gdouble x, y, angle;

  for (i = 0; i < 12; i++)
    {
      /* calculate */
      angle = HOURS_TO_RADIANS (i, 0);
      x = xc + sin (angle) * (radius * (1.0 - CLOCK_SCALE));
      y = yc + cos (angle) * (radius * (1.0 - CLOCK_SCALE));

      if (i == 0)
        {
          /* draw triangle */
          cairo_move_to (cr, x + radius * CLOCK_SCALE * 1.2, y - radius * CLOCK_SCALE);
          cairo_line_to (cr, x, y + radius * CLOCK_SCALE * 3.0);
          cairo_line_to (cr, x - radius * CLOCK_SCALE * 1.2, y - radius * CLOCK_SCALE);
          cairo_close_path (cr);
        }
      else if (i % 3 == 0)
        {
          /* draw rectangle */
          x = x + cos (angle) * radius * CLOCK_SCALE * 0.6 + sin (angle) * radius * CLOCK_SCALE;
          y = y + sin (angle) * radius * CLOCK_SCALE * 0.6 + cos (angle) * radius * CLOCK_SCALE;
          cairo_move_to (cr, x, y);
          x = x - sin (angle) * radius * CLOCK_SCALE * 3.0;
          y = y - cos (angle) * radius * CLOCK_SCALE * 3.0;
          cairo_line_to (cr, x, y);
          x = x - cos (angle) * radius * CLOCK_SCALE * 0.6 * 2;
          y = y - sin (angle) * radius * CLOCK_SCALE * 0.6 * 2;
          cairo_line_to (cr, x, y);
          x = x + sin (angle) * radius * CLOCK_SCALE * 3.0;
          y = y + cos (angle) * radius * CLOCK_SCALE * 3.0;
          cairo_line_to (cr, x, y);
          cairo_close_path (cr);
        }
      else
        {
          /* draw arc */
          cairo_move_to (cr, x, y);
          cairo_arc (cr, x, y, radius * CLOCK_SCALE, 0, 2 * G_PI);
          cairo_close_path (cr);
        }
    }

  /* fill the arcs */
  cairo_fill (cr);
}



static void
xfce_clock_analog_draw_pointer (cairo_t *cr,
                                gdouble  xc,
                                gdouble  yc,
                                gdouble  radius,
                                gdouble  angle,
                                gdouble  scale,
                                gboolean line)
{
  gdouble xs, ys;
  gdouble xt, yt;

  /* calculate tip position */
  xt = xc + sin (angle) * radius * scale;
  yt = yc + cos (angle) * radius * scale;

  if (line)
    {
      /* draw the line */
      cairo_move_to (cr, xc, yc);
      cairo_line_to (cr, xt, yt);

      /* draw the line */
      cairo_stroke (cr);
    }
  else
    {
      /* calculate start position */
      xs = xc + sin (angle - 0.5 * G_PI) * radius * CLOCK_SCALE;
      ys = yc + cos (angle - 0.5 * G_PI) * radius * CLOCK_SCALE;

      /* draw the pointer */
      cairo_move_to (cr, xs, ys);
      cairo_arc (cr, xc, yc, radius * CLOCK_SCALE, -angle + G_PI, -angle);
      cairo_line_to (cr, xt, yt);
      cairo_close_path (cr);

      /* fill the pointer */
      cairo_fill (cr);
    }
}



static gboolean
xfce_clock_analog_update (XfceClockAnalog *analog,
                          ClockTime       *time)
{
  GtkWidget *widget = GTK_WIDGET (analog);

  panel_return_val_if_fail (XFCE_CLOCK_IS_ANALOG (analog), FALSE);
  panel_return_val_if_fail (XFCE_IS_CLOCK_TIME (time), FALSE);

  /* update if the widget if visible */
  if (G_LIKELY (gtk_widget_get_visible (widget)))
    gtk_widget_queue_draw (widget);

  return TRUE;
}



GtkWidget *
xfce_clock_analog_new (ClockTime *time,
                       ClockSleepMonitor *sleep_monitor)
{
  XfceClockAnalog *analog = g_object_new (XFCE_CLOCK_TYPE_ANALOG, NULL);

  analog->time = time;
  analog->timeout = clock_time_timeout_new (CLOCK_INTERVAL_MINUTE,
                                            analog->time,
                                            sleep_monitor,
                                            G_CALLBACK (xfce_clock_analog_update), analog);

  return GTK_WIDGET (analog);
}
