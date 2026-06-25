/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- */
/*
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2009, 2013 - Paolo Borelli
 * Copyright (C) 2009 - Ignacio Casal Quinteiro
 * Copyright (C) 2010 - Krzesimir Nowak
 * Copyright (C) 2013 - Sébastien Wilmet
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

#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

/* If we are running from the source dir (e.g. during make check)
 * we override the path to read from the data dir
 */
static void
init_default_manager (void)
{
	gchar *dir;

	dir = g_build_filename (TOP_SRCDIR, "data", "language-specs", NULL);

	if (g_file_test (dir, G_FILE_TEST_IS_DIR))
	{
		GtkSourceLanguageManager *lm;
		gchar **lang_dirs;

		lm = gtk_source_language_manager_get_default ();

		lang_dirs = g_new0 (gchar *, 2);
		lang_dirs[0] = dir;

		gtk_source_language_manager_set_search_path (lm, lang_dirs);
		g_strfreev (lang_dirs);
	}
	else
	{
		g_free (dir);
	}
}

static void
test_get_default (void)
{
	GtkSourceLanguageManager *lm1, *lm2;

	lm1 = gtk_source_language_manager_get_default ();
	lm2 = gtk_source_language_manager_get_default ();
	g_assert_true (lm1 == lm2);
}

static void
test_get_language (void)
{
	GtkSourceLanguageManager *lm;
	const gchar * const *ids;

	lm = gtk_source_language_manager_get_default ();
	ids = gtk_source_language_manager_get_language_ids (lm);
	g_assert_nonnull (ids);

	while (*ids != NULL)
	{
		GtkSourceLanguage *lang1, *lang2;

		lang1 = gtk_source_language_manager_get_language (lm, *ids);
		g_assert_nonnull (lang1);
		g_assert_true (GTK_SOURCE_IS_LANGUAGE (lang1));
		g_assert_cmpstr (*ids, == , gtk_source_language_get_id (lang1));

		/* langs are owned by the manager */
		lang2 = gtk_source_language_manager_get_language (lm, *ids);
		g_assert_true (lang1 == lang2);

		++ids;
	}
}

static void
test_guess_language_null_null (void)
{
	GtkSourceLanguageManager *lm = gtk_source_language_manager_get_default ();

	gtk_source_language_manager_guess_language (lm, NULL, NULL);
}

static void
test_guess_language_empty_null (void)
{
	GtkSourceLanguageManager *lm = gtk_source_language_manager_get_default ();

	gtk_source_language_manager_guess_language (lm, "", NULL);
}

static void
test_guess_language_null_empty (void)
{
	GtkSourceLanguageManager *lm = gtk_source_language_manager_get_default ();

	gtk_source_language_manager_guess_language (lm, NULL, "");
}

static void
test_guess_language_empty_empty (void)
{
	GtkSourceLanguageManager *lm = gtk_source_language_manager_get_default ();

	gtk_source_language_manager_guess_language (lm, "", "");
}

static void
test_guess_language (void)
{
	GtkSourceLanguageManager *lm;
	GtkSourceLanguage *l;

	lm = gtk_source_language_manager_get_default ();

	g_test_trap_subprocess ("/LanguageManager/guess-language/subprocess/null_null", 0, 0);
	g_test_trap_assert_failed ();

	g_test_trap_subprocess ("/LanguageManager/guess-language/subprocess/empty_null", 0, 0);
	g_test_trap_assert_failed ();

	g_test_trap_subprocess ("/LanguageManager/guess-language/subprocess/null_empty", 0, 0);
	g_test_trap_assert_failed ();

	g_test_trap_subprocess ("/LanguageManager/guess-language/subprocess/empty_empty", 0, 0);
	g_test_trap_assert_failed ();

	l = gtk_source_language_manager_guess_language (lm, "foo.abcdef", NULL);
	g_assert_null (l);

	l = gtk_source_language_manager_guess_language (lm, "foo.abcdef", "");
	g_assert_null (l);

	l = gtk_source_language_manager_guess_language (lm, NULL, "image/png");
	g_assert_null (l);

	l = gtk_source_language_manager_guess_language (lm, "", "image/png");
	g_assert_null (l);

	l = gtk_source_language_manager_guess_language (lm, "foo.c", NULL);
	g_assert_cmpstr (gtk_source_language_get_id (l), ==, "c");

	l = gtk_source_language_manager_guess_language (lm, "foo.c", "");
	g_assert_cmpstr (gtk_source_language_get_id (l), ==, "c");

	l = gtk_source_language_manager_guess_language (lm, NULL, "text/x-csrc");
	g_assert_cmpstr (gtk_source_language_get_id (l), ==, "c");

	l = gtk_source_language_manager_guess_language (lm, "", "text/x-csrc");
	g_assert_cmpstr (gtk_source_language_get_id (l), ==, "c");

	l = gtk_source_language_manager_guess_language (lm, "foo.c", "text/x-csrc");
	g_assert_cmpstr (gtk_source_language_get_id (l), ==, "c");

	l = gtk_source_language_manager_guess_language (lm, "foo.mo", "text/x-modelica");
	g_assert_cmpstr (gtk_source_language_get_id (l), ==, "modelica");

	l = gtk_source_language_manager_guess_language (lm, "foo.mo", "");
	g_assert_cmpstr (gtk_source_language_get_id (l), ==, "modelica");

	/* when in disagreement, glob wins */
	l = gtk_source_language_manager_guess_language (lm, "foo.c", "text/x-fortran");
	g_assert_cmpstr (gtk_source_language_get_id (l), ==, "c");

	/* when content type is a descendent of the mime matched by the glob, mime wins */
	l = gtk_source_language_manager_guess_language (lm, "foo.xml", "application/xslt+xml");
	g_assert_cmpstr (gtk_source_language_get_id (l), ==, "xslt");
}

int
main (int argc, char** argv)
{
	gtk_test_init (&argc, &argv);

	init_default_manager ();

	g_test_add_func ("/LanguageManager/get-default", test_get_default);
	g_test_add_func ("/LanguageManager/get-language", test_get_language);
	g_test_add_func ("/LanguageManager/guess-language", test_guess_language);
	g_test_add_func ("/LanguageManager/guess-language/subprocess/null_null", test_guess_language_null_null);
	g_test_add_func ("/LanguageManager/guess-language/subprocess/empty_null", test_guess_language_empty_null);
	g_test_add_func ("/LanguageManager/guess-language/subprocess/null_empty", test_guess_language_null_empty);
	g_test_add_func ("/LanguageManager/guess-language/subprocess/empty_empty", test_guess_language_empty_empty);

	return g_test_run();
}
