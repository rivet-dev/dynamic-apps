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
#include "clock-lcd.h"

#define RELATIVE_SPACE (0.10)
#define RELATIVE_DIGIT (5 * RELATIVE_SPACE)
#define RELATIVE_DOTS  (3 * RELATIVE_SPACE)



static void      xfce_clock_lcd_set_property (GObject           *object,
                                              guint              prop_id,
                                              const GValue      *value,
                                              GParamSpec        *pspec);
static void      xfce_clock_lcd_get_property (GObject           *object,
                                              guint              prop_id,
                                              GValue            *value,
                                              GParamSpec        *pspec);
static void      xfce_clock_lcd_finalize     (GObject           *object);
static gboolean  xfce_clock_lcd_draw         (GtkWidget         *widget,
                                              cairo_t           *cr);
static gdouble   xfce_clock_lcd_get_ratio    (XfceClockLcd      *lcd);
static gdouble   xfce_clock_lcd_draw_dots    (cairo_t           *cr,
                                              gdouble            size,
                                              gdouble            offset_x,
                                              gdouble            offset_y);
static gdouble   xfce_clock_lcd_draw_digit   (cairo_t           *cr,
                                              guint              number,
                                              gdouble            size,
                                              gdouble            offset_x,
                                              gdouble            offset_y);
static gboolean  xfce_clock_lcd_update       (XfceClockLcd      *lcd,
                                              ClockTime         *time);




enum
{
  PROP_0,
  PROP_SHOW_SECONDS,
  PROP_SHOW_MILITARY,
  PROP_SHOW_MERIDIEM,
  PROP_FLASH_SEPARATORS,
  PROP_SIZE_RATIO,
  PROP_ORIENTATION
};

struct _XfceClockLcdClass
{
  GtkImageClass __parent__;
};

struct _XfceClockLcd
{
  GtkImage __parent__;

  ClockTimeTimeout   *timeout;

  guint               show_seconds : 1;
  guint               show_military : 1; /* 24-hour clock */
  guint               show_meridiem : 1; /* am/pm */
  guint               flash_separators : 1;

  ClockTime          *time;
};

typedef struct
{
  gdouble x;
  gdouble y;
}
LcdPoint;



XFCE_PANEL_DEFINE_TYPE (XfceClockLcd, xfce_clock_lcd, GTK_TYPE_IMAGE)



