/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2001 - Mikael Hermansson <tyan@linux.se>
 * Copyright (C) 2003 - Gustavo Giráldez <gustavo.giraldez@gmx.net>
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

#include <string.h>
#include <gtksourceview/gtksource.h>

#define TEST_TYPE_WIDGET             (test_widget_get_type ())
#define TEST_WIDGET(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), TEST_TYPE_WIDGET, TestWidget))
#define TEST_WIDGET_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), TEST_TYPE_WIDGET, TestWidgetClass))
#define TEST_IS_WIDGET(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TEST_TYPE_WIDGET))
#define TEST_IS_WIDGET_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), TEST_TYPE_WIDGET))
#define TEST_WIDGET_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), TEST_TYPE_WIDGET, TestWidgetClass))

typedef struct _TestWidget         TestWidget;
typedef struct _TestWidgetClass    TestWidgetClass;
typedef struct _TestWidgetPrivate  TestWidgetPrivate;

struct _TestWidget
{
	GtkGrid parent;

	TestWidgetPrivate *priv;
};

struct _TestWidgetClass
{
	GtkGridClass parent_class;
};

struct _TestWidgetPrivate
{
	GtkSourceView *view;
	GtkSourceBuffer *buffer;
	GtkSourceFile *file;
	GtkSourceMap *map;
	GtkCheckButton *show_top_border_window_checkbutton;
	GtkCheckButton *show_map_checkbutton;
	GtkCheckButton *draw_spaces_checkbutton;
	GtkCheckButton *smart_backspace_checkbutton;
	GtkCheckButton *indent_width_checkbutton;
	GtkSpinButton *indent_width_spinbutton;
	GtkLabel *cursor_position_info;
	GtkSourceStyleSchemeChooserButton *chooser_button;
	GtkComboBoxText *background_pattern;
};

GType test_widget_get_type (void);

G_DEFINE_TYPE_WITH_PRIVATE (TestWidget, test_widget, GTK_TYPE_GRID)

#define MARK_TYPE_1      "one"
#define MARK_TYPE_2      "two"

static const char *cmd_filename;

static void
remove_all_marks (GtkSourceBuffer *buffer)
{
	GtkTextIter start;
	GtkTextIter end;

	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &start, &end);

	gtk_source_buffer_remove_source_marks (buffer, &start, &end, NULL);
}

static GtkSourceLanguage *
get_language_for_file (GtkTextBuffer *buffer,
		       const gchar   *filename)
{
	GtkSourceLanguageManager *manager;
	GtkSourceLanguage *language;
	GtkTextIter start;
	GtkTextIter end;
	gchar *text;
	gchar *content_type;
	gboolean result_uncertain;

	gtk_text_buffer_get_start_iter (buffer, &start);
	gtk_text_buffer_get_iter_at_offset (buffer, &end, 1024);
	text = gtk_text_buffer_get_slice (buffer, &start, &end, TRUE);

	content_type = g_content_type_guess (filename,
					     (guchar*) text,
					     strlen (text),
					     &result_uncertain);

	if (result_uncertain)
	{
		g_free (content_type);
		content_type = NULL;
	}

	manager = gtk_source_language_manager_get_default ();
	language = gtk_source_language_manager_guess_language (manager,
							       filename,
							       content_type);

	g_message ("Detected '%s' mime type for file %s, chose language %s",
		   content_type != NULL ? content_type : "(null)",
		   filename,
		   language != NULL ? gtk_source_language_get_id (language) : "(none)");

	g_free (content_type);
	g_free (text);
	return language;
}

static GtkSourceLanguage *
get_language_by_id (const gchar *id)
{
	GtkSourceLanguageManager *manager;
	manager = gtk_source_language_manager_get_default ();
	return gtk_source_language_manager_get_language (manager, id);
}

