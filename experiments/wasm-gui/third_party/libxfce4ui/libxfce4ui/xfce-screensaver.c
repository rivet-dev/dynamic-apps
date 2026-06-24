/*
 * * Copyright (C) 2016 Eric Koegel <eric@xfce.org>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * SECTION:xfce-screensaver
 * @title: XfceScreensaver
 * @short_description: screensaver related shared functions
 * @include: libxfce4ui/libxfce4ui.h
 *
 * Since: 4.18.2
 **/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#include "xfce-screensaver.h"



#define NO_REPLY_TIMEOUT 2000

static void      xfce_screensaver_get_property    (GObject         *object,
                                                   guint            property_id,
                                                   GValue          *value,
                                                   GParamSpec      *pspec);
static void      xfce_screensaver_set_property    (GObject         *object,
                                                   guint            property_id,
                                                   const GValue    *value,
                                                   GParamSpec      *pspec);
static void      xfce_screensaver_constructed     (GObject         *object);
static void      xfce_screensaver_finalize        (GObject         *object);



/* in order of priority, used to browse the screensaver array below */
typedef enum
{
  SCREENSAVER_TYPE_XFCE,
  SCREENSAVER_TYPE_CINNAMON,
  SCREENSAVER_TYPE_MATE,
  SCREENSAVER_TYPE_FREEDESKTOP,
  SCREENSAVER_TYPE_OTHER
} ScreensaverType;

enum
{
  PROP_0 = 0,
  PROP_HEARTBEAT_COMMAND,
  PROP_LOCK_COMMAND,
  PROP_LOCK_ON_SLEEP,
};

struct _XfceScreensaver
{
  GObject parent;

  GDBusProxy *proxies[SCREENSAVER_TYPE_OTHER];
  guint screensaver_id;
  guint cookie;
  ScreensaverType screensaver_type;

  gboolean xfconf_initialized;
  gchar *heartbeat_command;
  gchar *lock_command;
  gboolean lock_on_sleep;
};

typedef struct
{
  const gchar *name;
  const gchar *path;
  const gchar *iface;
  gboolean running;
} DbusScreensaver;

static DbusScreensaver dbus_screensavers[] =
{
  { "org.xfce.ScreenSaver", "/org/xfce/ScreenSaver", "org.xfce.ScreenSaver", FALSE },
  { "org.cinnamon.ScreenSaver", "/org/cinnamon/ScreenSaver", "org.cinnamon.ScreenSaver", FALSE },
  { "org.mate.ScreenSaver", "/org/mate/ScreenSaver", "org.mate.ScreenSaver", FALSE },
  { "org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver", "org.freedesktop.ScreenSaver", FALSE },
};



G_DEFINE_TYPE (XfceScreensaver, xfce_screensaver, G_TYPE_OBJECT)



static void
xfce_screensaver_class_init (XfceScreensaverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = xfce_screensaver_get_property;
  object_class->set_property = xfce_screensaver_set_property;
  object_class->constructed = xfce_screensaver_constructed;
  object_class->finalize = xfce_screensaver_finalize;

#define XFCE_PARAM_FLAGS (G_PARAM_READWRITE \
                          | G_PARAM_CONSTRUCT \
                          | G_PARAM_STATIC_NAME \
                          | G_PARAM_STATIC_NICK \
                          | G_PARAM_STATIC_BLURB)

  g_object_class_install_property (object_class, PROP_HEARTBEAT_COMMAND,
                                   g_param_spec_string ("heartbeat-command",
                                                        "heartbeat-command",
                                                        "Inhibit the screensaver from activating, "
                                                        "e.g. xscreensaver-command --deactivate",
                                                        NULL,
                                                        XFCE_PARAM_FLAGS));

  g_object_class_install_property (object_class, PROP_LOCK_COMMAND,
                                   g_param_spec_string ("lock-command",
                                                        "lock-command",
                                                        "Lock the desktop, e.g. "
                                                        "xscreensaver-command --lock",
                                                        NULL,
                                                        XFCE_PARAM_FLAGS));

  g_object_class_install_property (object_class, PROP_LOCK_ON_SLEEP,
                                   g_param_spec_boolean ("lock-on-sleep",
                                                         "lock-on-sleep",
                                                         "Whether to lock before suspend/hibernate",
                                                         FALSE,
                                                         XFCE_PARAM_FLAGS));
