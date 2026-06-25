/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * @file tests/test-basic.c Unit test: basics
 *
 * @Copyright (C) 2004 Mike Hearn <mike@navi.cx>
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

        notify_init ("Basics");

        /* Long summary */
        n = notify_notification_new ("Summary that is very long 8374983278r32j4 rhjjfh dw8f 43jhf 8ds7 ur2389f jdbjkt h8924yf jkdbjkt 892hjfiHER98HEJIF BDSJHF hjdhF JKLH 890YRHEJHFU 89HRJKSHdd dddd ddddd dddd ddddd dddd ddddd dddd dddd ddd ddd dddd Fdd d ddddd dddddddd ddddddddhjkewdkjsjfjk sdhkjf hdkj dadasdadsa adsd asd sd saasd fadskfkhsjf hsdkhfkshfjkhsd kjfhsjdkhfj ksdhfkjshkjfsd sadhfjkhaskd jfhsdajkfhkjs dhfkjsdhfkjs adhjkfhasdkj fhdsakjhfjk asdhkjkfhd akfjshjfsk afhjkasdhf jkhsdaj hf kjsdfahkfh sakjhfksdah kfdashkjf ksdahfj shdjdh",
                                     "Content",
                                     NULL);
        notify_notification_set_timeout (n, 3000);      //3 seconds

        if (!notify_notification_show (n, NULL)) {
                fprintf (stderr, "failed to send notification\n");
                return 1;
        }

        g_object_unref (G_OBJECT (n));

        /* Long message */
        n = notify_notification_new ("Summary",
                                     "Content that is very long 8374983278r32j4 rhjjfh dw8f 43jhf 8ds7 ur2389f jdbjkt h8924yf jkdbjkt 892hjfiHER98HEJIF BDSJHF hjdhF JKLH 890YRHEJHFU 89HRJKSHdd dddd ddddd dddd ddddd dddd ddddd dddd dddd ddd ddd dddd Fdd d ddddd dddddddd ddddddddhjkewdkjsjfjk sdhkjf hdkj dadasdadsa adsd asd sd saasd fadskfkhsjf hsdkhfkshfjkhsd kjfhsjdkhfj ksdhfkjshkjfsd sadhfjkhaskd jfhsdajkfhkjs dhfkjsdhfkjs adhjkfhasdkj fhdsakjhfjk asdhkjkfhd akfjshjfsk afhjkasdhf jkhsdaj hf kjsdfahkfh sakjhfksdah kfdashkjf ksdahfj shdjdh",
                                     NULL);
        notify_notification_set_timeout (n, 3000);      //3 seconds

        if (!notify_notification_show (n, NULL)) {
                fprintf (stderr, "failed to send notification\n");
                return 1;
        }

        g_object_unref (G_OBJECT (n));

        /* Summary only */
        n = notify_notification_new ("Summary only there is no message content",
                                     NULL,
                                     NULL);
        notify_notification_set_timeout (n, NOTIFY_EXPIRES_NEVER);

        if (!notify_notification_show (n, NULL)) {
                fprintf (stderr, "failed to send notification\n");
                return 1;
        }

        g_object_unref (G_OBJECT (n));

        return 0;
}
