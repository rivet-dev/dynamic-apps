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

#ifndef __THUNAR_SHORTCUTS_MODEL_H__
#define __THUNAR_SHORTCUTS_MODEL_H__

#include <thunar/thunar-file.h>
#include <thunar/thunar-device.h>

G_BEGIN_DECLS;

typedef struct _ThunarShortcutsModelClass ThunarShortcutsModelClass;
typedef struct _ThunarShortcutsModel      ThunarShortcutsModel;
typedef enum   _ThunarShortcutGroup       ThunarShortcutGroup;

#define THUNAR_TYPE_SHORTCUTS_MODEL            (thunar_shortcuts_model_get_type ())
#define THUNAR_SHORTCUTS_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_SHORTCUTS_MODEL, ThunarShortcutsModel))
#define THUNAR_SHORTCUTS_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_SHORTCUTS_MODEL, ThunarShortcutsModelClass))
#define THUNAR_IS_SHORTCUTS_MODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_SHORTCUTS_MODEL))
#define THUNAR_IS_SHORTCUTS_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_SHORTCUTS_MODEL))
#define THUNAR_SHORTCUTS_MODEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_MODEL_SHORTCUTS_MODEL, ThunarShortcutsModelClass))

typedef enum
{
  THUNAR_SHORTCUTS_MODEL_COLUMN_IS_HEADER,
  THUNAR_SHORTCUTS_MODEL_COLUMN_IS_ITEM,
  THUNAR_SHORTCUTS_MODEL_COLUMN_VISIBLE,
  THUNAR_SHORTCUTS_MODEL_COLUMN_NAME,
  THUNAR_SHORTCUTS_MODEL_COLUMN_TOOLTIP,
  THUNAR_SHORTCUTS_MODEL_COLUMN_FILE,
  THUNAR_SHORTCUTS_MODEL_COLUMN_LOCATION,
  THUNAR_SHORTCUTS_MODEL_COLUMN_GICON,
  THUNAR_SHORTCUTS_MODEL_COLUMN_DEVICE,
  THUNAR_SHORTCUTS_MODEL_COLUMN_MUTABLE,
  THUNAR_SHORTCUTS_MODEL_COLUMN_CAN_EJECT,
  THUNAR_SHORTCUTS_MODEL_COLUMN_GROUP,
  THUNAR_SHORTCUTS_MODEL_COLUMN_BUSY,
  THUNAR_SHORTCUTS_MODEL_COLUMN_BUSY_PULSE,
  THUNAR_SHORTCUTS_MODEL_COLUMN_HIDDEN,
  THUNAR_SHORTCUTS_MODEL_N_COLUMNS,
} ThunarShortcutsModelColumn;

#define THUNAR_SHORTCUT_GROUP_DEVICES (THUNAR_SHORTCUT_GROUP_DEVICES_HEADER \
                                       | THUNAR_SHORTCUT_GROUP_DEVICES_FILESYSTEM \
                                       | THUNAR_SHORTCUT_GROUP_DEVICES_VOLUMES \
                                       | THUNAR_SHORTCUT_GROUP_DEVICES_MOUNTS)
#define THUNAR_SHORTCUT_GROUP_PLACES  (THUNAR_SHORTCUT_GROUP_PLACES_HEADER \
                                       | THUNAR_SHORTCUT_GROUP_PLACES_DEFAULT \
                                       | THUNAR_SHORTCUT_GROUP_PLACES_TRASH \
                                       | THUNAR_SHORTCUT_GROUP_PLACES_BOOKMARKS \
                                       | THUNAR_SHORTCUT_GROUP_PLACES_COMPUTER \
                                       | THUNAR_SHORTCUT_GROUP_PLACES_RECENT)
#define THUNAR_SHORTCUT_GROUP_NETWORK (THUNAR_SHORTCUT_GROUP_NETWORK_HEADER \
                                       | THUNAR_SHORTCUT_GROUP_NETWORK_DEFAULT \
                                       | THUNAR_SHORTCUT_GROUP_NETWORK_MOUNTS)
