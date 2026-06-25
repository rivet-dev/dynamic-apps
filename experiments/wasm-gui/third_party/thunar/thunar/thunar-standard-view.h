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

#ifndef __THUNAR_STANDARD_VIEW_H__
#define __THUNAR_STANDARD_VIEW_H__

#include <thunar/thunar-clipboard-manager.h>
#include <thunar/thunar-history.h>
#include <thunar/thunar-icon-factory.h>
#include <thunar/thunar-list-model.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-view.h>

G_BEGIN_DECLS;

/* avoid including libxfce4ui.h */
typedef struct _XfceGtkActionEntry  XfceGtkActionEntry;

typedef struct _ThunarStandardViewPrivate ThunarStandardViewPrivate;
typedef struct _ThunarStandardViewClass   ThunarStandardViewClass;
typedef struct _ThunarStandardView        ThunarStandardView;

#define THUNAR_TYPE_STANDARD_VIEW             (thunar_standard_view_get_type ())
#define THUNAR_STANDARD_VIEW(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_STANDARD_VIEW, ThunarStandardView))
#define THUNAR_STANDARD_VIEW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_STANDARD_VIEW, ThunarStandardViewClass))
#define THUNAR_IS_STANDARD_VIEW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_STANDARD_VIEW))
#define THUNAR_IS_STANDARD_VIEW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_STANDARD_VIEW))
#define THUNAR_STANDARD_VIEW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_STANDARD_VIEW, ThunarStandardViewClass))

/* #XfceGtkActionEntrys provided by this widget */
typedef enum
{
  THUNAR_STANDARD_VIEW_ACTION_SELECT_ALL_FILES,
  THUNAR_STANDARD_VIEW_ACTION_SELECT_BY_PATTERN,
  THUNAR_STANDARD_VIEW_ACTION_INVERT_SELECTION,
  THUNAR_STANDARD_VIEW_ACTION_UNSELECT_ALL_FILES,
  THUNAR_STANDARD_VIEW_ACTION_ARRANGE_ITEMS_MENU,
  THUNAR_STANDARD_VIEW_ACTION_SORT_BY_NAME,
  THUNAR_STANDARD_VIEW_ACTION_SORT_BY_SIZE,
  THUNAR_STANDARD_VIEW_ACTION_SORT_BY_TYPE,
  THUNAR_STANDARD_VIEW_ACTION_SORT_BY_MTIME,
  THUNAR_STANDARD_VIEW_ACTION_SORT_BY_DTIME,
  THUNAR_STANDARD_VIEW_ACTION_SORT_ASCENDING,
  THUNAR_STANDARD_VIEW_ACTION_SORT_DESCENDING,
  THUNAR_STANDARD_VIEW_ACTION_SORT_ORDER_TOGGLE,

  THUNAR_STANDARD_VIEW_N_ACTIONS
} ThunarStandardViewAction;

struct _ThunarStandardViewClass
{
  GtkScrolledWindowClass __parent__;

  /* Returns the list of currently selected GtkTreePath's, where
   * both the list and the items are owned by the caller. */
  GList       *(*get_selected_items)    (ThunarStandardView *standard_view);

  /* Selects all items in the view */
  void         (*select_all)            (ThunarStandardView *standard_view);

  /* Unselects all items in the view */
  void         (*unselect_all)          (ThunarStandardView *standard_view);

  /* Invert selection in the view */
  void         (*selection_invert)      (ThunarStandardView *standard_view);

  /* Selects the given item */
  void         (*select_path)           (ThunarStandardView *standard_view,
                                         GtkTreePath        *path);

  /* Called by the ThunarStandardView class to let derived class
   * place the cursor on the item/row referred to by path. If
   * start_editing is TRUE, the derived class should also start
   * editing that item/row.
   */
  void         (*set_cursor)            (ThunarStandardView *standard_view,
                                         GtkTreePath        *path,
                                         gboolean            start_editing);

  /* Called by the ThunarStandardView class to let derived class
   * scroll the view to the given path.
   */
  void         (*scroll_to_path)        (ThunarStandardView *standard_view,
                                         GtkTreePath        *path,
                                         gboolean            use_align,
                                         gfloat              row_align,
                                         gfloat              col_align);

  /* Returns the path at the given position or NULL if no item/row
   * is located at that coordinates. The path is freed by the caller.
   */
  GtkTreePath *(*get_path_at_pos)       (ThunarStandardView *standard_view,
                                         gint                x,
                                         gint                y);

