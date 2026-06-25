/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *
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

#define TEST_TYPE_SEARCH             (test_search_get_type ())
#define TEST_SEARCH(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), TEST_TYPE_SEARCH, TestSearch))
#define TEST_SEARCH_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), TEST_TYPE_SEARCH, TestSearchClass))
#define TEST_IS_SEARCH(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TEST_TYPE_SEARCH))
#define TEST_IS_SEARCH_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), TEST_TYPE_SEARCH))
#define TEST_SEARCH_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), TEST_TYPE_SEARCH, TestSearchClass))

typedef struct _TestSearch        TestSearch;
typedef struct _TestSearchClass   TestSearchClass;
typedef struct _TestSearchPrivate TestSearchPrivate;

struct _TestSearch
{
	GtkGrid parent;
	TestSearchPrivate *priv;
};

struct _TestSearchClass
{
	GtkGridClass parent_class;
};

struct _TestSearchPrivate
{
	GtkSourceView *source_view;
	GtkSourceBuffer *source_buffer;
	GtkSourceSearchContext *search_context;
	GtkSourceSearchSettings *search_settings;
	GtkEntry *replace_entry;
	GtkLabel *label_occurrences;
	GtkLabel *label_regex_error;

	guint idle_update_label_id;
};

GType test_search_get_type (void);

G_DEFINE_TYPE_WITH_PRIVATE (TestSearch, test_search, GTK_TYPE_GRID)

static void
open_file (TestSearch  *search,
	   const gchar *filename)
{
	gchar *contents;
	GError *error = NULL;
	GtkSourceLanguageManager *language_manager;
	GtkSourceLanguage *language;
	GtkTextIter iter;

	/* In a realistic application you would use GtkSourceFile of course. */
	if (!g_file_get_contents (filename, &contents, NULL, &error))
	{
		g_error ("Impossible to load file: %s", error->message);
	}

	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (search->priv->source_buffer),
				  contents,
				  -1);

	language_manager = gtk_source_language_manager_get_default ();
	language = gtk_source_language_manager_get_language (language_manager, "c");
	gtk_source_buffer_set_language (search->priv->source_buffer, language);

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (search->priv->source_buffer),
					&iter);

	gtk_text_buffer_select_range (GTK_TEXT_BUFFER (search->priv->source_buffer),
				      &iter,
				      &iter);

	g_free (contents);
}

static void
update_label_occurrences (TestSearch *search)
{
	gint occurrences_count;
	GtkTextIter select_start;
	GtkTextIter select_end;
	gint occurrence_pos;
	gchar *text;

	occurrences_count = gtk_source_search_context_get_occurrences_count (search->priv->search_context);

	gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (search->priv->source_buffer),
					      &select_start,
					      &select_end);

	occurrence_pos = gtk_source_search_context_get_occurrence_position (search->priv->search_context,
									    &select_start,
									    &select_end);

	if (occurrences_count == -1)
	{
		text = g_strdup ("");
	}
	else if (occurrence_pos == -1)
	{
		text = g_strdup_printf ("%u occurrences", occurrences_count);
	}
	else
	{
		text = g_strdup_printf ("%d of %u", occurrence_pos, occurrences_count);
	}

	gtk_label_set_text (search->priv->label_occurrences, text);
	g_free (text);
}

static void
update_label_regex_error (TestSearch *search)
{
	GError *error;

	error = gtk_source_search_context_get_regex_error (search->priv->search_context);

	if (error == NULL)
	{
		gtk_label_set_text (search->priv->label_regex_error, "");
		gtk_widget_hide (GTK_WIDGET (search->priv->label_regex_error));
	}
	else
	{
		gtk_label_set_text (search->priv->label_regex_error, error->message);
		gtk_widget_show (GTK_WIDGET (search->priv->label_regex_error));
		g_clear_error (&error);
	}
}

static void
search_entry_changed_cb (TestSearch *search,
			 GtkEntry   *entry)
{
	const gchar *text = gtk_entry_get_text (entry);
	gchar *unescaped_text = gtk_source_utils_unescape_search_text (text);

	gtk_source_search_settings_set_search_text (search->priv->search_settings, unescaped_text);
	g_free (unescaped_text);
}

static void
select_search_occurrence (TestSearch        *search,
			  const GtkTextIter *match_start,
			  const GtkTextIter *match_end)
{
	GtkTextMark *insert;

	gtk_text_buffer_select_range (GTK_TEXT_BUFFER (search->priv->source_buffer),
				      match_start,
				      match_end);

	insert = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (search->priv->source_buffer));

	gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (search->priv->source_view),
					    insert);
}