#undef XFCE_PARAM_FLAGS
}



static void
name_owner_changed (GDBusProxy *proxy,
                    GParamSpec *pspec,
                    XfceScreensaver *saver)
{
  /* update current proxy status */
  for (guint i = 0; i < SCREENSAVER_TYPE_OTHER; i++)
    {
      if (saver->proxies[i] == proxy)
        {
          gchar *owner = g_dbus_proxy_get_name_owner (proxy);
          if (owner == NULL)
            {
              dbus_screensavers[i].running = FALSE;
              if (saver->screensaver_type == i)
                {
                  saver->screensaver_type = SCREENSAVER_TYPE_OTHER;
                  saver->cookie = 0;
                  if (saver->screensaver_id != 0)
                    {
                      g_source_remove (saver->screensaver_id);
                      saver->screensaver_id = 0;
                    }
                }
            }
          else
            {
              dbus_screensavers[i].running = TRUE;
              g_free (owner);
            }
          break;
        }
    }

  /* update used screensaver */
  for (guint i = 0; i < SCREENSAVER_TYPE_OTHER; i++)
    {
      if (dbus_screensavers[i].running && i != saver->screensaver_type)
        {
          if (i < saver->screensaver_type)
            {
              gboolean inhibited = saver->cookie != 0 || saver->screensaver_id != 0;
              if (inhibited)
                xfce_screensaver_inhibit (saver, FALSE);
              saver->screensaver_type = i;
              if (inhibited)
                xfce_screensaver_inhibit (saver, TRUE);
            }
          else
            {
              g_warning ("%s running but unused: using %s instead",
                         dbus_screensavers[i].name, dbus_screensavers[saver->screensaver_type].name);
            }
        }
    }
}



static void
xfce_screensaver_init (XfceScreensaver *saver)
{
  GError *error = NULL;

  saver->screensaver_type = SCREENSAVER_TYPE_OTHER;

  for (guint i = 0; i < SCREENSAVER_TYPE_OTHER; i++)
    {
      saver->proxies[i] = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                         G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START_AT_CONSTRUCTION,
                                                         NULL,
                                                         dbus_screensavers[i].name,
                                                         dbus_screensavers[i].path,
                                                         dbus_screensavers[i].iface,
                                                         NULL,
                                                         &error);
      if (error != NULL)
        {
          g_warning ("Failed to get a proxy for %s: %s", dbus_screensavers[i].name, error->message);
          g_clear_error (&error);
        }
      else
        {
          name_owner_changed (saver->proxies[i], NULL, saver);
          g_signal_connect (saver->proxies[i], "notify::g-name-owner", G_CALLBACK (name_owner_changed), saver);
        }
    }
}



static void
xfce_screensaver_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
  XfceScreensaver *saver = XFCE_SCREENSAVER (object);

  switch (property_id)
    {
    case PROP_HEARTBEAT_COMMAND:
      g_value_set_string (value, saver->heartbeat_command);
      break;

    case PROP_LOCK_COMMAND:
      g_value_set_string (value, saver->lock_command);
      break;

    case PROP_LOCK_ON_SLEEP:
      g_value_set_boolean (value, saver->lock_on_sleep);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}



