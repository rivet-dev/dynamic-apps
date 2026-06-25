/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2020 Alexander Schwinn <alexxcons@xfce.org>
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

#ifndef __THUNAR_ACTION_MANAGER_H__
#define __THUNAR_ACTION_MANAGER_H__

#include <thunar/thunar-component.h>
#include <thunar/thunar-device.h>

G_BEGIN_DECLS;

/* avoid including libxfce4ui.h */
typedef struct _XfceGtkActionEntry  XfceGtkActionEntry;

typedef struct _ThunarActionManagerClass ThunarActionManagerClass;
typedef struct _ThunarActionManager      ThunarActionManager;

#define THUNAR_TYPE_ACTION_MANAGER            (thunar_action_manager_get_type ())
#define THUNAR_ACTION_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj),  THUNAR_TYPE_ACTION_MANAGER, ThunarActionManager))
#define THUNAR_ACTION_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST    ((klass),THUNAR_TYPE_ACTION_MANAGER, ThunarActionManagerClass))
#define THUNAR_IS_ACTION_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj),  THUNAR_TYPE_ACTION_MANAGER))
#define THUNAR_IS_ACTION_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE    ((klass),THUNAR_TYPE_ACTION_MANAGER))
#define THUNAR_ACTION_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS  ((obj),  THUNAR_TYPE_ACTION_MANAGER, ThunarActionManagerClass))

/* #XfceGtkActionEntrys provided by this widget */
typedef enum
{
  THUNAR_ACTION_MANAGER_ACTION_OPEN,
  THUNAR_ACTION_MANAGER_ACTION_EXECUTE,
  THUNAR_ACTION_MANAGER_ACTION_OPEN_IN_TAB,
  THUNAR_ACTION_MANAGER_ACTION_OPEN_IN_WINDOW,
  THUNAR_ACTION_MANAGER_ACTION_OPEN_LOCATION,
  THUNAR_ACTION_MANAGER_ACTION_OPEN_WITH_OTHER,
  THUNAR_ACTION_MANAGER_ACTION_SET_DEFAULT_APP,
  THUNAR_ACTION_MANAGER_ACTION_SENDTO_MENU,
  THUNAR_ACTION_MANAGER_ACTION_SENDTO_SHORTCUTS,
  THUNAR_ACTION_MANAGER_ACTION_SENDTO_DESKTOP,
  THUNAR_ACTION_MANAGER_ACTION_PROPERTIES,
  THUNAR_ACTION_MANAGER_ACTION_MAKE_LINK,
  THUNAR_ACTION_MANAGER_ACTION_DUPLICATE,
  THUNAR_ACTION_MANAGER_ACTION_RENAME,
  THUNAR_ACTION_MANAGER_ACTION_EMPTY_TRASH,
  THUNAR_ACTION_MANAGER_ACTION_REMOVE_FROM_RECENT,
  THUNAR_ACTION_MANAGER_ACTION_CREATE_FOLDER,
  THUNAR_ACTION_MANAGER_ACTION_CREATE_DOCUMENT,
  THUNAR_ACTION_MANAGER_ACTION_RESTORE,
  THUNAR_ACTION_MANAGER_ACTION_RESTORE_SHOW,
  THUNAR_ACTION_MANAGER_ACTION_MOVE_TO_TRASH,
  THUNAR_ACTION_MANAGER_ACTION_DELETE,
  THUNAR_ACTION_MANAGER_ACTION_TRASH_DELETE,
  THUNAR_ACTION_MANAGER_ACTION_PASTE,
  THUNAR_ACTION_MANAGER_ACTION_PASTE_INTO_FOLDER,
  THUNAR_ACTION_MANAGER_ACTION_COPY,
  THUNAR_ACTION_MANAGER_ACTION_CUT,
  THUNAR_ACTION_MANAGER_ACTION_MOUNT,
  THUNAR_ACTION_MANAGER_ACTION_UNMOUNT,
  THUNAR_ACTION_MANAGER_ACTION_EJECT,
  THUNAR_ACTION_MANAGER_ACTION_EDIT_LAUNCHER,

  THUNAR_ACTION_MANAGER_N_ACTIONS
} ThunarActionManagerAction;

typedef enum
{
  THUNAR_ACTION_MANAGER_CHANGE_DIRECTORY,
  THUNAR_ACTION_MANAGER_OPEN_AS_NEW_TAB,
  THUNAR_ACTION_MANAGER_OPEN_AS_NEW_WINDOW,
  THUNAR_ACTION_MANAGER_NO_ACTION,
} ThunarActionManagerFolderOpenAction;

GType               thunar_action_manager_get_type                       (void) G_GNUC_CONST;
void                thunar_action_manager_activate_selected_files        (ThunarActionManager                 *action_mgr,
                                                                          ThunarActionManagerFolderOpenAction  action,
                                                                          GAppInfo                            *app_info);
void                thunar_action_manager_open_selected_folders          (ThunarActionManager                 *action_mgr,
                                                                          gboolean                             open_in_tabs);
void                thunar_action_manager_set_widget                     (ThunarActionManager                 *action_mgr,
                                                                          GtkWidget                           *widget);
GtkWidget          *thunar_action_manager_get_widget                     (ThunarActionManager                 *action_mgr);
void                thunar_action_manager_append_accelerators            (ThunarActionManager                 *action_mgr,
                                                                          GtkAccelGroup                       *accel_group);
GtkWidget          *thunar_action_manager_append_menu_item               (ThunarActionManager                 *action_mgr,
                                                                          GtkMenuShell                        *menu,
                                                                          ThunarActionManagerAction            action,
                                                                          gboolean                             force);
gboolean            thunar_action_manager_append_open_section            (ThunarActionManager                 *action_mgr,
                                                                          GtkMenuShell                        *menu,
                                                                          gboolean                             support_tabs,
                                                                          gboolean                             support_change_directory,
                                                                          gboolean                             force);
gboolean            thunar_action_manager_append_custom_actions          (ThunarActionManager                 *action_mgr,
                                                                          GtkMenuShell                        *menu);
gboolean            thunar_action_manager_check_uca_key_activation       (ThunarActionManager                 *action_mgr,
                                                                          GdkEventKey                         *key_event);
void                thunar_action_manager_action_mount                   (ThunarActionManager                 *action_mgr);
gboolean            thunar_action_manager_action_unmount                 (ThunarActionManager                 *action_mgr);
gboolean            thunar_action_manager_action_eject                   (ThunarActionManager                 *action_mgr);
void                thunar_action_manager_set_selection                  (ThunarActionManager                 *action_mgr,
                                                                          GList                               *selected_thunar_files,
                                                                          ThunarDevice                        *selected_device,
                                                                          GFile                               *selected_location);
gboolean            thunar_action_manager_action_empty_trash             (ThunarActionManager                 *action_mgr);
gboolean            thunar_action_manager_action_restore                 (ThunarActionManager                 *action_mgr);
gboolean            thunar_action_manager_action_restore_and_show        (ThunarActionManager                 *action_mgr);
void                thunar_action_manager_set_searching                  (ThunarActionManager                 *action_mgr,
                                                                          gboolean                             b);
XfceGtkActionEntry *thunar_action_manager_get_action_entries             (void);

G_END_DECLS;

#endif /* !__THUNAR_ACTION_MANAGER_H__ */
