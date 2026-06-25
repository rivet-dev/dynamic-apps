/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- */
/*
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2013, 2016 - Sébastien Wilmet <swilmet@gnome.org>
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

#ifndef GTK_SOURCE_BUFFER_PRIVATE_H
#define GTK_SOURCE_BUFFER_PRIVATE_H

#include <gtk/gtk.h>

#include "gtksourcetypes.h"
#include "gtksourcetypes-private.h"
#include "gtksourcebuffer.h"

G_BEGIN_DECLS

GTK_SOURCE_INTERNAL
void			 _gtk_source_buffer_update_syntax_highlight	(GtkSourceBuffer        *buffer,
									 const GtkTextIter      *start,
									 const GtkTextIter      *end,
									 gboolean                synchronous);

GTK_SOURCE_INTERNAL
void			 _gtk_source_buffer_update_search_highlight	(GtkSourceBuffer        *buffer,
									 const GtkTextIter      *start,
									 const GtkTextIter      *end,
									 gboolean                synchronous);

GTK_SOURCE_INTERNAL
GtkSourceMark		*_gtk_source_buffer_source_mark_next		(GtkSourceBuffer        *buffer,
									 GtkSourceMark          *mark,
									 const gchar            *category);

GTK_SOURCE_INTERNAL
GtkSourceMark		*_gtk_source_buffer_source_mark_prev		(GtkSourceBuffer        *buffer,
									 GtkSourceMark          *mark,
									 const gchar            *category);

GTK_SOURCE_INTERNAL
GtkTextTag		*_gtk_source_buffer_get_bracket_match_tag	(GtkSourceBuffer        *buffer);

GTK_SOURCE_INTERNAL
void			 _gtk_source_buffer_add_search_context		(GtkSourceBuffer        *buffer,
									 GtkSourceSearchContext *search_context);

GTK_SOURCE_INTERNAL
void			 _gtk_source_buffer_set_as_invalid_character	(GtkSourceBuffer        *buffer,
									 const GtkTextIter      *start,
									 const GtkTextIter      *end);

GTK_SOURCE_INTERNAL
gboolean		 _gtk_source_buffer_has_invalid_chars		(GtkSourceBuffer        *buffer);

GTK_SOURCE_INTERNAL
GtkSourceBracketMatchType
			 _gtk_source_buffer_find_bracket_match		(GtkSourceBuffer        *buffer,
									 const GtkTextIter      *pos,
									 GtkTextIter            *bracket,
									 GtkTextIter            *bracket_match);

GTK_SOURCE_INTERNAL
void			 _gtk_source_buffer_save_and_clear_selection	(GtkSourceBuffer        *buffer);

GTK_SOURCE_INTERNAL
void			 _gtk_source_buffer_restore_selection		(GtkSourceBuffer        *buffer);

GTK_SOURCE_INTERNAL
gboolean		 _gtk_source_buffer_is_undo_redo_enabled	(GtkSourceBuffer        *buffer);

GTK_SOURCE_INTERNAL
gboolean		_gtk_source_buffer_has_source_marks		(GtkSourceBuffer        *buffer);

GTK_SOURCE_INTERNAL
gboolean		_gtk_source_buffer_has_spaces_tag		(GtkSourceBuffer        *buffer);

G_END_DECLS

#endif /* GTK_SOURCE_BUFFER_PRIVATE_H */
