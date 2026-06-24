/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#if !defined (EXO_INSIDE_EXO_H) && !defined (EXO_COMPILATION)
#error "Only <exo/exo.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef __EXO_UTILS_H__
#define __EXO_UTILS_H__

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

void                    exo_noop        (void) G_GNUC_DEPRECATED;
gint                    exo_noop_one    (void) G_GNUC_PURE G_GNUC_DEPRECATED;
gint                    exo_noop_zero   (void) G_GNUC_PURE G_GNUC_DEPRECATED;
gpointer                exo_noop_null   (void) G_GNUC_PURE G_GNUC_DEPRECATED;
gboolean                exo_noop_true   (void) G_GNUC_PURE G_GNUC_DEPRECATED;
gboolean                exo_noop_false  (void) G_GNUC_PURE G_GNUC_DEPRECATED;

G_END_DECLS

#endif /* !__EXO_UTILS_H__ */