static GtkSourceLanguage *
get_language (GtkTextBuffer *buffer,
	      GFile         *location)
{
	GtkSourceLanguage *language = NULL;
	GtkTextIter start;
	GtkTextIter end;
	gchar *text;
	gchar *lang_string;

	gtk_text_buffer_get_start_iter (buffer, &start);
	end = start;
	gtk_text_iter_forward_line (&end);

#define LANG_STRING "gtk-source-lang:"

	text = gtk_text_iter_get_slice (&start, &end);
	lang_string = strstr (text, LANG_STRING);

	if (lang_string != NULL)
	{
		gchar **tokens;

		lang_string += strlen (LANG_STRING);
		g_strchug (lang_string);

		tokens = g_strsplit_set (lang_string, " \t\n", 2);

		if (tokens != NULL && tokens[0] != NULL)
		{
			language = get_language_by_id (tokens[0]);
		}

		g_strfreev (tokens);
	}

	if (language == NULL)
	{
		gchar *filename = g_file_get_path (location);
		language = get_language_for_file (buffer, filename);
		g_free (filename);
	}

	g_free (text);
	return language;
}

static void
print_language_style_ids (GtkSourceLanguage *language)
{
	gchar **styles;

	g_assert_nonnull (language);

	styles = gtk_source_language_get_style_ids (language);

	if (styles == NULL)
	{
		g_print ("No styles in language '%s'\n",
			 gtk_source_language_get_name (language));
	}
	else
	{
		gchar **ids;
		g_print ("Styles in language '%s':\n",
			 gtk_source_language_get_name (language));

		for (ids = styles; *ids != NULL; ids++)
		{
			const gchar *name = gtk_source_language_get_style_name (language, *ids);

			g_print ("- %s (name: '%s')\n", *ids, name);
		}

		g_strfreev (styles);
	}

	g_print ("\n");
}

static void
load_cb (GtkSourceFileLoader *loader,
	 GAsyncResult        *result,
	 TestWidget          *self)
{
	GtkTextIter iter;
	GFile *location;
	GtkSourceLanguage *language = NULL;
	GError *error = NULL;

	gtk_source_file_loader_load_finish (loader, result, &error);

	if (error != NULL)
	{
		g_warning ("Error while loading the file: %s", error->message);
		g_clear_error (&error);
		g_clear_object (&self->priv->file);
		goto end;
	}

	/* move cursor to the beginning */
	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (self->priv->buffer), &iter);
	gtk_text_buffer_place_cursor (GTK_TEXT_BUFFER (self->priv->buffer), &iter);
	gtk_widget_grab_focus (GTK_WIDGET (self->priv->view));

	location = gtk_source_file_loader_get_location (loader);

	language = get_language (GTK_TEXT_BUFFER (self->priv->buffer), location);
	gtk_source_buffer_set_language (self->priv->buffer, language);

	if (language != NULL)
	{
		print_language_style_ids (language);
	}
	else
	{
		gchar *path = g_file_get_path (location);
		g_print ("No language found for file '%s'\n", path);
		g_free (path);
	}

end:
	g_object_unref (loader);
}

static void
open_file (TestWidget *self,
	   GFile      *location)
{
	GtkSourceFileLoader *loader;

	g_clear_object (&self->priv->file);
	self->priv->file = gtk_source_file_new ();

	gtk_source_file_set_location (self->priv->file, location);

	loader = gtk_source_file_loader_new (self->priv->buffer,
					     self->priv->file);

	remove_all_marks (self->priv->buffer);

	gtk_source_file_loader_load_async (loader,
					   G_PRIORITY_DEFAULT,
					   NULL,
					   NULL, NULL, NULL,
					   (GAsyncReadyCallback) load_cb,
					   self);
}

static void
show_line_numbers_toggled_cb (TestWidget     *self,
			      GtkCheckButton *button)
{
	gboolean enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	gtk_source_view_set_show_line_numbers (self->priv->view, enabled);
}

static void
show_line_marks_toggled_cb (TestWidget     *self,
			    GtkCheckButton *button)
{
	gboolean enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	gtk_source_view_set_show_line_marks (self->priv->view, enabled);
}

static void
show_right_margin_toggled_cb (TestWidget     *self,
			      GtkCheckButton *button)
{
	gboolean enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	gtk_source_view_set_show_right_margin (self->priv->view, enabled);
}

static void
right_margin_position_value_changed_cb (TestWidget    *self,
					GtkSpinButton *button)
{
	gint position = gtk_spin_button_get_value_as_int (button);
	gtk_source_view_set_right_margin_position (self->priv->view, position);
}

