/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- */
/*
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2013, 2014, 2015 - Sébastien Wilmet <swilmet@gnome.org>
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

static void
insert_text (GtkSourceBuffer *buffer,
	     const gchar     *text)
{
	GtkTextBuffer *text_buffer = GTK_TEXT_BUFFER (buffer);
	GtkTextIter iter;

	gtk_text_buffer_begin_user_action (text_buffer);
	gtk_text_buffer_get_end_iter (text_buffer, &iter);
	gtk_text_buffer_insert (text_buffer, &iter, text, -1);
	gtk_text_buffer_end_user_action (text_buffer);
}

static void
delete_first_line (GtkSourceBuffer *buffer)
{
	GtkTextIter start;
	GtkTextIter end;

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &start);
	gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (buffer), &end, 1);

	gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
	gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &start, &end);
	gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));
}

/* If forward is FALSE, the Backspace key is simulated. If forward is TRUE, the
 * Delete key is simulated.
 */
static void
delete_char_at_offset (GtkSourceBuffer *source_buffer,
		       gint             offset,
		       gboolean         forward)
{
	GtkTextBuffer *buffer = GTK_TEXT_BUFFER (source_buffer);
	GtkTextIter start;
	GtkTextIter end;

	gtk_text_buffer_get_iter_at_offset (buffer, &start, offset);
	end = start;
	gtk_text_iter_forward_char (&end);

	if (forward)
	{
		gtk_text_buffer_place_cursor (buffer, &start);
	}
	else
	{
		GtkTextIter start_copy;

		gtk_text_buffer_place_cursor (buffer, &end);

		/* Swap start and end so that start > end, to test if in the
		 * delete-range callback start and end are reordered.
		 */
		start_copy = start;
		start = end;
		end = start_copy;
	}

	gtk_text_buffer_begin_user_action (buffer);
	gtk_text_buffer_delete (buffer, &start, &end);
	gtk_text_buffer_end_user_action (buffer);
}

static gchar *
get_contents (GtkSourceBuffer *buffer)
{
	GtkTextIter start;
	GtkTextIter end;

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &start);
	gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (buffer), &end);

	return gtk_text_buffer_get_text (GTK_TEXT_BUFFER (buffer), &start, &end, TRUE);
}

static void
check_max_undo_levels (GtkSourceBuffer *buffer,
		       gboolean         several_user_actions)
{
	gint max_levels = gtk_source_buffer_get_max_undo_levels (buffer);
	gint nb_redos = 0;
	gint nb_undos = 0;
	gint i = 0;

	g_assert_cmpint (max_levels, >=, 0);

	/* Redo all actions */
	while (gtk_source_buffer_can_redo (buffer))
	{
		gtk_source_buffer_redo (buffer);
		nb_redos++;
		g_assert_cmpint (nb_redos, <=, max_levels);
	}

	/* Undo all actions */
	while (gtk_source_buffer_can_undo (buffer))
	{
		gtk_source_buffer_undo (buffer);
		nb_undos++;
		g_assert_cmpint (nb_undos, <=, max_levels);
	}

	/* Add max_levels+1 actions */
	for (i = 0; i <= max_levels; i++)
	{
		if (several_user_actions)
		{
			gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
			insert_text (buffer, "foobar\n");
			delete_char_at_offset (buffer, 0, FALSE);
			gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));
		}
		else
		{
			insert_text (buffer, "foobar\n");
		}
	}

	/* Check number of possible undos */
	nb_undos = 0;
	while (gtk_source_buffer_can_undo (buffer))
	{
		gtk_source_buffer_undo (buffer);
		nb_undos++;
	}

	g_assert_cmpint (nb_undos, ==, max_levels);
}

static void
test_get_set_max_undo_levels (void)
{
	GtkSourceBuffer *buffer = gtk_source_buffer_new (NULL);

	g_assert_cmpint (gtk_source_buffer_get_max_undo_levels (buffer), >=, -1);

	gtk_source_buffer_set_max_undo_levels (buffer, -1);
	g_assert_cmpint (gtk_source_buffer_get_max_undo_levels (buffer), ==, -1);

	gtk_source_buffer_set_max_undo_levels (buffer, 3);
	g_assert_cmpint (gtk_source_buffer_get_max_undo_levels (buffer), ==, 3);

	g_object_unref (buffer);
}

