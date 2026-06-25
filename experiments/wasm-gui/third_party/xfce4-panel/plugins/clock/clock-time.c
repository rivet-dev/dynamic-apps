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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <common/panel-private.h>

#include "clock-time.h"
#include "clock.h"

static void                 clock_time_finalize       (GObject          *object);
static void                 clock_time_get_property   (GObject          *object,
                                                       guint             prop_id,
                                                       GValue           *value,
                                                       GParamSpec       *pspec);
static void                 clock_time_set_property   (GObject          *object,
                                                       guint             prop_id,
                                                       const GValue     *value,
                                                       GParamSpec       *pspec);



#define DEFAULT_TIMEZONE ""

enum
{
  PROP_0,
  PROP_TIMEZONE
};

struct _ClockTimeClass
{
  GObjectClass        __parent__;
};

struct _ClockTime
{
  GObject             __parent__;

  gchar              *timezone_name;
  GTimeZone          *timezone;
};

struct _ClockTimeTimeout
{
  guint       interval;
  guint       timeout_id;
  guint       timeout_counter;
  guint       restart : 1;
  ClockTime  *time;
  guint       time_changed_id;
  ClockSleepMonitor *sleep_monitor;
};

enum
{
  TIME_CHANGED,
  LAST_SIGNAL
};

static guint clock_time_signals[LAST_SIGNAL] = { 0, };


XFCE_PANEL_DEFINE_TYPE (ClockTime, clock_time, G_TYPE_OBJECT)



