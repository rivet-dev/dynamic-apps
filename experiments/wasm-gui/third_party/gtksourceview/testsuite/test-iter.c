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

#include "gtksourceview/gtksourceiter.h"

static void
check_full_word_boundaries (gboolean     forward,
			    const gchar *buffer_text,
			    gint         initial_offset,
			    gint         result_offset)
{
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	buffer = gtk_text_buffer_new (NULL);
	gtk_text_buffer_set_text (buffer, buffer_text, -1);

	gtk_text_buffer_get_iter_at_offset (buffer, &iter, initial_offset);

	if (forward)
	{
		_gtk_source_iter_forward_full_word_end (&iter);
	}
	else
	{
		_gtk_source_iter_backward_full_word_start (&iter);
	}

	g_assert_cmpint (result_offset, ==, gtk_text_iter_get_offset (&iter));

	g_object_unref (buffer);
}

static void
test_forward_full_word_end (void)
{
	check_full_word_boundaries (TRUE, "  ---- abcd ", 2, 6);
	check_full_word_boundaries (TRUE, "  ---- abcd ", 0, 6);
	check_full_word_boundaries (TRUE, "  ---- abcd ", 4, 6);
	check_full_word_boundaries (TRUE, "  ---- abcd ", 8, 11);
	check_full_word_boundaries (TRUE, "  ---- abcd ", 11, 11);
	check_full_word_boundaries (TRUE, "  ---- abcd \n  ----", 11, 19);
}

static void
test_backward_full_word_start (void)
{
	check_full_word_boundaries (FALSE, "---- abcd  ", 9, 5);
	check_full_word_boundaries (FALSE, "---- abcd  ", 11, 5);
	check_full_word_boundaries (FALSE, "---- abcd  ", 7, 5);
	check_full_word_boundaries (FALSE, "---- abcd  ", 3, 0);
	check_full_word_boundaries (FALSE, " ---- abcd  ", 1, 1);
	check_full_word_boundaries (FALSE, "abcd \n ---- abcd  ", 7, 0);
}

static void
test_starts_full_word (void)
{
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	buffer = gtk_text_buffer_new (NULL);
	gtk_text_buffer_set_text (buffer, "foo--- ---bar", -1);

	gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);
	g_assert_true (_gtk_source_iter_starts_full_word (&iter));

	gtk_text_buffer_get_iter_at_offset (buffer, &iter, 1);
	g_assert_false (_gtk_source_iter_starts_full_word (&iter));

	gtk_text_buffer_get_iter_at_offset (buffer, &iter, 7);
	g_assert_true (_gtk_source_iter_starts_full_word (&iter));

	gtk_text_buffer_get_iter_at_offset (buffer, &iter, 10);
	g_assert_false (_gtk_source_iter_starts_full_word (&iter));

	gtk_text_buffer_set_text (buffer, " ab ", -1);
	gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);
	g_assert_false (_gtk_source_iter_starts_full_word (&iter));

	gtk_text_buffer_get_iter_at_offset (buffer, &iter, 4);
	g_assert_false (_gtk_source_iter_starts_full_word (&iter));

	g_object_unref (buffer);
}

static void
test_ends_full_word (void)
{
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	buffer = gtk_text_buffer_new (NULL);
	gtk_text_buffer_set_text (buffer, "foo--- ---bar ", -1);

	gtk_text_buffer_get_iter_at_offset (buffer, &iter, 14);
	g_assert_false (_gtk_source_iter_ends_full_word (&iter));

	gtk_text_buffer_get_iter_at_offset (buffer, &iter, 13);
	g_assert_true (_gtk_source_iter_ends_full_word (&iter));

	gtk_text_buffer_get_iter_at_offset (buffer, &iter, 12);
	g_assert_false (_gtk_source_iter_ends_full_word (&iter));

	gtk_text_buffer_get_iter_at_offset (buffer, &iter, 6);
	g_assert_true (_gtk_source_iter_ends_full_word (&iter));

	gtk_text_buffer_get_iter_at_offset (buffer, &iter, 3);
	g_assert_false (_gtk_source_iter_ends_full_word (&iter));

	gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);
	g_assert_false (_gtk_source_iter_ends_full_word (&iter));

	g_object_unref (buffer);
}