static void
test_single_action (void)
{
	GtkSourceBuffer *buffer = gtk_source_buffer_new (NULL);
	gtk_source_buffer_set_max_undo_levels (buffer, -1);

	g_assert_false (gtk_source_buffer_can_undo (buffer));
	g_assert_false (gtk_source_buffer_can_redo (buffer));

	insert_text (buffer, "foo");
	g_assert_true (gtk_source_buffer_can_undo (buffer));
	g_assert_false (gtk_source_buffer_can_redo (buffer));

	gtk_source_buffer_undo (buffer);
	g_assert_false (gtk_source_buffer_can_undo (buffer));
	g_assert_true (gtk_source_buffer_can_redo (buffer));

	gtk_source_buffer_redo (buffer);
	g_assert_true (gtk_source_buffer_can_undo (buffer));
	g_assert_false (gtk_source_buffer_can_redo (buffer));

	g_object_unref (buffer);
}

static void
test_lose_redo_actions (void)
{
	GtkSourceBuffer *buffer = gtk_source_buffer_new (NULL);
	gtk_source_buffer_set_max_undo_levels (buffer, -1);

	insert_text (buffer, "foo\n");
	insert_text (buffer, "bar\n");
	g_assert_true (gtk_source_buffer_can_undo (buffer));
	g_assert_false (gtk_source_buffer_can_redo (buffer));

	gtk_source_buffer_undo (buffer);
	g_assert_true (gtk_source_buffer_can_undo (buffer));
	g_assert_true (gtk_source_buffer_can_redo (buffer));

	insert_text (buffer, "baz\n");
	g_assert_true (gtk_source_buffer_can_undo (buffer));
	g_assert_false (gtk_source_buffer_can_redo (buffer));

	g_object_unref (buffer);
}

static void
test_max_undo_levels (void)
{
	GtkSourceBuffer *buffer = gtk_source_buffer_new (NULL);

	gint min = 0;
	gint max = 5;
	gint i;

	/* Increase */
	for (i = min; i <= max; i++)
	{
		gtk_source_buffer_set_max_undo_levels (buffer, i);
		check_max_undo_levels (buffer, FALSE);
		check_max_undo_levels (buffer, TRUE);
	}

	/* Decrease */
	for (i = max; i >= min; i--)
	{
		gtk_source_buffer_set_max_undo_levels (buffer, i);
		check_max_undo_levels (buffer, FALSE);
		check_max_undo_levels (buffer, TRUE);
	}

	/* can redo: TRUE -> FALSE */
	gtk_source_buffer_set_max_undo_levels (buffer, 3);
	check_max_undo_levels (buffer, FALSE);
	check_max_undo_levels (buffer, TRUE);

	while (gtk_source_buffer_can_redo (buffer))
	{
		gtk_source_buffer_redo (buffer);
	}

	gtk_source_buffer_undo (buffer);
	g_assert_true (gtk_source_buffer_can_redo (buffer));

	gtk_source_buffer_set_max_undo_levels (buffer, 2);
	g_assert_false (gtk_source_buffer_can_redo (buffer));

	g_object_unref (buffer);
}

