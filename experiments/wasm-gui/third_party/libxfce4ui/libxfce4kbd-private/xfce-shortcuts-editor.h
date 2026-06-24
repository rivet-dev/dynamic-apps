/* vi:set expandtab sw=2 sts=2: */
/*
 * Copyright (c) 2021 Sergios - Anestis Kefalidis <sergioskefalidis@gmail.com>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __XFCE_SHORTCUTS_EDITOR_H__
#define __XFCE_SHORTCUTS_EDITOR_H__

#include <gtk/gtk.h>
#include <libxfce4ui/libxfce4ui.h>

G_BEGIN_DECLS

typedef struct _XfceShortcutsEditorClass XfceShortcutsEditorClass;
typedef struct _XfceShortcutsEditor      XfceShortcutsEditor;

#define XFCE_TYPE_SHORTCUTS_EDITOR            (xfce_shortcuts_editor_get_type ())
#define XFCE_SHORTCUTS_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_SHORTCUTS_EDITOR, XfceShortcutsEditor))
#define XFCE_SHORTCUTS_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_SHORTCUTS_EDITOR, XfceShortcutsEditorClass))
#define XFCE_IS_SHORTCUTS_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_SHORTCUTS_EDITOR))
#define XFCE_IS_SHORTCUTS_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_SHORTCUTS_EDITOR))
#define XFCE_SHORTCUTS_EDITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_SHORTCUTS_EDITOR, XfceShortcutsEditorClass))

typedef struct
{
  gchar              *section_name;
  XfceGtkActionEntry *entries;
  size_t              size;
} XfceShortcutsEditorSection;

GType        xfce_shortcuts_editor_get_type        (void) G_GNUC_CONST;

GtkWidget   *xfce_shortcuts_editor_new             (int                         argument_count,
                                                    ...) G_GNUC_MALLOC;
GtkWidget   *xfce_shortcuts_editor_new_array       (XfceShortcutsEditorSection *sections,
                                                    int                         n_sections) G_GNUC_MALLOC;
GtkWidget   *xfce_shortcuts_editor_new_variadic    (int                         argument_count,
                                                    va_list                     argument_list) G_GNUC_MALLOC;

G_END_DECLS

#endif /* !__XFCE_SHORTCUTS_EDITOR_DIALOG_H__ */