static void
backward_search_finished (GtkSourceSearchContext *search_context,
			  GAsyncResult           *result,
			  TestSearch             *search)
{
	GtkTextIter match_start;
	GtkTextIter match_end;

	if (gtk_source_search_context_backward_finish (search_context,
						       result,
						       &match_start,
						       &match_end,
						       NULL,
						       NULL))
	{
		select_search_occurrence (search, &match_start, &match_end);
	}
}

static void
button_previous_clicked_cb (TestSearch *search,
			    GtkButton  *button)
{
	GtkTextIter start_at;

	gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (search->priv->source_buffer),
					      &start_at,
					      NULL);

	gtk_source_search_context_backward_async (search->priv->search_context,
						  &start_at,
						  NULL,
						  (GAsyncReadyCallback)backward_search_finished,
						  search);
}

static void
forward_search_finished (GtkSourceSearchContext *search_context,
			 GAsyncResult           *result,
			 TestSearch             *search)
{
	GtkTextIter match_start;
	GtkTextIter match_end;

	if (gtk_source_search_context_forward_finish (search_context,
						      result,
						      &match_start,
						      &match_end,
						      NULL,
						      NULL))
	{
		select_search_occurrence (search, &match_start, &match_end);
	}
}

static void
button_next_clicked_cb (TestSearch *search,
			GtkButton  *button)
{
	GtkTextIter start_at;

	gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (search->priv->source_buffer),
					      NULL,
					      &start_at);

	gtk_source_search_context_forward_async (search->priv->search_context,
						 &start_at,
						 NULL,
						 (GAsyncReadyCallback)forward_search_finished,
						 search);
}

static void
button_replace_clicked_cb (TestSearch *search,
			   GtkButton  *button)
{
	GtkTextIter match_start;
	GtkTextIter match_end;
	GtkTextIter iter;
	GtkEntryBuffer *entry_buffer;
	gint replace_length;

	gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (search->priv->source_buffer),
					      &match_start,
					      &match_end);

	entry_buffer = gtk_entry_get_buffer (search->priv->replace_entry);
	replace_length = gtk_entry_buffer_get_bytes (entry_buffer);

	gtk_source_search_context_replace (search->priv->search_context,
					   &match_start,
					   &match_end,
					   gtk_entry_get_text (search->priv->replace_entry),
					   replace_length,
					   NULL);

	gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (search->priv->source_buffer),
					      NULL,
					      &iter);

	gtk_source_search_context_forward_async (search->priv->search_context,
						 &iter,
						 NULL,
						 (GAsyncReadyCallback)forward_search_finished,
						 search);
}

static void
button_replace_all_clicked_cb (TestSearch *search,
			       GtkButton  *button)
{
	GtkEntryBuffer *entry_buffer = gtk_entry_get_buffer (search->priv->replace_entry);
	gint replace_length = gtk_entry_buffer_get_bytes (entry_buffer);

	gtk_source_search_context_replace_all (search->priv->search_context,
					       gtk_entry_get_text (search->priv->replace_entry),
					       replace_length,
					       NULL);
}

static gboolean
update_label_idle_cb (TestSearch *search)
{
	search->priv->idle_update_label_id = 0;

	update_label_occurrences (search);

	return G_SOURCE_REMOVE;
}

static void
mark_set_cb (GtkTextBuffer *buffer,
	     GtkTextIter   *location,
	     GtkTextMark   *mark,
	     TestSearch    *search)
{
	GtkTextMark *insert;
	GtkTextMark *selection_bound;

	insert = gtk_text_buffer_get_insert (buffer);
	selection_bound = gtk_text_buffer_get_selection_bound (buffer);

	if ((mark == insert || mark == selection_bound) &&
	    search->priv->idle_update_label_id == 0)
	{
		search->priv->idle_update_label_id = g_idle_add ((GSourceFunc)update_label_idle_cb,
								 search);
	}
}

static void
highlight_toggled_cb (TestSearch      *search,
		      GtkToggleButton *button)
{
	gtk_source_search_context_set_highlight (search->priv->search_context,
						 gtk_toggle_button_get_active (button));
}

static void
match_case_toggled_cb (TestSearch      *search,
		       GtkToggleButton *button)
{
	gtk_source_search_settings_set_case_sensitive (search->priv->search_settings,
						       gtk_toggle_button_get_active (button));
}

