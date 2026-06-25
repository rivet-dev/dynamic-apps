/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- *
 *
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2009 - Jesse van den Kieboom
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

#ifndef GTK_SOURCE_GUTTER_PRIVATE_H
#define GTK_SOURCE_GUTTER_PRIVATE_H

#include <gtk/gtk.h>
#include "gtksourcetypes.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL
GtkSourceGutter *	_gtk_source_gutter_new		(GtkSourceView     *view,
							 GtkTextWindowType  type);

G_GNUC_INTERNAL
void			_gtk_source_gutter_draw		(GtkSourceGutter *gutter,
							 GtkSourceView   *view,
							 cairo_t         *cr);


G_END_DECLS

#endif /* GTK_SOURCE_GUTTER_PRIVATE_H */
