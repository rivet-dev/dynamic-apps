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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <unistd.h>

#include <gio/gio.h>

#include <libxfce4util/libxfce4util.h>
#include <common/panel-debug.h>

#include "clock-sleep-monitor.h"



/* Configuration:
 *
 * - Compile time checks for runtime detection mechanisms.
 *   Currently only D-BUS is used, which is implied.
 *
 * - Compile time checks for available systems.
 *   Currently none, but could check for libraries, target OS.
 */

#define SLEEP_MONITOR_USE_LOGIND 1
#define SLEEP_MONITOR_USE_CONSOLEKIT 1


#define LOGIND_RUNNING() (access ("/run/systemd/seats/", F_OK) >= 0)

/* Base class
 *
 * Provides the signal `woke-up()` that is used to notify the timeouts
 * of wakeup events.
 */

static guint clock_sleep_monitor_woke_up_signal = 0;

struct _ClockSleepMonitor
{
  GObject parent_instance;
};

struct _ClockSleepMonitorClass
{
  GObjectClass parent_class;
};

typedef struct _ClockSleepMonitorClass ClockSleepMonitorClass;

G_DEFINE_TYPE (ClockSleepMonitor, clock_sleep_monitor, G_TYPE_OBJECT)

static void clock_sleep_monitor_finalize (GObject *object);

static void clock_sleep_monitor_class_init (ClockSleepMonitorClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = clock_sleep_monitor_finalize;

  clock_sleep_monitor_woke_up_signal =
    g_signal_new (
      g_intern_static_string ("woke-up"),
      G_TYPE_FROM_CLASS (gobject_class),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);
}

static void clock_sleep_monitor_init (ClockSleepMonitor *monitor)
{
}

static void clock_sleep_monitor_finalize (GObject *object)
{
  G_OBJECT_CLASS (clock_sleep_monitor_parent_class)->finalize (object);
}



#if defined (SLEEP_MONITOR_USE_LOGIND) || defined (SLEEP_MONITOR_USE_CONSOLEKIT)

struct _ClockSleepDBusMonitor
{
  ClockSleepMonitor parent_instance;
  GDBusProxy *proxy;
};

struct _ClockSleepDBusMonitorClass
{
  ClockSleepMonitorClass parent_class;
};

typedef struct _ClockSleepDBusMonitor ClockSleepDBusMonitor;
typedef struct _ClockSleepDBusMonitorClass ClockSleepDBusMonitorClass;

GType clock_sleep_dbus_monitor_get_type (void) G_GNUC_CONST;

#define XFCE_TYPE_CLOCK_SLEEP_DBUS_MONITOR (clock_sleep_dbus_monitor_get_type ())
#define XFCE_CLOCK_SLEEP_DBUS_MONITOR(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), XFCE_TYPE_CLOCK_SLEEP_DBUS_MONITOR, ClockSleepDBusMonitor))
#define XFCE_IS_CLOCK_SLEEP_DBUS_MONITOR(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), XFCE_TYPE_CLOCK_SLEEP_DBUS_MONITOR))

G_DEFINE_TYPE (ClockSleepDBusMonitor, clock_sleep_dbus_monitor, XFCE_TYPE_CLOCK_SLEEP_MONITOR)

static void clock_sleep_dbus_monitor_finalize (GObject *object);

static void clock_sleep_dbus_monitor_class_init (ClockSleepDBusMonitorClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = clock_sleep_dbus_monitor_finalize;
}

static void clock_sleep_dbus_monitor_init (ClockSleepDBusMonitor *monitor)
{
}

static void clock_sleep_dbus_monitor_finalize (GObject *object)
{
  ClockSleepDBusMonitor *monitor = XFCE_CLOCK_SLEEP_DBUS_MONITOR (object);
  g_return_if_fail (monitor != NULL);

  if (monitor->proxy != NULL)
    {
      g_signal_handlers_disconnect_by_data (monitor->proxy, monitor);
      g_object_unref (G_OBJECT (monitor->proxy));
    }

  G_OBJECT_CLASS (clock_sleep_dbus_monitor_parent_class)->finalize (object);
}

