/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- */
/*
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2014 - Sébastien Wilmet <swilmet@gnome.org>
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
#include "gtksourceview/gtksourceencoding-private.h"

static void
test_remove_duplicates (void)
{
	GSList *list = NULL;
	const GtkSourceEncoding *utf8;
	const GtkSourceEncoding *iso;

	utf8 = gtk_source_encoding_get_utf8 ();
	iso = gtk_source_encoding_get_from_charset ("ISO-8859-15");

	/* Before: [UTF-8, ISO-8859-15, UTF-8] */
	list = g_slist_prepend (list, (gpointer) utf8);
	list = g_slist_prepend (list, (gpointer) iso);
	list = g_slist_prepend (list, (gpointer) utf8);

	/* After: [UTF-8, ISO-8859-15] */
	list = _gtk_source_encoding_remove_duplicates (list, GTK_SOURCE_ENCODING_DUPLICATES_KEEP_FIRST);

	g_assert_cmpint (2, ==, g_slist_length (list));
	g_assert_true (list->data == utf8);
	g_assert_true (list->next->data == iso);

	/* Before: [UTF-8, ISO-8859-15, UTF-8] */
	list = g_slist_append (list, (gpointer) utf8);

	/* After: [ISO-8859-15, UTF-8] */
	list = _gtk_source_encoding_remove_duplicates (list, GTK_SOURCE_ENCODING_DUPLICATES_KEEP_LAST);

	g_assert_cmpint (2, ==, g_slist_length (list));
	g_assert_true (list->data == iso);
	g_assert_true (list->next->data == utf8);

	g_slist_free (list);
}

int
main (int argc, char **argv)
{
	gtk_test_init (&argc, &argv);

	g_test_add_func ("/Encoding/remove_duplicates", test_remove_duplicates);

	return g_test_run ();
}
