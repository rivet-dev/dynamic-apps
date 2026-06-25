/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2015 - Université Catholique de Louvain
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
 *
 * Author: Sébastien Wilmet
 */

#include <gtksourceview/gtksource.h>

static void
fill_buffer (GtkTextBuffer *buffer,
	     GtkTextTag    *tag)
{
	GtkTextIter iter;

	gtk_text_buffer_set_text (buffer, "", 0);

	gtk_text_buffer_get_start_iter (buffer, &iter);
	gtk_text_buffer_insert (buffer, &iter,
				"---\n"
				"\tText without draw-spaces tag.\n"
				"\tNon-breaking whitespace: .\n"
				"\tTrailing spaces.\t  \n"
				"---\n\n",
				-1);

	gtk_text_buffer_insert_with_tags (buffer, &iter,
					  "---\n"
					  "\tText with draw-spaces tag.\n"
					  "\tNon-breaking whitespace: .\n"
					  "\tTrailing spaces.\t  \n"
					  "---",
					  -1,
					  tag,
					  NULL);
}

static void
create_window (void)
{
	GtkWidget *window;
	GtkWidget *hgrid;
	GtkWidget *panel_grid;
	GtkWidget *scrolled_window;
	GtkWidget *matrix_checkbutton;
	GtkWidget *tag_set_checkbutton;
	GtkWidget *tag_checkbutton;
	GtkWidget *implicit_trailing_newline_checkbutton;
	GtkSourceView *view;
	GtkSourceBuffer *buffer;
	GtkTextTag *tag;
	GtkSourceSpaceDrawer *space_drawer;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
	g_signal_connect (window, "destroy", gtk_main_quit, NULL);

	hgrid = gtk_grid_new ();
	gtk_orientable_set_orientation (GTK_ORIENTABLE (hgrid), GTK_ORIENTATION_HORIZONTAL);

	view = GTK_SOURCE_VIEW (gtk_source_view_new ());

	g_object_set (view,
		      "expand", TRUE,
		      NULL);

	gtk_text_view_set_monospace (GTK_TEXT_VIEW (view), TRUE);

	buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));

	tag = gtk_source_buffer_create_source_tag (buffer,
						   NULL,
						   "draw-spaces", FALSE,
						   NULL);

	fill_buffer (GTK_TEXT_BUFFER (buffer), tag);

	space_drawer = gtk_source_view_get_space_drawer (view);
	gtk_source_space_drawer_set_types_for_locations (space_drawer,
							 GTK_SOURCE_SPACE_LOCATION_ALL,
							 GTK_SOURCE_SPACE_TYPE_NBSP);
	gtk_source_space_drawer_set_types_for_locations (space_drawer,
							 GTK_SOURCE_SPACE_LOCATION_TRAILING,
							 GTK_SOURCE_SPACE_TYPE_ALL);

	panel_grid = gtk_grid_new ();
	gtk_orientable_set_orientation (GTK_ORIENTABLE (panel_grid), GTK_ORIENTATION_VERTICAL);
	gtk_container_add (GTK_CONTAINER (hgrid), panel_grid);

	gtk_grid_set_row_spacing (GTK_GRID (panel_grid), 6);
	g_object_set (panel_grid,
		      "margin", 6,
		      NULL);

	matrix_checkbutton = gtk_check_button_new_with_label ("GtkSourceSpaceDrawer enable-matrix");
	gtk_container_add (GTK_CONTAINER (panel_grid), matrix_checkbutton);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (matrix_checkbutton), TRUE);
	g_object_bind_property (matrix_checkbutton, "active",
				space_drawer, "enable-matrix",
				G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

	tag_set_checkbutton = gtk_check_button_new_with_label ("GtkSourceTag draw-spaces-set");
	gtk_container_add (GTK_CONTAINER (panel_grid), tag_set_checkbutton);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tag_set_checkbutton), TRUE);
	g_object_bind_property (tag_set_checkbutton, "active",
				tag, "draw-spaces-set",
				G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

	tag_checkbutton = gtk_check_button_new_with_label ("GtkSourceTag draw-spaces");
	gtk_container_add (GTK_CONTAINER (panel_grid), tag_checkbutton);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tag_checkbutton), FALSE);
	g_object_bind_property (tag_checkbutton, "active",
				tag, "draw-spaces",
				G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

	implicit_trailing_newline_checkbutton = gtk_check_button_new_with_label ("Implicit trailing newline");
	gtk_widget_set_margin_top (implicit_trailing_newline_checkbutton, 12);
	gtk_container_add (GTK_CONTAINER (panel_grid), implicit_trailing_newline_checkbutton);
	g_object_bind_property (buffer, "implicit-trailing-newline",
				implicit_trailing_newline_checkbutton, "active",
				G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (view));
	gtk_container_add (GTK_CONTAINER (hgrid), scrolled_window);

	gtk_container_add (GTK_CONTAINER (window), hgrid);

	gtk_widget_show_all (window);
}

gint
main (gint    argc,
      gchar **argv)
{
	gtk_init (&argc, &argv);

	create_window ();

	gtk_main ();

	return 0;
}