static void
check_extra_natural_word_boundaries (gboolean     forward,
				     const gchar *buffer_text,
				     gint         initial_offset,
				     gint         result_offset)
{
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	buffer = gtk_text_buffer_new (NULL);
	gtk_text_buffer_set_text (buffer, buffer_text, -1);

	gtk_text_buffer_get_iter_at_offset (buffer, &iter, initial_offset);

	if (forward)
	{
		_gtk_source_iter_forward_extra_natural_word_end (&iter);
	}
	else
	{
		_gtk_source_iter_backward_extra_natural_word_start (&iter);
	}

	g_assert_cmpint (result_offset, ==, gtk_text_iter_get_offset (&iter));

	g_object_unref (buffer);
}

static void
test_forward_extra_natural_word_end (void)
{
	const gchar *str = "hello_world ---- blah";

	check_extra_natural_word_boundaries (TRUE, str, 0, 11);
	check_extra_natural_word_boundaries (TRUE, str, 1, 11);
	check_extra_natural_word_boundaries (TRUE, str, 5, 11);
	check_extra_natural_word_boundaries (TRUE, str, 6, 11);
	check_extra_natural_word_boundaries (TRUE, str, 11, 21);
	check_extra_natural_word_boundaries (TRUE, str, 21, 21);

	check_extra_natural_word_boundaries (TRUE, "ab ", 2, 2);
	check_extra_natural_word_boundaries (TRUE, "a_ ", 2, 2);
	check_extra_natural_word_boundaries (TRUE, "ab \ncd", 2, 6);
	check_extra_natural_word_boundaries (TRUE, "a_ \n_d", 2, 6);

	check_extra_natural_word_boundaries (TRUE, "__ ab", 0, 2);
	check_extra_natural_word_boundaries (TRUE, "--__--", 0, 4);
	check_extra_natural_word_boundaries (TRUE, "--__-- ab", 0, 4);
}

static void
test_backward_extra_natural_word_start (void)
{
	const gchar *str = "hello_world ---- blah";

	check_extra_natural_word_boundaries (FALSE, str, 21, 17);
	check_extra_natural_word_boundaries (FALSE, str, 20, 17);
	check_extra_natural_word_boundaries (FALSE, str, 17, 0);
	check_extra_natural_word_boundaries (FALSE, str, 11, 0);
	check_extra_natural_word_boundaries (FALSE, str, 6, 0);
	check_extra_natural_word_boundaries (FALSE, str, 5, 0);
	check_extra_natural_word_boundaries (FALSE, str, 0, 0);

	check_extra_natural_word_boundaries (FALSE, " cd", 1, 1);
	check_extra_natural_word_boundaries (FALSE, " _d", 1, 1);
	check_extra_natural_word_boundaries (FALSE, "ab\n cd", 4, 0);
	check_extra_natural_word_boundaries (FALSE, "_b\n c_", 4, 0);

	check_extra_natural_word_boundaries (FALSE, "ab __", 5, 3);
	check_extra_natural_word_boundaries (FALSE, "--__--", 6, 2);
	check_extra_natural_word_boundaries (FALSE, "ab --__--", 9, 5);
}

static void
check_starts_extra_natural_word (const gchar *buffer_text,
				 gint         offset,
				 gboolean     starts)
{
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	buffer = gtk_text_buffer_new (NULL);
	gtk_text_buffer_set_text (buffer, buffer_text, -1);

	gtk_text_buffer_get_iter_at_offset (buffer, &iter, offset);
	g_assert_cmpint (starts, ==, _gtk_source_iter_starts_extra_natural_word (&iter, TRUE));

	g_object_unref (buffer);
}

static void
test_starts_extra_natural_word (void)
{
	check_starts_extra_natural_word ("ab", 2, FALSE);
	check_starts_extra_natural_word ("hello", 0, TRUE);
	check_starts_extra_natural_word ("__", 0, TRUE);
	check_starts_extra_natural_word (" hello", 0, FALSE);
	check_starts_extra_natural_word (" hello", 1, TRUE);
	check_starts_extra_natural_word ("_hello", 1, FALSE);
	check_starts_extra_natural_word ("()", 1, FALSE);
	check_starts_extra_natural_word ("__", 1, FALSE);
	check_starts_extra_natural_word (" __", 1, TRUE);
	check_starts_extra_natural_word (" __hello", 1, TRUE);
	check_starts_extra_natural_word ("hello_", 5, FALSE);
}

