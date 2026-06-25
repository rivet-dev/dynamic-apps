/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
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
static int counter;
static gboolean is_playing;

static void
prev_callback (NotifyNotification *n,
               const char         *action)
{
        char *body;

        g_assert (action != NULL);
        g_assert (strcmp (action, "previous") == 0);

        printf ("You clicked Previous\n");
        body = g_strdup_printf ("Playing some fine song %d", --counter);
        notify_notification_update (n,
                                    "Music Player",
                                    body,
                                    "audio-x-generic");
        g_free (body);

        if (!notify_notification_show (n, NULL)) {
                fprintf (stderr, "failed to send update\n");
        }
}

static void
pause_callback (NotifyNotification *n,
                const char         *action)
{
        char *body;

        g_assert (action != NULL);
        g_assert (strcmp (action, "pause") == 0);

        printf ("You clicked Play/Pause\n");
        is_playing = !is_playing;
        if (is_playing) {
                body = g_strdup_printf ("Playing some fine song %d", counter);
        } else {
                body = g_strdup_printf ("Not playing some fine song %d", counter);
        }

        notify_notification_update (n,
                                    "Music Player",
                                    body,
                                    "audio-x-generic");
        g_free (body);

        if (!notify_notification_show (n, NULL)) {
                fprintf (stderr, "failed to send update\n");
        }
}

static void
next_callback (NotifyNotification *n,
               const char         *action)
{
        char *body;

        g_assert (action != NULL);
        g_assert (strcmp (action, "next") == 0);

        printf ("You clicked Next\n");
        body = g_strdup_printf ("Playing some fine song %d", ++counter);
        notify_notification_update (n,
                                    "Music Player",
                                    body,
                                    "audio-x-generic");
        g_free (body);

        if (!notify_notification_show (n, NULL)) {
                fprintf (stderr, "failed to send update\n");
        }
}


int
main (int argc, char **argv)
{
        NotifyNotification *n;
        GVariant           *hint;

        if (!notify_init ("Resident Test"))
                exit (1);

        loop = g_main_loop_new (NULL, FALSE);

        counter = 0;
        is_playing = TRUE;

        n = notify_notification_new ("Music Player",
                                     "Playing some fine song",
                                     "audio-x-generic");
        hint = g_variant_new_boolean (TRUE);
        notify_notification_set_hint (n, "resident", hint);
        notify_notification_add_action (n,
                                        "previous",
                                        "Previous",
                                        (NotifyActionCallback) prev_callback,
                                        NULL,
                                        NULL);
        notify_notification_add_action (n,
                                        "pause",
                                        "Pause",
                                        (NotifyActionCallback)
                                        pause_callback,
                                        NULL,
                                        NULL);
        notify_notification_add_action (n,
                                        "next",
                                        "Next",
                                        (NotifyActionCallback) next_callback,
                                        NULL,
                                        NULL);

        if (!notify_notification_show (n, NULL)) {
                fprintf (stderr, "failed to send notification\n");
                return 1;
        }

        g_main_loop_run (loop);

        return 0;
}
