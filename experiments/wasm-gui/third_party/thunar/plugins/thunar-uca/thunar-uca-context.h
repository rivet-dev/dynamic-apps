/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_UCA_CONTEXT_H__
#define __THUNAR_UCA_CONTEXT_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS;

typedef struct _ThunarUcaContext ThunarUcaContext;

ThunarUcaContext *thunar_uca_context_new        (GtkWidget              *window,
                                                 GList                  *files);

ThunarUcaContext *thunar_uca_context_ref        (ThunarUcaContext       *context);
void              thunar_uca_context_unref      (ThunarUcaContext       *context);

GList            *thunar_uca_context_get_files  (const ThunarUcaContext *context);
GtkWidget        *thunar_uca_context_get_window (const ThunarUcaContext *context);

G_END_DECLS;

#endif /* !__THUNAR_UCA_CONTEXT_H__ */
