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

#define N_NOTIFICATIONS 30

int main (int argc, char **argv)
{
  gint i;

  if (!notify_init ("Test notification positioning"))
    {
      g_error ("Failed to initialize libnotify.");

      return EXIT_FAILURE;
    }

  g_print ("%s", "Testing notification positioning\n");

  for (i = 0; i < N_NOTIFICATIONS; i++)
    {
      NotifyNotification *notification;
      const gchar        *body;
      gchar              *summary;
      gint                type;

      summary = g_strdup_printf ("Notification %i", i);

      type = rand () % 3;

      if (type == 0)
        body = "Short body";
      else if (type == 1)
        body = "Enlarge your body";
      else
        body = "Huge body with a lot of text.\nAnd with a new line too!";

      notification = notify_notification_new (summary,
                                              body,
                                              NULL);

      if (!notify_notification_show (notification, NULL))
        {
          g_error ("Failed");

          g_object_unref (notification);
          g_free (summary);

          return EXIT_FAILURE;
        }

      g_object_unref (notification);
      g_free (summary);
    }

  return EXIT_SUCCESS;
}
