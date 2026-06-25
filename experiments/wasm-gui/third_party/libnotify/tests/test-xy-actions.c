/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * @file tests/test-xy-actions.c Unit test: X, Y hints and actions
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

static GMainLoop *loop;

static void
action_cb (NotifyNotification *n,
           const char         *action)
{
        printf ("You clicked '%s'\n", action);

        g_main_loop_quit (loop);
}

int
main (int argc, char **argv)
{
        NotifyNotification *n;

        notify_init ("XY");

        loop = g_main_loop_new (NULL, FALSE);

        n = notify_notification_new ("System update available",
                                     "New system updates are available. It is "
                                     "recommended that you install the updates.",
                                     NULL);

        notify_notification_set_hint_int32 (n, "x", 600);
        notify_notification_set_hint_int32 (n, "y", 10);
        notify_notification_add_action (n,
                                        "help",
                                        "Help",
                                        (NotifyActionCallback) action_cb,
                                        NULL,
                                        NULL);
        notify_notification_add_action (n,
                                        "update",
                                        "Update",
                                        (NotifyActionCallback) action_cb,
                                        NULL,
                                        NULL);

        if (!notify_notification_show (n, NULL)) {
                fprintf (stderr, "failed to send notification\n");
                return 1;
        }

        g_main_loop_run (loop);

        return 0;
}