static void
test_not_undoable_action (void)
{
	GtkSourceBuffer *buffer = gtk_source_buffer_new (NULL);
	gtk_source_buffer_set_max_undo_levels (buffer, -1);

	/* On empty buffer */
	gtk_source_buffer_begin_not_undoable_action (buffer);
	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), "foo\n", -1);
	gtk_source_buffer_end_not_undoable_action (buffer);

	g_assert_false (gtk_source_buffer_can_undo (buffer));
	g_assert_false (gtk_source_buffer_can_redo (buffer));

	/* begin_user_action inside */
	gtk_source_buffer_begin_not_undoable_action (buffer);
	gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
	gtk_text_buffer_insert_at_cursor (GTK_TEXT_BUFFER (buffer), "bar\n", -1);
	gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));
	gtk_source_buffer_end_not_undoable_action (buffer);

	g_assert_false (gtk_source_buffer_can_undo (buffer));
	g_assert_false (gtk_source_buffer_can_redo (buffer));

	/* In the middle of an action history */
	insert_text (buffer, "foo\n");
	insert_text (buffer, "bar\n");
	g_assert_true (gtk_source_buffer_can_undo (buffer));
	g_assert_false (gtk_source_buffer_can_redo (buffer));

	gtk_source_buffer_undo (buffer);
	g_assert_true (gtk_source_buffer_can_undo (buffer));
	g_assert_true (gtk_source_buffer_can_redo (buffer));

	gtk_source_buffer_begin_not_undoable_action (buffer);
	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), "new text\n", -1);
	gtk_source_buffer_end_not_undoable_action (buffer);

	g_assert_false (gtk_source_buffer_can_undo (buffer));
	g_assert_false (gtk_source_buffer_can_redo (buffer));

	/* Empty undoable action */
	insert_text (buffer, "foo\n");
	insert_text (buffer, "bar\n");
	gtk_source_buffer_undo (buffer);
	g_assert_true (gtk_source_buffer_can_undo (buffer));
	g_assert_true (gtk_source_buffer_can_redo (buffer));

	gtk_source_buffer_begin_not_undoable_action (buffer);
	gtk_source_buffer_end_not_undoable_action (buffer);

	g_assert_false (gtk_source_buffer_can_undo (buffer));
	g_assert_false (gtk_source_buffer_can_redo (buffer));

	/* Behavior _during_ a not undoable action */

	/* The API doesn't explain what should be the behaviors in the
	 * following situations (also for nested). So it is just "undefinied
	 * behavior", and it can change in the future.
	 * What is certain is that after the last end_not_undoable_action() (if
	 * the calls are nested), the history is cleared and it is not possible
	 * to undo or redo.
	 */
	insert_text (buffer, "foo\n");
	insert_text (buffer, "bar\n");
	gtk_source_buffer_undo (buffer);

	gtk_source_buffer_begin_not_undoable_action (buffer);
	g_assert_true (gtk_source_buffer_can_undo (buffer));
	g_assert_true (gtk_source_buffer_can_redo (buffer));

	gtk_source_buffer_redo (buffer);
	g_assert_true (gtk_source_buffer_can_undo (buffer));
	g_assert_false (gtk_source_buffer_can_redo (buffer));

	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), "new text\n", -1);

	gtk_source_buffer_end_not_undoable_action (buffer);
	g_assert_false (gtk_source_buffer_can_undo (buffer));
	g_assert_false (gtk_source_buffer_can_redo (buffer));

	/* Nested */
	insert_text (buffer, "foo\n");
	insert_text (buffer, "bar\n");
	gtk_source_buffer_undo (buffer);

	gtk_source_buffer_begin_not_undoable_action (buffer);
	insert_text (buffer, "foo\n");

	gtk_source_buffer_begin_not_undoable_action (buffer);
	insert_text (buffer, "inserted text\n");

	gtk_source_buffer_end_not_undoable_action (buffer);
	insert_text (buffer, "blah\n");

	gtk_source_buffer_end_not_undoable_action (buffer);
	g_assert_false (gtk_source_buffer_can_undo (buffer));
	g_assert_false (gtk_source_buffer_can_redo (buffer));

	insert_text (buffer, "blah\n");
	g_assert_true (gtk_source_buffer_can_undo (buffer));
	g_assert_false (gtk_source_buffer_can_redo (buffer));

	g_object_unref (buffer);
}

static void
check_contents_history (GtkSourceBuffer *buffer,
			GList           *contents_history)
{
	GList *l;

	/* Go to the end */
	while (gtk_source_buffer_can_redo (buffer))
	{
		gtk_source_buffer_redo (buffer);
	}

	/* Check all the undo's */
	for (l = g_list_last (contents_history); l != NULL; l = l->prev)
	{
		gchar *cur_contents = get_contents (buffer);
		g_assert_cmpstr (cur_contents, ==, l->data);
		g_free (cur_contents);

		if (gtk_source_buffer_can_undo (buffer))
		{
			gtk_source_buffer_undo (buffer);
		}
		else
		{
			g_assert_null (l->prev);
		}
	}

	/* Check all the redo's */
	for (l = contents_history; l != NULL; l = l->next)
	{
		gchar *cur_contents = get_contents (buffer);
		g_assert_cmpstr (cur_contents, ==, l->data);
		g_free (cur_contents);

		if (gtk_source_buffer_can_redo (buffer))
		{
			gtk_source_buffer_redo (buffer);
		}
		else
		{
			g_assert_null (l->next);
		}
	}
}

