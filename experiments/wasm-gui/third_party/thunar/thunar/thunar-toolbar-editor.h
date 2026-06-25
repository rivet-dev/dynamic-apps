/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2021 Sergios - Anestis Kefalidis <sergioskefalidis@gmail.com>
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

#ifndef __THUNAR_TOOLBAR_EDITOR_H__
#define __THUNAR_TOOLBAR_EDITOR_H__

G_BEGIN_DECLS;

typedef struct _ThunarToolbarEditorClass ThunarToolbarEditorClass;
typedef struct _ThunarToolbarEditor      ThunarToolbarEditor;

#define THUNAR_TYPE_TOOLBAR_EDITOR             (thunar_toolbar_editor_get_type ())
#define THUNAR_TOOLBAR_EDITOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_TOOLBAR_EDITOR, ThunarToolbarEditor))
#define THUNAR_TOOLBAR_EDITOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_TOOLBAR_EDITOR, ThunarToolbarEditorClass))
#define THUNAR_IS_TOOLBAR_EDITOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_TOOLBAR_EDITOR))
#define THUNAR_IS_TOOLBAR_EDITOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_TOOLBAR_EDITOR))
#define THUNAR_TOOLBAR_EDITOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_TOOLBAR_EDITOR, ThunarToolbarEditorClass))

GType      thunar_toolbar_editor_get_type (void) G_GNUC_CONST;

void       thunar_show_toolbar_editor     (GtkWidget *window,
                                           GtkWidget *window_toolbar);

G_END_DECLS;

#endif /* !__THUNAR_TOOLBAR_EDITOR_H__ */
