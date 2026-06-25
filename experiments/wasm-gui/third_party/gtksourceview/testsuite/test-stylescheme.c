/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- */
/*
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2015 - Paolo Borelli
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

typedef struct _TestFixture TestFixture;

struct _TestFixture {
	GtkSourceStyleSchemeManager *manager;
};

/* If we are running from the source dir (e.g. during make check)
 * we override the path to read from the data dir
 */
static void
test_fixture_setup (TestFixture   *fixture,
                    gconstpointer  data)
{
	gchar *dir;
	gchar **style_dirs;

	dir = g_build_filename (TOP_SRCDIR, "data", "styles", NULL);

	fixture->manager = gtk_source_style_scheme_manager_get_default ();

	if (g_file_test (dir, G_FILE_TEST_IS_DIR))
	{
		style_dirs = g_new0 (gchar *, 3);
		style_dirs[0] = dir;
		style_dirs[1] = g_test_build_filename (G_TEST_DIST, "styles", NULL);
	}
	else
	{
		const gchar * const *current;
		int i;

		g_free (dir);

		current = gtk_source_style_scheme_manager_get_search_path (fixture->manager);
		style_dirs = g_new0 (gchar *, g_strv_length ((gchar **)current) + 2);
		for (i = 0; current[i] != NULL; i++)
		{
			style_dirs[i] = g_strdup(current[i]);
		}
		style_dirs[i] = g_test_build_filename (G_TEST_DIST, "styles", NULL);
	}

	gtk_source_style_scheme_manager_set_search_path (fixture->manager, style_dirs);
	g_strfreev (style_dirs);
}

static void
test_fixture_teardown (TestFixture   *fixture,
                       gconstpointer  data)
{
}

static void
compare_strv (const gchar **strv,
	      const gchar **expected_strv)
{
	if (expected_strv != NULL)
	{
		guint n, i;

		n = g_strv_length ((gchar **) expected_strv);
		for (i = 0; i < n; i++)
		{
			g_assert_cmpstr (strv[i], ==, expected_strv[i]);
		}
	}
	else
	{
		g_assert_null (strv);
	}
}

static void
check_scheme (GtkSourceStyleScheme  *scheme,
              const gchar           *expected_id,
              const gchar           *expected_name,
              const gchar           *expected_description,
              const gchar          **expected_authors,
              const gchar           *style_id,
              const gchar           *background_rgba)
{
	GtkSourceStyle *style;

	g_assert_cmpstr (gtk_source_style_scheme_get_id (scheme), ==, expected_id);
	g_assert_cmpstr (gtk_source_style_scheme_get_name (scheme), ==, expected_name);
	g_assert_cmpstr (gtk_source_style_scheme_get_description (scheme), ==, expected_description);
	compare_strv ((const gchar **)gtk_source_style_scheme_get_authors (scheme), expected_authors);

	style = gtk_source_style_scheme_get_style (scheme, style_id);
	g_assert_true (GTK_SOURCE_IS_STYLE (style));

	if (background_rgba != NULL)
	{
		gchar *background = NULL;

		g_object_get (style,
		              "background", &background,
			      NULL);

		g_assert_cmpstr (background_rgba, ==, background);
		g_free (background);
	}
}

static void
test_scheme_properties (TestFixture   *fixture,
                        gconstpointer  data)
{
	GtkSourceStyleScheme *scheme;
	const gchar *authors[] = { "Paolo Borelli", "John Doe", NULL};

	scheme = gtk_source_style_scheme_manager_get_scheme (fixture->manager, "test");

	check_scheme (scheme, "test", "Test", "Test color scheme", authors, "def:comment", NULL);

	/* Check that net-address remapped correctly to "underlined" */
	check_scheme (scheme, "test", "Test", "Test color scheme", authors, "def:net-address", "#FFFFFF");
}

static void
test_named_color_alpha (TestFixture   *fixture,
                        gconstpointer  data)
{
	GtkSourceStyleScheme *scheme;
	GdkRGBA color1, color2;
	gboolean res;

	scheme = gtk_source_style_scheme_manager_get_scheme (fixture->manager, "test");

	/* Use these two semi private methods to compare a named color and a normal one */
	res = _gtk_source_style_scheme_get_current_line_color (scheme, &color1);
	g_assert_true (res);

	res = _gtk_source_style_scheme_get_background_pattern_color (scheme, &color2);
	g_assert_true (res);

	g_assert_true (gdk_rgba_equal (&color1, &color2));
}

int
main (int argc, char** argv)
{
	gtk_test_init (&argc, &argv);

	g_test_add ("/StyleScheme/scheme-properties", TestFixture, NULL, test_fixture_setup, test_scheme_properties, test_fixture_teardown);
	g_test_add ("/StyleScheme/named-colors-alpha", TestFixture, NULL, test_fixture_setup, test_named_color_alpha, test_fixture_teardown);

	return g_test_run();
}