static void
at_word_boundaries_toggled_cb (TestSearch      *search,
			       GtkToggleButton *button)
{
	gtk_source_search_settings_set_at_word_boundaries (search->priv->search_settings,
							   gtk_toggle_button_get_active (button));
}

static void
wrap_around_toggled_cb (TestSearch      *search,
			GtkToggleButton *button)
{
	gtk_source_search_settings_set_wrap_around (search->priv->search_settings,
						    gtk_toggle_button_get_active (button));
}

static void
regex_toggled_cb (TestSearch      *search,
		  GtkToggleButton *button)
{
	gtk_source_search_settings_set_regex_enabled (search->priv->search_settings,
						      gtk_toggle_button_get_active (button));
}

static void
test_search_dispose (GObject *object)
{
	TestSearch *search = TEST_SEARCH (object);

	g_clear_object (&search->priv->source_buffer);
	g_clear_object (&search->priv->search_context);
	g_clear_object (&search->priv->search_settings);

	G_OBJECT_CLASS (test_search_parent_class)->dispose (object);
}

static void
test_search_class_init (TestSearchClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = test_search_dispose;

	gtk_widget_class_set_template_from_resource (widget_class,
						     "/org/gnome/gtksourceview/tests/ui/test-search.ui");

	gtk_widget_class_bind_template_child_private (widget_class, TestSearch, source_view);
	gtk_widget_class_bind_template_child_private (widget_class, TestSearch, replace_entry);
	gtk_widget_class_bind_template_child_private (widget_class, TestSearch, label_occurrences);
	gtk_widget_class_bind_template_child_private (widget_class, TestSearch, label_regex_error);

	gtk_widget_class_bind_template_callback (widget_class, search_entry_changed_cb);
	gtk_widget_class_bind_template_callback (widget_class, button_previous_clicked_cb);
	gtk_widget_class_bind_template_callback (widget_class, button_next_clicked_cb);
	gtk_widget_class_bind_template_callback (widget_class, button_replace_clicked_cb);
	gtk_widget_class_bind_template_callback (widget_class, button_replace_all_clicked_cb);

	/* It is also possible to bind the properties with
	 * g_object_bind_property(), between the check buttons and the source
	 * buffer. But GtkBuilder and Glade don't support that yet.
	 */
	gtk_widget_class_bind_template_callback (widget_class, highlight_toggled_cb);
	gtk_widget_class_bind_template_callback (widget_class, match_case_toggled_cb);
	gtk_widget_class_bind_template_callback (widget_class, at_word_boundaries_toggled_cb);
	gtk_widget_class_bind_template_callback (widget_class, wrap_around_toggled_cb);
	gtk_widget_class_bind_template_callback (widget_class, regex_toggled_cb);
}

static void
test_search_init (TestSearch *search)
{
	search->priv = test_search_get_instance_private (search);

	gtk_widget_init_template (GTK_WIDGET (search));

	search->priv->source_buffer = GTK_SOURCE_BUFFER (
		gtk_text_view_get_buffer (GTK_TEXT_VIEW (search->priv->source_view)));

	g_object_ref (search->priv->source_buffer);

	open_file (search, TOP_SRCDIR "/gtksourceview/gtksourcesearchcontext.c");

	search->priv->search_settings = gtk_source_search_settings_new ();

	search->priv->search_context = gtk_source_search_context_new (search->priv->source_buffer,
								      search->priv->search_settings);

	g_signal_connect_swapped (search->priv->search_context,
				  "notify::occurrences-count",
				  G_CALLBACK (update_label_occurrences),
				  search);

	g_signal_connect (search->priv->source_buffer,
			  "mark-set",
			  G_CALLBACK (mark_set_cb),
			  search);

	g_signal_connect_swapped (search->priv->search_context,
				  "notify::regex-error",
				  G_CALLBACK (update_label_regex_error),
				  search);

	update_label_regex_error (search);
}

static TestSearch *
test_search_new (void)
{
	return g_object_new (test_search_get_type (), NULL);
}

gint
main (gint argc, gchar *argv[])
{
	GtkWidget *window;
	TestSearch *search;

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	gtk_window_set_default_size (GTK_WINDOW (window), 700, 500);

	g_signal_connect (window,
			  "destroy",
			  G_CALLBACK (gtk_main_quit),
			  NULL);

	search = test_search_new ();
	gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (search));

	gtk_widget_show (window);

	gtk_main ();

	return 0;
}