static void
check_ends_extra_natural_word (const gchar *buffer_text,
			       gint         offset,
			       gboolean     ends)
{
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	buffer = gtk_text_buffer_new (NULL);
	gtk_text_buffer_set_text (buffer, buffer_text, -1);

	gtk_text_buffer_get_iter_at_offset (buffer, &iter, offset);
	g_assert_cmpint (ends, ==, _gtk_source_iter_ends_extra_natural_word (&iter, TRUE));

	g_object_unref (buffer);
}

static void
test_ends_extra_natural_word (void)
{
	check_ends_extra_natural_word ("ab", 0, FALSE);
	check_ends_extra_natural_word ("ab", 2, TRUE);
	check_ends_extra_natural_word ("__", 2, TRUE);
	check_ends_extra_natural_word ("ab ", 3, FALSE);
	check_ends_extra_natural_word ("ab ", 2, TRUE);
	check_ends_extra_natural_word ("ab_", 2, FALSE);
	check_ends_extra_natural_word ("()", 1, FALSE);
	check_ends_extra_natural_word ("__ ", 1, FALSE);
	check_ends_extra_natural_word ("__ab ", 2, FALSE);
	check_ends_extra_natural_word ("__ ", 2, TRUE);
}

static void
check_word_boundaries (const gchar *buffer_text,
		       gint         offset,
		       gboolean     starts_word_result,
		       gboolean     ends_word_result,
		       gboolean     inside_word_result)
{
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	buffer = gtk_text_buffer_new (NULL);
	gtk_text_buffer_set_text (buffer, buffer_text, -1);

	gtk_text_buffer_get_iter_at_offset (buffer, &iter, offset);

	g_assert_cmpint (starts_word_result, ==, _gtk_source_iter_starts_word (&iter));
	g_assert_cmpint (ends_word_result, ==, _gtk_source_iter_ends_word (&iter));
	g_assert_cmpint (inside_word_result, ==, _gtk_source_iter_inside_word (&iter));

	g_object_unref (buffer);
}

static void
test_word_boundaries (void)
{
	check_word_boundaries ("ab()cd", 0, TRUE, FALSE, TRUE);
	check_word_boundaries ("ab()cd", 1, FALSE, FALSE, TRUE);
	check_word_boundaries ("ab()cd", 2, TRUE, TRUE, TRUE);
	check_word_boundaries ("ab()cd", 3, FALSE, FALSE, TRUE);
	check_word_boundaries ("ab()cd", 4, TRUE, TRUE, TRUE);
	check_word_boundaries ("ab()cd", 5, FALSE, FALSE, TRUE);
	check_word_boundaries ("ab()cd", 6, FALSE, TRUE, FALSE);

	check_word_boundaries (" ab", 0, FALSE, FALSE, FALSE);
	check_word_boundaries ("ab ", 3, FALSE, FALSE, FALSE);

	check_word_boundaries (" () ", 1, TRUE, FALSE, TRUE);
	check_word_boundaries (" () ", 3, FALSE, TRUE, FALSE);
}

static void
check_word_boundaries_movement (gboolean     forward,
				const gchar *buffer_text,
				gint         initial_offset,
				gint         result_offset,
				gboolean     ret)
{
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	buffer = gtk_text_buffer_new (NULL);
	gtk_text_buffer_set_text (buffer, buffer_text, -1);

	gtk_text_buffer_get_iter_at_offset (buffer, &iter, initial_offset);

	if (forward)
	{
		g_assert_cmpint (ret, ==, _gtk_source_iter_forward_visible_word_end (&iter));
	}
	else
	{
		g_assert_cmpint (ret, ==, _gtk_source_iter_backward_visible_word_start (&iter));
	}

	g_assert_cmpint (result_offset, ==, gtk_text_iter_get_offset (&iter));

	g_object_unref (buffer);
}

