/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2003-2007 Benedikt Meurer <benny@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __THUNAR_GDK_EXTENSIONS_H__
#define __THUNAR_GDK_EXTENSIONS_H__

#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS;

GdkScreen *thunar_gdk_screen_open             (const gchar *display_name,
                                               GError     **error);

void       thunar_gdk_cairo_set_source_pixbuf (cairo_t     *cr,
                                               GdkPixbuf   *pixbuf,
                                               gdouble      pixbuf_x,
                                               gdouble      pixbuf_y,
                                               gint         scale_factor);

G_END_DECLS;

#endif /* !__THUNAR_GDK_EXTENSIONS_H__ */