static void
test_contents (void)
{
	GtkSourceBuffer *buffer = gtk_source_buffer_new (NULL);
	GList *contents_history = g_list_append (NULL, get_contents (buffer));

	gtk_source_buffer_set_max_undo_levels (buffer, -1);

	insert_text (buffer, "hello\n");
	contents_history = g_list_append (contents_history, get_contents (buffer));
	check_contents_history (buffer, contents_history);

	insert_text (buffer, "world\n");
	contents_history = g_list_append (contents_history, get_contents (buffer));
	check_contents_history (buffer, contents_history);

	delete_first_line (buffer);
	contents_history = g_list_append (contents_history, get_contents (buffer));
	check_contents_history (buffer, contents_history);

	delete_first_line (buffer);
	contents_history = g_list_append (contents_history, get_contents (buffer));
	check_contents_history (buffer, contents_history);

	g_list_free_full (contents_history, g_free);
	g_object_unref (buffer);
}

static void
test_merge_actions (void)
{
	GtkSourceBuffer *buffer = gtk_source_buffer_new (NULL);
	GList *contents_history = g_list_append (NULL, get_contents (buffer));

	gtk_source_buffer_set_max_undo_levels (buffer, -1);

	/* Different action types (an insert followed by a delete) */
	insert_text (buffer, "a");
	contents_history = g_list_append (contents_history, get_contents (buffer));

	delete_char_at_offset (buffer, 0, FALSE);
	contents_history = g_list_append (contents_history, get_contents (buffer));
	check_contents_history (buffer, contents_history);

	/* Mergeable inserts */
	insert_text (buffer, "b");
	insert_text (buffer, "c");
	contents_history = g_list_append (contents_history, get_contents (buffer));
	check_contents_history (buffer, contents_history);

	/* Mergeable deletes */
	delete_char_at_offset (buffer, 1, FALSE);
	delete_char_at_offset (buffer, 0, FALSE);
	contents_history = g_list_append (contents_history, get_contents (buffer));
	check_contents_history (buffer, contents_history);

	/* Non-mergeable deletes */
	insert_text (buffer, "def");
	contents_history = g_list_append (contents_history, get_contents (buffer));

	delete_char_at_offset (buffer, 2, FALSE);
	contents_history = g_list_append (contents_history, get_contents (buffer));

	delete_char_at_offset (buffer, 0, TRUE);
	delete_char_at_offset (buffer, 0, TRUE);
	contents_history = g_list_append (contents_history, get_contents (buffer));
	check_contents_history (buffer, contents_history);

	/* Insert two words */
	insert_text (buffer, "g");
	insert_text (buffer, "h");
	contents_history = g_list_append (contents_history, get_contents (buffer));

	insert_text (buffer, " ");
	insert_text (buffer, "i");
	contents_history = g_list_append (contents_history, get_contents (buffer));
	check_contents_history (buffer, contents_history);

	/* Delete the two words (with backspace) */
	delete_char_at_offset (buffer, 3, FALSE);
	delete_char_at_offset (buffer, 2, FALSE);
	contents_history = g_list_append (contents_history, get_contents (buffer));

	delete_char_at_offset (buffer, 1, FALSE);
	delete_char_at_offset (buffer, 0, FALSE);
	contents_history = g_list_append (contents_history, get_contents (buffer));
	check_contents_history (buffer, contents_history);

	/* Delete two words (with delete) */
	insert_text (buffer, "jk l");
	contents_history = g_list_append (contents_history, get_contents (buffer));

	delete_char_at_offset (buffer, 0, TRUE);
	delete_char_at_offset (buffer, 0, TRUE);
	contents_history = g_list_append (contents_history, get_contents (buffer));

	delete_char_at_offset (buffer, 0, TRUE);
	delete_char_at_offset (buffer, 0, TRUE);
	contents_history = g_list_append (contents_history, get_contents (buffer));
	check_contents_history (buffer, contents_history);

	g_list_free_full (contents_history, g_free);
	g_object_unref (buffer);
}