static void
xfce_screensaver_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
  XfceScreensaver *saver = XFCE_SCREENSAVER (object);
  const gchar *str_value;

  switch (property_id)
    {
    case PROP_HEARTBEAT_COMMAND:
      g_free (saver->heartbeat_command);
      saver->heartbeat_command = NULL;
      str_value = g_value_get_string (value);
      if (!xfce_str_is_empty (str_value))
        saver->heartbeat_command = g_strdup (str_value);
      DBG ("saver->heartbeat_command %s", saver->heartbeat_command);
      break;

    case PROP_LOCK_COMMAND:
      g_free (saver->lock_command);
      saver->lock_command = NULL;
      str_value = g_value_get_string (value);
      if (!xfce_str_is_empty (str_value))
        saver->lock_command = g_strdup (str_value);
      DBG ("saver->lock_command %s", saver->lock_command);
      break;

    case PROP_LOCK_ON_SLEEP:
      saver->lock_on_sleep = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}



static void
xfce_screensaver_constructed (GObject *object)
{
  XfceScreensaver *saver = XFCE_SCREENSAVER (object);
  GError *error = NULL;

  saver->xfconf_initialized = xfconf_init (&error);
  if (!saver->xfconf_initialized)
    {
      g_critical ("Xfconf initialization failed: %s", error->message);
      g_clear_error (&error);
    }
  else
    {
      XfconfChannel *power_channel = xfconf_channel_get ("xfce4-power-manager");
      XfconfChannel *session_channel = xfconf_channel_get ("xfce4-session");
      XfconfChannel *saver_channel = xfconf_channel_get ("xfce4-screensaver");
      xfconf_g_property_bind (power_channel,
                              "/xfce4-power-manager/heartbeat-command",
                              G_TYPE_STRING,
                              G_OBJECT (saver),
                              "heartbeat-command");
      xfconf_g_property_bind (session_channel,
                              "/general/LockCommand",
                              G_TYPE_STRING,
                              G_OBJECT (saver),
                              "lock-command");

      /* keep components having a "lock-on-sleep" setting in sync */
      xfconf_g_property_bind (power_channel,
                              "/xfce4-power-manager/lock-screen-suspend-hibernate",
                              G_TYPE_BOOLEAN,
                              G_OBJECT (saver),
                              "lock-on-sleep");
      xfconf_g_property_bind (session_channel,
                              "/shutdown/LockScreen",
                              G_TYPE_BOOLEAN,
                              G_OBJECT (saver),
                              "lock-on-sleep");
      xfconf_g_property_bind (saver_channel,
                              "/lock/sleep-activation",
                              G_TYPE_BOOLEAN,
                              G_OBJECT (saver),
                              "lock-on-sleep");
    }

  G_OBJECT_CLASS (xfce_screensaver_parent_class)->constructed (object);
}



static void
xfce_screensaver_finalize (GObject *object)
{
  XfceScreensaver *saver = XFCE_SCREENSAVER (object);

  if (saver->screensaver_id != 0)
    {
      g_source_remove (saver->screensaver_id);
      saver->screensaver_id = 0;
    }

  for (guint i = 0; i < SCREENSAVER_TYPE_OTHER; i++)
    g_clear_object (&saver->proxies[i]);

  if (saver->heartbeat_command)
    {
      g_free (saver->heartbeat_command);
      saver->heartbeat_command = NULL;
    }

  if (saver->lock_command)
    {
      g_free (saver->lock_command);
      saver->lock_command = NULL;
    }

  if (saver->xfconf_initialized)
    xfconf_shutdown ();

  G_OBJECT_CLASS (xfce_screensaver_parent_class)->finalize (object);
}



/**
 * xfce_screensaver_new:
 *
 * Creates a new #XfceScreensaver object or increases the reference count
 * of the current object.
 *
 * Returns: (transfer full): An #XfceScreensaver object, to be released with
 * g_object_unref() when no longer used.
 *
 * Since: 4.18.2
 **/
XfceScreensaver *
xfce_screensaver_new (void)
{
  static gpointer *saver = NULL;

  if (saver != NULL)
    {
      g_object_ref (saver);
    }
  else
    {
      saver = g_object_new (XFCE_TYPE_SCREENSAVER, NULL);
      g_object_add_weak_pointer (G_OBJECT (saver), (gpointer *) &saver);
    }

  return XFCE_SCREENSAVER (saver);
}



