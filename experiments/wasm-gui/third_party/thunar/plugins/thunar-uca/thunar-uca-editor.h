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

#ifndef __THUNAR_UCA_EDITOR_H__
#define __THUNAR_UCA_EDITOR_H__

#include <thunar-uca/thunar-uca-model.h>

G_BEGIN_DECLS;

typedef struct _ThunarUcaEditorClass ThunarUcaEditorClass;
typedef struct _ThunarUcaEditor      ThunarUcaEditor;

#define THUNAR_UCA_TYPE_EDITOR            (thunar_uca_editor_get_type ())
#define THUNAR_UCA_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_UCA_TYPE_EDITOR, ThunarUcaEditor))
#define THUNAR_UCA_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_UCA_TYPE_EDITOR, ThunarUcaEditorwClass))
#define THUNAR_UCA_IS_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_UCA_TYPE_EDITOR))
#define THUNAR_UCA_IS_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_UCA_TYPE_EDITOR))
#define THUNAR_UCA_EDITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_UCA_TYPE_EDITOR, ThunarUcaEditorwClass))

GType thunar_uca_editor_get_type      (void) G_GNUC_CONST;
void  thunar_uca_editor_register_type (ThunarxProviderPlugin  *plugin);

void  thunar_uca_editor_load          (ThunarUcaEditor        *uca_editor,
                                       ThunarUcaModel         *uca_model,
                                       GtkTreeIter            *iter);
void  thunar_uca_editor_save          (ThunarUcaEditor        *uca_editor,
                                       ThunarUcaModel         *uca_model,
                                       GtkTreeIter            *iter);

G_END_DECLS;

#endif /* !__THUNAR_UCA_EDITOR_H__ */
