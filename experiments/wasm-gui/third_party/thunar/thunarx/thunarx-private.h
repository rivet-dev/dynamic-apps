/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined(THUNARX_INSIDE_THUNARX_H) && !defined(THUNARX_COMPILATION)
#error "Only <thunarx/thunarx.h> can be included directly, this file may disappear or change contents"
#endif

#ifndef __THUNARX_PRIVATE_H__
#define __THUNARX_PRIVATE_H__

#include <glib-object.h>

#define THUNAR_THREADS_ENTER \
G_GNUC_BEGIN_IGNORE_DEPRECATIONS \
  gdk_threads_enter(); \
G_GNUC_END_IGNORE_DEPRECATIONS

#define THUNAR_THREADS_LEAVE \
G_GNUC_BEGIN_IGNORE_DEPRECATIONS \
  gdk_threads_leave (); \
G_GNUC_END_IGNORE_DEPRECATIONS

G_BEGIN_DECLS

G_GNUC_INTERNAL
void   thunarx_object_list_take_reference (GList      *object_list,
                                           gpointer    target);

G_GNUC_INTERNAL
gchar *thunarx_param_spec_get_option_name (GParamSpec *pspec) G_GNUC_MALLOC;

G_END_DECLS

#endif /* !__THUNARX_PRIVATE_H__ */