static void
test_several_user_actions (void)
{
	GtkSourceBuffer *source_buffer = gtk_source_buffer_new (NULL);
	GtkTextBuffer *text_buffer = GTK_TEXT_BUFFER (source_buffer);
	GList *contents_history = g_list_append (NULL, get_contents (source_buffer));
	GtkTextIter iter;

	gtk_source_buffer_set_max_undo_levels (source_buffer, -1);

	/* Contiguous insertions */
	gtk_text_buffer_begin_user_action (text_buffer);
	insert_text (source_buffer, "hello\n");
	insert_text (source_buffer, "world\n");
	gtk_text_buffer_end_user_action (text_buffer);

	contents_history = g_list_append (contents_history, get_contents (source_buffer));
	check_contents_history (source_buffer, contents_history);

	/* Non-contiguous insertions */
	gtk_text_buffer_begin_user_action (text_buffer);
	gtk_text_buffer_get_iter_at_offset (text_buffer, &iter, 0);
	gtk_text_buffer_insert (text_buffer, &iter, "a", -1);
	gtk_text_buffer_get_iter_at_offset (text_buffer, &iter, 2);
	gtk_text_buffer_insert (text_buffer, &iter, "b", -1);
	gtk_text_buffer_end_user_action (text_buffer);

	contents_history = g_list_append (contents_history, get_contents (source_buffer));
	check_contents_history (source_buffer, contents_history);

	/* Non-contiguous deletions (removes the 'a' and 'b' just inserted). */
	gtk_text_buffer_begin_user_action (text_buffer);
	delete_char_at_offset (source_buffer, 2, FALSE);
	delete_char_at_offset (source_buffer, 0, FALSE);
	gtk_text_buffer_end_user_action (text_buffer);

	contents_history = g_list_append (contents_history, get_contents (source_buffer));
	check_contents_history (source_buffer, contents_history);

	/* Contiguous deletions */
	gtk_text_buffer_begin_user_action (text_buffer);
	delete_first_line (source_buffer);
	delete_first_line (source_buffer);
	gtk_text_buffer_end_user_action (text_buffer);

	contents_history = g_list_append (contents_history, get_contents (source_buffer));
	check_contents_history (source_buffer, contents_history);

	/* Mixed insertions/deletions */
	gtk_text_buffer_begin_user_action (text_buffer);
	gtk_text_buffer_set_text (text_buffer, "ahbello\n", -1);
	delete_char_at_offset (source_buffer, 2, FALSE);
	delete_char_at_offset (source_buffer, 0, FALSE);
	insert_text (source_buffer, "world\n");
	gtk_text_buffer_end_user_action (text_buffer);

	contents_history = g_list_append (contents_history, get_contents (source_buffer));
	check_contents_history (source_buffer, contents_history);

	g_list_free_full (contents_history, g_free);
	g_object_unref (source_buffer);
}