static void on_prepare_sleep_signal (GDBusProxy *proxy,
                                     gchar *sender_name,
                                     gchar *signal_name,
                                     GVariant *parameters,
                                     ClockSleepMonitor *monitor)
{
  const gchar *format_string = "(b)";
  gboolean going_to_sleep;

  if (strcmp (signal_name, "PrepareForSleep") != 0)
    return;

  if (!g_variant_check_format_string (parameters, format_string, FALSE))
    {
      g_critical ("unexpected format string: %s", g_variant_get_type_string (parameters));
      return;
    }

  g_variant_get (parameters, format_string, &going_to_sleep);

  if (!going_to_sleep)
    g_signal_emit (G_OBJECT (monitor), clock_sleep_monitor_woke_up_signal, 0);
}

static void
proxy_ready (GObject *source_object,
             GAsyncResult *res,
             gpointer data)
{
  ClockSleepDBusMonitor *monitor = data;
  GError *error = NULL;
  GDBusProxy *proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

  if (proxy != NULL)
    {
      if (monitor->proxy != NULL)
        {
          panel_debug (PANEL_DEBUG_CLOCK, "dropping proxy for %s", g_dbus_proxy_get_name (proxy));
          g_object_unref (proxy);
        }
      else
        {
          gchar *owner_name = g_dbus_proxy_get_name_owner (proxy);
          if (owner_name == NULL)
            {
              panel_debug (PANEL_DEBUG_CLOCK, "d-bus service %s not active", g_dbus_proxy_get_name (proxy));
              g_object_unref (proxy);
              return;
            }
          g_free (owner_name);

          panel_debug (PANEL_DEBUG_CLOCK, "keeping proxy for %s", g_dbus_proxy_get_name (proxy));
          g_signal_connect (proxy, "g-signal", G_CALLBACK (on_prepare_sleep_signal), monitor);
          monitor->proxy = proxy;
        }
    }
  else
    {
      panel_debug (PANEL_DEBUG_CLOCK, "could not get proxy: %s", error->message);
      g_error_free (error);
    }
}

static ClockSleepMonitor* clock_sleep_dbus_monitor_create (void)
{
  ClockSleepDBusMonitor *monitor;

  panel_debug (PANEL_DEBUG_CLOCK, "trying to instantiate d-bus sleep monitor");

  monitor = g_object_new (XFCE_TYPE_CLOCK_SLEEP_DBUS_MONITOR, NULL);

  if (!LOGIND_RUNNING ())
    panel_debug (PANEL_DEBUG_CLOCK, "logind not running");
  else
    g_dbus_proxy_new_for_bus (
      G_BUS_TYPE_SYSTEM,
      G_DBUS_PROXY_FLAGS_NONE,
      NULL,
      "org.freedesktop.login1",
      "/org/freedesktop/login1",
      "org.freedesktop.login1.Manager",
      NULL,
      proxy_ready,
      monitor);

  g_dbus_proxy_new_for_bus (
    G_BUS_TYPE_SYSTEM,
    G_DBUS_PROXY_FLAGS_NONE,
    NULL,
    "org.freedesktop.ConsoleKit",
    "/org/freedesktop/ConsoleKit/Manager",
    "org.freedesktop.ConsoleKit.Manager",
    NULL,
    proxy_ready,
    monitor);

  return XFCE_CLOCK_SLEEP_MONITOR (monitor);
}

#endif /* defined (SLEEP_MONITOR_USE_LOGIND) || defined (SLEEP_MONITOR_USE_CONSOLEKIT) */



/* Factory registration
 *
 * Collect available implementations in a reasonable order.
 */

typedef ClockSleepMonitor* (*SleepMonitorFactory) (void);

static SleepMonitorFactory sleep_monitor_factories[] =
{
#if defined (SLEEP_MONITOR_USE_LOGIND) || defined (SLEEP_MONITOR_USE_CONSOLEKIT)
  clock_sleep_dbus_monitor_create,
#endif
  NULL
};

ClockSleepMonitor *clock_sleep_monitor_create (void)
{
  SleepMonitorFactory *factory_ptr = &sleep_monitor_factories[0];
  ClockSleepMonitor *monitor = NULL;

  for (; monitor == NULL && *factory_ptr != NULL; factory_ptr++)
    monitor = (*factory_ptr) ();

  if (monitor == NULL && sleep_monitor_factories[0] != NULL)
    panel_debug (PANEL_DEBUG_CLOCK, "could not instantiate a sleep monitor");

  return monitor;
}
