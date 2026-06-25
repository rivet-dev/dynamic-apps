/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- */
/*
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2017 - Sébastien Wilmet
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

static gchar *
get_text (GtkTextBuffer *buffer)
{
	GtkTextIter start;
	GtkTextIter end;

	gtk_text_buffer_get_bounds (buffer, &start, &end);
	return gtk_text_buffer_get_text (buffer, &start, &end, TRUE);
}

#define N_CASES_INITIAL_SELECTION_FOR_SINGLE_LINE (6)

static void
set_initial_selection_for_single_line (GtkTextBuffer *buffer,
				       gint           line_num,
				       gint           case_num)
{
	GtkTextIter start;
	GtkTextIter end;

	switch (case_num)
	{
		case 0:
			gtk_text_buffer_get_iter_at_line (buffer, &start, line_num);
			gtk_text_buffer_place_cursor (buffer, &start);
			break;

		case 1:
			gtk_text_buffer_get_iter_at_line_offset (buffer, &start, line_num, 1);
			gtk_text_buffer_place_cursor (buffer, &start);
			break;

		case 2:
			gtk_text_buffer_get_iter_at_line (buffer, &start, line_num);
			gtk_text_iter_forward_to_line_end (&start);
			gtk_text_buffer_place_cursor (buffer, &start);
			break;

		case 3:
			gtk_text_buffer_get_iter_at_line (buffer, &start, line_num);
			end = start;
			gtk_text_iter_forward_to_line_end (&end);
			gtk_text_buffer_select_range (buffer, &start, &end);
			break;

		case 4:
			gtk_text_buffer_get_iter_at_line (buffer, &start, line_num);
			gtk_text_buffer_get_iter_at_line (buffer, &end, line_num + 1);
			gtk_text_buffer_select_range (buffer, &start, &end);
			break;

		case 5:
			gtk_text_buffer_get_iter_at_line_offset (buffer, &start, line_num, 1);
			gtk_text_buffer_get_iter_at_line_offset (buffer, &end, line_num, 2);
			gtk_text_buffer_select_range (buffer, &start, &end);
			break;

		default:
			g_assert_not_reached ();
	}
}

static void
test_move_lines__move_single_line (void)
{
	GtkSourceView *view;
	GtkTextBuffer *buffer;
	GtkTextIter start;
	GtkTextIter end;
	gchar *text;
	gint case_num;

	view = GTK_SOURCE_VIEW (gtk_source_view_new ());
	g_object_ref_sink (view);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	/* Move down first line */
	for (case_num = 0; case_num < N_CASES_INITIAL_SELECTION_FOR_SINGLE_LINE; case_num++)
	{
		gtk_text_buffer_set_text (buffer,
					  "line1\n"
					  "line2\n"
					  "line3",
					  -1);

		set_initial_selection_for_single_line (buffer, 0, case_num);

		g_signal_emit_by_name (view, "move-lines", TRUE);
		text = get_text (buffer);
		g_assert_cmpstr (text, ==,
				 "line2\n"
				 "line1\n"
				 "line3");
		g_free (text);

		gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
		g_assert_cmpint (gtk_text_iter_get_offset (&start), ==, 6);
		g_assert_cmpint (gtk_text_iter_get_offset (&end), ==, 12);
	}

	/* Move up second line */
	for (case_num = 0; case_num < N_CASES_INITIAL_SELECTION_FOR_SINGLE_LINE; case_num++)
	{
		gtk_text_buffer_set_text (buffer,
					  "line1\n"
					  "line2\n"
					  "line3",
					  -1);

		set_initial_selection_for_single_line (buffer, 1, case_num);

		g_signal_emit_by_name (view, "move-lines", FALSE);
		text = get_text (buffer);
		g_assert_cmpstr (text, ==,
				 "line2\n"
				 "line1\n"
				 "line3");
		g_free (text);

		gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
		g_assert_true (gtk_text_iter_is_start (&start));
		g_assert_cmpint (gtk_text_iter_get_offset (&end), ==, 6);
	}

	/* Move down second line, without final newline */
	for (case_num = 0; case_num < N_CASES_INITIAL_SELECTION_FOR_SINGLE_LINE; case_num++)
	{
		gtk_text_buffer_set_text (buffer,
					  "line1\n"
					  "line2\n"
					  "line3",
					  -1);

		set_initial_selection_for_single_line (buffer, 1, case_num);

		g_signal_emit_by_name (view, "move-lines", TRUE);
		text = get_text (buffer);
		g_assert_cmpstr (text, ==,
				 "line1\n"
				 "line3\n"
				 "line2");
		g_free (text);

		gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
		g_assert_cmpint (gtk_text_iter_get_offset (&start), ==, 12);
		g_assert_true (gtk_text_iter_is_end (&end));
	}

	/* Move down second line, with final newline */
	for (case_num = 0; case_num < N_CASES_INITIAL_SELECTION_FOR_SINGLE_LINE; case_num++)
	{
		gtk_text_buffer_set_text (buffer,
					  "line1\n"
					  "line2\n"
					  "line3\n",
					  -1);

		set_initial_selection_for_single_line (buffer, 1, case_num);

		g_signal_emit_by_name (view, "move-lines", TRUE);
		text = get_text (buffer);
		g_assert_cmpstr (text, ==,
				 "line1\n"
				 "line3\n"
				 "line2\n");
		g_free (text);

		gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
		g_assert_cmpint (gtk_text_iter_get_offset (&start), ==, 12);
		g_assert_true (gtk_text_iter_is_end (&end));
	}

	/* Move up third line, without final newline */
	for (case_num = 0; case_num < N_CASES_INITIAL_SELECTION_FOR_SINGLE_LINE; case_num++)
	{
		gtk_text_buffer_set_text (buffer,
					  "line1\n"
					  "line2\n"
					  "line3",
					  -1);

		set_initial_selection_for_single_line (buffer, 2, case_num);

		g_signal_emit_by_name (view, "move-lines", FALSE);
		text = get_text (buffer);
		g_assert_cmpstr (text, ==,
				 "line1\n"
				 "line3\n"
				 "line2");
		g_free (text);

		gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
		g_assert_cmpint (gtk_text_iter_get_offset (&start), ==, 6);
		g_assert_cmpint (gtk_text_iter_get_offset (&end), ==, 12);
	}

	/* Move up third line, with final newline */
	for (case_num = 0; case_num < N_CASES_INITIAL_SELECTION_FOR_SINGLE_LINE; case_num++)
	{
		gtk_text_buffer_set_text (buffer,
					  "line1\n"
					  "line2\n"
					  "line3\n",
					  -1);

		set_initial_selection_for_single_line (buffer, 2, case_num);

		g_signal_emit_by_name (view, "move-lines", FALSE);
		text = get_text (buffer);
		g_assert_cmpstr (text, ==,
				 "line1\n"
				 "line3\n"
				 "line2\n");
		g_free (text);

		gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
		g_assert_cmpint (gtk_text_iter_get_offset (&start), ==, 6);
		g_assert_cmpint (gtk_text_iter_get_offset (&end), ==, 12);
	}

	/* Move down last line */
	gtk_text_buffer_set_text (buffer,
				  "line1\n"
				  "line2\n"
				  "line3",
				  -1);

	gtk_text_buffer_get_end_iter (buffer, &end);
	gtk_text_buffer_place_cursor (buffer, &end);

	g_signal_emit_by_name (view, "move-lines", TRUE);
	text = get_text (buffer);
	g_assert_cmpstr (text, ==,
			 "line1\n"
			 "line2\n"
			 "line3");
	g_free (text);

	gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
	g_assert_true (gtk_text_iter_is_end (&start));
	g_assert_true (gtk_text_iter_is_end (&end));

	/* Move up first line */
	gtk_text_buffer_set_text (buffer,
				  "line1\n"
				  "line2\n"
				  "line3",
				  -1);

	gtk_text_buffer_get_start_iter (buffer, &start);
	gtk_text_buffer_place_cursor (buffer, &start);

	g_signal_emit_by_name (view, "move-lines", FALSE);
	text = get_text (buffer);
	g_assert_cmpstr (text, ==,
			 "line1\n"
			 "line2\n"
			 "line3");
	g_free (text);

	gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
	g_assert_true (gtk_text_iter_is_start (&start));
	g_assert_true (gtk_text_iter_is_start (&end));

	g_object_unref (view);
}