static void
test_modified (void)
{
	GtkSourceBuffer *source_buffer;
	GtkTextBuffer *text_buffer;

	source_buffer = gtk_source_buffer_new (NULL);
	text_buffer = GTK_TEXT_BUFFER (source_buffer);
	gtk_source_buffer_set_max_undo_levels (source_buffer, -1);

	gtk_text_buffer_set_modified (text_buffer, FALSE);
	insert_text (source_buffer, "foo\n");

	g_assert_true (gtk_text_buffer_get_modified (text_buffer));
	gtk_source_buffer_undo (source_buffer);
	g_assert_false (gtk_text_buffer_get_modified (text_buffer));
	gtk_source_buffer_redo (source_buffer);
	g_assert_true (gtk_text_buffer_get_modified (text_buffer));

	gtk_text_buffer_set_modified (text_buffer, FALSE);
	gtk_source_buffer_undo (source_buffer);
	g_assert_true (gtk_text_buffer_get_modified (text_buffer));
	gtk_source_buffer_redo (source_buffer);
	g_assert_false (gtk_text_buffer_get_modified (text_buffer));

	gtk_source_buffer_undo (source_buffer);
	g_assert_true (gtk_text_buffer_get_modified (text_buffer));
	insert_text (source_buffer, "bar\n");
	g_assert_true (gtk_text_buffer_get_modified (text_buffer));
	gtk_source_buffer_undo (source_buffer);
	g_assert_true (gtk_text_buffer_get_modified (text_buffer));

	g_object_unref (source_buffer);

	/* Inside not undoable action. */
	source_buffer = gtk_source_buffer_new (NULL);
	text_buffer = GTK_TEXT_BUFFER (source_buffer);
	gtk_source_buffer_set_max_undo_levels (source_buffer, -1);

	gtk_text_buffer_set_modified (text_buffer, TRUE);

	gtk_source_buffer_begin_not_undoable_action (source_buffer);
	insert_text (source_buffer, "a\n");
	gtk_text_buffer_set_modified (text_buffer, FALSE);
	gtk_source_buffer_end_not_undoable_action (source_buffer);

	insert_text (source_buffer, "b\n");
	g_assert_true (gtk_text_buffer_get_modified (text_buffer));

	gtk_source_buffer_undo (source_buffer);
	g_assert_false (gtk_text_buffer_get_modified (text_buffer));

	g_object_unref (source_buffer);
}

static void
empty_user_actions (GtkTextBuffer *text_buffer,
		    gint           count)
{
	gint i;

	for (i = 0; i < count; i++)
	{
		gtk_text_buffer_begin_user_action (text_buffer);
		gtk_text_buffer_end_user_action (text_buffer);
	}
}

static void
test_empty_user_actions (void)
{
	GtkSourceBuffer *source_buffer;
	GtkTextBuffer *text_buffer;
	GList *contents_history = NULL;

	source_buffer = gtk_source_buffer_new (NULL);
	text_buffer = GTK_TEXT_BUFFER (source_buffer);
	gtk_source_buffer_set_max_undo_levels (source_buffer, -1);

	contents_history = g_list_append (contents_history, get_contents (source_buffer));

	empty_user_actions (text_buffer, 3);
	check_contents_history (source_buffer, contents_history);

	insert_text (source_buffer, "foo\n");
	contents_history = g_list_append (contents_history, get_contents (source_buffer));
	check_contents_history (source_buffer, contents_history);

	empty_user_actions (text_buffer, 1);
	check_contents_history (source_buffer, contents_history);

	insert_text (source_buffer, "bar\n");
	contents_history = g_list_append (contents_history, get_contents (source_buffer));
	check_contents_history (source_buffer, contents_history);

	gtk_source_buffer_undo (source_buffer);
	empty_user_actions (text_buffer, 1);
	check_contents_history (source_buffer, contents_history);

	g_object_unref (source_buffer);
	g_list_free_full (contents_history, g_free);
}

/* Test for https://bugzilla.gnome.org/show_bug.cgi?id=672893
 * TODO More complete unit tests for selection restoring would be better.
 */