static void
highlight_syntax_toggled_cb (TestWidget     *self,
			     GtkCheckButton *button)
{
	gboolean enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	gtk_source_buffer_set_highlight_syntax (self->priv->buffer, enabled);
}

static void
highlight_matching_bracket_toggled_cb (TestWidget     *self,
				       GtkCheckButton *button)
{
	gboolean enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	gtk_source_buffer_set_highlight_matching_brackets (self->priv->buffer, enabled);
}

static void
highlight_current_line_toggled_cb (TestWidget     *self,
				   GtkCheckButton *button)
{
	gboolean enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	gtk_source_view_set_highlight_current_line (self->priv->view, enabled);
}

static void
wrap_lines_toggled_cb (TestWidget     *self,
		       GtkCheckButton *button)
{
	gboolean enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (self->priv->view),
				     enabled ? GTK_WRAP_WORD : GTK_WRAP_NONE);
}

static void
auto_indent_toggled_cb (TestWidget     *self,
			GtkCheckButton *button)
{
	gboolean enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	gtk_source_view_set_auto_indent (self->priv->view, enabled);
}

static void
indent_spaces_toggled_cb (TestWidget     *self,
			  GtkCheckButton *button)
{
	gboolean enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	gtk_source_view_set_insert_spaces_instead_of_tabs (self->priv->view, enabled);
}

static void
tab_width_value_changed_cb (TestWidget    *self,
			    GtkSpinButton *button)
{
	gint tab_width = gtk_spin_button_get_value_as_int (button);
	gtk_source_view_set_tab_width (self->priv->view, tab_width);
}

static void
update_indent_width (TestWidget *self)
{
	gint indent_width = -1;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->priv->indent_width_checkbutton)))
	{
		indent_width = gtk_spin_button_get_value_as_int (self->priv->indent_width_spinbutton);
	}

	gtk_source_view_set_indent_width (self->priv->view, indent_width);
}

static void
smart_home_end_changed_cb (TestWidget  *self,
			   GtkComboBox *combo)
{
	GtkSourceSmartHomeEndType type;
	gint active = gtk_combo_box_get_active (combo);

	switch (active)
	{
		case 0:
			type = GTK_SOURCE_SMART_HOME_END_DISABLED;
			break;

		case 1:
			type = GTK_SOURCE_SMART_HOME_END_BEFORE;
			break;

		case 2:
			type = GTK_SOURCE_SMART_HOME_END_AFTER;
			break;

		case 3:
			type = GTK_SOURCE_SMART_HOME_END_ALWAYS;
			break;

		default:
			type = GTK_SOURCE_SMART_HOME_END_DISABLED;
			break;
	}

	gtk_source_view_set_smart_home_end (self->priv->view, type);
}

static void
backward_string_clicked_cb (TestWidget *self)
{
	GtkTextIter iter;
	GtkTextMark *insert;

	insert = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (self->priv->buffer));

	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (self->priv->buffer),
	                                  &iter,
	                                  insert);

	if (gtk_source_buffer_iter_backward_to_context_class_toggle (self->priv->buffer,
	                                                             &iter,
	                                                             "string"))
	{
		gtk_text_buffer_place_cursor (GTK_TEXT_BUFFER (self->priv->buffer), &iter);
		gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (self->priv->view), insert);
	}

	gtk_widget_grab_focus (GTK_WIDGET (self->priv->view));
}

static void
forward_string_clicked_cb (TestWidget *self)
{
	GtkTextIter iter;
	GtkTextMark *insert;

	insert = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (self->priv->buffer));

	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (self->priv->buffer),
	                                  &iter,
	                                  insert);

	if (gtk_source_buffer_iter_forward_to_context_class_toggle (self->priv->buffer,
								    &iter,
								    "string"))
	{
		gtk_text_buffer_place_cursor (GTK_TEXT_BUFFER (self->priv->buffer), &iter);
		gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (self->priv->view), insert);
	}

	gtk_widget_grab_focus (GTK_WIDGET (self->priv->view));
}

