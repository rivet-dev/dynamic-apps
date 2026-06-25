/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
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
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

static GMainLoop *loop;

static void
previous_callback (NotifyNotification *n,
                   const char         *action)
{
        g_assert (action != NULL);
        g_assert (strcmp (action, "media-skip-backward") == 0);

        printf ("You clicked Previous\n");

        notify_notification_close (n, NULL);

        g_main_loop_quit (loop);
}

static void
pause_callback (NotifyNotification *n,
                const char         *action)
{
        g_assert (action != NULL);
        g_assert (strcmp (action, "media-playback-pause") == 0);

        printf ("You clicked Pause\n");

        notify_notification_close (n, NULL);

        g_main_loop_quit (loop);
}

static void
next_callback (NotifyNotification *n,
               const char         *action)
{
        g_assert (action != NULL);
        g_assert (strcmp (action, "media-skip-forward") == 0);

        printf ("You clicked Next\n");

        notify_notification_close (n, NULL);

        g_main_loop_quit (loop);
}


int
main (int argc, char **argv)
{
        NotifyNotification *n;

        if (!notify_init ("Action Icon Test"))
                exit (1);

        loop = g_main_loop_new (NULL, FALSE);

        n = notify_notification_new ("Music Player",
                                     "Some solid funk",
                                     NULL);

        notify_notification_set_hint (n, "action-icons", g_variant_new_boolean (TRUE));

        notify_notification_add_action (n,
                                        "media-skip-backward",
                                        "Previous",
                                        (NotifyActionCallback) previous_callback,
                                        NULL,
                                        NULL);
        notify_notification_add_action (n,
                                        "media-playback-pause",
                                        "Pause",
                                        (NotifyActionCallback) pause_callback,
                                        NULL,
                                        NULL);
        notify_notification_add_action (n,
                                        "media-skip-forward",
                                        "Next",
                                        (NotifyActionCallback) next_callback,
                                        NULL,
                                        NULL);

        if (!notify_notification_show (n, NULL)) {
                fprintf (stderr, "failed to send notification\n");
                return 1;
        }


        n = notify_notification_new ("Music Player",
                                     "Shouldn't have icons",
                                     NULL);

        notify_notification_set_hint (n, "action-icons", g_variant_new_boolean (FALSE));

        notify_notification_add_action (n,
                                        "media-skip-backward",
                                        "Previous",
                                        (NotifyActionCallback) previous_callback,
                                        NULL,
                                        NULL);

        if (!notify_notification_show (n, NULL)) {
                fprintf (stderr, "failed to send notification\n");
                return 1;
        }

        g_main_loop_run (loop);

        return 0;
}
