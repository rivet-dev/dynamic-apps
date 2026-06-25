/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_DND_H__
#define __THUNAR_DND_H__

#include <thunar/thunar-file.h>

G_BEGIN_DECLS;

GdkDragAction thunar_dnd_ask     (GtkWidget    *widget,
                                  ThunarFile   *folder,
                                  GList        *path_list,
                                  GdkDragAction actions);

gboolean      thunar_dnd_perform (GtkWidget    *widget,
                                  ThunarFile   *file,
                                  GList        *uri_list,
                                  GdkDragAction action,
                                  GClosure     *new_files_closure);

G_END_DECLS;

#endif /* !__THUNAR_DND_H__ */