static void
open_button_clicked_cb (TestWidget *self)
{
	GtkWidget *main_window;
	GtkWidget *chooser;
	gint response;
	static gchar *last_dir;

	main_window = gtk_widget_get_toplevel (GTK_WIDGET (self->priv->view));

	chooser = gtk_file_chooser_dialog_new ("Open file...",
					       GTK_WINDOW (main_window),
					       GTK_FILE_CHOOSER_ACTION_OPEN,
					       "Cancel", GTK_RESPONSE_CANCEL,
					       "Open", GTK_RESPONSE_OK,
					       NULL);

	if (last_dir == NULL)
	{
		last_dir = g_strdup (TOP_SRCDIR "/gtksourceview");
	}

	if (last_dir != NULL && g_path_is_absolute (last_dir))
	{
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser),
						     last_dir);
	}

	response = gtk_dialog_run (GTK_DIALOG (chooser));

	if (response == GTK_RESPONSE_OK)
	{
		GFile *file;

		file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (chooser));

		if (file != NULL)
		{
			g_free (last_dir);
			last_dir = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (chooser));
			open_file (self, file);
			g_object_unref (file);
		}
	}

	gtk_widget_destroy (chooser);
}

#define NON_BLOCKING_PAGINATION

#ifndef NON_BLOCKING_PAGINATION

static void
begin_print (GtkPrintOperation        *operation,
	     GtkPrintContext          *context,
	     GtkSourcePrintCompositor *compositor)
{
	gint n_pages;

	while (!gtk_source_print_compositor_paginate (compositor, context))
		;

	n_pages = gtk_source_print_compositor_get_n_pages (compositor);
	gtk_print_operation_set_n_pages (operation, n_pages);
}

#else

static gboolean
paginate (GtkPrintOperation        *operation,
	  GtkPrintContext          *context,
	  GtkSourcePrintCompositor *compositor)
{
	g_print ("Pagination progress: %.2f %%\n", gtk_source_print_compositor_get_pagination_progress (compositor) * 100.0);

	if (gtk_source_print_compositor_paginate (compositor, context))
	{
		gint n_pages;

		g_assert_cmpint (gtk_source_print_compositor_get_pagination_progress (compositor), ==, 1.0);
		g_print ("Pagination progress: %.2f %%\n", gtk_source_print_compositor_get_pagination_progress (compositor) * 100.0);

		n_pages = gtk_source_print_compositor_get_n_pages (compositor);
		gtk_print_operation_set_n_pages (operation, n_pages);

		return TRUE;
	}

	return FALSE;
}

#endif

#define ENABLE_CUSTOM_OVERLAY

static void
draw_page (GtkPrintOperation        *operation,
	   GtkPrintContext          *context,
	   gint                      page_nr,
	   GtkSourcePrintCompositor *compositor)
{
#ifdef ENABLE_CUSTOM_OVERLAY

	/* This part of the code shows how to add a custom overlay to the
	   printed text generated by GtkSourcePrintCompositor */

	cairo_t *cr;
	PangoLayout *layout;
	PangoFontDescription *desc;
	PangoRectangle rect;


	cr = gtk_print_context_get_cairo_context (context);

	cairo_save (cr);

	layout = gtk_print_context_create_pango_layout (context);

	pango_layout_set_text (layout, "Draft", -1);

	desc = pango_font_description_from_string ("Sans Bold 120");
	pango_layout_set_font_description (layout, desc);
	pango_font_description_free (desc);


	pango_layout_get_extents (layout, NULL, &rect);

  	cairo_move_to (cr,
  		       (gtk_print_context_get_width (context) - ((double) rect.width / (double) PANGO_SCALE)) / 2,
  		       (gtk_print_context_get_height (context) - ((double) rect.height / (double) PANGO_SCALE)) / 2);

	pango_cairo_layout_path (cr, layout);

  	/* Font Outline */
	cairo_set_source_rgba (cr, 0.85, 0.85, 0.85, 0.80);
	cairo_set_line_width (cr, 0.5);
	cairo_stroke_preserve (cr);

	/* Font Fill */
	cairo_set_source_rgba (cr, 0.8, 0.8, 0.8, 0.60);
	cairo_fill (cr);

	g_object_unref (layout);
	cairo_restore (cr);
#endif

	/* To print page_nr you only need to call the following function */
	gtk_source_print_compositor_draw_page (compositor, context, page_nr);
}

