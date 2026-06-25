/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * @file tests/test-xy.c Unit test: X, Y hints
 *
 * @Copyright (C) 2005 Christian Hammond <chipx86@chipx86.com>
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
        NotifyNotification *n;

        notify_init ("XY");

        n = notify_notification_new ("X, Y Test",
                                     "This notification should point to 150, 10",
                                     NULL);

        notify_notification_set_hint_int32 (n, "x", 150);
        notify_notification_set_hint_int32 (n, "y", 10);

        if (!notify_notification_show (n, NULL)) {
                fprintf (stderr, "failed to send notification\n");
                return 1;
        }

        return 0;
}
