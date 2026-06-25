/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * @file tests/test-urgency.c Unit test: urgency levels
 *
 * @Copyright(C) 2006 Christian Hammond <chipx8@chipx86.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or(at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA  02111-1307, USA.
 */

#include <libnotify/notify.h>
#include <stdlib.h>

int
main (int argc, char *argv[])
{
        NotifyNotification *n;

        notify_init ("Urgency");

        n = notify_notification_new ("Low Urgency",
                                     "Joe signed online.",
                                     NULL);
        notify_notification_set_urgency (n, NOTIFY_URGENCY_LOW);
        if (!notify_notification_show (n, NULL)) {
                fprintf (stderr, "failed to send notification\n");
                exit (1);
        }

        g_object_unref (G_OBJECT (n));


        n = notify_notification_new ("Normal Urgency",
                                     "You have a meeting in 10 minutes.",
                                     NULL);
        notify_notification_set_urgency (n, NOTIFY_URGENCY_NORMAL);
        if (!notify_notification_show (n, NULL)) {
                fprintf (stderr, "failed to send notification\n");
                exit (1);
        }

        g_object_unref (G_OBJECT (n));


        n = notify_notification_new ("Critical Urgency",
                                     "This message will self-destruct in 10 seconds.",
                                     NULL);
        notify_notification_set_urgency (n, NOTIFY_URGENCY_CRITICAL);
        notify_notification_set_timeout (n, 10000);     // 10 seconds

        if (!notify_notification_show (n, NULL)) {
                fprintf (stderr, "failed to send notification\n");
                exit (1);
        }

        g_object_unref (G_OBJECT (n));

        return 0;
}
