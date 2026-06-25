/*
 *  xfce4-notifyd
 *
 *  Copyright (c) 2011 Jérôme Guelfucci <jeromeg@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gprintf.h>
#include <stdlib.h>
#include <string.h>
#include <libnotify/notify.h>

static void action1_callback (NotifyNotification *notification,
                              const char         *action,
                              gpointer            loop)
{
  g_assert (action != NULL);
  g_assert (strcmp (action, "action1") == 0);
  g_assert (loop != NULL);

  g_printf ("You clicked the first action\n");

  notify_notification_close (notification, NULL);

  g_main_loop_quit (loop);
}

static void action2_callback (NotifyNotification *notification,
                              const char         *action,
                              gpointer            loop)
{
  g_assert (action != NULL);
  g_assert (strcmp (action, "action2") == 0);
  g_assert (loop != NULL);

  g_printf ("You clicked the second action\n");

  notify_notification_close (notification, NULL);

  g_main_loop_quit (loop);
}

static void clear_callback (NotifyNotification *notification,
                            const char         *action,
                            gpointer            loop)
{
  g_assert (action != NULL);
  g_assert (strcmp (action, "clear") == 0);
  g_assert (loop != NULL);

  g_printf ("You clicked the clear action\n");

  notify_notification_clear_actions (notification);

  notify_notification_add_action (notification,
                                  "action1",
                                  "First Action",
                                  (NotifyActionCallback) action1_callback,
                                  loop,
                                  NULL);

  notify_notification_add_action (notification,
                                  "action2",
                                  "Second Action",
                                  (NotifyActionCallback) action2_callback,
                                  loop,
                                  NULL);

  if (!notify_notification_show (notification, NULL))
    {
      g_error ("Failed");
      g_main_loop_quit (loop);
    }
}

int main (int argc, char **argv)
{
  NotifyNotification *notification;
  GMainLoop          *loop;

  if (!notify_init ("Notification with actions test"))
    {
      g_error ("Failed to initialize libnotify.");

      return EXIT_FAILURE;
    }

  loop = g_main_loop_new (NULL, FALSE);

  notification = notify_notification_new ("Action!",
                                          "Click my shiny actions",
                                          NULL);

  notify_notification_add_action (notification,
                                  "action1",
                                  "First Action",
                                  (NotifyActionCallback) action1_callback,
                                  loop,
                                  NULL);

  notify_notification_add_action (notification,
                                  "action2",
                                  "Second Action",
                                  (NotifyActionCallback) action2_callback,
                                  loop,
                                  NULL);

  notify_notification_add_action (notification,
                                  "clear",
                                  "Clear me",
                                  (NotifyActionCallback) clear_callback,
                                  loop,
                                  NULL);

  if (!notify_notification_show (notification, NULL))
    {
      g_error ("Failed");
      g_object_unref (notification);

      return EXIT_FAILURE;
    }

  g_main_loop_run (loop);

  g_object_unref (notification);

  return EXIT_SUCCESS;
}