  /* Returns the visible range */
  gboolean     (*get_visible_range)     (ThunarStandardView *standard_view,
                                         GtkTreePath       **start_path,
                                         GtkTreePath       **end_path);

  /* Sets the item/row that is highlighted for feedback. NULL is
   * passed for path to disable the highlighting.
   */
  void         (*highlight_path)        (ThunarStandardView  *standard_view,
                                         GtkTreePath         *path);

  /* external signals */
  void         (*start_open_location)   (ThunarStandardView *standard_view,
                                         const gchar        *initial_text);

  /* Appends view-specific menu items to the given menu */
  void        (*append_menu_items)      (ThunarStandardView *standard_view, GtkMenu *menu, GtkAccelGroup *accel_group);

  /* Connects view-specific accelerators to the given accelGroup */
  void        (*connect_accelerators)    (ThunarStandardView *standard_view, GtkAccelGroup *accel_group);

  /* Disconnects view-specific accelerators to the given accelGroup */
  void        (*disconnect_accelerators)    (ThunarStandardView *standard_view, GtkAccelGroup *accel_group);

  /* Internal action signals */
  gboolean     (*delete_selected_files) (ThunarStandardView *standard_view);

  /* Set the CellLayoutDataFunc to be used */
  void        (*cell_layout_data_func)    (GtkCellLayout     *layout,
                                           GtkCellRenderer   *cell,
                                           GtkTreeModel      *model,
                                           GtkTreeIter       *iter,
                                           gpointer           data);

  void        (*queue_redraw)           (ThunarStandardView *standard_view);

  /* The name of the property in ThunarPreferences, that determines
   * the last (and default) zoom-level for the view classes (i.e. in
   * case of ThunarIconView, this is "last-icon-view-zoom-level").
   */
  const gchar *zoom_level_property_name;
};

struct _ThunarStandardView
{
  GtkScrolledWindow __parent__;

  ThunarPreferences         *preferences;

  ThunarListModel           *model;

  ThunarIconFactory         *icon_factory;
  GtkCellRenderer           *icon_renderer;
  GtkCellRenderer           *name_renderer;

  GBinding                  *loading_binding;
  gboolean                   loading;
  GtkAccelGroup             *accel_group;

  ThunarStandardViewPrivate *priv;
};

GType               thunar_standard_view_get_type              (void) G_GNUC_CONST;

void                thunar_standard_view_context_menu          (ThunarStandardView       *standard_view);
void                thunar_standard_view_queue_popup           (ThunarStandardView       *standard_view,
                                                                GdkEventButton           *event);
void                thunar_standard_view_selection_changed     (ThunarStandardView       *standard_view);
void                thunar_standard_view_set_history           (ThunarStandardView       *standard_view,
                                                                ThunarHistory            *history);
ThunarHistory      *thunar_standard_view_get_history           (ThunarStandardView       *standard_view);
ThunarHistory      *thunar_standard_view_copy_history          (ThunarStandardView       *standard_view);
void                thunar_standard_view_append_menu_items     (ThunarStandardView       *standard_view,
                                                                GtkMenu                  *menu,
                                                               GtkAccelGroup            *accel_group);
GtkWidget          *thunar_standard_view_append_menu_item      (ThunarStandardView       *standard_view,
                                                                GtkMenu                  *menu,
                                                                ThunarStandardViewAction  action);
void                _thunar_standard_view_open_on_middle_click (ThunarStandardView       *standard_view,
                                                                GtkTreePath              *tree_path,
                                                                guint                     event_state);

void                thunar_standard_view_set_searching         (ThunarStandardView       *standard_view,
                                                                gchar                    *search_query);
gchar              *thunar_standard_view_get_search_query      (ThunarStandardView       *standard_view);

void                thunar_standard_view_update_statusbar_text (ThunarStandardView       *standard_view);

void                thunar_standard_view_save_view_type        (ThunarStandardView       *standard_view,
                                                                GType                     type);
GType               thunar_standard_view_get_saved_view_type   (ThunarStandardView       *standard_view);

XfceGtkActionEntry *thunar_standard_view_get_action_entries (void);

void                thunar_standard_view_queue_redraw          (ThunarStandardView       *standard_view);


G_END_DECLS;

#endif /* !__THUNAR_STANDARD_VIEW_H__ */
