/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_COLUMN_EDITOR_H__
#define __THUNAR_COLUMN_EDITOR_H__

#include <thunar/thunar-column-model.h>

G_BEGIN_DECLS;

typedef struct _ThunarColumnEditorClass ThunarColumnEditorClass;
typedef struct _ThunarColumnEditor      ThunarColumnEditor;

#define THUNAR_TYPE_COLUMN_EDITOR             (thunar_column_editor_get_type ())
#define THUNAR_COLUMN_EDITOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_COLUMN_EDITOR, ThunarColumnEditor))
#define THUNAR_COLUMN_EDITOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_COLUMN_EDITOR, ThunarColumnEditorClass))
#define THUNAR_IS_COLUMN_EDITOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_COLUMN_EDITOR))
#define THUNAR_IS_COLUMN_EDITOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_COLUMN_EDITOR))
#define THUNAR_COLUMN_EDITOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_COLUMN_EDITOR, ThunarColumnEditorClass))

GType      thunar_column_editor_get_type (void) G_GNUC_CONST;

gboolean   thunar_show_column_editor     (gpointer parent);

G_END_DECLS;

#endif /* !__THUNAR_COLUMN_EDITOR_H__ */