#define N_CASES_INITIAL_SELECTION_FOR_SEVERAL_LINES (3)

static void
set_initial_selection_for_several_lines (GtkTextBuffer *buffer,
					 gint           start_line_num,
					 gint           end_line_num,
					 gint           case_num)
{
	GtkTextIter start;
	GtkTextIter end;

	switch (case_num)
	{
		case 0:
			gtk_text_buffer_get_iter_at_line (buffer, &start, start_line_num);
			gtk_text_buffer_get_iter_at_line (buffer, &end, end_line_num);
			gtk_text_iter_forward_to_line_end (&end);
			gtk_text_buffer_select_range (buffer, &start, &end);
			break;

		case 1:
			gtk_text_buffer_get_iter_at_line (buffer, &start, start_line_num);
			gtk_text_buffer_get_iter_at_line (buffer, &end, end_line_num + 1);
			gtk_text_buffer_select_range (buffer, &start, &end);
			break;

		case 2:
			gtk_text_buffer_get_iter_at_line_offset (buffer, &start, start_line_num, 1);
			gtk_text_buffer_get_iter_at_line_offset (buffer, &end, end_line_num, 1);
			gtk_text_buffer_select_range (buffer, &start, &end);
			break;

		default:
			g_assert_not_reached ();
	}
}

