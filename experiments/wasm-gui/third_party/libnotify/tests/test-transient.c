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

int
main (int argc, char **argv)
{
        NotifyNotification *n;
        GVariant           *hint;

        if (!notify_init ("Transient Test"))
                exit (1);

        loop = g_main_loop_new (NULL, FALSE);

        n = notify_notification_new ("Some transient change",
                                     "Something happened",
                                     "audio-volume-medium");
        hint = g_variant_new_boolean (TRUE);
        notify_notification_set_hint (n, "transient", hint);

        if (!notify_notification_show (n, NULL)) {
                fprintf (stderr, "failed to send notification\n");
                return 1;
        }

        g_main_loop_run (loop);

        return 0;
}
