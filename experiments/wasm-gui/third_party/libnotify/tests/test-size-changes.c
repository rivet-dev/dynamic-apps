/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * @file tests/test-size-changes.c Unit test: Notification size changes
 *
 * @Copyright (C) 2006 Christian Hammond <chipx86@chipx86.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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
#include <stdio.h>
#include <unistd.h>

int
main ()
{
        NotifyNotification *n1, *n2, *n3;

        notify_init ("Size Changes");

        n1 = notify_notification_new ("Notification 1",
                                      "Notification number 1!",
                                      NULL);
        notify_notification_set_timeout (n1, 7000);

        if (!notify_notification_show (n1, NULL)) {
                fprintf (stderr, "failed to send notification\n");
                return 1;
        }

        g_object_unref (G_OBJECT (n1));

        n2 = notify_notification_new ("Notification 2",
                                      "Notification number 2!",
                                      NULL);
        notify_notification_set_timeout (n2, 7000);

        if (!notify_notification_show (n2, NULL)) {
                fprintf (stderr, "failed to send notification\n");
                return 1;
        }


        n3 = notify_notification_new ("Notification 3",
                                      "Notification number 3!",
                                      NULL);
        notify_notification_set_timeout (n3, 7000);

        if (!notify_notification_show (n3, NULL)) {
                fprintf (stderr, "failed to send notification\n");
                return 1;
        }

        g_object_unref (G_OBJECT (n3));

        sleep (2);

        notify_notification_update (n2,
                                    "Longer Notification 2",
                                    "This is a much longer notification.\n"
                                    "Two lines.\n"
                                    "Well, okay, three.\n" "Last one.",
                                    NULL);

        if (!notify_notification_show (n2, NULL)) {
                fprintf (stderr, "failed to send notification\n");
                return 1;
        }

        return 0;
}