static void
end_print (GtkPrintOperation        *operation,
	   GtkPrintContext          *context,
	   GtkSourcePrintCompositor *compositor)
{
	g_object_unref (compositor);
}

#define LINE_NUMBERS_FONT_NAME	"Sans 8"
#define HEADER_FONT_NAME	"Sans 11"
#define FOOTER_FONT_NAME	"Sans 11"
#define BODY_FONT_NAME		"Monospace 9"

/*
#define SETUP_FROM_VIEW
*/

#undef SETUP_FROM_VIEW

static void
print_button_clicked_cb (TestWidget *self)
{
	gchar *basename = NULL;
	GtkSourcePrintCompositor *compositor;
	GtkPrintOperation *operation;

	if (self->priv->file != NULL)
	{
		GFile *location;

		location = gtk_source_file_get_location (self->priv->file);

		if (location != NULL)
		{
			basename = g_file_get_basename (location);
		}
	}

#ifdef SETUP_FROM_VIEW
	compositor = gtk_source_print_compositor_new_from_view (self->priv->view);
#else
	compositor = gtk_source_print_compositor_new (self->priv->buffer);

	gtk_source_print_compositor_set_tab_width (compositor,
						   gtk_source_view_get_tab_width (self->priv->view));

	gtk_source_print_compositor_set_wrap_mode (compositor,
						   gtk_text_view_get_wrap_mode (GTK_TEXT_VIEW (self->priv->view)));

	gtk_source_print_compositor_set_print_line_numbers (compositor, 1);

	gtk_source_print_compositor_set_body_font_name (compositor,
							BODY_FONT_NAME);

	/* To test line numbers font != text font */
	gtk_source_print_compositor_set_line_numbers_font_name (compositor,
								LINE_NUMBERS_FONT_NAME);

	gtk_source_print_compositor_set_header_format (compositor,
						       TRUE,
						       "Printed on %A",
						       "test-widget",
						       "%F");

	gtk_source_print_compositor_set_footer_format (compositor,
						       TRUE,
						       "%T",
						       basename,
						       "Page %N/%Q");

	gtk_source_print_compositor_set_print_header (compositor, TRUE);
	gtk_source_print_compositor_set_print_footer (compositor, TRUE);

	gtk_source_print_compositor_set_header_font_name (compositor,
							  HEADER_FONT_NAME);

	gtk_source_print_compositor_set_footer_font_name (compositor,
							  FOOTER_FONT_NAME);
#endif
	operation = gtk_print_operation_new ();

	gtk_print_operation_set_job_name (operation, basename);

	gtk_print_operation_set_show_progress (operation, TRUE);

#ifndef NON_BLOCKING_PAGINATION
  	g_signal_connect (G_OBJECT (operation), "begin-print",
			  G_CALLBACK (begin_print), compositor);
#else
  	g_signal_connect (G_OBJECT (operation), "paginate",
			  G_CALLBACK (paginate), compositor);
#endif
	g_signal_connect (G_OBJECT (operation), "draw-page",
			  G_CALLBACK (draw_page), compositor);
	g_signal_connect (G_OBJECT (operation), "end-print",
			  G_CALLBACK (end_print), compositor);

	gtk_print_operation_run (operation,
				 GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
				 NULL, NULL);

	g_object_unref (operation);
	g_free (basename);
}

static void
update_cursor_position_info (TestWidget *self)
{
	gchar *msg;
	gint offset;
	gint line;
	guint column;
	GtkTextIter iter;
	gchar **classes;
	gchar **classes_ptr;
	GString *classes_str;
	GtkSourceLanguage *lang;
	const char *language = "none";

	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (self->priv->buffer),
					  &iter,
					  gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (self->priv->buffer)));

	offset = gtk_text_iter_get_offset (&iter);
	line = gtk_text_iter_get_line (&iter) + 1;
	column = gtk_source_view_get_visual_column (self->priv->view, &iter) + 1;

	classes = gtk_source_buffer_get_context_classes_at_iter (self->priv->buffer, &iter);

	classes_str = g_string_new ("");

	for (classes_ptr = classes; classes_ptr != NULL && *classes_ptr != NULL; classes_ptr++)
	{
		if (classes_ptr != classes)
		{
			g_string_append (classes_str, ", ");
		}

		g_string_append_printf (classes_str, "%s", *classes_ptr);
	}

	g_strfreev (classes);

	if ((lang = gtk_source_buffer_get_language (self->priv->buffer)))
		language = gtk_source_language_get_id (lang);

	msg = g_strdup_printf ("language: %s offset: %d, line: %d, column: %u, classes: %s",
	                       language,
	                       offset,
	                       line,
	                       column,
	                       classes_str->str);

	gtk_label_set_text (self->priv->cursor_position_info, msg);

	g_free (msg);
	g_string_free (classes_str, TRUE);
}