static void
test_bug_672893_selection_restoring (void)
{
	GtkSourceBuffer *source_buffer;
	GtkTextBuffer *text_buffer;
	GtkTextIter start;
	GtkTextIter end;
	GtkTextIter iter;

	source_buffer = gtk_source_buffer_new (NULL);
	text_buffer = GTK_TEXT_BUFFER (source_buffer);
	gtk_source_buffer_set_max_undo_levels (source_buffer, -1);

	gtk_text_buffer_set_text (text_buffer, "What if it's just all green cheese.", -1);

	/* Delete selection */
	gtk_text_buffer_get_iter_at_offset (text_buffer, &start, 0);
	gtk_text_buffer_get_iter_at_offset (text_buffer, &end, 8);
	gtk_text_buffer_select_range (text_buffer, &start, &end);
	gtk_text_buffer_delete_selection (text_buffer, TRUE, TRUE);

	gtk_text_buffer_get_selection_bounds (text_buffer, &start, &end);
	g_assert_cmpint (0, ==, gtk_text_iter_get_offset (&start));
	g_assert_cmpint (0, ==, gtk_text_iter_get_offset (&end));

	/* Undo -> selection restored */
	gtk_source_buffer_undo (source_buffer);
	gtk_text_buffer_get_selection_bounds (text_buffer, &start, &end);
	g_assert_cmpint (0, ==, gtk_text_iter_get_offset (&start));
	g_assert_cmpint (8, ==, gtk_text_iter_get_offset (&end));

	/* Click somewhere else */
	gtk_text_buffer_get_end_iter (text_buffer, &iter);
	gtk_text_buffer_place_cursor (text_buffer, &iter);

	/* Redo the deletion -> no selection */
	gtk_source_buffer_redo (source_buffer);
	gtk_text_buffer_get_selection_bounds (text_buffer, &start, &end);
	g_assert_cmpint (0, ==, gtk_text_iter_get_offset (&start));
	g_assert_cmpint (0, ==, gtk_text_iter_get_offset (&end));

	/* Undo -> selection still restored correctly, even if we clicked
	 * somewhere else.
	 */
	gtk_source_buffer_undo (source_buffer);
	gtk_text_buffer_get_selection_bounds (text_buffer, &start, &end);
	g_assert_cmpint (0, ==, gtk_text_iter_get_offset (&start));
	g_assert_cmpint (8, ==, gtk_text_iter_get_offset (&end));

	g_object_unref (source_buffer);
}

static void
test_mix_user_action_and_not_undoable_action (void)
{
	GtkSourceBuffer *source_buffer;
	GtkTextBuffer *text_buffer;
	GList *contents_history = NULL;

	source_buffer = gtk_source_buffer_new (NULL);
	text_buffer = GTK_TEXT_BUFFER (source_buffer);

	gtk_source_buffer_set_max_undo_levels (source_buffer, -1);

	/* Case 1 */
	gtk_text_buffer_set_text (text_buffer, "", -1);

	gtk_text_buffer_begin_user_action (text_buffer);
	gtk_source_buffer_begin_not_undoable_action (source_buffer);
	gtk_source_buffer_end_not_undoable_action (source_buffer);
	contents_history = g_list_append (contents_history, get_contents (source_buffer));

	gtk_text_buffer_insert_at_cursor (text_buffer, "a\n", -1);
	gtk_text_buffer_end_user_action (text_buffer);
	contents_history = g_list_append (contents_history, get_contents (source_buffer));

	check_contents_history (source_buffer, contents_history);

	g_list_free_full (contents_history, g_free);
	contents_history = NULL;

	/* Case 2 */
	gtk_text_buffer_set_text (text_buffer, "", -1);

	gtk_text_buffer_begin_user_action (text_buffer);
	gtk_source_buffer_begin_not_undoable_action (source_buffer);
	gtk_text_buffer_end_user_action (text_buffer);
	gtk_source_buffer_end_not_undoable_action (source_buffer);
	contents_history = g_list_append (contents_history, get_contents (source_buffer));

	gtk_text_buffer_insert_at_cursor (text_buffer, "a\n", -1);
	contents_history = g_list_append (contents_history, get_contents (source_buffer));

	check_contents_history (source_buffer, contents_history);

	g_list_free_full (contents_history, g_free);
	contents_history = NULL;

	/* Case 3 */
	gtk_text_buffer_set_text (text_buffer, "", -1);

	gtk_source_buffer_begin_not_undoable_action (source_buffer);
	gtk_text_buffer_begin_user_action (text_buffer);
	gtk_text_buffer_insert_at_cursor (text_buffer, "a\n", -1);
	gtk_text_buffer_end_user_action (text_buffer);
	gtk_source_buffer_end_not_undoable_action (source_buffer);
	contents_history = g_list_append (contents_history, get_contents (source_buffer));

	check_contents_history (source_buffer, contents_history);

	g_list_free_full (contents_history, g_free);
	contents_history = NULL;

	/* Case 4 */
	gtk_text_buffer_set_text (text_buffer, "", -1);

	gtk_source_buffer_begin_not_undoable_action (source_buffer);
	gtk_text_buffer_begin_user_action (text_buffer);
	gtk_text_buffer_insert_at_cursor (text_buffer, "a\n", -1);
	gtk_source_buffer_end_not_undoable_action (source_buffer);
	contents_history = g_list_append (contents_history, get_contents (source_buffer));
	gtk_text_buffer_end_user_action (text_buffer);

	gtk_text_buffer_insert_at_cursor (text_buffer, "b\n", -1);
	contents_history = g_list_append (contents_history, get_contents (source_buffer));

	check_contents_history (source_buffer, contents_history);

	g_list_free_full (contents_history, g_free);
	contents_history = NULL;

	/* Case 5 */
	gtk_text_buffer_set_text (text_buffer, "", -1);

	gtk_source_buffer_begin_not_undoable_action (source_buffer);
	gtk_text_buffer_begin_user_action (text_buffer);
	gtk_source_buffer_end_not_undoable_action (source_buffer);
	contents_history = g_list_append (contents_history, get_contents (source_buffer));

	gtk_text_buffer_insert_at_cursor (text_buffer, "a\n", -1);
	gtk_text_buffer_end_user_action (text_buffer);
	contents_history = g_list_append (contents_history, get_contents (source_buffer));

	check_contents_history (source_buffer, contents_history);

	g_list_free_full (contents_history, g_free);
	contents_history = NULL;

	/* Case 6 */
	gtk_text_buffer_set_text (text_buffer, "", -1);

	gtk_text_buffer_begin_user_action (text_buffer);
	gtk_text_buffer_insert_at_cursor (text_buffer, "a\n", -1);
	gtk_source_buffer_begin_not_undoable_action (source_buffer);
	gtk_text_buffer_end_user_action (text_buffer);
	gtk_source_buffer_end_not_undoable_action (source_buffer);
	contents_history = g_list_append (contents_history, get_contents (source_buffer));

	gtk_text_buffer_insert_at_cursor (text_buffer, "b\n", -1);
	contents_history = g_list_append (contents_history, get_contents (source_buffer));

	check_contents_history (source_buffer, contents_history);

	g_list_free_full (contents_history, g_free);
	contents_history = NULL;

	/* Case 7 */
	gtk_text_buffer_set_text (text_buffer, "", -1);

	gtk_text_buffer_begin_user_action (text_buffer);
	gtk_text_buffer_insert_at_cursor (text_buffer, "a\n", -1);
	gtk_source_buffer_begin_not_undoable_action (source_buffer);
	gtk_source_buffer_end_not_undoable_action (source_buffer);
	contents_history = g_list_append (contents_history, get_contents (source_buffer));
	gtk_text_buffer_end_user_action (text_buffer);

	gtk_text_buffer_insert_at_cursor (text_buffer, "b\n", -1);
	contents_history = g_list_append (contents_history, get_contents (source_buffer));

	check_contents_history (source_buffer, contents_history);

	g_list_free_full (contents_history, g_free);
	contents_history = NULL;

	g_object_unref (source_buffer);
}

