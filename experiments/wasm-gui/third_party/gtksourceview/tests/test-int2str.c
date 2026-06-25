/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *
 * This file is part of GtkSourceView
 *
 * Copyright 2019 - Christian Hergert <chergert@redhat.com>
 *
 * GtkSourceView is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * GtkSourceView is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <gtksourceview/gtksource.h>
#include <gtksourceview/gtksourceutils-private.h>

gint
main (gint   argc,
      gchar *argv[])
{
	GTimer *timer = g_timer_new ();
	gdouble d;
	guint i;

	g_timer_reset (timer);
	for (i = 0; i < 20000; i++)
	{
		const gchar *str;

		_gtk_source_utils_int_to_string (i, &str);
	}
	d = g_timer_elapsed (timer, NULL);
	g_print ("int_to_string: %lf\n", d);

	g_timer_reset (timer);
	for (i = 0; i < 20000; i++)
	{
		gchar tmpbuf[12];

		g_snprintf (tmpbuf, 12, "%u", i);
	}
	d = g_timer_elapsed (timer, NULL);
	g_print ("     g_snprintf: %lf\n", d);

	/* Make sure implementation is correct */
	for (i = 0; i < 20000; i++)
	{
		const gchar *str1;
		gint len1, len2;
		gchar str2[12];

		len1 = _gtk_source_utils_int_to_string (i, &str1);
		len2 = g_snprintf (str2, sizeof str2, "%u", i);

		g_assert_cmpint (len1, ==, len2);
		g_assert_cmpstr (str1, ==, str2);
	}

	return 0;
}
