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

#ifndef __EXO_DIE_ENUM_TYPES_H__
#define __EXO_DIE_ENUM_TYPES_H__

#include <exo/exo.h>

G_BEGIN_DECLS

#define EXO_DIE_TYPE_EDITOR_MODE (exo_die_editor_mode_get_type ())

/**
 * ExoDieEditorMode:
 * @EXO_DIE_EDITOR_MODE_APPLICATION : application launcher editing.
 * @EXO_DIE_EDITOR_MODE_LINK        : link editing.
 * @EXO_DIE_EDITOR_MODE_DIRECTORY   : menu directory editing.
 *
 * Editing mode for exo-desktop-item-edit
 *
 **/
typedef enum
{
  EXO_DIE_EDITOR_MODE_APPLICATION,
  EXO_DIE_EDITOR_MODE_LINK,
  EXO_DIE_EDITOR_MODE_DIRECTORY
} ExoDieEditorMode;

GType exo_die_editor_mode_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* !__EXO_DIE_ENUM_TYPES_H__ */