int
main (int argc, char **argv)
{
	gtk_test_init (&argc, &argv);

	g_test_add_func ("/UndoManager/test-get-set-max-undo-levels",
			 test_get_set_max_undo_levels);

	g_test_add_func ("/UndoManager/test-single-action",
			 test_single_action);

	g_test_add_func ("/UndoManager/test-lose-redo-actions",
			 test_lose_redo_actions);

	g_test_add_func ("/UndoManager/test-max-undo-levels",
			 test_max_undo_levels);

	g_test_add_func ("/UndoManager/test-not-undoable-action",
			 test_not_undoable_action);

	g_test_add_func ("/UndoManager/test-contents",
			 test_contents);

	g_test_add_func ("/UndoManager/test-merge-actions",
			 test_merge_actions);

	g_test_add_func ("/UndoManager/test-several-user-actions",
			 test_several_user_actions);

	g_test_add_func ("/UndoManager/test-modified",
			 test_modified);

	g_test_add_func ("/UndoManager/test-empty-user-actions",
			 test_empty_user_actions);

	g_test_add_func ("/UndoManager/test-bug-672893-selection-restoring",
			 test_bug_672893_selection_restoring);

	g_test_add_func ("/UndoManager/mix-user-action-and-not-undoable-action",
			 test_mix_user_action_and_not_undoable_action);

	return g_test_run ();
}