static void
clock_time_class_init (ClockTimeClass *klass)
{
  GObjectClass      *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = clock_time_finalize;
  gobject_class->get_property = clock_time_get_property;
  gobject_class->set_property = clock_time_set_property;

  g_object_class_install_property (gobject_class,
                                   PROP_TIMEZONE,
                                   g_param_spec_string ("timezone",
                                                        NULL, NULL,
                                                        DEFAULT_TIMEZONE,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  clock_time_signals[TIME_CHANGED] =
    g_signal_new (g_intern_static_string ("time-changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}



static void
clock_time_init (ClockTime *time)
{
  time->timezone_name = g_strdup (DEFAULT_TIMEZONE);
  time->timezone = NULL;
}



static void
clock_time_finalize (GObject *object)
{
  ClockTime *time = XFCE_CLOCK_TIME (object);

  g_free (time->timezone_name);

  if (time->timezone != NULL)
    g_time_zone_unref (time->timezone);

  G_OBJECT_CLASS (clock_time_parent_class)->finalize (object);
}



static void
clock_time_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  ClockTime *time = XFCE_CLOCK_TIME (object);

  switch (prop_id)
    {
    case PROP_TIMEZONE:
      g_value_set_string (value, time->timezone_name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
clock_time_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  ClockTime     *time = XFCE_CLOCK_TIME (object);
  const gchar   *str_value;

  switch (prop_id)
    {
    case PROP_TIMEZONE:
      str_value = g_value_get_string (value);
      if (g_strcmp0 (time->timezone_name, str_value) != 0)
        {
          g_free (time->timezone_name);
          if (time->timezone != NULL)
            g_time_zone_unref (time->timezone);
          if (str_value == NULL || g_strcmp0 (str_value, "") == 0)
            {
              time->timezone_name = g_strdup (DEFAULT_TIMEZONE);
              time->timezone = NULL;
            }
          else
            {
              time->timezone_name = g_strdup (str_value);
#if GLIB_CHECK_VERSION(2, 68, 0)
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
              time->timezone = g_time_zone_new_identifier (str_value);
              if (time->timezone == NULL)
                time->timezone = g_time_zone_new_utc ();
G_GNUC_END_IGNORE_DEPRECATIONS
#else
              time->timezone = g_time_zone_new (str_value);
#endif
            }

          g_signal_emit (G_OBJECT (time), clock_time_signals[TIME_CHANGED], 0);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



GDateTime *
clock_time_get_time (ClockTime *time)
{
  GDateTime *date_time;

  panel_return_val_if_fail (XFCE_IS_CLOCK_TIME (time), NULL);

  if (time->timezone != NULL)
    date_time = g_date_time_new_now (time->timezone);
  else
    date_time = g_date_time_new_now_local ();

  return date_time;
}



gchar *
clock_time_strdup_strftime (ClockTime       *time,
                            const gchar     *format)
{
  GDateTime *date_time;
  gchar     *str;

  panel_return_val_if_fail (XFCE_IS_CLOCK_TIME (time), NULL);

  date_time = clock_time_get_time (time);
  str = g_date_time_format (date_time, format);

  g_date_time_unref (date_time);

  /* Explicitely return NULL if a format specifier fails */
  if (!str ||
      g_strcmp0 (str, "") == 0)
    return NULL;
  else
    return str;
}



guint
clock_time_interval_from_format (const gchar *format)
{
  const gchar *p;

  if (G_UNLIKELY (panel_str_is_empty (format)))
      return CLOCK_INTERVAL_MINUTE;

  for (p = format; *p != '\0'; ++p)
    {
      if (p[0] == '%' && p[1] != '\0')
        {
          switch (*++p)
            {
            case 'c':
            case 'N':
            case 'r':
            case 's':
            case 'S':
            case 'T':
            case 'X':
              return CLOCK_INTERVAL_SECOND;
            }
        }
    }

  return CLOCK_INTERVAL_MINUTE;
}



static gboolean
clock_time_timeout_running (gpointer user_data)
{
  ClockTimeTimeout *timeout = user_data;
  GDateTime        *time;

  g_signal_emit (G_OBJECT (timeout->time), clock_time_signals[TIME_CHANGED], 0);

  /* check if the timeout still runs in time and sync again if necessary */
  if (timeout->interval == CLOCK_INTERVAL_MINUTE)
    {
      /* accurate to the second */
      time = clock_time_get_time (timeout->time);
      timeout->restart = (g_date_time_get_second (time) != 0);
      g_date_time_unref (time);
    }
  else if (++timeout->timeout_counter == 10)
    {
      /* accurate to the tenth of a second */
      time = clock_time_get_time (timeout->time);
      timeout->restart = (g_date_time_get_microsecond (time) / 100000 != 0);
      g_date_time_unref (time);
      timeout->timeout_counter = 0;
    }

  return !timeout->restart;
}



static void
clock_time_timeout_destroyed (gpointer user_data)
{
  ClockTimeTimeout *timeout = user_data;

  timeout->timeout_id = 0;

  if (G_UNLIKELY (timeout->restart))
    clock_time_timeout_set_interval (timeout, timeout->interval);
}



static gboolean
clock_time_timeout_sync (gpointer user_data)
{
  ClockTimeTimeout *timeout = user_data;

  g_signal_emit (G_OBJECT (timeout->time), clock_time_signals[TIME_CHANGED], 0);

  /* start the real timeout */
  timeout->timeout_id = g_timeout_add_full (G_PRIORITY_HIGH, timeout->interval * 1000,
                                            clock_time_timeout_running, timeout,
                                            clock_time_timeout_destroyed);

  /* stop the sync timeout */
  return FALSE;
}



ClockTimeTimeout *
clock_time_timeout_new (guint       interval,
                        ClockTime  *time,
                        ClockSleepMonitor *sleep_monitor,
                        GCallback   c_handler,
                        gpointer    gobject)
{
  ClockTimeTimeout *timeout;

  panel_return_val_if_fail (XFCE_IS_CLOCK_TIME (time), NULL);
  panel_return_val_if_fail (sleep_monitor == NULL || XFCE_IS_CLOCK_SLEEP_MONITOR (sleep_monitor), NULL);

  panel_return_val_if_fail (interval > 0, NULL);

  timeout = g_slice_new0 (ClockTimeTimeout);
  timeout->interval = 0;
  timeout->timeout_id = 0;
  timeout->restart = FALSE;
  timeout->time = time;

  timeout->time_changed_id =
    g_signal_connect_swapped (G_OBJECT (time), "time-changed",
                              c_handler, gobject);

  g_object_ref (G_OBJECT (timeout->time));

  if (sleep_monitor != NULL)
    {
      timeout->sleep_monitor = sleep_monitor;
      g_signal_connect_swapped (G_OBJECT (sleep_monitor), "woke-up",
                                G_CALLBACK (clock_time_timeout_restart), timeout);
      g_object_ref (G_OBJECT (sleep_monitor));
    }

  clock_time_timeout_set_interval (timeout, interval);

  return timeout;
}



void
clock_time_timeout_set_interval (ClockTimeTimeout *timeout,
                                 guint             interval)
{
  GDateTime *time;
  guint      next_interval;
  gboolean   restart;

  panel_return_if_fail (timeout != NULL);
  panel_return_if_fail (interval > 0);

  restart = timeout->restart;

  /* leave if nothing changed and we're not restarting */
  if (!restart && timeout->interval == interval)
    return;
  timeout->interval = interval;
  timeout->restart = FALSE;

  /* stop running timeout */
  if (G_LIKELY (timeout->timeout_id != 0))
    g_source_remove (timeout->timeout_id);
  timeout->timeout_id = 0;
  timeout->timeout_counter = 0;

  /* run function when not restarting */
  if (!restart)
    g_signal_emit (G_OBJECT (timeout->time), clock_time_signals[TIME_CHANGED], 0);

  time = clock_time_get_time (timeout->time);
  if (interval == CLOCK_INTERVAL_MINUTE)
    {
      /* get the seconds to the next minute */
      next_interval = (60 - g_date_time_get_second (time)) * 1000;
    }
  else
    {
      /* get the milliseconds to the next second */
      next_interval = 1000 - g_date_time_get_microsecond (time) / 1000;
    }
  g_date_time_unref (time);

  if (next_interval > 0)
    {
      /* start the sync timeout */
      timeout->timeout_id = g_timeout_add_full (G_PRIORITY_HIGH, next_interval,
                                                clock_time_timeout_sync, timeout, NULL);
    }
  else
    {
      /* directly start running the normal timeout */
      timeout->timeout_id = g_timeout_add_full (G_PRIORITY_HIGH, interval * 1000,
                                                clock_time_timeout_running, timeout,
                                                clock_time_timeout_destroyed);
    }
}



void
clock_time_timeout_restart (ClockTimeTimeout *timeout)
{
  panel_return_if_fail (timeout != NULL);

  g_signal_emit (G_OBJECT (timeout->time), clock_time_signals[TIME_CHANGED], 0);

  timeout->restart = 1;
  clock_time_timeout_set_interval (timeout, timeout->interval);
}



void
clock_time_timeout_free (ClockTimeTimeout *timeout)
{
  panel_return_if_fail (timeout != NULL);

  timeout->restart = FALSE;

  if (timeout->time != NULL && timeout->time_changed_id != 0)
    g_signal_handler_disconnect (timeout->time, timeout->time_changed_id);

  g_object_unref (G_OBJECT (timeout->time));

  if (timeout->sleep_monitor != NULL)
    {
      g_signal_handlers_disconnect_by_data (timeout->sleep_monitor, timeout);
      g_object_unref (G_OBJECT (timeout->sleep_monitor));
    }

  if (G_LIKELY (timeout->timeout_id != 0))
    g_source_remove (timeout->timeout_id);

  g_slice_free (ClockTimeTimeout, timeout);
}



ClockTime *
clock_time_new (void)
{
  return g_object_new (XFCE_TYPE_CLOCK_TIME, NULL);
}
