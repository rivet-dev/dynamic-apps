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

#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

/* This measures the execution times for:
 * - basic search: with gtk_text_iter_forward_search();
 * - "smart" search: the first search with gtk_text_iter_forward_search(), later
 *   searches with gtk_text_iter_forward_to_tag_toggle();
 * - regex search.
 *
 * For the "smart" search, only the first search is measured. Later searches
 * are really fast (going to the previous/next occurrence is done in O(log n)).
 * Different search flags are also tested. We can see a big difference between
 * the case sensitive search and case insensitive.
 */

#define NB_LINES 100000

static void
on_notify_search_occurrences_count_cb (GtkSourceSearchContext *search_context,
				       GParamSpec             *spec,
				       GTimer                 *timer)
{
	g_print ("smart asynchronous search, case sensitive: %lf seconds.\n",
		 g_timer_elapsed (timer, NULL));

	gtk_main_quit ();
}

int
main (int argc, char *argv[])
{
	GtkSourceBuffer *buffer;
	GtkSourceSearchContext *search_context;
	GtkSourceSearchSettings *search_settings;
	GtkTextIter iter;
	GtkTextIter match_end;
	GTimer *timer;
	gint i;
	GtkTextSearchFlags flags;
	gchar *regex_pattern;

	gtk_init (&argc, &argv);

	buffer = gtk_source_buffer_new (NULL);

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &iter);

	for (i = 0; i < NB_LINES; i++)
	{
		gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer),
					&iter,
					"A line of text to fill the text buffer. Is it long enough?\n",
					-1);
	}

	gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &iter, "foo\n", -1);

	/* Basic search, no flags */

	timer = g_timer_new ();

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &iter);

	flags = 0;

	while (gtk_text_iter_forward_search (&iter, "foo", flags, NULL, &match_end, NULL))
	{
		iter = match_end;
	}

	g_timer_stop (timer);
	g_print ("basic forward search, no flags: %lf seconds.\n",
		 g_timer_elapsed (timer, NULL));

	/* Basic search, with flags always enabled by gsv */

	g_timer_start (timer);

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &iter);

	flags = GTK_TEXT_SEARCH_VISIBLE_ONLY | GTK_TEXT_SEARCH_TEXT_ONLY;

	while (gtk_text_iter_forward_search (&iter, "foo", flags, NULL, &match_end, NULL))
	{
		iter = match_end;
	}

	g_timer_stop (timer);
	g_print ("basic forward search, visible and text only flags: %lf seconds.\n",
		 g_timer_elapsed (timer, NULL));

	/* Basic search, with default flags in gsv */

	g_timer_start (timer);

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &iter);

	flags = GTK_TEXT_SEARCH_VISIBLE_ONLY |
		GTK_TEXT_SEARCH_TEXT_ONLY |
		GTK_TEXT_SEARCH_CASE_INSENSITIVE;

	while (gtk_text_iter_forward_search (&iter, "foo", flags, NULL, &match_end, NULL))
	{
		iter = match_end;
	}

	g_timer_stop (timer);
	g_print ("basic forward search, all flags: %lf seconds.\n",
		 g_timer_elapsed (timer, NULL));

	/* Smart forward search, with default flags in gsv */

	search_settings = gtk_source_search_settings_new ();
	search_context = gtk_source_search_context_new (buffer, search_settings);

	g_timer_start (timer);

	gtk_source_search_settings_set_search_text (search_settings, "foo");

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &iter);

	while (gtk_source_search_context_forward (search_context, &iter, NULL, &match_end, NULL))
	{
		iter = match_end;
	}

	g_timer_stop (timer);
	g_print ("smart synchronous forward search, case insensitive: %lf seconds.\n",
		 g_timer_elapsed (timer, NULL));

	/* Smart forward search, case sensitive */

	g_timer_start (timer);

	gtk_source_search_settings_set_search_text (search_settings, NULL);
	gtk_source_search_settings_set_case_sensitive (search_settings, TRUE);
	gtk_source_search_settings_set_search_text (search_settings, "foo");

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &iter);

	while (gtk_source_search_context_forward (search_context, &iter, NULL, &match_end, NULL))
	{
		iter = match_end;
	}

	g_timer_stop (timer);
	g_print ("smart synchronous forward search, case sensitive: %lf seconds.\n",
		 g_timer_elapsed (timer, NULL));

	/* Regex search: search "foo" */

	g_timer_start (timer);

	gtk_source_search_settings_set_search_text (search_settings, NULL);
	gtk_source_search_settings_set_regex_enabled (search_settings, TRUE);
	gtk_source_search_settings_set_search_text (search_settings, "foo");

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &iter);

	while (gtk_source_search_context_forward (search_context, &iter, NULL, &match_end, NULL))
	{
		iter = match_end;
	}

	g_timer_stop (timer);
	g_print ("regex search: 'foo' (no partial matches): %lf seconds.\n",
		 g_timer_elapsed (timer, NULL));

	/* Regex search: search "fill" */

	g_timer_start (timer);

	gtk_source_search_settings_set_search_text (search_settings, "fill");

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &iter);

	while (gtk_source_search_context_forward (search_context, &iter, NULL, &match_end, NULL))
	{
		iter = match_end;
	}

	g_timer_stop (timer);
	g_print ("regex search: 'fill' (no partial matches): %lf seconds.\n",
		 g_timer_elapsed (timer, NULL));

	/* Regex search: search single lines */

	g_timer_start (timer);

	gtk_source_search_settings_set_search_text (search_settings, ".*");

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &iter);

	while (gtk_source_search_context_forward (search_context, &iter, NULL, &match_end, NULL))
	{
		iter = match_end;
	}

	g_timer_stop (timer);
	g_print ("regex search: match single lines (no partial matches): %lf seconds.\n",
		 g_timer_elapsed (timer, NULL));

	/* Regex search: search matches of 3 lines */

	g_timer_start (timer);

	/* The space at the beginning of the pattern permits to not have contiguous
	 * matches. There is a performance issue with contiguous matches.
	 */
	gtk_source_search_settings_set_search_text (search_settings, " (.*\n){3}");

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &iter);

	while (gtk_source_search_context_forward (search_context, &iter, NULL, &match_end, NULL))
	{
		iter = match_end;
	}

	g_timer_stop (timer);
	g_print ("regex search: matches of 3 lines (small partial matches): %lf seconds.\n",
		 g_timer_elapsed (timer, NULL));

	/* Regex search: search matches of really big chunks */

	g_timer_start (timer);

	regex_pattern = g_strdup_printf (" (.*\n){%d}", NB_LINES / 10);
	gtk_source_search_settings_set_search_text (search_settings, regex_pattern);
	g_free (regex_pattern);

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &iter);

	while (gtk_source_search_context_forward (search_context, &iter, NULL, &match_end, NULL))
	{
		iter = match_end;
	}

	g_timer_stop (timer);
	g_print ("regex search: 10 matches of %d lines (big partial matches): %lf seconds.\n",
		 NB_LINES / 10,
		 g_timer_elapsed (timer, NULL));

	/* Smart search, case sensitive, asynchronous */

	/* The asynchronous overhead doesn't depend on the search flags, it
	 * depends on the maximum number of lines to scan in one batch, and
	 * (obviously), on the buffer size.
	 * You can tune SCAN_BATCH_SIZE in gtksourcesearchcontext.c to see a
	 * difference in the overhead.
	 */

	g_signal_connect (search_context,
			  "notify::occurrences-count",
			  G_CALLBACK (on_notify_search_occurrences_count_cb),
			  timer);

	g_timer_start (timer);

	gtk_source_search_settings_set_search_text (search_settings, NULL);
	gtk_source_search_settings_set_regex_enabled (search_settings, FALSE);
	gtk_source_search_settings_set_search_text (search_settings, "foo");

	gtk_main ();
	return 0;
}
