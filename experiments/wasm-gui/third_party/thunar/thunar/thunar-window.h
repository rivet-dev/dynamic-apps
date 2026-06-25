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

#ifndef __THUNAR_WINDOW_H__
#define __THUNAR_WINDOW_H__

#include <thunar/thunar-action-manager.h>
#include <thunar/thunar-enum-types.h>
#include <thunar/thunar-folder.h>

#include <libxfce4ui/libxfce4ui.h>

G_BEGIN_DECLS;

typedef struct _ThunarWindowClass ThunarWindowClass;
typedef struct _ThunarWindow      ThunarWindow;

#define THUNAR_TYPE_WINDOW            (thunar_window_get_type ())
#define THUNAR_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_WINDOW, ThunarWindow))
#define THUNAR_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_WINDOW, ThunarWindowClass))
#define THUNAR_IS_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_WINDOW))
#define THUNAR_IS_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_WINDOW))
#define THUNAR_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_WINDOW, ThunarWindowClass))

/* #XfceGtkActionEntrys provided by this widget */
typedef enum
{
  THUNAR_WINDOW_ACTION_FILE_MENU,
  THUNAR_WINDOW_ACTION_NEW_TAB,
  THUNAR_WINDOW_ACTION_NEW_WINDOW,
  THUNAR_WINDOW_ACTION_DETACH_TAB,
  THUNAR_WINDOW_ACTION_CLOSE_TAB,
  THUNAR_WINDOW_ACTION_CLOSE_WINDOW,
  THUNAR_WINDOW_ACTION_CLOSE_ALL_WINDOWS,
  THUNAR_WINDOW_ACTION_EDIT_MENU,
  THUNAR_WINDOW_ACTION_UNDO,
  THUNAR_WINDOW_ACTION_REDO,
  THUNAR_WINDOW_ACTION_PREFERENCES,
  THUNAR_WINDOW_ACTION_VIEW_MENU,
  THUNAR_WINDOW_ACTION_RELOAD,
  THUNAR_WINDOW_ACTION_RELOAD_ALT,
  THUNAR_WINDOW_ACTION_VIEW_SPLIT,
  THUNAR_WINDOW_ACTION_VIEW_LOCATION_SELECTOR_MENU,
  THUNAR_WINDOW_ACTION_VIEW_LOCATION_SELECTOR_ENTRY,
  THUNAR_WINDOW_ACTION_VIEW_LOCATION_SELECTOR_BUTTONS,
  THUNAR_WINDOW_ACTION_VIEW_SIDE_PANE_MENU,
  THUNAR_WINDOW_ACTION_VIEW_SIDE_PANE_SHORTCUTS,
  THUNAR_WINDOW_ACTION_VIEW_SIDE_PANE_TREE,
  THUNAR_WINDOW_ACTION_TOGGLE_SIDE_PANE,
  THUNAR_WINDOW_ACTION_TOGGLE_IMAGE_PREVIEW,
  THUNAR_WINDOW_ACTION_VIEW_STATUSBAR,
  THUNAR_WINDOW_ACTION_VIEW_MENUBAR,
  THUNAR_WINDOW_ACTION_CONFIGURE_TOOLBAR,
  THUNAR_WINDOW_ACTION_SHOW_HIDDEN,
  THUNAR_WINDOW_ACTION_ZOOM_IN,
  THUNAR_WINDOW_ACTION_ZOOM_IN_ALT_1,
  THUNAR_WINDOW_ACTION_ZOOM_IN_ALT_2,
  THUNAR_WINDOW_ACTION_ZOOM_OUT,
  THUNAR_WINDOW_ACTION_ZOOM_OUT_ALT,
  THUNAR_WINDOW_ACTION_ZOOM_RESET,
  THUNAR_WINDOW_ACTION_ZOOM_RESET_ALT,
  THUNAR_WINDOW_ACTION_CLEAR_DIRECTORY_SPECIFIC_SETTINGS,
  THUNAR_WINDOW_ACTION_VIEW_AS_ICONS,
  THUNAR_WINDOW_ACTION_VIEW_AS_DETAILED_LIST,
  THUNAR_WINDOW_ACTION_VIEW_AS_COMPACT_LIST,
  THUNAR_WINDOW_ACTION_GO_MENU,
  THUNAR_WINDOW_ACTION_BOOKMARKS_MENU,
  THUNAR_WINDOW_ACTION_OPEN_PARENT,
  THUNAR_WINDOW_ACTION_BACK,
  THUNAR_WINDOW_ACTION_BACK_ALT,
  THUNAR_WINDOW_ACTION_FORWARD,
  THUNAR_WINDOW_ACTION_OPEN_FILE_SYSTEM,
  THUNAR_WINDOW_ACTION_OPEN_HOME,
  THUNAR_WINDOW_ACTION_OPEN_DESKTOP,
  THUNAR_WINDOW_ACTION_OPEN_COMPUTER,
  THUNAR_WINDOW_ACTION_OPEN_RECENT,
  THUNAR_WINDOW_ACTION_OPEN_TRASH,
  THUNAR_WINDOW_ACTION_OPEN_LOCATION,
  THUNAR_WINDOW_ACTION_OPEN_LOCATION_ALT,
  THUNAR_WINDOW_ACTION_OPEN_TEMPLATES,
  THUNAR_WINDOW_ACTION_OPEN_NETWORK,
  THUNAR_WINDOW_ACTION_HELP_MENU,
  THUNAR_WINDOW_ACTION_CONTENTS,
  THUNAR_WINDOW_ACTION_ABOUT,
  THUNAR_WINDOW_ACTION_SWITCH_PREV_TAB,
  THUNAR_WINDOW_ACTION_SWITCH_NEXT_TAB,
  THUNAR_WINDOW_ACTION_SEARCH,
  THUNAR_WINDOW_ACTION_CANCEL_SEARCH,
  THUNAR_WINDOW_ACTION_SHOW_HIGHLIGHT,

  THUNAR_WINDOW_N_ACTIONS
} ThunarWindowAction;