static void
test_forward_word_end (void)
{
	check_word_boundaries_movement (TRUE, "---- aaaa", 0, 4, TRUE);
	check_word_boundaries_movement (TRUE, "---- aaaa", 1, 4, TRUE);
	check_word_boundaries_movement (TRUE, "---- aaaa", 4, 9, FALSE);
	check_word_boundaries_movement (TRUE, "---- aaaa", 5, 9, FALSE);
	check_word_boundaries_movement (TRUE, "---- aaaa", 6, 9, FALSE);
	check_word_boundaries_movement (TRUE, "aaaa ----", 0, 4, TRUE);
	check_word_boundaries_movement (TRUE, "aaaa ----", 1, 4, TRUE);
	check_word_boundaries_movement (TRUE, "aaaa ----", 4, 9, FALSE);
	check_word_boundaries_movement (TRUE, "aaaa ----", 5, 9, FALSE);
	check_word_boundaries_movement (TRUE, "aaaa ----", 6, 9, FALSE);

	check_word_boundaries_movement (TRUE, "abcd", 2, 4, FALSE);
	check_word_boundaries_movement (TRUE, "abcd ", 2, 4, TRUE);
	check_word_boundaries_movement (TRUE, " abcd()", 0, 5, TRUE);
	check_word_boundaries_movement (TRUE, "abcd()efgh", 4, 6, TRUE);

	check_word_boundaries_movement (TRUE, "ab ", 2, 2, FALSE);
	check_word_boundaries_movement (TRUE, "ab \n", 2, 2, FALSE);
	check_word_boundaries_movement (TRUE, "ab \ncd", 2, 6, FALSE);

	check_word_boundaries_movement (TRUE, "--__--", 0, 2, TRUE);
	check_word_boundaries_movement (TRUE, "--__--", 2, 4, TRUE);
	check_word_boundaries_movement (TRUE, "--__--", 4, 6, FALSE);
}

static void
test_backward_word_start (void)
{
	check_word_boundaries_movement (FALSE, "aaaa ----", 9, 5, TRUE);
	check_word_boundaries_movement (FALSE, "aaaa ----", 8, 5, TRUE);
	check_word_boundaries_movement (FALSE, "aaaa ----", 5, 0, TRUE);
	check_word_boundaries_movement (FALSE, "aaaa ----", 4, 0, TRUE);
	check_word_boundaries_movement (FALSE, "aaaa ----", 3, 0, TRUE);
	check_word_boundaries_movement (FALSE, "---- aaaa", 9, 5, TRUE);
	check_word_boundaries_movement (FALSE, "---- aaaa", 8, 5, TRUE);
	check_word_boundaries_movement (FALSE, "---- aaaa", 5, 0, TRUE);
	check_word_boundaries_movement (FALSE, "---- aaaa", 4, 0, TRUE);
	check_word_boundaries_movement (FALSE, "---- aaaa", 3, 0, TRUE);

	check_word_boundaries_movement (FALSE, "abcd", 2, 0, TRUE);
	check_word_boundaries_movement (FALSE, "()abcd ", 7, 2, TRUE);
	check_word_boundaries_movement (FALSE, "abcd()", 6, 4, TRUE);
	check_word_boundaries_movement (FALSE, "abcd()", 0, 0, FALSE);

	check_word_boundaries_movement (FALSE, " cd", 1, 1, FALSE);
	check_word_boundaries_movement (FALSE, "\n cd", 2, 2, FALSE);
	check_word_boundaries_movement (FALSE, "ab\n cd", 4, 0, TRUE);

	check_word_boundaries_movement (FALSE, "--__--", 6, 4, TRUE);
	check_word_boundaries_movement (FALSE, "--__--", 4, 2, TRUE);
	check_word_boundaries_movement (FALSE, "--__--", 2, 0, TRUE);
}

static void
check_get_leading_spaces_end_boundary (const gchar *text,
				       gint         iter_offset,
				       gint         expected_leading_end_offset)
{
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	GtkTextIter leading_end;

	buffer = gtk_text_buffer_new (NULL);
	gtk_text_buffer_set_text (buffer, text, -1);

	gtk_text_buffer_get_iter_at_offset (buffer, &iter, iter_offset);
	_gtk_source_iter_get_leading_spaces_end_boundary (&iter, &leading_end);
	g_assert_cmpint (gtk_text_iter_get_offset (&leading_end), ==, expected_leading_end_offset);

	g_object_unref (buffer);
}

