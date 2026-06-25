/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- */
/*
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2010 - Ignacio Casal Quinteiro
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

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <gtksourceview/gtksource.h>
#include "gtksourceview/gtksourcebufferinputstream.h"

static void
test_consecutive_read (const gchar          *inbuf,
		       const gchar          *outbuf,
		       GtkSourceNewlineType  type,
		       gsize                 read_chunk_len)
{
	GtkTextBuffer *buf;
	GtkSourceBufferInputStream *in;
	gsize outlen;
	gssize n, r;
	GError *err = NULL;
	gchar *b;
	gboolean close;

	buf = gtk_text_buffer_new (NULL);
	gtk_text_buffer_set_text (buf, inbuf, -1);

	b = g_malloc (200);
	in = _gtk_source_buffer_input_stream_new (buf, type, TRUE);

	outlen = strlen (outbuf);
	n = 0;

	do
	{
		r = g_input_stream_read (G_INPUT_STREAM (in), b + n, read_chunk_len, NULL, &err);
		g_assert_cmpint (r, >=, 0);
		g_assert_no_error (err);

		n += r;
	} while (r != 0);

	g_assert_cmpint (n, ==, outlen);

	b[n] = '\0';

	g_assert_cmpstr (b, ==, outbuf);

	close = g_input_stream_close (G_INPUT_STREAM (in), NULL, &err);
	g_assert_true (close);
	g_assert_no_error (err);

	g_object_unref (buf);
	g_object_unref (in);
	g_free (b);
}

static void
test_empty (void)
{
	/* empty file should not have a trailing newline */
	test_consecutive_read ("", "", GTK_SOURCE_NEWLINE_TYPE_CR_LF, 10);
}

static void
test_consecutive_cut_char (void)
{
	/* first \n is read then fo and then is added \r but not \n */
	test_consecutive_read ("\nfo\nbar\n\nblah\n", "\r\nfo\r\nbar\r\n\r\nblah\r\n\r\n", GTK_SOURCE_NEWLINE_TYPE_CR_LF, 8);
	test_consecutive_read ("\nfo\nbar\n\nblah", "\r\nfo\r\nbar\r\n\r\nblah\r\n", GTK_SOURCE_NEWLINE_TYPE_CR_LF, 8);
}

static void
test_consecutive_big_read (void)
{
	test_consecutive_read ("\nfo\nbar\n\nblah\n", "\rfo\rbar\r\rblah\r\r", GTK_SOURCE_NEWLINE_TYPE_CR, 200);
	test_consecutive_read ("\nfo\nbar\n\nblah", "\rfo\rbar\r\rblah\r", GTK_SOURCE_NEWLINE_TYPE_CR, 200);

	test_consecutive_read ("\rfo\rbar\r\rblah\r", "\nfo\nbar\n\nblah\n\n", GTK_SOURCE_NEWLINE_TYPE_LF, 200);
	test_consecutive_read ("\rfo\rbar\r\rblah", "\nfo\nbar\n\nblah\n", GTK_SOURCE_NEWLINE_TYPE_LF, 200);

	test_consecutive_read ("\r\nfo\r\nbar\r\n\r\nblah\r\n", "\nfo\nbar\n\nblah\n\n", GTK_SOURCE_NEWLINE_TYPE_LF, 200);
	test_consecutive_read ("\r\nfo\r\nbar\r\n\r\nblah", "\nfo\nbar\n\nblah\n", GTK_SOURCE_NEWLINE_TYPE_LF, 200);

	test_consecutive_read ("\nfo\nbar\n\nblah\n", "\r\nfo\r\nbar\r\n\r\nblah\r\n\r\n", GTK_SOURCE_NEWLINE_TYPE_CR_LF, 200);
	test_consecutive_read ("\nfo\nbar\n\nblah", "\r\nfo\r\nbar\r\n\r\nblah\r\n", GTK_SOURCE_NEWLINE_TYPE_CR_LF, 200);
}

