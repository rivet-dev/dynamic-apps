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
#include <stdlib.h>
#include <libnotify/notify.h>

int main (int argc, char **argv)
{
  NotifyNotification *notification;

  if (!notify_init ("Notification with text test"))
    {
      g_error ("Failed to initialize libnotify.");

      return EXIT_FAILURE;
    }

  g_print ("%s", "Testing notification with text summary and body\n");

  notification = notify_notification_new ("Test text support",
                                          "Does it work?",
                                          NULL);

  if (!notify_notification_show (notification, NULL))
    {
      g_error ("Failed");
      g_object_unref (notification);

      return EXIT_FAILURE;
    }

  g_object_unref (notification);

  g_print ("%s", "Testing notification with text summary and no body\n");

  notification = notify_notification_new ("Summary only support",
                                          NULL,
                                          NULL);

  if (!notify_notification_show (notification, NULL))
    {
      g_error ("Failed");
      g_object_unref (notification);

      return EXIT_FAILURE;
    }

  g_object_unref (notification);

  g_print ("%s", "Testing notification with markup in the body\n");

  notification = notify_notification_new ("Markup support",
                                          "<i>Italic</i>\n"
                                          "<b>Bold</b>\n"
                                          "<u>Underlined</u>\n"
                                          "<a href=\"http://www.xfce.org\">Xfce Web site</a>",
                                          NULL);

  if (!notify_notification_show (notification, NULL))
    {
      g_error ("Failed");
      g_object_unref (notification);

      return EXIT_FAILURE;
    }

  g_object_unref (notification);

  return EXIT_SUCCESS;
}