static void
test_get_leading_spaces_end_boundary (void)
{
	check_get_leading_spaces_end_boundary ("  abc\n", 0, 2);
	check_get_leading_spaces_end_boundary ("  \n", 0, 2);
	check_get_leading_spaces_end_boundary ("\t\n", 0, 1);
	check_get_leading_spaces_end_boundary ("\t\r\n", 0, 1);
	check_get_leading_spaces_end_boundary ("\t\r", 0, 1);
	check_get_leading_spaces_end_boundary (" \t \n", 0, 3);

	/* No-Break Space U+00A0 */
	check_get_leading_spaces_end_boundary ("\302\240abc\n", 0, 1);
	check_get_leading_spaces_end_boundary (" \t\302\240\t\n", 0, 4);

	/* Narrow No-Break Space U+202F */
	check_get_leading_spaces_end_boundary ("\342\200\257abc\n", 0, 1);
	check_get_leading_spaces_end_boundary ("\t \342\200\257\n", 0, 3);
}

static void
check_get_trailing_spaces_start_boundary (const gchar *text,
					  gint         iter_offset,
					  gint         expected_trailing_start_offset)
{
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	GtkTextIter trailing_start;

	buffer = gtk_text_buffer_new (NULL);
	gtk_text_buffer_set_text (buffer, text, -1);

	gtk_text_buffer_get_iter_at_offset (buffer, &iter, iter_offset);
	_gtk_source_iter_get_trailing_spaces_start_boundary (&iter, &trailing_start);
	g_assert_cmpint (gtk_text_iter_get_offset (&trailing_start), ==, expected_trailing_start_offset);

	g_object_unref (buffer);
}

static void
test_get_trailing_spaces_start_boundary (void)
{
	check_get_trailing_spaces_start_boundary ("", 0, 0);
	check_get_trailing_spaces_start_boundary ("a", 0, 1);
	check_get_trailing_spaces_start_boundary ("a ", 0, 1);
	check_get_trailing_spaces_start_boundary ("a \n", 0, 1);
	check_get_trailing_spaces_start_boundary ("a \r\n", 0, 1);
	check_get_trailing_spaces_start_boundary ("a \r", 0, 1);
	check_get_trailing_spaces_start_boundary ("a\t\n", 0, 1);
	check_get_trailing_spaces_start_boundary (" \t\t  \n", 0, 0);
	check_get_trailing_spaces_start_boundary ("\n", 1, 1);

	/* No-Break Space U+00A0 */
	check_get_trailing_spaces_start_boundary ("a\302\240", 0, 1);
	check_get_trailing_spaces_start_boundary ("a \t\302\240 \t\302\240", 0, 1);

	/* Narrow No-Break Space U+202F */
	check_get_trailing_spaces_start_boundary ("a\342\200\257", 0, 1);
	check_get_trailing_spaces_start_boundary (" \ta;\t  \342\200\257 \t\302\240\n", 0, 4);
}

int
main (int argc, char **argv)
{
	gtk_test_init (&argc, &argv);

	g_test_add_func ("/Iter/full-word/forward", test_forward_full_word_end);
	g_test_add_func ("/Iter/full-word/backward", test_backward_full_word_start);
	g_test_add_func ("/Iter/full-word/starts", test_starts_full_word);
	g_test_add_func ("/Iter/full-word/ends", test_ends_full_word);

	g_test_add_func ("/Iter/extra-natural-word/forward", test_forward_extra_natural_word_end);
	g_test_add_func ("/Iter/extra-natural-word/backward", test_backward_extra_natural_word_start);
	g_test_add_func ("/Iter/extra-natural-word/starts", test_starts_extra_natural_word);
	g_test_add_func ("/Iter/extra-natural-word/ends", test_ends_extra_natural_word);

	g_test_add_func ("/Iter/custom-word/boundaries", test_word_boundaries);
	g_test_add_func ("/Iter/custom-word/forward", test_forward_word_end);
	g_test_add_func ("/Iter/custom-word/backward", test_backward_word_start);

	g_test_add_func ("/Iter/get_leading_spaces_end_boundary", test_get_leading_spaces_end_boundary);
	g_test_add_func ("/Iter/get_trailing_spaces_start_boundary", test_get_trailing_spaces_start_boundary);

	return g_test_run ();
}