static void
test_consecutive_middle_read (void)
{
	test_consecutive_read ("\nfo\nbar\n\nblah\n", "\rfo\rbar\r\rblah\r\r", GTK_SOURCE_NEWLINE_TYPE_CR, 6);
	test_consecutive_read ("\nfo\nbar\n\nblah", "\rfo\rbar\r\rblah\r", GTK_SOURCE_NEWLINE_TYPE_CR, 6);

	test_consecutive_read ("\rfo\rbar\r\rblah\r", "\nfo\nbar\n\nblah\n\n", GTK_SOURCE_NEWLINE_TYPE_LF, 6);
	test_consecutive_read ("\rfo\rbar\r\rblah", "\nfo\nbar\n\nblah\n", GTK_SOURCE_NEWLINE_TYPE_LF, 6);

	test_consecutive_read ("\r\nfo\r\nbar\r\n\r\nblah\r\n", "\nfo\nbar\n\nblah\n\n", GTK_SOURCE_NEWLINE_TYPE_LF, 6);
	test_consecutive_read ("\r\nfo\r\nbar\r\n\r\nblah", "\nfo\nbar\n\nblah\n", GTK_SOURCE_NEWLINE_TYPE_LF, 6);

	test_consecutive_read ("\nfo\nbar\n\nblah\n", "\r\nfo\r\nbar\r\n\r\nblah\r\n\r\n", GTK_SOURCE_NEWLINE_TYPE_CR_LF, 6);
	test_consecutive_read ("\nfo\nbar\n\nblah", "\r\nfo\r\nbar\r\n\r\nblah\r\n", GTK_SOURCE_NEWLINE_TYPE_CR_LF, 6);
}

static void
test_consecutive_multibyte_cut (void)
{
	test_consecutive_read ("hello\nhello\xe6\x96\x87\nworld\n", "hello\rhello\xe6\x96\x87\rworld\r\r", GTK_SOURCE_NEWLINE_TYPE_CR, 6);
	test_consecutive_read ("hello\rhello\xe6\x96\x87\rworld\r", "hello\rhello\xe6\x96\x87\rworld\r\r", GTK_SOURCE_NEWLINE_TYPE_CR, 6);
	test_consecutive_read ("hello\nhello\xe6\x96\x87\nworld\n", "hello\nhello\xe6\x96\x87\nworld\n\n", GTK_SOURCE_NEWLINE_TYPE_LF, 6);
}

static void
test_consecutive_multibyte_big_read (void)
{
	test_consecutive_read ("hello\nhello\xe6\x96\x87\nworld\n", "hello\rhello\xe6\x96\x87\rworld\r\r", GTK_SOURCE_NEWLINE_TYPE_CR, 200);
	test_consecutive_read ("hello\rhello\xe6\x96\x87\rworld\r", "hello\rhello\xe6\x96\x87\rworld\r\r", GTK_SOURCE_NEWLINE_TYPE_CR, 200);
	test_consecutive_read ("hello\nhello\xe6\x96\x87\nworld\n", "hello\nhello\xe6\x96\x87\nworld\n\n", GTK_SOURCE_NEWLINE_TYPE_LF, 200);
}

gint
main (gint   argc,
      gchar *argv[])
{
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/buffer-input-stream/empty", test_empty);

	g_test_add_func ("/buffer-input-stream/consecutive_cut_char", test_consecutive_cut_char);
	g_test_add_func ("/buffer-input-stream/consecutive_big_read", test_consecutive_big_read);
	g_test_add_func ("/buffer-input-stream/consecutive_middle_read", test_consecutive_middle_read);

	g_test_add_func ("/buffer-input-stream/consecutive_multibyte_cut", test_consecutive_multibyte_cut);
	g_test_add_func ("/buffer-input-stream/consecutive_multibyte_big_read", test_consecutive_multibyte_big_read);

	return g_test_run ();
}