static gboolean
xfce_reset_screen_saver (gpointer user_data)
{
  XfceScreensaver *saver = user_data;

  TRACE ("entering\n");

  /* If we found an interface during the setup, use it */
  if (saver->screensaver_type != SCREENSAVER_TYPE_OTHER)
    {
      GVariant *response = g_dbus_proxy_call_sync (saver->proxies[saver->screensaver_type],
                                                   "SimulateUserActivity",
                                                   NULL,
                                                   G_DBUS_CALL_FLAGS_NONE,
                                                   NO_REPLY_TIMEOUT,
                                                   NULL,
                                                   NULL);
      if (response != NULL)
        g_variant_unref (response);
    }
  else if (saver->heartbeat_command)
    {
      DBG ("running heartbeat command: %s", saver->heartbeat_command);
      g_spawn_command_line_async (saver->heartbeat_command, NULL);
    }

  /* continue until we're removed */
  return TRUE;
}



/**
 * xfce_screensaver_inhibit:
 * @saver: the #XfceScreensaver object
 * @inhibit: whether to inhibit the screensaver from activating
 *
 * Calling this function with @inhibit as %TRUE will prevent the user's
 * screensaver from activating. This is useful when the user is watching
 * a movie or giving a presentation.
 *
 * Calling this function with @inhibit as %FALSE will remove any current
 * screensaver inhibit the #XfceScreensaver object has.
 *
 * Since: 4.18.2
 **/
void
xfce_screensaver_inhibit (XfceScreensaver *saver,
                          gboolean inhibit)
{
  /* SCREENSAVER_TYPE_FREEDESKTOP, SCREENSAVER_TYPE_MATE and SCREENSAVER_TYPE_XFCE
   * don't need a periodic timer because they have an actual inhibit/uninhibit setup */
  switch (saver->screensaver_type)
    {
    case SCREENSAVER_TYPE_XFCE:
    case SCREENSAVER_TYPE_MATE:
    case SCREENSAVER_TYPE_FREEDESKTOP:
      if (inhibit)
        {
          GVariant *response = g_dbus_proxy_call_sync (saver->proxies[saver->screensaver_type],
                                                       "Inhibit",
                                                       g_variant_new ("(ss)",
                                                                      PACKAGE_NAME,
                                                                      "Inhibit requested"),
                                                       G_DBUS_CALL_FLAGS_NONE,
                                                       -1,
                                                       NULL, NULL);
          if (response != NULL)
            {
              g_variant_get (response, "(u)", &saver->cookie);
              g_variant_unref (response);
            }
        }
      else
      {
        GVariant *response = g_dbus_proxy_call_sync (saver->proxies[saver->screensaver_type],
                                                     "UnInhibit",
                                                     g_variant_new ("(u)",
                                                                    saver->cookie),
                                                     G_DBUS_CALL_FLAGS_NONE,
                                                     -1,
                                                     NULL, NULL);
        saver->cookie = 0;
        if (response != NULL)
          g_variant_unref (response);
      }
      break;

    case SCREENSAVER_TYPE_CINNAMON:
    case SCREENSAVER_TYPE_OTHER:
      /* remove any existing keepalive */
      if (saver->screensaver_id != 0)
        {
          g_source_remove (saver->screensaver_id);
          saver->screensaver_id = 0;
        }

      if (inhibit)
        {
          /* Reset the screensaver timers every so often
           * so they don't activate */
          saver->screensaver_id = g_timeout_add_seconds (20,
                                                         xfce_reset_screen_saver,
                                                         saver);
        }
      break;

    default:
      g_warn_if_reached ();
      break;
    }
}



/**
 * xfce_screensaver_lock:
 * @saver: the #XfceScreensaver object
 *
 * Attempts to lock the screen, either with one of the screensaver
 * D-Bus proxies, the Xfconf lock command, or one of the
 * fallback scripts such as xdg-screensaver.
 *
 * Returns: %TRUE if the lock attempt returns success, %FALSE otherwise.
 *
 * Since: 4.18.2
 **/
