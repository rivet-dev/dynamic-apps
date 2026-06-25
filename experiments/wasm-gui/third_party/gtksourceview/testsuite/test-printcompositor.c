/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- */
/*
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2009 - Emmanuel Rodriguez
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
test_buffer_ref (void)
{
	GtkSourcePrintCompositor *compositor;
	GtkSourceBuffer *buffer = NULL;
	GtkSourceBuffer *buffer_original = NULL;

	buffer_original = gtk_source_buffer_new (NULL);

	compositor = gtk_source_print_compositor_new (buffer_original);
	buffer = gtk_source_print_compositor_get_buffer (compositor);
	g_assert_true (GTK_SOURCE_IS_BUFFER (buffer));

	g_object_unref (G_OBJECT (buffer_original));
	buffer = gtk_source_print_compositor_get_buffer (compositor);
	g_assert_true (GTK_SOURCE_IS_BUFFER (buffer));
}

static void
test_buffer_view_ref (void)
{
	GtkSourcePrintCompositor *compositor;
	GtkWidget *view = NULL;
	GtkSourceBuffer *buffer = NULL;

	view = gtk_source_view_new ();
	compositor = gtk_source_print_compositor_new_from_view (GTK_SOURCE_VIEW (view));
	buffer = gtk_source_print_compositor_get_buffer (compositor);
	g_assert_true (GTK_SOURCE_IS_BUFFER (buffer));

	gtk_widget_destroy (view);
	buffer = gtk_source_print_compositor_get_buffer (compositor);
	g_assert_true (GTK_SOURCE_IS_BUFFER (buffer));

	g_object_unref (G_OBJECT (compositor));
}

int
main (int argc, char** argv)
{
	gtk_test_init (&argc, &argv);

	g_test_add_func ("/PrintCompositor/buffer-ref", test_buffer_ref);
	g_test_add_func ("/PrintCompositor/buffer-view-ref", test_buffer_view_ref);

	return g_test_run();
}