static void
mark_set_cb (GtkTextBuffer *buffer,
	     GtkTextIter   *iter,
	     GtkTextMark   *mark,
	     TestWidget    *self)
{
	if (mark == gtk_text_buffer_get_insert (buffer))
	{
		update_cursor_position_info (self);
	}
}

static void
line_mark_activated_cb (GtkSourceGutter *gutter,
			GtkTextIter     *iter,
			GdkEventButton  *event,
			TestWidget      *self)
{
	GSList *mark_list;
	const gchar *mark_type;

	mark_type = event->button == 1 ? MARK_TYPE_1 : MARK_TYPE_2;

	/* get the marks already in the line */
	mark_list = gtk_source_buffer_get_source_marks_at_line (self->priv->buffer,
								gtk_text_iter_get_line (iter),
								mark_type);

	if (mark_list != NULL)
	{
		/* just take the first and delete it */
		gtk_text_buffer_delete_mark (GTK_TEXT_BUFFER (self->priv->buffer),
					     GTK_TEXT_MARK (mark_list->data));
	}
	else
	{
		/* no mark found: create one */
		gtk_source_buffer_create_source_mark (self->priv->buffer,
						      NULL,
						      mark_type,
						      iter);
	}

	g_slist_free (mark_list);
}

static void
bracket_matched_cb (GtkSourceBuffer           *buffer,
		    GtkTextIter               *iter,
		    GtkSourceBracketMatchType  state)
{
	GEnumClass *eclass;
	GEnumValue *evalue;

	eclass = G_ENUM_CLASS (g_type_class_ref (GTK_SOURCE_TYPE_BRACKET_MATCH_TYPE));
	evalue = g_enum_get_value (eclass, state);

	g_print ("Bracket match state: '%s'\n", evalue->value_nick);

	g_type_class_unref (eclass);

	if (state == GTK_SOURCE_BRACKET_MATCH_FOUND)
	{
		g_return_if_fail (iter != NULL);

		g_print ("Matched bracket: '%c' at row: %"G_GINT32_FORMAT", col: %"G_GINT32_FORMAT"\n",
		         gtk_text_iter_get_char (iter),
		         gtk_text_iter_get_line (iter) + 1,
		         gtk_text_iter_get_line_offset (iter) + 1);
	}
}

static gchar *
mark_tooltip_func (GtkSourceMarkAttributes *attrs,
                   GtkSourceMark           *mark,
                   GtkSourceView           *view)
{
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	gint line;
	gint column;

	buffer = gtk_text_mark_get_buffer (GTK_TEXT_MARK (mark));

	gtk_text_buffer_get_iter_at_mark (buffer, &iter, GTK_TEXT_MARK (mark));
	line = gtk_text_iter_get_line (&iter) + 1;
	column = gtk_text_iter_get_line_offset (&iter);

	if (g_strcmp0 (gtk_source_mark_get_category (mark), MARK_TYPE_1) == 0)
	{
		return g_strdup_printf ("Line: %d, Column: %d", line, column);
	}
	else
	{
		return g_strdup_printf ("<b>Line</b>: %d\n<i>Column</i>: %d", line, column);
	}
}

