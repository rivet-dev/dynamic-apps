/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 * Copyright (c) 2009 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#if !defined(_GARCON_INSIDE_GARCON_H) && !defined(GARCON_COMPILATION)
#error "Only <garcon/garcon.h> can be included directly. This file may disappear or change contents."
#endif

#ifndef __GARCON_PRIVATE_H__
#define __GARCON_PRIVATE_H__

G_BEGIN_DECLS

/* Macro for new g_?list_free_full function */
#define _garcon_g_slist_free_full(list,free_func) \
  g_slist_free_full (list, (GDestroyNotify) free_func)
#define _garcon_g_list_free_full(list,free_func) \
  g_list_free_full (list, (GDestroyNotify) free_func)

/* if XDG_MENU_PREFIX is not set, default to "xfce-" so garcon doesn't
 * break when xfce is not started with startxfce4 */
#define _GARCON_DEFAULT_MENU_PREFIX "xfce-"

GFile    *_garcon_file_new_for_unknown_input    (const gchar *path,
                                                 GFile       *parent);

GFile    *_garcon_file_new_relative_to_file     (const gchar *path,
                                                 GFile       *file);

gchar    *_garcon_file_get_uri_relative_to_file (const gchar *path,
                                                 GFile       *file);

G_END_DECLS

#endif /* !__GARCON_PRIVATE_H__ */
