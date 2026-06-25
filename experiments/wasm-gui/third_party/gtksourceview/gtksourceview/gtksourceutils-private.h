/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2007 - Gustavo Giráldez
 * Copyright (C) 2007 - Paolo Maggi
 * Copyright (C) 2017 - Sébastien Wilmet <swilmet@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GTK_SOURCE_UTILS_PRIVATE_H
#define GTK_SOURCE_UTILS_PRIVATE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL
gchar **	_gtk_source_utils_get_default_dirs		(const gchar *basename);

G_GNUC_INTERNAL
GSList *	_gtk_source_utils_get_file_list			(gchar       **path,
								 const gchar  *suffix,
								 gboolean      only_dirs);

G_GNUC_INTERNAL
gint		_gtk_source_utils_string_to_int			(const gchar *str);

G_GNUC_INTERNAL
gint		_gtk_source_utils_int_to_string			(guint         value,
								 const gchar **outstr);

G_GNUC_INTERNAL
gchar *		_gtk_source_utils_pango_font_description_to_css	(const PangoFontDescription *font_desc);

/* Note: it returns duplicated string. */
G_GNUC_INTERNAL
gchar *		_gtk_source_utils_dgettext			(const gchar *domain,
								 const gchar *msgid) G_GNUC_FORMAT(2);
G_GNUC_INTERNAL
void _gtk_source_view_jump_to_iter (GtkTextView       *view,
                                    const GtkTextIter *iter,
                                    double             within_margin,
                                    gboolean           use_align,
                                    double             xalign,
                                    double             yalign);

G_END_DECLS

#endif /* GTK_SOURCE_UTILS_PRIVATE_H */