static void
add_source_mark_attributes (GtkSourceView *view)
{
	GdkRGBA color;
	GtkSourceMarkAttributes *attrs;

	attrs = gtk_source_mark_attributes_new ();

	gdk_rgba_parse (&color, "lightgreen");
	gtk_source_mark_attributes_set_background (attrs, &color);

	gtk_source_mark_attributes_set_icon_name (attrs, "list-add");

	g_signal_connect_object (attrs,
				 "query-tooltip-markup",
				 G_CALLBACK (mark_tooltip_func),
				 view,
				 0);

	gtk_source_view_set_mark_attributes (view, MARK_TYPE_1, attrs, 1);
	g_object_unref (attrs);

	attrs = gtk_source_mark_attributes_new ();

	gdk_rgba_parse (&color, "pink");
	gtk_source_mark_attributes_set_background (attrs, &color);

	gtk_source_mark_attributes_set_icon_name (attrs, "list-remove");

	g_signal_connect_object (attrs,
				 "query-tooltip-markup",
				 G_CALLBACK (mark_tooltip_func),
				 view,
				 0);

	gtk_source_view_set_mark_attributes (view, MARK_TYPE_2, attrs, 2);
	g_object_unref (attrs);
}

static void
on_background_pattern_changed (GtkComboBox *combobox,
                               TestWidget  *self)
{
	gchar *text;

	text = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (combobox));

	if (g_strcmp0 (text, "Grid") == 0)
	{
		gtk_source_view_set_background_pattern (self->priv->view,
		                                        GTK_SOURCE_BACKGROUND_PATTERN_TYPE_GRID);
	}
	else
	{
		gtk_source_view_set_background_pattern (self->priv->view,
		                                        GTK_SOURCE_BACKGROUND_PATTERN_TYPE_NONE);
	}

	g_free (text);
}

static void
test_widget_dispose (GObject *object)
{
	TestWidget *self = TEST_WIDGET (object);

	g_clear_object (&self->priv->buffer);
	g_clear_object (&self->priv->file);

	G_OBJECT_CLASS (test_widget_parent_class)->dispose (object);
}

static void
test_widget_class_init (TestWidgetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = test_widget_dispose;

	gtk_widget_class_set_template_from_resource (widget_class,
						     "/org/gnome/gtksourceview/tests/ui/test-widget.ui");

	gtk_widget_class_bind_template_callback (widget_class, open_button_clicked_cb);
	gtk_widget_class_bind_template_callback (widget_class, print_button_clicked_cb);
	gtk_widget_class_bind_template_callback (widget_class, highlight_syntax_toggled_cb);
	gtk_widget_class_bind_template_callback (widget_class, highlight_matching_bracket_toggled_cb);
	gtk_widget_class_bind_template_callback (widget_class, show_line_numbers_toggled_cb);
	gtk_widget_class_bind_template_callback (widget_class, show_line_marks_toggled_cb);
	gtk_widget_class_bind_template_callback (widget_class, show_right_margin_toggled_cb);
	gtk_widget_class_bind_template_callback (widget_class, right_margin_position_value_changed_cb);
	gtk_widget_class_bind_template_callback (widget_class, highlight_current_line_toggled_cb);
	gtk_widget_class_bind_template_callback (widget_class, wrap_lines_toggled_cb);
	gtk_widget_class_bind_template_callback (widget_class, auto_indent_toggled_cb);
	gtk_widget_class_bind_template_callback (widget_class, indent_spaces_toggled_cb);
	gtk_widget_class_bind_template_callback (widget_class, tab_width_value_changed_cb);
	gtk_widget_class_bind_template_callback (widget_class, backward_string_clicked_cb);
	gtk_widget_class_bind_template_callback (widget_class, forward_string_clicked_cb);
	gtk_widget_class_bind_template_callback (widget_class, smart_home_end_changed_cb);

	gtk_widget_class_bind_template_child_private (widget_class, TestWidget, view);
	gtk_widget_class_bind_template_child_private (widget_class, TestWidget, map);
	gtk_widget_class_bind_template_child_private (widget_class, TestWidget, show_top_border_window_checkbutton);
	gtk_widget_class_bind_template_child_private (widget_class, TestWidget, show_map_checkbutton);
	gtk_widget_class_bind_template_child_private (widget_class, TestWidget, draw_spaces_checkbutton);
	gtk_widget_class_bind_template_child_private (widget_class, TestWidget, smart_backspace_checkbutton);
	gtk_widget_class_bind_template_child_private (widget_class, TestWidget, indent_width_checkbutton);
	gtk_widget_class_bind_template_child_private (widget_class, TestWidget, indent_width_spinbutton);
	gtk_widget_class_bind_template_child_private (widget_class, TestWidget, cursor_position_info);
	gtk_widget_class_bind_template_child_private (widget_class, TestWidget, chooser_button);
	gtk_widget_class_bind_template_child_private (widget_class, TestWidget, background_pattern);
}