#define THUNAR_SHORTCUT_GROUP_HEADER  (THUNAR_SHORTCUT_GROUP_DEVICES_HEADER \
                                       | THUNAR_SHORTCUT_GROUP_PLACES_HEADER \
                                       | THUNAR_SHORTCUT_GROUP_NETWORK_HEADER)

enum _ThunarShortcutGroup
{
  /* THUNAR_SHORTCUT_GROUP_PLACES */
  THUNAR_SHORTCUT_GROUP_PLACES_HEADER      = (1 << 0),  /* places header */
  THUNAR_SHORTCUT_GROUP_PLACES_COMPUTER    = (1 << 1),  /* computer */
  THUNAR_SHORTCUT_GROUP_PLACES_DEFAULT     = (1 << 2),  /* home and desktop */
  THUNAR_SHORTCUT_GROUP_PLACES_RECENT      = (1 << 3),  /* recent */
  THUNAR_SHORTCUT_GROUP_PLACES_TRASH       = (1 << 4),  /* trash */
  THUNAR_SHORTCUT_GROUP_PLACES_BOOKMARKS   = (1 << 5),  /* gtk-bookmarks */

  /* THUNAR_SHORTCUT_GROUP_DEVICES */
  THUNAR_SHORTCUT_GROUP_DEVICES_HEADER     = (1 << 6),  /* devices header */
  THUNAR_SHORTCUT_GROUP_DEVICES_FILESYSTEM = (1 << 7),  /* local filesystem */
  THUNAR_SHORTCUT_GROUP_DEVICES_VOLUMES    = (1 << 8),  /* local ThunarDevices */
  THUNAR_SHORTCUT_GROUP_DEVICES_MOUNTS     = (1 << 9),  /* local mounts, like cameras and archives */

  /* THUNAR_SHORTCUT_GROUP_NETWORK */
  THUNAR_SHORTCUT_GROUP_NETWORK_HEADER     = (1 << 10),  /* network header */
  THUNAR_SHORTCUT_GROUP_NETWORK_DEFAULT    = (1 << 11), /* browse network */
  THUNAR_SHORTCUT_GROUP_NETWORK_MOUNTS     = (1 << 12), /* remote ThunarDevices */
};



GType                  thunar_shortcuts_model_get_type      (void) G_GNUC_CONST;

ThunarShortcutsModel  *thunar_shortcuts_model_get_default   (void);

gboolean               thunar_shortcuts_model_has_bookmark  (ThunarShortcutsModel *model,
                                                             GFile                *file);

gboolean               thunar_shortcuts_model_iter_for_file (ThunarShortcutsModel *model,
                                                             ThunarFile           *file,
                                                             GtkTreeIter          *iter);

gboolean               thunar_shortcuts_model_drop_possible (ThunarShortcutsModel *model,
                                                             GtkTreePath          *path);

void                   thunar_shortcuts_model_add           (ThunarShortcutsModel *model,
                                                             GtkTreePath          *dst_path,
                                                             gpointer              file);
void                   thunar_shortcuts_model_move          (ThunarShortcutsModel *model,
                                                             GtkTreePath          *src_path,
                                                             GtkTreePath          *dst_path);
void                   thunar_shortcuts_model_remove        (ThunarShortcutsModel *model,
                                                             GtkTreePath          *path);
void                   thunar_shortcuts_model_rename        (ThunarShortcutsModel *model,
                                                             GtkTreeIter          *iter,
                                                             const gchar          *name);
void                   thunar_shortcuts_model_set_busy      (ThunarShortcutsModel *model,
                                                             ThunarDevice         *device,
                                                             gboolean              busy);
void                   thunar_shortcuts_model_set_hidden    (ThunarShortcutsModel *model,
                                                             GtkTreePath          *path,
                                                             gboolean              hidden);

G_END_DECLS;

#endif /* !__THUNAR_SHORTCUTS_MODEL_H__ */