static void
test_move_lines__move_several_lines (void)
{
	GtkSourceView *view;
	GtkTextBuffer *buffer;
	GtkTextIter start;
	GtkTextIter end;
	gchar *text;
	gint case_num;

	view = GTK_SOURCE_VIEW (gtk_source_view_new ());
	g_object_ref_sink (view);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	/* Move down first two lines */
	for (case_num = 0; case_num < N_CASES_INITIAL_SELECTION_FOR_SEVERAL_LINES; case_num++)
	{
		gtk_text_buffer_set_text (buffer,
					  "line1\n"
					  "line2\n"
					  "line3\n"
					  "line4",
					  -1);

		set_initial_selection_for_several_lines (buffer, 0, 1, case_num);

		g_signal_emit_by_name (view, "move-lines", TRUE);
		text = get_text (buffer);
		g_assert_cmpstr (text, ==,
				 "line3\n"
				 "line1\n"
				 "line2\n"
				 "line4");
		g_free (text);

		gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
		g_assert_cmpint (gtk_text_iter_get_offset (&start), ==, 6);
		g_assert_cmpint (gtk_text_iter_get_offset (&end), ==, 18);
	}

	/* Move up second and third lines */
	for (case_num = 0; case_num < N_CASES_INITIAL_SELECTION_FOR_SEVERAL_LINES; case_num++)
	{
		gtk_text_buffer_set_text (buffer,
					  "line1\n"
					  "line2\n"
					  "line3\n"
					  "line4",
					  -1);

		set_initial_selection_for_several_lines (buffer, 1, 2, case_num);

		g_signal_emit_by_name (view, "move-lines", FALSE);
		text = get_text (buffer);
		g_assert_cmpstr (text, ==,
				 "line2\n"
				 "line3\n"
				 "line1\n"
				 "line4");
		g_free (text);

		gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
		g_assert_true (gtk_text_iter_is_start (&start));
		g_assert_cmpint (gtk_text_iter_get_offset (&end), ==, 12);
	}

	/* Move down second and third lines, without final newline */
	for (case_num = 0; case_num < N_CASES_INITIAL_SELECTION_FOR_SEVERAL_LINES; case_num++)
	{
		gtk_text_buffer_set_text (buffer,
					  "line1\n"
					  "line2\n"
					  "line3\n"
					  "line4",
					  -1);

		set_initial_selection_for_several_lines (buffer, 1, 2, case_num);

		g_signal_emit_by_name (view, "move-lines", TRUE);
		text = get_text (buffer);
		g_assert_cmpstr (text, ==,
				 "line1\n"
				 "line4\n"
				 "line2\n"
				 "line3");
		g_free (text);

		gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
		g_assert_cmpint (gtk_text_iter_get_offset (&start), ==, 12);
		g_assert_true (gtk_text_iter_is_end (&end));
	}

	/* Move down second and third lines, with final newline */
	for (case_num = 0; case_num < N_CASES_INITIAL_SELECTION_FOR_SEVERAL_LINES; case_num++)
	{
		gtk_text_buffer_set_text (buffer,
					  "line1\n"
					  "line2\n"
					  "line3\n"
					  "line4\n",
					  -1);

		set_initial_selection_for_several_lines (buffer, 1, 2, case_num);

		g_signal_emit_by_name (view, "move-lines", TRUE);
		text = get_text (buffer);
		g_assert_cmpstr (text, ==,
				 "line1\n"
				 "line4\n"
				 "line2\n"
				 "line3\n");
		g_free (text);

		gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
		g_assert_cmpint (gtk_text_iter_get_offset (&start), ==, 12);
		g_assert_true (gtk_text_iter_is_end (&end));
	}

	/* Move up third and fourth lines, without final newline */
	for (case_num = 0; case_num < N_CASES_INITIAL_SELECTION_FOR_SEVERAL_LINES; case_num++)
	{
		gtk_text_buffer_set_text (buffer,
					  "line1\n"
					  "line2\n"
					  "line3\n"
					  "line4",
					  -1);

		set_initial_selection_for_several_lines (buffer, 2, 3, case_num);

		g_signal_emit_by_name (view, "move-lines", FALSE);
		text = get_text (buffer);
		g_assert_cmpstr (text, ==,
				 "line1\n"
				 "line3\n"
				 "line4\n"
				 "line2");
		g_free (text);

		gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
		g_assert_cmpint (gtk_text_iter_get_offset (&start), ==, 6);
		g_assert_cmpint (gtk_text_iter_get_offset (&end), ==, 18);
	}

	/* Move up third and fourth lines, with final newline */
	for (case_num = 0; case_num < N_CASES_INITIAL_SELECTION_FOR_SEVERAL_LINES; case_num++)
	{
		gtk_text_buffer_set_text (buffer,
					  "line1\n"
					  "line2\n"
					  "line3\n"
					  "line4\n",
					  -1);

		set_initial_selection_for_several_lines (buffer, 2, 3, case_num);

		g_signal_emit_by_name (view, "move-lines", FALSE);
		text = get_text (buffer);
		g_assert_cmpstr (text, ==,
				 "line1\n"
				 "line3\n"
				 "line4\n"
				 "line2\n");
		g_free (text);

		gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
		g_assert_cmpint (gtk_text_iter_get_offset (&start), ==, 6);
		g_assert_cmpint (gtk_text_iter_get_offset (&end), ==, 18);
	}

	/* Move down last two lines */
	gtk_text_buffer_set_text (buffer,
				  "line1\n"
				  "line2\n"
				  "line3\n"
				  "line4",
				  -1);

	set_initial_selection_for_several_lines (buffer, 2, 3, 0);

	g_signal_emit_by_name (view, "move-lines", TRUE);
	text = get_text (buffer);
	g_assert_cmpstr (text, ==,
			 "line1\n"
			 "line2\n"
			 "line3\n"
			 "line4");
	g_free (text);

	/* Move up first two lines */
	gtk_text_buffer_set_text (buffer,
				  "line1\n"
				  "line2\n"
				  "line3\n"
				  "line4",
				  -1);

	set_initial_selection_for_several_lines (buffer, 0, 1, 0);

	g_signal_emit_by_name (view, "move-lines", FALSE);
	text = get_text (buffer);
	g_assert_cmpstr (text, ==,
			 "line1\n"
			 "line2\n"
			 "line3\n"
			 "line4");
	g_free (text);

	g_object_unref (view);
}

int
main (int argc, char **argv)
{
	gtk_test_init (&argc, &argv);

	g_test_add_func ("/view/move-lines/move-single-line", test_move_lines__move_single_line);
	g_test_add_func ("/view/move-lines/move-several-lines", test_move_lines__move_several_lines);

	return g_test_run();
}