GType                     thunar_window_get_type                            (void) G_GNUC_CONST;
ThunarFile               *thunar_window_get_current_directory               (ThunarWindow        *window);
void                      thunar_window_set_current_directory               (ThunarWindow        *window,
                                                                             ThunarFile          *current_directory);
GList                    *thunar_window_get_directories                     (ThunarWindow        *window,
                                                                             gint                *active_page);
gboolean                  thunar_window_set_directories                     (ThunarWindow        *window,
                                                                             gchar              **uris,
                                                                             gint                 active_page);
void                      thunar_window_update_directories                  (ThunarWindow        *window,
                                                                             ThunarFile          *old_directory,
                                                                             ThunarFile          *new_directory);
void                      thunar_window_notebook_toggle_split_view          (ThunarWindow        *window);
void                      thunar_window_notebook_open_new_tab               (ThunarWindow        *window,
                                                                             ThunarFile          *directory);
void                      thunar_window_notebook_add_new_tab                (ThunarWindow        *window,
                                                                             ThunarFile          *directory,
                                                                             ThunarNewTabBehavior behavior);
void                      thunar_window_notebook_remove_tab                 (ThunarWindow        *window,
                                                                             gint                 tab);
void                      thunar_window_notebook_set_current_tab            (ThunarWindow        *window,
                                                                             gint                 tab);
gboolean                  thunar_window_has_shortcut_sidepane               (ThunarWindow        *window);
gboolean                  thunar_window_has_tree_view_sidepane              (ThunarWindow        *window);
GtkWidget*                thunar_window_get_sidepane                        (ThunarWindow        *window);
void                      thunar_window_append_menu_item                    (ThunarWindow        *window,
                                                                             GtkMenuShell        *menu,
                                                                             ThunarWindowAction   action);
ThunarActionManager*      thunar_window_get_action_manager                  (ThunarWindow        *window);
void                      thunar_window_redirect_menu_tooltips_to_statusbar (ThunarWindow        *window,
                                                                             GtkMenu             *menu);
const XfceGtkActionEntry* thunar_window_get_action_entry                    (ThunarWindow        *window,
                                                                             ThunarWindowAction   action);
void                      thunar_window_open_files_in_location              (ThunarWindow        *window,
                                                                             GList               *files_to_select);
void                      thunar_window_show_and_select_files               (ThunarWindow        *window,
                                                                             GList               *files_to_select);
void                      thunar_window_update_search                       (ThunarWindow        *window);
gboolean                  thunar_window_action_cancel_search                (ThunarWindow        *window);
gboolean                  thunar_window_action_search                       (ThunarWindow        *window);
void                      thunar_window_update_statusbar                    (ThunarWindow        *window);
void                      thunar_window_toolbar_toggle_item_visibility      (ThunarWindow        *window,
                                                                             gint                 index);
void                      thunar_window_toolbar_swap_items                  (ThunarWindow        *window,
                                                                             gint                 index_a,
                                                                             gint                 index_b);

XfceGtkActionEntry*       thunar_window_get_action_entries                  (void);

void                      thunar_window_reconnect_accelerators              (ThunarWindow        *window);
void                      thunar_window_focus_view                          (ThunarWindow        *window,
                                                                             GtkWidget           *view);


G_END_DECLS;

#endif /* !__THUNAR_WINDOW_H__ */