gboolean
xfce_screensaver_lock (XfceScreensaver *saver)
{
  GVariant *response;
  GError *error = NULL;
  gint status;

  /* prioritize user command and don't try anything else it that fails */
  if (saver->lock_command != NULL)
    {
      gchar **argv;

      DBG ("running lock command: %s", saver->lock_command);

      /* prevent accidental recursive calling of lock command if it calls
       * xfce_screensaver_lock() in any way (as e.g. xflock4 does) */
      if (g_getenv ("XFCE_SCREENSAVER_LOCK") != NULL)
        {
          g_warning ("Recursive call of %s", saver->lock_command);
          return FALSE;
        }

      if (g_shell_parse_argv (saver->lock_command, NULL, &argv, NULL))
        {
          gchar **env = g_environ_setenv (g_get_environ (), "XFCE_SCREENSAVER_LOCK", "", TRUE);
          gboolean ret = g_spawn_sync (NULL, argv, env, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, &status, NULL)
                         && g_spawn_check_exit_status (status, NULL);
          g_strfreev (env);
          g_strfreev (argv);
          return ret;
        }

      return FALSE;
    }

  /* try dbus screensavers */
  for (guint i = 0; i < SCREENSAVER_TYPE_OTHER; i++)
    {
      if ((saver->screensaver_type == SCREENSAVER_TYPE_OTHER && saver->proxies[i] != NULL)
          || saver->screensaver_type == i)
        {
          switch (i)
            {
            case SCREENSAVER_TYPE_XFCE:
              response = g_dbus_proxy_call_sync (saver->proxies[i],
                                                 "Lock",
                                                 NULL,
                                                 G_DBUS_CALL_FLAGS_NONE,
                                                 -1,
                                                 NULL,
                                                 &error);
              break;

            case SCREENSAVER_TYPE_CINNAMON:
              response = g_dbus_proxy_call_sync (saver->proxies[i],
                                                 "Lock",
                                                 g_variant_new ("(s)", PACKAGE_NAME),
                                                 G_DBUS_CALL_FLAGS_NONE,
                                                 -1,
                                                 NULL,
                                                 &error);
              break;

            case SCREENSAVER_TYPE_MATE:
            case SCREENSAVER_TYPE_FREEDESKTOP:
              response = g_dbus_proxy_call_sync (saver->proxies[i],
                                                 "Lock",
                                                 NULL,
                                                 G_DBUS_CALL_FLAGS_NONE,
                                                 NO_REPLY_TIMEOUT,
                                                 NULL,
                                                 &error);

              /* mate-screensaver does not send a reply in case of success, and for screensavers
               * using org.freedesktop.ScreenSaver we're not sure, so if no other error is received
               * after a reasonnable timeout, consider it a success */
              if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT))
                {
                  response = g_variant_ref_sink (g_variant_new ("()"));
                  g_clear_error (&error);
                }
              break;

            default:
              g_warn_if_reached ();
              continue;
            }

          if (response != NULL)
            {
              g_variant_unref (response);
              return TRUE;
            }
          else
            {
              /* if it's running and can lock it should succeed, don't try anything else */
              gboolean running = !g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_NAME_HAS_NO_OWNER);
              gboolean can_lock = !g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD);
              g_clear_error (&error);
              if (running && can_lock)
                return FALSE;
            }
        }
    }

  /* no user command or dbus interface set up */
  if (g_spawn_command_line_sync ("xdg-screensaver lock", NULL, NULL, &status, NULL)
      && g_spawn_check_exit_status (status, NULL))
    return TRUE;

  if (g_spawn_command_line_sync ("xscreensaver-command --lock", NULL, NULL, &status, NULL)
      && g_spawn_check_exit_status (status, NULL))
    return TRUE;

  if (g_spawn_command_line_sync ("light-locker-command --lock", NULL, NULL, &status, NULL)
      && g_spawn_check_exit_status (status, NULL))
    return TRUE;

  return FALSE;
}