static void
xfce_clock_lcd_class_init (XfceClockLcdClass *klass)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *gtkwidget_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->set_property = xfce_clock_lcd_set_property;
  gobject_class->get_property = xfce_clock_lcd_get_property;
  gobject_class->finalize = xfce_clock_lcd_finalize;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->draw = xfce_clock_lcd_draw;

  g_object_class_install_property (gobject_class,
                                   PROP_SIZE_RATIO,
                                   g_param_spec_double ("size-ratio", NULL, NULL,
                                                        -1, G_MAXDOUBLE, -1.0,
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

  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_MILITARY,
                                   g_param_spec_boolean ("show-military", NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE
                                                         | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_MERIDIEM,
                                   g_param_spec_boolean ("show-meridiem", NULL, NULL,
                                                         TRUE,
                                                         G_PARAM_READWRITE
                                                         | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_FLASH_SEPARATORS,
                                   g_param_spec_boolean ("flash-separators", NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE
                                                         | G_PARAM_STATIC_STRINGS));
}



static void
xfce_clock_lcd_init (XfceClockLcd *lcd)
{
  lcd->show_seconds = FALSE;
  lcd->show_meridiem = FALSE;
  lcd->show_military = TRUE;
  lcd->flash_separators = FALSE;
}



static void
xfce_clock_lcd_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  XfceClockLcd *lcd = XFCE_CLOCK_LCD (object);
  gboolean      show_seconds;

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      break;

    case PROP_SHOW_SECONDS:
      lcd->show_seconds = g_value_get_boolean (value);
      break;

    case PROP_SHOW_MILITARY:
      lcd->show_military = g_value_get_boolean (value);
      break;

    case PROP_SHOW_MERIDIEM:
      lcd->show_meridiem = g_value_get_boolean (value);
      break;

    case PROP_FLASH_SEPARATORS:
      lcd->flash_separators = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

  g_object_notify (object, "size-ratio");

  /* reschedule the timeout and resize */
  show_seconds = lcd->show_seconds || lcd->flash_separators;
  clock_time_timeout_set_interval (lcd->timeout,
      show_seconds ? CLOCK_INTERVAL_SECOND : CLOCK_INTERVAL_MINUTE);
  gtk_widget_queue_resize (GTK_WIDGET (lcd));
}



static void
xfce_clock_lcd_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  XfceClockLcd *lcd = XFCE_CLOCK_LCD (object);
  gdouble       ratio;

  switch (prop_id)
    {
    case PROP_SHOW_SECONDS:
      g_value_set_boolean (value, lcd->show_seconds);
      break;

    case PROP_SHOW_MILITARY:
      g_value_set_boolean (value, lcd->show_military);
      break;

    case PROP_SHOW_MERIDIEM:
      g_value_set_boolean (value, lcd->show_meridiem);
      break;

    case PROP_FLASH_SEPARATORS:
      g_value_set_boolean (value, lcd->flash_separators);
      break;

    case PROP_SIZE_RATIO:
      ratio = xfce_clock_lcd_get_ratio (lcd);
      g_value_set_double (value, ratio);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
xfce_clock_lcd_finalize (GObject *object)
{
  /* stop the timeout */
  clock_time_timeout_free (XFCE_CLOCK_LCD (object)->timeout);

  (*G_OBJECT_CLASS (xfce_clock_lcd_parent_class)->finalize) (object);
}



static gboolean
xfce_clock_lcd_draw (GtkWidget *widget,
                     cairo_t   *cr)
{
  XfceClockLcd *lcd = XFCE_CLOCK_LCD (widget);
  gdouble       offset_x, offset_y;
  gint          ticks, i;
  gdouble       size;
  gdouble       ratio;
  GDateTime    *time;
  GtkAllocation allocation;
  GtkStyleContext *ctx;
  GdkRGBA          fg_rgba;

  panel_return_val_if_fail (XFCE_CLOCK_IS_LCD (lcd), FALSE);
  panel_return_val_if_fail (cr != NULL, FALSE);

  /* get the width:height ratio */
  ratio = xfce_clock_lcd_get_ratio (XFCE_CLOCK_LCD (widget));

  /* make sure we also fit on small vertical panels */
  gtk_widget_get_allocation (widget, &allocation);
  size = MIN ((gdouble) allocation.width / ratio, allocation.height);

  /* set correct color */
  ctx = gtk_widget_get_style_context (widget);
  gtk_style_context_get_color (ctx, gtk_widget_get_state_flags (widget), &fg_rgba);
  gdk_cairo_set_source_rgba (cr, &fg_rgba);

  /* begin offsets */
  offset_x = rint ((allocation.width - (size * ratio)) / 2.00);
  offset_y = rint ((allocation.height - size) / 2.00);

  /* only allow positive values from the base point */
  offset_x = MAX (0.00, offset_x);
  offset_y = MAX (0.00, offset_y);

  cairo_push_group (cr);

  /* width of the clear line */
  cairo_set_line_width (cr, MAX (size * 0.05, 1.5));

  /* get the local time */
  time = clock_time_get_time (lcd->time);

  /* draw the hours */
  ticks = g_date_time_get_hour (time);

  /* convert 24h clock to 12h clock */
  if (!lcd->show_military && ticks > 12)
    ticks -= 12;

  if (ticks == 1 || (ticks >= 10 && ticks < 20))
    offset_x -= size * (RELATIVE_SPACE * 4);

  /* queue a resize when the number of hour digits changed,
   * because we might miss the exact second (due to slightly delayed
   * timeout) we queue a resize the first 3 seconds or anything in
   * the first minute */
  if ((ticks == 10 || ticks == 0) && g_date_time_get_minute (time) == 0
      && (!lcd->show_seconds || g_date_time_get_second (time) < 3))
    g_object_notify (G_OBJECT (lcd), "size-ratio");

  if (ticks >= 10)
    {
      /* draw the number and increase the offset */
      offset_x = xfce_clock_lcd_draw_digit (cr, ticks >= 20 ? 2 : 1, size, offset_x, offset_y);
    }

  /* draw the other number of the hour and increase the offset */
  offset_x = xfce_clock_lcd_draw_digit (cr, ticks % 10, size, offset_x, offset_y);

  for (i = 0; i < 2; i++)
    {
      /* get the time */
      if (i == 0)
        {
          /* get the minutes */
          ticks = g_date_time_get_minute (time);
        }
      else
        {
          /* leave when we don't want seconds */
          if (!lcd->show_seconds)
            break;

          /* get the seconds */
          ticks = g_date_time_get_second (time);
        }

      /* draw the dots */
      if (lcd->flash_separators && (g_date_time_get_second (time) % 2) == 1)
        offset_x += size * RELATIVE_SPACE * 2;
      else
        offset_x = xfce_clock_lcd_draw_dots (cr, size, offset_x, offset_y);

      /* draw the first digit */
      offset_x = xfce_clock_lcd_draw_digit (cr, (ticks - (ticks % 10)) / 10, size, offset_x, offset_y);

      /* draw the second digit */
      offset_x = xfce_clock_lcd_draw_digit (cr, ticks % 10, size, offset_x, offset_y);
    }

  if (lcd->show_meridiem)
    {
      /* am or pm? */
      ticks = g_date_time_get_hour (time) >= 12 ? 11 : 10;

      /* draw the digit */
      xfce_clock_lcd_draw_digit (cr, ticks, size, offset_x, offset_y);
    }

  /* drop the pushed group */
  g_date_time_unref (time);
  cairo_pop_group_to_source (cr);
  cairo_paint (cr);

  return FALSE;
}



static gdouble
xfce_clock_lcd_get_ratio (XfceClockLcd *lcd)
{
  gdouble    ratio;
  gint       ticks;
  GDateTime *time;

  /* get the local time */
  time = clock_time_get_time (lcd->time);

  /* 8:8(space)8 */
  ratio = (3 * RELATIVE_DIGIT) + RELATIVE_DOTS + RELATIVE_SPACE;

  ticks = g_date_time_get_hour (time);
  g_date_time_unref (time);

  if (!lcd->show_military && ticks > 12)
    ticks -= 12;

  if (ticks == 1)
    ratio -= RELATIVE_SPACE * 4; /* only show 1 */
  else if (ticks >= 10 && ticks < 20)
    ratio += RELATIVE_SPACE * 2; /* 1 + space */
  else if (ticks >= 20)
    ratio += RELATIVE_DIGIT + RELATIVE_SPACE;

  /* (space):88 */
  if (lcd->show_seconds)
    ratio += (2 * RELATIVE_DIGIT) + RELATIVE_SPACE + RELATIVE_DOTS;

  if (lcd->show_meridiem)
    ratio += RELATIVE_DIGIT + RELATIVE_SPACE;

  return ratio;
}



static gdouble
xfce_clock_lcd_draw_dots (cairo_t *cr,
                          gdouble  size,
                          gdouble  offset_x,
                          gdouble  offset_y)
{
  gint i;

  if (size >= 10)
    {
      /* draw the dots (with rounding) */
      for (i = 1; i < 3; i++)
        cairo_rectangle (cr, rint (offset_x), rint (offset_y + size * RELATIVE_DOTS * i),
                         rint (size * RELATIVE_SPACE), rint (size * RELATIVE_SPACE));
    }
  else
    {
      /* draw the dots */
      for (i = 1; i < 3; i++)
        cairo_rectangle (cr, offset_x, offset_y + size * RELATIVE_DOTS * i,
                         size * RELATIVE_SPACE, size * RELATIVE_SPACE);
    }

  /* fill the dots */
  cairo_fill (cr);

  return (offset_x + size * RELATIVE_SPACE * 2);
}



/*
 * number:
 *
 * ##1##
 * 6   2
 * ##7##
 * 5   3
 * ##4##
 */
static gdouble
xfce_clock_lcd_draw_digit (cairo_t *cr,
                           guint    number,
                           gdouble  size,
                           gdouble  offset_x,
                           gdouble  offset_y)
{
  guint   i, j;
  gint    segment;
  gdouble x, y;
  gdouble rel_x, rel_y;

  /* coordicates to draw for each segment */
  const LcdPoint segment_points[][6] = {
    /* 1 */ { { 0, 0 }, { 0.5, 0 }, { 0.4, 0.1 }, { 0.1, 0.1 }, { -1, }, { -1, } },
    /* 2 */ { { 0.4, 0.1 }, { 0.5, 0.0 }, { 0.5, 0.5 }, { 0.4, 0.45 }, { -1, },  { -1, } },
    /* 3 */ { { 0.4, 0.55 }, { 0.5, 0.5 }, { 0.5, 1 }, { 0.4, 0.9 }, { -1, },  { -1, } },
    /* 4 */ { { 0.1, 0.9 }, { 0.4, 0.9 }, { 0.5, 1 }, { 0.0, 1 }, { -1, },  { -1, } },
    /* 5 */ { { 0.0, 0.5 }, { 0.1, 0.55 }, { 0.1, 0.90 }, { 0.0, 1}, { -1, },  { -1, } },
    /* 6 */ { { 0.0, 0.0 }, { 0.1, 0.1 }, { 0.1, 0.45 }, { 0.0, 0.5 }, { -1, },  { -1, } },
    /* 7 */ { { 0.0, 0.5 }, { 0.1, 0.45 }, { 0.4, 0.45 }, { 0.5, 0.5 }, { 0.4, 0.55 }, { 0.1, 0.55 } },
  };

  /* space line, mirrored to other side */
  const LcdPoint clear_points[] = {
    { 0, 0 }, { 0.25, 0.25 }, { 0.25, 0.375 }, { 0, 0.5 },
    { 0.25, 0.625 }, { 0.25, 0.75 }, { 0, 1 }
  };

  /* segment to draw for each number: 0, 1, ..., 9, A, P */
  const gint numbers[][8] = {
    { 0, 1, 2, 3, 4, 5, -1 },
    { 1, 2, -1 },
    { 0, 1, 6, 4, 3, -1 },
    { 0, 1, 6, 2, 3, -1 },
    { 5, 6, 1, 2, -1 },
    { 0, 5, 6, 2, 3, -1 },
    { 0, 5, 4, 3, 2, 6, -1 },
    { 0, 1, 2, -1 },
    { 0, 1, 2, 3, 4, 5, 6, -1 },
    { 3, 2, 1, 0, 5, 6, -1 },
    { 4, 5, 0, 1, 2, 6, -1 },
    { 4, 5, 0, 1, 6, -1 }
  };

  panel_return_val_if_fail (number <= 11, offset_x);

  for (i = 0; i < 9; i++)
    {
      /* get the segment we're going to draw */
      segment = numbers[number][i];

      /* leave when there are no more segments left */
      if (segment == -1)
        break;

      /* walk through the coordinate points */
      for (j = 0; j < 6; j++)
        {
          /* get the relative sizes */
          rel_x = segment_points[segment][j].x;
          rel_y = segment_points[segment][j].y;

          /* leave when there are no valid coordinates */
          if (rel_x == -1.00 || rel_y == -1.00)
            break;

          /* get x and y coordinates for this point */
          x = rel_x * size + offset_x;
          y = rel_y * size + offset_y;

          if (G_UNLIKELY (j == 0))
            cairo_move_to (cr, x, y);
          else
            cairo_line_to (cr, x, y);
        }

      cairo_close_path (cr);
    }

  cairo_fill (cr);

  /* clear the space between the segments to get the lcd look */
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  for (j = 0; j < 2; j++)
    {
      for (i = 0; i < G_N_ELEMENTS (clear_points); i++)
        {
          x = (j == 0 ? clear_points[i].x : 0.5 - clear_points[i].x) * size + offset_x;
          y = clear_points[i].y * size + offset_y;

          if (G_UNLIKELY (i == 0))
            cairo_move_to (cr, x, y);
          else
            cairo_line_to (cr, x, y);
        }

      cairo_stroke (cr);
    }
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

  return (offset_x + size * (RELATIVE_DIGIT + RELATIVE_SPACE));
}



static gboolean
xfce_clock_lcd_update (XfceClockLcd *lcd,
                       ClockTime    *time)
{
  GtkWidget *widget = GTK_WIDGET (lcd);

  panel_return_val_if_fail (XFCE_CLOCK_IS_LCD (lcd), FALSE);

  /* update if the widget if visible */
  if (G_LIKELY (gtk_widget_get_visible (widget)))
    gtk_widget_queue_draw (widget);

  return TRUE;
}



GtkWidget *
xfce_clock_lcd_new (ClockTime *time,
                    ClockSleepMonitor *sleep_monitor)
{
  XfceClockLcd *lcd = g_object_new (XFCE_CLOCK_TYPE_LCD, NULL);

  lcd->time = time;
  lcd->timeout = clock_time_timeout_new (CLOCK_INTERVAL_MINUTE,
                                         lcd->time,
                                         sleep_monitor,
                                         G_CALLBACK (xfce_clock_lcd_update), lcd);

  return GTK_WIDGET (lcd);
}
