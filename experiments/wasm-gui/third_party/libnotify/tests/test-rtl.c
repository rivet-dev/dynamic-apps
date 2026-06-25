/*
 * @file tests/test-basic.c Unit test: basics
 *
 * @Copyright (C) 2004 Mike Hearn <mike@navi.cx>
 *
 * Test Case by Djihed Afifi <djihed@gmail.com>
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

        notify_init("Basics");

        n = notify_notification_new ("اختبار",
                                     "يوفر ويكي عربآيز مناخا للنقاش وتبادل الخبرات والمعرفة حول اللغة العربية ولسانياتها ومايتعلّق بدعمها والإرتقاء بها في الحوسبة عموماً والبرمجيات الحرة على وجه الخصوص. هذا الويكي عبارة عن بيئة تعاونية تشاركية مفتوحة للجميع بدون قيود، لغتنا الجميلة بانتظار مساهمتك فلا تبخل عليها ",
                                     NULL);
        notify_notification_set_timeout (n, 3000);

        if (!notify_notification_show (n, NULL)) {
                fprintf (stderr, "failed to send notification\n");
                return 1;
        }

        g_object_unref (G_OBJECT (n));

        return 0;
}
