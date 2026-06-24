/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>.
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

#ifndef __EXO_DIE_EDITOR_H__
#define __EXO_DIE_EDITOR_H__

#include <exo-desktop-item-edit/exo-die-enum-types.h>

G_BEGIN_DECLS;

typedef struct _ExoDieEditorClass ExoDieEditorClass;
typedef struct _ExoDieEditor      ExoDieEditor;

#define EXO_DIE_TYPE_EDITOR             (exo_die_editor_get_type ())
#define EXO_DIE_EDITOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), EXO_DIE_TYPE_EDITOR, ExoDieEditor))
#define EXO_DIE_EDITOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), EXO_DIE_TYPE_EDITOR, ExoDieEditorClass))
#define EXO_DIE_IS_EDITOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EXO_DIE_TYPE_EDITOR))
#define EXO_DIE_IS_EDITOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), EXO_DIE_TYPE_EDITOR))
#define EXO_DIE_EDITOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), EXO_DIE_TYPE_EDITOR, ExoDieEditorClass))

GType             exo_die_editor_get_type     (void) G_GNUC_CONST;

GtkWidget        *exo_die_editor_new          (void) G_GNUC_MALLOC;

gboolean          exo_die_editor_get_complete (ExoDieEditor     *editor);

ExoDieEditorMode  exo_die_editor_get_mode     (ExoDieEditor     *editor);
void              exo_die_editor_set_mode     (ExoDieEditor     *editor,
                                               ExoDieEditorMode  mode);

const gchar      *exo_die_editor_get_name     (ExoDieEditor     *editor);
void              exo_die_editor_set_name     (ExoDieEditor     *editor,
                                               const gchar      *name);

const gchar      *exo_die_editor_get_comment  (ExoDieEditor     *editor);
void              exo_die_editor_set_comment  (ExoDieEditor     *editor,
                                               const gchar      *comment);

const gchar      *exo_die_editor_get_command  (ExoDieEditor     *editor);
void              exo_die_editor_set_command  (ExoDieEditor     *editor,
                                               const gchar      *command);

const gchar      *exo_die_editor_get_url      (ExoDieEditor     *editor);
void              exo_die_editor_set_url      (ExoDieEditor     *editor,
                                               const gchar      *url);

const gchar      *exo_die_editor_get_path     (ExoDieEditor     *editor);
void              exo_die_editor_set_path     (ExoDieEditor     *editor,
                                               const gchar      *path);

const gchar      *exo_die_editor_get_icon     (ExoDieEditor     *editor);
void              exo_die_editor_set_icon     (ExoDieEditor     *editor,
                                               const gchar      *icon);

gboolean          exo_die_editor_get_snotify  (ExoDieEditor     *editor);
void              exo_die_editor_set_snotify  (ExoDieEditor     *editor,
                                               gboolean          snotify);

gboolean          exo_die_editor_get_terminal (ExoDieEditor     *editor);
void              exo_die_editor_set_terminal (ExoDieEditor     *editor,
                                               gboolean          terminal);

G_END_DECLS;

#endif /* !__EXO_DIE_EDITOR_H__ */