static void
show_top_border_window_toggled_cb (GtkToggleButton *checkbutton,
				   TestWidget      *self)
{
	gint size;

	size = gtk_toggle_button_get_active (checkbutton) ? 20 : 0;

	gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (self->priv->view),
					      GTK_TEXT_WINDOW_TOP,
					      size);
}

static void
test_widget_init (TestWidget *self)
{
	GtkSourceSpaceDrawer *space_drawer;
	GFile *file;

	self->priv = test_widget_get_instance_private (self);

	gtk_widget_init_template (GTK_WIDGET (self));

	self->priv->buffer = GTK_SOURCE_BUFFER (
		gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->priv->view)));

	g_signal_connect_swapped (self->priv->buffer,
	                          "notify::language",
	                          G_CALLBACK (update_cursor_position_info),
	                          self);

	g_object_ref (self->priv->buffer);

	g_signal_connect (self->priv->show_top_border_window_checkbutton,
			  "toggled",
			  G_CALLBACK (show_top_border_window_toggled_cb),
			  self);

	g_signal_connect_swapped (self->priv->indent_width_checkbutton,
				  "toggled",
				  G_CALLBACK (update_indent_width),
				  self);

	g_signal_connect_swapped (self->priv->indent_width_spinbutton,
				  "value-changed",
				  G_CALLBACK (update_indent_width),
				  self);

	g_signal_connect (self->priv->buffer,
			  "mark-set",
			  G_CALLBACK (mark_set_cb),
			  self);

	g_signal_connect_swapped (self->priv->buffer,
				  "changed",
				  G_CALLBACK (update_cursor_position_info),
				  self);

	g_signal_connect (self->priv->buffer,
			  "bracket-matched",
			  G_CALLBACK (bracket_matched_cb),
			  NULL);

	add_source_mark_attributes (self->priv->view);

	g_signal_connect (self->priv->view,
			  "line-mark-activated",
			  G_CALLBACK (line_mark_activated_cb),
			  self);

	g_object_bind_property (self->priv->chooser_button,
	                        "style-scheme",
	                        self->priv->buffer,
	                        "style-scheme",
	                        G_BINDING_SYNC_CREATE);

	g_object_bind_property (self->priv->show_map_checkbutton,
	                        "active",
	                        self->priv->map,
	                        "visible",
	                        G_BINDING_SYNC_CREATE);

	g_object_bind_property (self->priv->smart_backspace_checkbutton,
	                        "active",
	                        self->priv->view,
	                        "smart-backspace",
	                        G_BINDING_SYNC_CREATE);

	g_signal_connect (self->priv->background_pattern,
	                  "changed",
	                  G_CALLBACK (on_background_pattern_changed),
	                  self);

	space_drawer = gtk_source_view_get_space_drawer (self->priv->view);
	g_object_bind_property (self->priv->draw_spaces_checkbutton, "active",
				space_drawer, "enable-matrix",
				G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

	if (cmd_filename)
		file = g_file_new_for_commandline_arg (cmd_filename);
	else
		file = g_file_new_for_path (TOP_SRCDIR "/gtksourceview/gtksourcebuffer.c");

	open_file (self, file);
	g_object_unref (file);
}

static TestWidget *
test_widget_new (void)
{
	return g_object_new (test_widget_get_type (), NULL);
}

int
main (int argc, char *argv[])
{
	GtkWidget *window;
	TestWidget *test_widget;

	gtk_init (&argc, &argv);
	gtk_source_init ();

	if (argc == 2 && g_file_test (argv[1], G_FILE_TEST_IS_REGULAR))
	{
		cmd_filename = argv[1];
	}

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (window), 900, 600);

	g_signal_connect (window, "destroy", gtk_main_quit, NULL);

	test_widget = test_widget_new ();
	gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (test_widget));

	gtk_widget_show (window);

	gtk_main ();

	gtk_source_finalize ();

	return 0;
}
