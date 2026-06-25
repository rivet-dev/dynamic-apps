/*
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2013 - Sébastien Wilmet <swilmet@gnome.org>
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

static void
test_unescape_search_text (void)
{
	gchar *unescaped_text;

	unescaped_text = gtk_source_utils_unescape_search_text ("\\n");
	g_assert_cmpstr (unescaped_text, ==, "\n");
	g_free (unescaped_text);

	unescaped_text = gtk_source_utils_unescape_search_text ("\\r");
	g_assert_cmpstr (unescaped_text, ==, "\r");
	g_free (unescaped_text);

	unescaped_text = gtk_source_utils_unescape_search_text ("\\t");
	g_assert_cmpstr (unescaped_text, ==, "\t");
	g_free (unescaped_text);

	unescaped_text = gtk_source_utils_unescape_search_text ("\\\\");
	g_assert_cmpstr (unescaped_text, ==, "\\");
	g_free (unescaped_text);

	unescaped_text = gtk_source_utils_unescape_search_text ("foo\\n bar\\r ß\\t hello\\\\blah");
	g_assert_cmpstr (unescaped_text, ==, "foo\n bar\r ß\t hello\\blah");
	g_free (unescaped_text);

	unescaped_text = gtk_source_utils_unescape_search_text ("foo\n bar\r ß\t hello\\blah");
	g_assert_cmpstr (unescaped_text, ==, "foo\n bar\r ß\t hello\\blah");
	g_free (unescaped_text);
}

static void
test_escape_search_text (void)
{
	gchar *escaped_text;

	escaped_text = gtk_source_utils_escape_search_text ("\n");
	g_assert_cmpstr (escaped_text, ==, "\\n");
	g_free (escaped_text);

	escaped_text = gtk_source_utils_escape_search_text ("\r");
	g_assert_cmpstr (escaped_text, ==, "\\r");
	g_free (escaped_text);

	escaped_text = gtk_source_utils_escape_search_text ("\t");
	g_assert_cmpstr (escaped_text, ==, "\\t");
	g_free (escaped_text);

	escaped_text = gtk_source_utils_escape_search_text ("\\");
	g_assert_cmpstr (escaped_text, ==, "\\\\");
	g_free (escaped_text);

	escaped_text = gtk_source_utils_escape_search_text ("foo\n bar\r ß\t hello\\blah");
	g_assert_cmpstr (escaped_text, ==, "foo\\n bar\\r ß\\t hello\\\\blah");
	g_free (escaped_text);
}

int
main (int argc, char **argv)
{
	gtk_test_init (&argc, &argv);

	g_test_add_func ("/Utils/unescape_search_text", test_unescape_search_text);
	g_test_add_func ("/Utils/escape_search_text", test_escape_search_text);

	return g_test_run ();
}
