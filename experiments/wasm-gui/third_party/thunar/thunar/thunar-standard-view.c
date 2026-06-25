/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gdk/gdkkeysyms.h>

#include <libxfce4util/libxfce4util.h>

#include <thunar/thunar-action-manager.h>
#include <thunar/thunar-application.h>
#include <thunar/thunar-menu.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-dnd.h>
#include <thunar/thunar-enum-types.h>
#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-history.h>
#include <thunar/thunar-icon-renderer.h>
#include <thunar/thunar-text-renderer.h>
#include <thunar/thunar-marshal.h>
#include <thunar/thunar-pango-extensions.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-properties-dialog.h>
#include <thunar/thunar-renamer-dialog.h>
#include <thunar/thunar-simple-job.h>
#include <thunar/thunar-standard-view.h>
#include <thunar/thunar-thumbnailer.h>
#include <thunar/thunar-util.h>
#include <thunar/thunar-details-view.h>

#if defined(GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#endif



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
  PROP_LOADING,
  PROP_SEARCHING,
  PROP_DISPLAY_NAME,
  PROP_FULL_PARSED_PATH,
  PROP_SELECTED_FILES,
  PROP_SHOW_HIDDEN,
  PROP_STATUSBAR_TEXT,
  PROP_ZOOM_LEVEL,
  PROP_DIRECTORY_SPECIFIC_SETTINGS,
  PROP_THUMBNAIL_DRAW_FRAMES,
  PROP_SORT_COLUMN,
  PROP_SORT_COLUMN_DEFAULT,
  PROP_SORT_ORDER,
  PROP_SORT_ORDER_DEFAULT,
  PROP_ACCEL_GROUP,
  N_PROPERTIES
};

/* Signal identifiers */
enum
{
  START_OPEN_LOCATION,
  LAST_SIGNAL,
};

/* Identifiers for DnD target types */
enum
{
  TARGET_TEXT_URI_LIST,
  TARGET_XDND_DIRECT_SAVE0,
  TARGET_NETSCAPE_URL,
  TARGET_APPLICATION_OCTET_STREAM,
};



static void                 thunar_standard_view_component_init             (ThunarComponentIface     *iface);
static void                 thunar_standard_view_navigator_init             (ThunarNavigatorIface     *iface);
static void                 thunar_standard_view_view_init                  (ThunarViewIface          *iface);
static GObject             *thunar_standard_view_constructor                (GType                     type,
                                                                             guint                     n_construct_properties,
                                                                             GObjectConstructParam    *construct_properties);
static void                 thunar_standard_view_dispose                    (GObject                  *object);
static void                 thunar_standard_view_finalize                   (GObject                  *object);
static void                 thunar_standard_view_get_property               (GObject                  *object,
                                                                             guint                     prop_id,
                                                                             GValue                   *value,
                                                                             GParamSpec               *pspec);
static void                 thunar_standard_view_set_property               (GObject                  *object,
                                                                             guint                     prop_id,
                                                                             const GValue             *value,
                                                                             GParamSpec               *pspec);
static void                 thunar_standard_view_scale_changed              (GObject                  *object,
                                                                             GParamSpec               *pspec,
                                                                             gpointer                  user_data);
static void                 thunar_standard_view_realize                    (GtkWidget                *widget);
static void                 thunar_standard_view_unrealize                  (GtkWidget                *widget);
static void                 thunar_standard_view_grab_focus                 (GtkWidget                *widget);
static gboolean             thunar_standard_view_draw                       (GtkWidget                *widget,
                                                                             cairo_t                  *cr);
static GList               *thunar_standard_view_get_selected_files_component         (ThunarComponent          *component);
static void                 thunar_standard_view_set_selected_files_component         (ThunarComponent          *component,
                                                                                       GList                    *selected_files);
static ThunarFile          *thunar_standard_view_get_current_directory      (ThunarNavigator          *navigator);
static void                 thunar_standard_view_set_current_directory      (ThunarNavigator          *navigator,
                                                                             ThunarFile               *current_directory);
static gboolean             thunar_standard_view_get_loading                (ThunarView               *view);
static void                 thunar_standard_view_set_loading                (ThunarStandardView       *standard_view,
                                                                             gboolean                  loading);
static gboolean             thunar_standard_view_get_searching              (ThunarView               *view);
static const gchar         *thunar_standard_view_get_statusbar_text         (ThunarView               *view);
static gboolean             thunar_standard_view_get_show_hidden            (ThunarView               *view);
static void                 thunar_standard_view_set_show_hidden            (ThunarView               *view,
                                                                             gboolean                  show_hidden);
static ThunarZoomLevel      thunar_standard_view_get_zoom_level             (ThunarView               *view);
static void                 thunar_standard_view_set_zoom_level             (ThunarView               *view,
                                                                             ThunarZoomLevel           zoom_level);
static void                 thunar_standard_view_reset_zoom_level           (ThunarView               *view);
static void                 thunar_standard_view_apply_directory_specific_settings    (ThunarStandardView   *standard_view,
                                                                                       ThunarFile           *directory);
static void                 thunar_standard_view_set_directory_specific_settings      (ThunarStandardView   *standard_view,
                                                                                       gboolean              directory_specific_settings);
static void                 thunar_standard_view_reload                     (ThunarView               *view,
                                                                             gboolean                  reload_info);
static gboolean             thunar_standard_view_get_visible_range          (ThunarView               *view,
                                                                             ThunarFile              **start_file,
                                                                             ThunarFile              **end_file);
static void                 thunar_standard_view_scroll_to_file             (ThunarView               *view,
                                                                             ThunarFile               *file,
                                                                             gboolean                  select_file,
                                                                             gboolean                  use_align,
                                                                             gfloat                    row_align,
                                                                             gfloat                    col_align);
static GdkDragAction        thunar_standard_view_get_dest_actions           (ThunarStandardView       *standard_view,
                                                                             GdkDragContext           *context,
                                                                             gint                      x,
                                                                             gint                      y,
                                                                             guint                     timestamp,
                                                                             ThunarFile              **file_return);
static ThunarFile          *thunar_standard_view_get_drop_file              (ThunarStandardView       *standard_view,
                                                                             gint                      x,
                                                                             gint                      y,
                                                                             GtkTreePath             **path_return);
static void                 thunar_standard_view_current_directory_destroy  (ThunarFile               *current_directory,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_current_directory_changed  (ThunarFile               *current_directory,
                                                                             ThunarStandardView       *standard_view);
static GList               *thunar_standard_view_get_selected_files_view    (ThunarView               *view);
static void                 thunar_standard_view_set_selected_files_view    (ThunarView               *view,
                                                                             GList                    *selected_files);
static gboolean             thunar_standard_view_select_all_files           (ThunarView               *view);
static gboolean             thunar_standard_view_select_by_pattern          (ThunarView               *view);
static gboolean             thunar_standard_view_selection_invert           (ThunarView               *view);
static gboolean             thunar_standard_view_unselect_all_files         (ThunarView               *view);
static GClosure            *thunar_standard_view_new_files_closure          (ThunarStandardView       *standard_view,
                                                                             GtkWidget                *source_view);
static void                 thunar_standard_view_new_files                  (ThunarStandardView       *standard_view,
                                                                             GList                    *path_list);
static gboolean             thunar_standard_view_button_release_event       (GtkWidget                *view,
                                                                             GdkEventButton           *event,
                                                                             ThunarStandardView       *standard_view);
static gboolean             thunar_standard_view_motion_notify_event        (GtkWidget                *view,
                                                                             GdkEventMotion           *event,
                                                                             ThunarStandardView       *standard_view);
static gboolean             thunar_standard_view_key_press_event            (GtkWidget                *view,
                                                                             GdkEventKey              *event,
                                                                             ThunarStandardView       *standard_view);
static gboolean             thunar_standard_view_scroll_event               (GtkWidget                *view,
                                                                             GdkEventScroll           *event,
                                                                             ThunarStandardView       *standard_view);
static gboolean             thunar_standard_view_drag_drop                  (GtkWidget                *view,
                                                                             GdkDragContext           *context,
                                                                             gint                      x,
                                                                             gint                      y,
                                                                             guint                     timestamp,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_drag_data_received         (GtkWidget                *view,
                                                                             GdkDragContext           *context,
                                                                             gint                      x,
                                                                             gint                      y,
                                                                             GtkSelectionData         *selection_data,
                                                                             guint                     info,
                                                                             guint                     timestamp,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_drag_leave                 (GtkWidget                *view,
                                                                             GdkDragContext           *context,
                                                                             guint                     timestamp,
                                                                             ThunarStandardView       *standard_view);
static gboolean             thunar_standard_view_drag_motion                (GtkWidget                *view,
                                                                             GdkDragContext           *context,
                                                                             gint                      x,
                                                                             gint                      y,
                                                                             guint                     timestamp,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_drag_begin                 (GtkWidget                *view,
                                                                             GdkDragContext           *context,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_drag_data_get              (GtkWidget                *view,
                                                                             GdkDragContext           *context,
                                                                             GtkSelectionData         *selection_data,
                                                                             guint                     info,
                                                                             guint                     timestamp,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_drag_data_delete           (GtkWidget                *view,
                                                                             GdkDragContext           *context,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_drag_end                   (GtkWidget                *view,
                                                                             GdkDragContext           *context,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_select_after_row_deleted   (ThunarListModel          *model,
                                                                             GtkTreePath              *path,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_row_changed                (ThunarListModel          *model,
                                                                             GtkTreePath              *path,
                                                                             GtkTreeIter              *iter,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_rows_reordered             (ThunarListModel          *model,
                                                                             GtkTreePath              *path,
                                                                             GtkTreeIter              *iter,
                                                                             gpointer                  new_order,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_error                      (ThunarListModel          *model,
                                                                             const GError             *error,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_search_done                (ThunarListModel          *model,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_sort_column_changed        (GtkTreeSortable          *tree_sortable,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_loading_unbound            (gpointer                  user_data);
static gboolean             thunar_standard_view_drag_scroll_timer          (gpointer                  user_data);
static void                 thunar_standard_view_drag_scroll_timer_destroy  (gpointer                  user_data);
static gboolean             thunar_standard_view_drag_timer                 (gpointer                  user_data);
static void                 thunar_standard_view_drag_timer_destroy         (gpointer                  user_data);
static void                 thunar_standard_view_finished_thumbnailing      (ThunarThumbnailer        *thumbnailer,
                                                                             guint                     request,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_thumbnailing_destroyed     (gpointer                  data);
static void                 thunar_standard_view_cancel_thumbnailing        (ThunarStandardView       *standard_view);
static void                 thunar_standard_view_schedule_thumbnail_timeout (ThunarStandardView       *standard_view);
static void                 thunar_standard_view_schedule_thumbnail_idle    (ThunarStandardView       *standard_view);
static gboolean             thunar_standard_view_request_thumbnails         (gpointer                  data);
static gboolean             thunar_standard_view_request_thumbnails_lazy    (gpointer                  data);
static void                 thunar_standard_view_thumbnail_mode_toggled     (ThunarStandardView       *standard_view,
                                                                             GParamSpec               *pspec,
                                                                             ThunarIconFactory        *icon_factory);
static void                 thunar_standard_view_scrolled                   (GtkAdjustment            *adjustment,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_size_allocate              (ThunarStandardView       *standard_view,
                                                                             GtkAllocation            *allocation);
static void                 thunar_standard_view_connect_accelerators       (ThunarStandardView       *standard_view);
static void                 thunar_standard_view_disconnect_accelerators    (ThunarStandardView       *standard_view);

static gboolean             thunar_standard_view_action_sort_by_name               (ThunarStandardView       *standard_view);
static gboolean             thunar_standard_view_action_sort_by_type               (ThunarStandardView       *standard_view);
static gboolean             thunar_standard_view_action_sort_by_date               (ThunarStandardView       *standard_view);
static gboolean             thunar_standard_view_action_sort_by_date_deleted       (ThunarStandardView       *standard_view);
static gboolean             thunar_standard_view_action_sort_by_size               (ThunarStandardView       *standard_view);
static gboolean             thunar_standard_view_action_sort_ascending             (ThunarStandardView       *standard_view);
static gboolean             thunar_standard_view_action_sort_descending            (ThunarStandardView       *standard_view);
static void                 thunar_standard_view_set_sort_column                   (ThunarStandardView       *standard_view,
                                                                                    ThunarColumn              column);
static void                 thunar_standard_view_set_sort_order                    (ThunarStandardView       *standard_view,
                                                                                    GtkSortType               order);
static gboolean             thunar_standard_view_toggle_sort_order                 (ThunarStandardView       *standard_view);
static void                 thunar_standard_view_store_sort_column                 (ThunarStandardView       *standard_view);
static void                 thunar_standard_view_highlight_option_changed          (ThunarStandardView       *standard_view);
static void                 thunar_standard_view_cell_layout_data_func             (GtkCellLayout            *layout,
                                                                                    GtkCellRenderer          *cell,
                                                                                    GtkTreeModel             *model,
                                                                                    GtkTreeIter              *iter,
                                                                                    gpointer                  data);

struct _ThunarStandardViewPrivate
{
  /* current directory of the view */
  ThunarFile             *current_directory;

  /* history of the current view */
  ThunarHistory          *history;

  /* zoom-level support */
  ThunarZoomLevel         zoom_level;

  /* directory specific settings */
  gboolean                directory_specific_settings;

  /* scroll_to_file support */
  GHashTable             *scroll_to_files;

  /* statusbar */
  gchar                  *statusbar_text;
  guint                   statusbar_text_idle_id;

  /* right-click drag/popup support */
  GList                  *drag_g_file_list;
  guint                   drag_scroll_timer_id;
  guint                   drag_timer_id;
  GdkEvent               *drag_timer_event;
  gint                    drag_x;
  gint                    drag_y;

  /* drop site support */
  guint                   drop_data_ready : 1; /* whether the drop data was received already */
  guint                   drop_highlight : 1;
  guint                   drop_occurred : 1;   /* whether the data was dropped */
  GList                  *drop_file_list;      /* the list of URIs that are contained in the drop data */

  /* the "new-files" closure, which is used to select files whenever
   * new files are created by a ThunarJob associated with this view
   */
  GClosure               *new_files_closure;

  /* the "new-files" path list that was remembered in the closure callback
   * if the view is currently being loaded and as such the folder may
   * not have all "new-files" at hand. This list is used when the
   * folder tells that it's ready loading and the view will try again
   * to select exactly these files.
   */
  GList                  *new_files_path_list;

  /* scroll-to-file support */
  ThunarFile             *scroll_to_file;
  guint                   scroll_to_select : 1;
  guint                   scroll_to_use_align : 1;
  gfloat                  scroll_to_row_align;
  gfloat                  scroll_to_col_align;

  /* #GList of currently selected #ThunarFile<!---->s */
  GList                  *selected_files;
  guint                   restore_selection_idle_id;

  /* support for generating thumbnails */
  ThunarThumbnailer      *thumbnailer;
  guint                   thumbnail_request;
  guint                   thumbnail_source_id;
  gboolean                thumbnailing_scheduled;

  /* row insert and delete signal IDs, for blocking/unblocking */
  gulong                  row_changed_id;
  gulong                  row_deleted_id;

  /* current sort column ID and it's fallback
   * the default is only relevant for directory specific settings */
  ThunarColumn            sort_column;
  ThunarColumn            sort_column_default;

  /* current sort_order (GTK_SORT_ASCENDING || GTK_SORT_DESCENDING)
   * the default is only relevant for directory specific settings */
  GtkSortType             sort_order;
  GtkSortType             sort_order_default;

  /* current search query, used to allow switching between views with different (or NULL) search queries */
  gchar                  *search_query;

  /* if you are wondering why we don't check if search_query is NULL instead of adding a new variable,
   * we don't want to show the spinner when the search query is empty (i.e. "") */
  gboolean                active_search;

  /* used to restore the view type after a search is completed */
  GType                   type;
};

static XfceGtkActionEntry thunar_standard_view_action_entries[] =
{
    { THUNAR_STANDARD_VIEW_ACTION_SELECT_ALL_FILES,   "<Actions>/ThunarStandardView/select-all-files",   "<Primary>a", XFCE_GTK_MENU_ITEM,       N_ ("Select _all Files"),     N_ ("Select all files in this window"),                   NULL, G_CALLBACK (thunar_standard_view_select_all_files),            },
    { THUNAR_STANDARD_VIEW_ACTION_SELECT_BY_PATTERN,  "<Actions>/ThunarStandardView/select-by-pattern",  "<Primary>s", XFCE_GTK_MENU_ITEM,       N_ ("Select _by Pattern..."), N_ ("Select all files that match a certain pattern"),     NULL, G_CALLBACK (thunar_standard_view_select_by_pattern),           },
    { THUNAR_STANDARD_VIEW_ACTION_INVERT_SELECTION,   "<Actions>/ThunarStandardView/invert-selection",   "",           XFCE_GTK_MENU_ITEM,       N_ ("_Invert Selection"),     N_ ("Select all files but not those currently selected"), NULL, G_CALLBACK (thunar_standard_view_selection_invert),            },
    { THUNAR_STANDARD_VIEW_ACTION_UNSELECT_ALL_FILES, "<Actions>/ThunarStandardView/unselect-all-files", "Escape",     XFCE_GTK_MENU_ITEM,       N_ ("U_nselect all Files"),   N_ ("Unselect all files in this window"),                 NULL, G_CALLBACK (thunar_standard_view_unselect_all_files),          },
    { THUNAR_STANDARD_VIEW_ACTION_ARRANGE_ITEMS_MENU, "<Actions>/ThunarStandardView/arrange-items-menu", "",           XFCE_GTK_MENU_ITEM,       N_ ("Arran_ge Items"),        NULL,                                                     NULL, G_CALLBACK (NULL),                                             },
    { THUNAR_STANDARD_VIEW_ACTION_SORT_ORDER_TOGGLE,  "<Actions>/ThunarStandardView/toggle-sort-order",  "",           XFCE_GTK_MENU_ITEM,       N_ ("T_oggle Sort Order"),    N_ ("Toggle ascending/descending sort order"),            NULL, G_CALLBACK (thunar_standard_view_toggle_sort_order),           },
    { THUNAR_STANDARD_VIEW_ACTION_SORT_BY_NAME,       "<Actions>/ThunarStandardView/sort-by-name",       "",           XFCE_GTK_RADIO_MENU_ITEM, N_ ("By _Name"),              N_ ("Keep items sorted by their name"),                   NULL, G_CALLBACK (thunar_standard_view_action_sort_by_name),         },
    { THUNAR_STANDARD_VIEW_ACTION_SORT_BY_SIZE,       "<Actions>/ThunarStandardView/sort-by-size",       "",           XFCE_GTK_RADIO_MENU_ITEM, N_ ("By _Size"),              N_ ("Keep items sorted by their size"),                   NULL, G_CALLBACK (thunar_standard_view_action_sort_by_size),         },
    { THUNAR_STANDARD_VIEW_ACTION_SORT_BY_TYPE,       "<Actions>/ThunarStandardView/sort-by-type",       "",           XFCE_GTK_RADIO_MENU_ITEM, N_ ("By _Type"),              N_ ("Keep items sorted by their type"),                   NULL, G_CALLBACK (thunar_standard_view_action_sort_by_type),         },
    { THUNAR_STANDARD_VIEW_ACTION_SORT_BY_MTIME,      "<Actions>/ThunarStandardView/sort-by-mtime",      "",           XFCE_GTK_RADIO_MENU_ITEM, N_ ("By Modification _Date"), N_ ("Keep items sorted by their modification date"),      NULL, G_CALLBACK (thunar_standard_view_action_sort_by_date),         },
    { THUNAR_STANDARD_VIEW_ACTION_SORT_BY_DTIME,      "<Actions>/ThunarStandardView/sort-by-dtime",      "",           XFCE_GTK_RADIO_MENU_ITEM, N_ ("By D_eletion Date"),     N_ ("Keep items sorted by their deletion date"),          NULL, G_CALLBACK (thunar_standard_view_action_sort_by_date_deleted), },
    { THUNAR_STANDARD_VIEW_ACTION_SORT_ASCENDING,     "<Actions>/ThunarStandardView/sort-ascending",     "",           XFCE_GTK_RADIO_MENU_ITEM, N_ ("_Ascending"),            N_ ("Sort items in ascending order"),                     NULL, G_CALLBACK (thunar_standard_view_action_sort_ascending),       },
    { THUNAR_STANDARD_VIEW_ACTION_SORT_DESCENDING,    "<Actions>/ThunarStandardView/sort-descending",    "",           XFCE_GTK_RADIO_MENU_ITEM, N_ ("_Descending"),           N_ ("Sort items in descending order"),                    NULL, G_CALLBACK (thunar_standard_view_action_sort_descending),      },
};

#define get_action_entry(id) xfce_gtk_get_action_entry_by_id(thunar_standard_view_action_entries,G_N_ELEMENTS(thunar_standard_view_action_entries),id)

/* Target types for dragging from the view */
static const GtkTargetEntry drag_targets[] =
{
  { "text/uri-list", 0, TARGET_TEXT_URI_LIST, },
};

/* Target types for dropping to the view */
static const GtkTargetEntry drop_targets[] =
{
  { "text/uri-list", 0, TARGET_TEXT_URI_LIST, },
  { "application/octet-stream", 0, TARGET_APPLICATION_OCTET_STREAM, },
  { "XdndDirectSave0", 0, TARGET_XDND_DIRECT_SAVE0, },
  { "_NETSCAPE_URL", 0, TARGET_NETSCAPE_URL, },
};



static guint       standard_view_signals[LAST_SIGNAL];
static GParamSpec *standard_view_props[N_PROPERTIES] = { NULL, };



G_DEFINE_ABSTRACT_TYPE_WITH_CODE (ThunarStandardView, thunar_standard_view, GTK_TYPE_SCROLLED_WINDOW,
    G_IMPLEMENT_INTERFACE (THUNAR_TYPE_NAVIGATOR, thunar_standard_view_navigator_init)
    G_IMPLEMENT_INTERFACE (THUNAR_TYPE_COMPONENT, thunar_standard_view_component_init)
    G_IMPLEMENT_INTERFACE (THUNAR_TYPE_VIEW, thunar_standard_view_view_init)
    G_ADD_PRIVATE (ThunarStandardView))


static gboolean
thunar_standard_view_action_sort_ascending (ThunarStandardView *standard_view)
{

  if (standard_view->priv->sort_order == GTK_SORT_DESCENDING)
    thunar_standard_view_set_sort_column (standard_view, standard_view->priv->sort_column);
  return TRUE;
}



static gboolean
thunar_standard_view_action_sort_descending (ThunarStandardView *standard_view)
{
  if (standard_view->priv->sort_order == GTK_SORT_ASCENDING)
    thunar_standard_view_set_sort_column (standard_view, standard_view->priv->sort_column);
  return TRUE;
}



static void
thunar_standard_view_set_sort_column (ThunarStandardView *standard_view,
                                      ThunarColumn        column)
{
  GtkSortType order = standard_view->priv->sort_order;

  if (standard_view->priv->sort_column == column)
    order = (order == GTK_SORT_ASCENDING ? GTK_SORT_DESCENDING: GTK_SORT_ASCENDING);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (standard_view->model), column, order);
}



static void
thunar_standard_view_set_sort_order (ThunarStandardView *standard_view,
                                     GtkSortType         order)
{
  if (standard_view->priv->sort_order == order)
    return;

  standard_view->priv->sort_order = order;
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (standard_view->model), standard_view->priv->sort_column, order);
}



static gboolean
thunar_standard_view_action_sort_by_name (ThunarStandardView *standard_view)
{
  thunar_standard_view_set_sort_column (standard_view, THUNAR_COLUMN_NAME);
  return TRUE;
}



static gboolean
thunar_standard_view_action_sort_by_size (ThunarStandardView *standard_view)
{
  thunar_standard_view_set_sort_column (standard_view, THUNAR_COLUMN_SIZE);
  return TRUE;
}



static gboolean
thunar_standard_view_action_sort_by_type (ThunarStandardView *standard_view)
{
  thunar_standard_view_set_sort_column (standard_view, THUNAR_COLUMN_TYPE);
  return TRUE;
}



static gboolean
thunar_standard_view_action_sort_by_date (ThunarStandardView *standard_view)
{
  thunar_standard_view_set_sort_column (standard_view, THUNAR_COLUMN_DATE_MODIFIED);
  return TRUE;
}



static gboolean
thunar_standard_view_action_sort_by_date_deleted (ThunarStandardView *standard_view)
{
  thunar_standard_view_set_sort_column (standard_view, THUNAR_COLUMN_DATE_DELETED);
  return TRUE;
}



static gboolean
thunar_standard_view_toggle_sort_order (ThunarStandardView *standard_view)
{
  thunar_standard_view_set_sort_column (standard_view, standard_view->priv->sort_column);
  return TRUE;
}



static void
thunar_standard_view_class_init (ThunarStandardViewClass *klass)
{
  GtkWidgetClass *gtkwidget_class;
  GObjectClass   *gobject_class;
  gpointer        g_iface;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->constructor = thunar_standard_view_constructor;
  gobject_class->dispose = thunar_standard_view_dispose;
  gobject_class->finalize = thunar_standard_view_finalize;
  gobject_class->get_property = thunar_standard_view_get_property;
  gobject_class->set_property = thunar_standard_view_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->realize = thunar_standard_view_realize;
  gtkwidget_class->unrealize = thunar_standard_view_unrealize;
  gtkwidget_class->grab_focus = thunar_standard_view_grab_focus;
  gtkwidget_class->draw = thunar_standard_view_draw;

  klass->cell_layout_data_func = thunar_standard_view_cell_layout_data_func;

  xfce_gtk_translate_action_entries (thunar_standard_view_action_entries, G_N_ELEMENTS (thunar_standard_view_action_entries));

  /**
   * ThunarStandardView:loading:
   *
   * Whether the folder associated with this view is
   * currently being loaded from the underlying media.
   *
   * Override property to set the property as writable
   * for the binding.
   **/
  standard_view_props[PROP_LOADING] =
      g_param_spec_override ("loading",
                             g_param_spec_boolean ("loading",
                                                   "loading",
                                                   "loading",
                                                   FALSE,
                                                   EXO_PARAM_READWRITE));

  /**
   * ThunarStandardView:searching:
   *
   * Whether the folder associated with this view is
   * currently being searched.
   **/
  standard_view_props[PROP_SEARCHING] =
      g_param_spec_override ("searching",
                             g_param_spec_boolean ("searching",
                                                   "searching",
                                                   "searching",
                                                   FALSE,
                                                   EXO_PARAM_READABLE));

  /**
   * ThunarStandardView:display-name:
   *
   * Display name of the current directory, for label text
   **/
  standard_view_props[PROP_DISPLAY_NAME] =
      g_param_spec_string ("display-name",
                           "display-name",
                           "display-name",
                           NULL,
                           EXO_PARAM_READABLE);

  /**
   * ThunarStandardView:full-parsed-path
   *
   * Full parsed path of the current directory, for label tooltip
   **/
  standard_view_props[PROP_FULL_PARSED_PATH] =
      g_param_spec_string ("full-parsed-path",
                           "full-parsed-path",
                           "full-parsed-path",
                           NULL,
                           EXO_PARAM_READABLE);

  /**
   * ThunarStandardView:directory-specific-settings:
   *
   * Whether to use directory specific settings.
   **/
  standard_view_props[PROP_DIRECTORY_SPECIFIC_SETTINGS] =
    g_param_spec_boolean ("directory-specific-settings",
                          "directory-specific-settings",
                          "directory-specific-settings",
                          FALSE,
                          EXO_PARAM_READWRITE);

  /**
   * ThunarStandardView:thumbnail-draw-frames:
   *
   * Whether to draw black frames around thumbnails.
   * This looks neat, but will delay the first draw a bit.
   * May have an impact on older systems, on folders with many pictures.
   **/
  standard_view_props[PROP_THUMBNAIL_DRAW_FRAMES] =
      g_param_spec_boolean ("thumbnail-draw-frames",
                            "thumbnail-draw-frames",
                            "thumbnail-draw-frames",
                            FALSE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarStandardView:sort-column:
   *
   * The sort column currently used for this view.
   **/
  standard_view_props[PROP_SORT_COLUMN] =
      g_param_spec_enum ("sort-column",
                         "SortColumn",
                         NULL,
                         THUNAR_TYPE_COLUMN,
                         THUNAR_COLUMN_NAME,
                         EXO_PARAM_READWRITE);

  /**
   * ThunarStandardView:sort-column-default:
   *
   * Only relevant for directory specific settings
   * The sort column to use if no directory specific settings are found for a directory
   **/
  standard_view_props[PROP_SORT_COLUMN_DEFAULT] =
      g_param_spec_enum ("sort-column-default",
                         "SortColumnDefault",
                         NULL,
                         THUNAR_TYPE_COLUMN,
                         THUNAR_COLUMN_NAME,
                         EXO_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  /**
   * ThunarStandardView:sort-order:
   *
   * The sort order currently used for this view.
   **/
  standard_view_props[PROP_SORT_ORDER] =
      g_param_spec_enum ("sort-order",
                         "SortOrder",
                         NULL,
                         GTK_TYPE_SORT_TYPE,
                         GTK_SORT_ASCENDING,
                         EXO_PARAM_READWRITE);

  /**
   * ThunarStandardView:sort-order-default:
   *
   * Only relevant for directory specific settings
   * The sort order to use if no directory specific settings are found for a directory
   **/
  standard_view_props[PROP_SORT_ORDER_DEFAULT] =
      g_param_spec_enum ("sort-order-default",
                         "SortOrderDefault",
                         NULL,
                         GTK_TYPE_SORT_TYPE,
                         GTK_SORT_ASCENDING,
                         EXO_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  /* override ThunarComponent's properties */
  g_iface = g_type_default_interface_peek (THUNAR_TYPE_COMPONENT);
  standard_view_props[PROP_SELECTED_FILES] =
      g_param_spec_override ("selected-files",
                             g_object_interface_find_property (g_iface, "selected-files"));

  /* override ThunarNavigator's properties */
  g_iface = g_type_default_interface_peek (THUNAR_TYPE_NAVIGATOR);
  standard_view_props[PROP_CURRENT_DIRECTORY] =
      g_param_spec_override ("current-directory",
                             g_object_interface_find_property (g_iface, "current-directory"));

  /* override ThunarView's properties */
  g_iface = g_type_default_interface_peek (THUNAR_TYPE_VIEW);
  standard_view_props[PROP_STATUSBAR_TEXT] =
      g_param_spec_override ("statusbar-text",
                             g_object_interface_find_property (g_iface, "statusbar-text"));

  standard_view_props[PROP_SHOW_HIDDEN] =
      g_param_spec_override ("show-hidden",
                             g_object_interface_find_property (g_iface, "show-hidden"));

  standard_view_props[PROP_ZOOM_LEVEL] =
      g_param_spec_override ("zoom-level",
                             g_object_interface_find_property (g_iface, "zoom-level"));

  standard_view_props[PROP_ACCEL_GROUP] =
                                   g_param_spec_object ("accel-group",
                                                        "accel-group",
                                                        "accel-group",
                                                        GTK_TYPE_ACCEL_GROUP,
                                                        G_PARAM_WRITABLE);

  /* install all properties */
  g_object_class_install_properties (gobject_class, N_PROPERTIES, standard_view_props);

  /**
   * ThunarStandardView::start-opn-location:
   * @standard_view : a #ThunarStandardView.
   * @initial_text  : the inital location text.
   *
   * Emitted by @standard_view, whenever the user requests to
   * select a custom location (using either the "Open Location"
   * dialog or the location entry widget) by specifying an
   * @initial_text (i.e. if the user types "/" into the
   * view).
   **/
  standard_view_signals[START_OPEN_LOCATION] =
    g_signal_new (I_("start-open-location"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarStandardViewClass, start_open_location),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1, G_TYPE_STRING);
}



static void
thunar_standard_view_component_init (ThunarComponentIface *iface)
{
  iface->get_selected_files = thunar_standard_view_get_selected_files_component;
  iface->set_selected_files = thunar_standard_view_set_selected_files_component;
}



static void
thunar_standard_view_navigator_init (ThunarNavigatorIface *iface)
{
  iface->get_current_directory = thunar_standard_view_get_current_directory;
  iface->set_current_directory = thunar_standard_view_set_current_directory;
}



static void
thunar_standard_view_view_init (ThunarViewIface *iface)
{
  iface->get_loading = thunar_standard_view_get_loading;
  iface->get_statusbar_text = thunar_standard_view_get_statusbar_text;
  iface->get_show_hidden = thunar_standard_view_get_show_hidden;
  iface->set_show_hidden = thunar_standard_view_set_show_hidden;
  iface->get_zoom_level = thunar_standard_view_get_zoom_level;
  iface->set_zoom_level = thunar_standard_view_set_zoom_level;
  iface->reset_zoom_level = thunar_standard_view_reset_zoom_level;
  iface->reload = thunar_standard_view_reload;
  iface->get_visible_range = thunar_standard_view_get_visible_range;
  iface->scroll_to_file = thunar_standard_view_scroll_to_file;
  iface->get_selected_files = thunar_standard_view_get_selected_files_view;
  iface->set_selected_files = thunar_standard_view_set_selected_files_view;
}



static void
thunar_standard_view_init (ThunarStandardView *standard_view)
{
  standard_view->priv = thunar_standard_view_get_instance_private (standard_view);

  /* allocate the scroll_to_files mapping (directory GFile -> first visible child GFile) */
  standard_view->priv->scroll_to_files = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal, g_object_unref, g_object_unref);

  /* grab a reference on the preferences */
  standard_view->preferences = thunar_preferences_get ();

  /* create a thumbnailer */
  standard_view->priv->thumbnailer = thunar_thumbnailer_get ();
  g_signal_connect (G_OBJECT (standard_view->priv->thumbnailer), "request-finished", G_CALLBACK (thunar_standard_view_finished_thumbnailing), standard_view);
  standard_view->priv->thumbnailing_scheduled = FALSE;

  /* initialize the scrolled window */
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (standard_view),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (standard_view), NULL);
  gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (standard_view), NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (standard_view), GTK_SHADOW_IN);

  /* setup the history support */
  standard_view->priv->history = g_object_new (THUNAR_TYPE_HISTORY, NULL);
  g_signal_connect_swapped (G_OBJECT (standard_view->priv->history), "change-directory", G_CALLBACK (thunar_navigator_change_directory), standard_view);

  /* setup the list model */
  standard_view->model = thunar_list_model_new ();
  standard_view->priv->row_deleted_id = g_signal_connect_after (G_OBJECT (standard_view->model), "row-deleted", G_CALLBACK (thunar_standard_view_select_after_row_deleted), standard_view);
  standard_view->priv->row_changed_id = g_signal_connect (G_OBJECT (standard_view->model), "row-changed", G_CALLBACK (thunar_standard_view_row_changed), standard_view);
  g_signal_connect (G_OBJECT (standard_view->model), "rows-reordered", G_CALLBACK (thunar_standard_view_rows_reordered), standard_view);
  g_signal_connect (G_OBJECT (standard_view->model), "error", G_CALLBACK (thunar_standard_view_error), standard_view);
  g_signal_connect (G_OBJECT (standard_view->model), "search-done", G_CALLBACK (thunar_standard_view_search_done), standard_view);
  g_object_bind_property (G_OBJECT (standard_view->preferences), "misc-case-sensitive", G_OBJECT (standard_view->model), "case-sensitive", G_BINDING_SYNC_CREATE);
  g_object_bind_property (G_OBJECT (standard_view->preferences), "misc-date-style", G_OBJECT (standard_view->model), "date-style", G_BINDING_SYNC_CREATE);
  g_object_bind_property (G_OBJECT (standard_view->preferences), "misc-date-custom-style", G_OBJECT (standard_view->model), "date-custom-style", G_BINDING_SYNC_CREATE);
  g_object_bind_property (G_OBJECT (standard_view->preferences), "misc-folders-first", G_OBJECT (standard_view->model), "folders-first", G_BINDING_SYNC_CREATE);
  g_object_bind_property (G_OBJECT (standard_view->preferences), "misc-file-size-binary", G_OBJECT (standard_view->model), "file-size-binary", G_BINDING_SYNC_CREATE);
  g_object_bind_property (G_OBJECT (standard_view->preferences), "misc-folder-item-count", G_OBJECT (standard_view->model), "folder-item-count", G_BINDING_SYNC_CREATE);

  standard_view->priv->thumbnail_request = 0;

  /* setup the icon renderer */
  standard_view->icon_renderer = thunar_icon_renderer_new ();
  g_object_ref_sink (G_OBJECT (standard_view->icon_renderer));
  g_object_bind_property (G_OBJECT (standard_view), "zoom-level", G_OBJECT (standard_view->icon_renderer), "size", G_BINDING_SYNC_CREATE);
  g_object_bind_property (G_OBJECT (standard_view->icon_renderer), "size", G_OBJECT (standard_view->priv->thumbnailer), "thumbnail-size", G_BINDING_SYNC_CREATE);
  g_object_bind_property (G_OBJECT (standard_view->preferences), "misc-highlighting-enabled", G_OBJECT (standard_view->icon_renderer), "highlighting-enabled", G_BINDING_SYNC_CREATE);
  g_signal_connect (G_OBJECT (standard_view), "notify::scale-factor", G_CALLBACK (thunar_standard_view_scale_changed), NULL);

  /* setup the name renderer */
  standard_view->name_renderer = thunar_text_renderer_new ();
  g_object_set (standard_view->name_renderer,
#if PANGO_VERSION_CHECK (1, 44, 0)
                "attributes", thunar_pango_attr_disable_hyphens (),
#endif
                "alignment", PANGO_ALIGN_CENTER,
                "xalign", 0.5,
                NULL);
  g_object_ref_sink (G_OBJECT (standard_view->name_renderer));
  g_object_bind_property (G_OBJECT (standard_view->preferences), "misc-highlighting-enabled", G_OBJECT (standard_view->name_renderer), "highlighting-enabled", G_BINDING_SYNC_CREATE);

  /* this is required in order to disable foreground & background colors on the text renderers when the feature is disabled */
  g_object_bind_property (G_OBJECT (standard_view->preferences), "misc-highlighting-enabled", G_OBJECT (standard_view->name_renderer), "foreground-set", G_BINDING_SYNC_CREATE);

  /* TODO: prelit underline
  g_object_bind_property (G_OBJECT (standard_view->preferences), "misc-single-click", G_OBJECT (standard_view->name_renderer), "follow-prelit", G_BINDING_SYNC_CREATE);*/

  /* be sure to update the selection whenever the folder changes */
  g_signal_connect_swapped (G_OBJECT (standard_view->model), "notify::folder", G_CALLBACK (thunar_standard_view_selection_changed), standard_view);

  /* be sure to update the statusbar text whenever the number of
   * files in our model changes.
   */
  g_signal_connect_swapped (G_OBJECT (standard_view->model), "notify::num-files", G_CALLBACK (thunar_standard_view_update_statusbar_text), standard_view);

  /* be sure to update the statusbar text whenever the file-size-binary property changes */
  g_signal_connect_swapped (G_OBJECT (standard_view->model), "notify::file-size-binary", G_CALLBACK (thunar_standard_view_update_statusbar_text), standard_view);

  /* connect to size allocation signals for generating thumbnail requests */
  g_signal_connect_after (G_OBJECT (standard_view), "size-allocate",
                          G_CALLBACK (thunar_standard_view_size_allocate), NULL);

  /* add widget to css class */
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (standard_view)), "standard-view");

  standard_view->accel_group = NULL;

  standard_view->priv->search_query = NULL;
  standard_view->priv->active_search = FALSE;
  standard_view->priv->type = 0;
}

static void thunar_standard_view_store_sort_column  (ThunarStandardView *standard_view)
{
  GtkSortType      sort_order;
  gint             sort_column;

  if (gtk_tree_sortable_get_sort_column_id (GTK_TREE_SORTABLE (standard_view->model), &sort_column, &sort_order))
    {
      standard_view->priv->sort_column     = sort_column;
      standard_view->priv->sort_order      = sort_order;
    }
}


static GObject*
thunar_standard_view_constructor (GType                  type,
                                  guint                  n_construct_properties,
                                  GObjectConstructParam *construct_properties)
{
  ThunarStandardView *standard_view;
  ThunarZoomLevel     zoom_level;
  GtkAdjustment      *adjustment;
  GtkWidget          *view;
  GObject            *object;

  /* let the GObject constructor create the instance */
  object = G_OBJECT_CLASS (thunar_standard_view_parent_class)->constructor (type,
                                                                            n_construct_properties,
                                                                            construct_properties);

  /* cast to standard_view for convenience */
  standard_view = THUNAR_STANDARD_VIEW (object);

  /* setup the default zoom-level, determined from the "last-<view>-zoom-level" preference */
  g_object_get (G_OBJECT (standard_view->preferences), THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->zoom_level_property_name, &zoom_level, NULL);
  thunar_view_set_zoom_level (THUNAR_VIEW (standard_view), zoom_level);

  /* save the "zoom-level" as "last-<view>-zoom-level" whenever the user changes the zoom level */
  g_object_bind_property (object, "zoom-level", G_OBJECT (standard_view->preferences), THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->zoom_level_property_name, G_BINDING_DEFAULT);

  /* determine the real view widget (treeview or iconview) */
  view = gtk_bin_get_child (GTK_BIN (object));

  /* apply our list model to the real view (the child of the scrolled window),
   * we therefore assume that all real views have the "model" property.
   */
  g_object_set (G_OBJECT (view), "model", standard_view->model, NULL);

  /* apply the single-click settings to the view */
  g_object_bind_property (G_OBJECT (standard_view->preferences), "misc-single-click", G_OBJECT (view), "single-click", G_BINDING_SYNC_CREATE);
  g_object_bind_property (G_OBJECT (standard_view->preferences), "misc-single-click-timeout", G_OBJECT (view), "single-click-timeout", G_BINDING_SYNC_CREATE);

  /* apply the default sort column and sort order */
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (standard_view->model), standard_view->priv->sort_column_default, standard_view->priv->sort_order_default);

  /* stay informed about changes to the sort column/order */
  g_signal_connect (G_OBJECT (standard_view->model), "sort-column-changed", G_CALLBACK (thunar_standard_view_sort_column_changed), standard_view);

  /* setup support to navigate using a horizontal mouse wheel and the back and forward buttons */
  g_signal_connect (G_OBJECT (view), "scroll-event", G_CALLBACK (thunar_standard_view_scroll_event), object);

  /* need to catch certain keys for the internal view widget */
  g_signal_connect (G_OBJECT (view), "key-press-event", G_CALLBACK (thunar_standard_view_key_press_event), object);

  /* setup the real view as drop site */
  gtk_drag_dest_set (view, 0, drop_targets, G_N_ELEMENTS (drop_targets), GDK_ACTION_ASK | GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_MOVE);
  g_signal_connect (G_OBJECT (view), "drag-drop", G_CALLBACK (thunar_standard_view_drag_drop), object);
  g_signal_connect (G_OBJECT (view), "drag-data-received", G_CALLBACK (thunar_standard_view_drag_data_received), object);
  g_signal_connect (G_OBJECT (view), "drag-leave", G_CALLBACK (thunar_standard_view_drag_leave), object);
  g_signal_connect (G_OBJECT (view), "drag-motion", G_CALLBACK (thunar_standard_view_drag_motion), object);

  /* setup the real view as drag source */
  gtk_drag_source_set (view, GDK_BUTTON1_MASK, drag_targets, G_N_ELEMENTS (drag_targets), GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
  g_signal_connect (G_OBJECT (view), "drag-begin", G_CALLBACK (thunar_standard_view_drag_begin), object);
  g_signal_connect (G_OBJECT (view), "drag-data-get", G_CALLBACK (thunar_standard_view_drag_data_get), object);
  g_signal_connect (G_OBJECT (view), "drag-data-delete", G_CALLBACK (thunar_standard_view_drag_data_delete), object);
  g_signal_connect (G_OBJECT (view), "drag-end", G_CALLBACK (thunar_standard_view_drag_end), object);

  /* connect to scroll events for generating thumbnail requests */
  adjustment = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (standard_view));
  g_signal_connect (adjustment, "value-changed",
                    G_CALLBACK (thunar_standard_view_scrolled), object);
  adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (standard_view));
  g_signal_connect (adjustment, "value-changed",
                    G_CALLBACK (thunar_standard_view_scrolled), object);

  /* synchronise the "directory-specific-settings" property with the global "misc-directory-specific-settings" property */
  g_object_bind_property (standard_view->preferences, "misc-directory-specific-settings",
                          standard_view, "directory-specific-settings",
                          G_BINDING_SYNC_CREATE);

  /* done, we have a working object */
  return object;
}



static void
thunar_standard_view_dispose (GObject *object)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (object);

  /* cancel pending thumbnail sources and requests */
  thunar_standard_view_cancel_thumbnailing (standard_view);

  /* unregister the "loading" binding */
  if (G_UNLIKELY (standard_view->loading_binding != NULL))
    {
      g_object_unref (standard_view->loading_binding);
      standard_view->loading_binding = NULL;
    }

  /* be sure to cancel any pending drag autoscroll timer */
  if (G_UNLIKELY (standard_view->priv->drag_scroll_timer_id != 0))
    g_source_remove (standard_view->priv->drag_scroll_timer_id);

  /* be sure to cancel any pending drag timer */
  if (G_UNLIKELY (standard_view->priv->drag_timer_id != 0))
    g_source_remove (standard_view->priv->drag_timer_id);

  /* be sure to free any pending drag timer event */
  if (G_UNLIKELY (standard_view->priv->drag_timer_event != NULL))
    {
      gdk_event_free (standard_view->priv->drag_timer_event);
      standard_view->priv->drag_timer_event = NULL;
    }

  /* disconnect from file */
  if (standard_view->priv->current_directory != NULL)
    {
      g_signal_handlers_disconnect_matched (standard_view->priv->current_directory,
                                            G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, standard_view);
      g_object_unref (standard_view->priv->current_directory);
      standard_view->priv->current_directory = NULL;
    }

  (*G_OBJECT_CLASS (thunar_standard_view_parent_class)->dispose) (object);
}



static void
thunar_standard_view_finalize (GObject *object)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (object);

  /* some safety checks */
  _thunar_assert (standard_view->loading_binding == NULL);
  _thunar_assert (standard_view->icon_factory == NULL);

  /* disconnect accelerators */
  thunar_standard_view_disconnect_accelerators (standard_view);

  /* release the thumbnailer */
  g_signal_handlers_disconnect_by_func (standard_view->priv->thumbnailer, thunar_standard_view_finished_thumbnailing, standard_view);
  g_object_unref (standard_view->priv->thumbnailer);

  /* release the scroll_to_file reference (if any) */
  if (G_UNLIKELY (standard_view->priv->scroll_to_file != NULL))
    g_object_unref (G_OBJECT (standard_view->priv->scroll_to_file));

  /* release the selected_files list (if any) */
  thunar_g_list_free_full (standard_view->priv->selected_files);

  /* release the drag path list (just in case the drag-end wasn't fired before) */
  thunar_g_list_free_full (standard_view->priv->drag_g_file_list);

  /* release the drop path list (just in case the drag-leave wasn't fired before) */
  thunar_g_list_free_full (standard_view->priv->drop_file_list);

  /* release the history */
  g_object_unref (standard_view->priv->history);

  /* release the reference on the name renderer */
  g_object_unref (G_OBJECT (standard_view->name_renderer));

  /* release the reference on the icon renderer */
  g_object_unref (G_OBJECT (standard_view->icon_renderer));

  /* drop any existing "new-files" closure */
  if (G_UNLIKELY (standard_view->priv->new_files_closure != NULL))
    {
      g_closure_invalidate (standard_view->priv->new_files_closure);
      g_closure_unref (standard_view->priv->new_files_closure);
      standard_view->priv->new_files_closure = NULL;
    }

  /* drop any remaining "new-files" paths */
  thunar_g_list_free_full (standard_view->priv->new_files_path_list);

  /* release our reference on the preferences */
  g_object_unref (G_OBJECT (standard_view->preferences));

  /* disconnect from the list model */
  g_signal_handlers_disconnect_matched (G_OBJECT (standard_view->model), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, standard_view);
  g_object_unref (G_OBJECT (standard_view->model));

  /* remove selection restore timeout */
  if (standard_view->priv->restore_selection_idle_id != 0)
    g_source_remove (standard_view->priv->restore_selection_idle_id);

  /* free the statusbar text (if any) */
  if (standard_view->priv->statusbar_text_idle_id != 0)
    g_source_remove (standard_view->priv->statusbar_text_idle_id);
  g_free (standard_view->priv->statusbar_text);

  /* release the scroll_to_files hash table */
  g_hash_table_destroy (standard_view->priv->scroll_to_files);

  (*G_OBJECT_CLASS (thunar_standard_view_parent_class)->finalize) (object);
}



static void
thunar_standard_view_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ThunarFile *current_directory;
  gboolean thumbnail_draw_frames;

  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      g_value_set_object (value, thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (object)));
      break;

    case PROP_LOADING:
      g_value_set_boolean (value, thunar_view_get_loading (THUNAR_VIEW (object)));
      break;

    case PROP_SEARCHING:
      g_value_set_boolean (value, thunar_standard_view_get_searching (THUNAR_VIEW (object)));
      break;

    case PROP_DISPLAY_NAME:
      current_directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (object));
      if (THUNAR_STANDARD_VIEW (object)->priv->search_query != NULL)
        {
          gchar *label = g_strjoin (NULL, "Search for '", THUNAR_STANDARD_VIEW (object)->priv->search_query, "' in ", thunar_file_get_display_name (current_directory), NULL);
          g_value_take_string (value, label);
        }
      else
        {
          if (current_directory != NULL)
            g_value_set_static_string (value, thunar_file_get_display_name (current_directory));
        }
      break;

    case PROP_FULL_PARSED_PATH:
      current_directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (object));
      if (current_directory != NULL)
        g_value_take_string (value, g_file_get_parse_name (thunar_file_get_file (current_directory)));
      break;

    case PROP_SELECTED_FILES:
      g_value_set_boxed (value, thunar_component_get_selected_files (THUNAR_COMPONENT (object)));
      break;

    case PROP_SHOW_HIDDEN:
      g_value_set_boolean (value, thunar_view_get_show_hidden (THUNAR_VIEW (object)));
      break;

    case PROP_STATUSBAR_TEXT:
      g_value_set_static_string (value, thunar_view_get_statusbar_text (THUNAR_VIEW (object)));
      break;

    case PROP_ZOOM_LEVEL:
      g_value_set_enum (value, thunar_view_get_zoom_level (THUNAR_VIEW (object)));
      break;

    case PROP_THUMBNAIL_DRAW_FRAMES:
      g_object_get (G_OBJECT (THUNAR_STANDARD_VIEW(object)->icon_factory), "thumbnail-draw-frames", &thumbnail_draw_frames, NULL);
      g_value_set_boolean (value, thumbnail_draw_frames);
      break;

    case PROP_SORT_COLUMN:
      g_value_set_enum (value, THUNAR_STANDARD_VIEW (object)->priv->sort_column);
      break;

    case PROP_SORT_ORDER:
      g_value_set_enum (value, THUNAR_STANDARD_VIEW (object)->priv->sort_order);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_standard_view_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (object);

  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (object), g_value_get_object (value));
      break;

    case PROP_LOADING:
      thunar_standard_view_set_loading (standard_view, g_value_get_boolean (value));
      break;

    case PROP_SELECTED_FILES:
      thunar_component_set_selected_files (THUNAR_COMPONENT (object), g_value_get_boxed (value));
      break;

    case PROP_SHOW_HIDDEN:
      thunar_view_set_show_hidden (THUNAR_VIEW (object), g_value_get_boolean (value));
      break;

    case PROP_ZOOM_LEVEL:
      thunar_view_set_zoom_level (THUNAR_VIEW (object), g_value_get_enum (value));
      break;

    case PROP_DIRECTORY_SPECIFIC_SETTINGS:
      thunar_standard_view_set_directory_specific_settings (standard_view, g_value_get_boolean (value));
      break;

    case PROP_THUMBNAIL_DRAW_FRAMES:
      g_object_set (G_OBJECT (standard_view->icon_factory), "thumbnail-draw-frames", g_value_get_boolean (value), NULL);
      thunar_standard_view_reload(THUNAR_VIEW (object), TRUE);
      break;

    case PROP_SORT_COLUMN:
      thunar_standard_view_set_sort_column (standard_view, g_value_get_enum (value));
      break;

    case PROP_SORT_COLUMN_DEFAULT:
      standard_view->priv->sort_column_default = g_value_get_enum (value);
      break;

    case PROP_SORT_ORDER:
      thunar_standard_view_set_sort_order (standard_view, g_value_get_enum (value));
      break;

    case PROP_SORT_ORDER_DEFAULT:
      standard_view->priv->sort_order_default = g_value_get_enum (value);
      break;

    case PROP_ACCEL_GROUP:
      thunar_standard_view_disconnect_accelerators (standard_view);
      standard_view->accel_group = g_value_dup_object (value);
      thunar_standard_view_connect_accelerators (standard_view);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_standard_view_scale_changed (GObject    *object,
                                    GParamSpec *pspec,
                                    gpointer    user_data)
{
    gtk_widget_queue_draw (GTK_WIDGET (object));
}



static void
thunar_standard_view_realize (GtkWidget *widget)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (widget);
  GtkIconTheme       *icon_theme;

  /* let the GtkWidget do its work */
  GTK_WIDGET_CLASS (thunar_standard_view_parent_class)->realize (widget);

  /* determine the icon factory for the screen on which we are realized */
  icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (widget));
  standard_view->icon_factory = thunar_icon_factory_get_for_icon_theme (icon_theme);
  g_object_bind_property (G_OBJECT (standard_view->icon_renderer), "size", G_OBJECT (standard_view->icon_factory), "thumbnail-size", G_BINDING_SYNC_CREATE);

  /* we need to redraw whenever the "thumbnail_mode" property is toggled */
  g_signal_connect_swapped (standard_view->icon_factory,
                            "notify::thumbnail_mode",
                            G_CALLBACK (thunar_standard_view_thumbnail_mode_toggled),
                            standard_view);

  /* apply the thumbnail frame preferences after icon_factory got initialized */
  g_object_bind_property (G_OBJECT (standard_view->preferences), "misc-thumbnail-draw-frames", G_OBJECT (standard_view), "thumbnail-draw-frames", G_BINDING_SYNC_CREATE);

  /* apply/unapply the highlights to name & icon renderers whenever the property changes */
  g_signal_connect_swapped (standard_view->preferences, "notify::misc-highlighting-enabled",
                            G_CALLBACK (thunar_standard_view_highlight_option_changed), standard_view);
  thunar_standard_view_highlight_option_changed (standard_view);

  /* store sort information to keep indicators in menu in sync */
  thunar_standard_view_store_sort_column (standard_view);
}



static void
thunar_standard_view_unrealize (GtkWidget *widget)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (widget);

  /* Disconnect from methods which make use of the icon factory */
  g_signal_handlers_disconnect_by_func (G_OBJECT (standard_view->model), thunar_standard_view_row_changed, standard_view);
  g_signal_handlers_disconnect_by_func (G_OBJECT (standard_view->preferences), thunar_standard_view_highlight_option_changed, standard_view);
  g_signal_handlers_disconnect_by_data (G_OBJECT (standard_view->icon_factory), standard_view);

  /* drop the reference on the icon factory */
  g_object_unref (G_OBJECT (standard_view->icon_factory));
  standard_view->icon_factory = NULL;

  /* let the GtkWidget do its work */
  GTK_WIDGET_CLASS (thunar_standard_view_parent_class)->unrealize (widget);
}



static void
thunar_standard_view_grab_focus (GtkWidget *widget)
{
  /* forward the focus grab to the real view */
  gtk_widget_grab_focus (gtk_bin_get_child (GTK_BIN (widget)));
}



static gboolean
thunar_standard_view_draw (GtkWidget      *widget,
                           cairo_t        *cr)
{
  gboolean result = FALSE;
  GtkAllocation a;
  GtkStyleContext *context;

  /* let the scrolled window do it's work */
  cairo_save (cr);
  result = (*GTK_WIDGET_CLASS (thunar_standard_view_parent_class)->draw) (widget, cr);
  cairo_restore (cr);

  /* render the folder drop shadow */
  if (G_UNLIKELY (THUNAR_STANDARD_VIEW (widget)->priv->drop_highlight))
    {
      gtk_widget_get_allocation (widget, &a);

      context = gtk_widget_get_style_context (widget);

      gtk_style_context_save (context);
      gtk_style_context_set_state (context, GTK_STATE_FLAG_DROP_ACTIVE);
      gtk_render_frame (context, cr, 0, 0, a.width, a.height);
      gtk_style_context_restore (context);
    }

  return result;
}



static GList*
thunar_standard_view_get_selected_files_component (ThunarComponent *component)
{
  return THUNAR_STANDARD_VIEW (component)->priv->selected_files;
}



static GList*
thunar_standard_view_get_selected_files_view (ThunarView *view)
{
  return THUNAR_STANDARD_VIEW (view)->priv->selected_files;
}



static void
thunar_standard_view_set_selected_files_component (ThunarComponent *component,
                                                   GList           *selected_files)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (component);
  GtkTreePath        *first_path = NULL;
  GList              *paths;
  GList              *lp;

  /* release the previous selected files list (if any) */
  if (G_UNLIKELY (standard_view->priv->selected_files != NULL))
    {
      thunar_g_list_free_full (standard_view->priv->selected_files);
      standard_view->priv->selected_files = NULL;
    }

  /* check if we're still loading */
  if (thunar_view_get_loading (THUNAR_VIEW (standard_view)))
    {
      /* remember a copy of the list for later */
      standard_view->priv->selected_files = thunar_g_list_copy_deep (selected_files);
    }
  else
    {
      /* verify that we have a valid model */
      if (G_UNLIKELY (standard_view->model == NULL))
        return;

      /* unselect all previously selected files */
      (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->unselect_all) (standard_view);

      /* determine the tree paths for the given files */
      paths = thunar_list_model_get_paths_for_files (standard_view->model, selected_files);
      if (G_LIKELY (paths != NULL))
        {
          /* determine the first path */
          for (first_path = paths->data, lp = paths; lp != NULL; lp = lp->next)
            {
              /* check if this path is located before the current first_path */
              if (gtk_tree_path_compare (lp->data, first_path) < 0)
                first_path = lp->data;
            }

          /* place the cursor on the first selected path (must be first for GtkTreeView) */
          (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->set_cursor) (standard_view, first_path, FALSE);

          /* select the given tree paths paths */
          for (first_path = paths->data, lp = paths; lp != NULL; lp = lp->next)
            {
              /* select the path */
              (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->select_path) (standard_view, lp->data);
            }

          /* scroll to the first path (previously determined) */
          (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->scroll_to_path) (standard_view, first_path, FALSE, 0.0f, 0.0f);

          /* release the tree paths */
          g_list_free_full (paths, (GDestroyNotify) gtk_tree_path_free);
        }
    }
}



static void
thunar_standard_view_set_selected_files_view (ThunarView *view,
                                              GList      *selected_files)
{
  thunar_standard_view_set_selected_files_component (THUNAR_COMPONENT (view), selected_files);
}



static ThunarFile*
thunar_standard_view_get_current_directory (ThunarNavigator *navigator)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (navigator);
  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), NULL);
  return standard_view->priv->current_directory;
}



static void
thunar_standard_view_scroll_position_save (ThunarStandardView *standard_view)
{
  ThunarFile    *first_file;
  GFile         *gfile;

  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* store the previous directory in the scroll hash table */
  if (standard_view->priv->current_directory != NULL)
    {
      gfile = thunar_file_get_file (standard_view->priv->current_directory);

      if (thunar_view_get_visible_range (THUNAR_VIEW (standard_view), &first_file, NULL))
        {
          /* add the file to our internal mapping of directories to scroll files */
          g_hash_table_replace (standard_view->priv->scroll_to_files,
                                g_object_ref (gfile),
                                g_object_ref (thunar_file_get_file (first_file)));
          g_object_unref (first_file);
        }
    }
}



static void
thunar_standard_view_restore_selection_from_history (ThunarStandardView *standard_view)
{
  GList       selected_files;
  ThunarFile *selected_file;

  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  _thunar_return_if_fail (THUNAR_IS_FILE (standard_view->priv->current_directory));

  /* reset the selected files list */
  selected_files.data = NULL;
  selected_files.prev = NULL;
  selected_files.next = NULL;

  /* determine the next file in the history */
  selected_file = thunar_history_peek_forward (standard_view->priv->history);
  if (selected_file != NULL)
    {
      /* mark the file from history for selection if it is inside the new
       * directory */
      if (thunar_file_is_parent (standard_view->priv->current_directory, selected_file))
        selected_files.data = selected_file;
      else
        g_object_unref (selected_file);
    }

  /* do the same with the previous file in the history */
  if (selected_files.data == NULL)
    {
      selected_file = thunar_history_peek_back (standard_view->priv->history);
      if (selected_file != NULL)
        {
          /* mark the file from history for selection if it is inside the
           * new directory */
          if (thunar_file_is_parent (standard_view->priv->current_directory, selected_file))
            selected_files.data = selected_file;
          else
            g_object_unref (selected_file);
        }
    }

  /* select the previous or next file from the history if it is inside the
   * new current directory */
  if (selected_files.data != NULL)
    {
      thunar_component_set_selected_files (THUNAR_COMPONENT (standard_view), &selected_files);
      g_object_unref (G_OBJECT (selected_files.data));
    }
}



static void
thunar_standard_view_set_current_directory (ThunarNavigator *navigator,
                                            ThunarFile      *current_directory)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (navigator);
  ThunarFolder       *folder;

  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  _thunar_return_if_fail (current_directory == NULL || THUNAR_IS_FILE (current_directory));

  /* get the current directory */
  if (standard_view->priv->current_directory == current_directory)
    return;

  /* cancel any pending thumbnail sources and requests */
  thunar_standard_view_cancel_thumbnailing (standard_view);

  /* disconnect any previous "loading" binding */
  if (G_LIKELY (standard_view->loading_binding != NULL))
    {
      g_object_unref (standard_view->loading_binding);
      standard_view->loading_binding = NULL;
    }

  /* store the current scroll position */
  if (current_directory != NULL)
    thunar_standard_view_scroll_position_save (standard_view);

  /* release previous directory */
  if (standard_view->priv->current_directory != NULL)
    {
      g_signal_handlers_disconnect_matched (standard_view->priv->current_directory,
                                            G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, standard_view);
      g_object_unref (standard_view->priv->current_directory);
    }

  /* check if we want to reset the directory */
  if (G_UNLIKELY (current_directory == NULL))
    {
      /* unset */
      standard_view->priv->current_directory = NULL;

      /* resetting the folder for the model can take some time if the view has
       * to update the selection every time (i.e. closing a window with a lot of
       * selected files), so we temporarily disconnect the model from the view.
       */
      g_object_set (G_OBJECT (gtk_bin_get_child (GTK_BIN (standard_view))), "model", NULL, NULL);

      /* reset the folder for the model */
      thunar_list_model_set_folder (standard_view->model, NULL, NULL);

      /* reconnect the model to the view */
      g_object_set (G_OBJECT (gtk_bin_get_child (GTK_BIN (standard_view))), "model", standard_view->model, NULL);

      /* and we're done */
      return;
    }

  /* take ref on new directory */
  standard_view->priv->current_directory = g_object_ref (current_directory);
  g_signal_connect (G_OBJECT (current_directory), "destroy", G_CALLBACK (thunar_standard_view_current_directory_destroy), standard_view);
  g_signal_connect (G_OBJECT (current_directory), "changed", G_CALLBACK (thunar_standard_view_current_directory_changed), standard_view);

  /* scroll to top-left when changing folder */
  gtk_adjustment_set_value (gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (standard_view)), 0.0);
  gtk_adjustment_set_value (gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (standard_view)), 0.0);

  /* store the directory in the history */
  thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (standard_view->priv->history), current_directory);

  /* if directory specific settings are enabled, apply them */
  if (standard_view->priv->directory_specific_settings)
    thunar_standard_view_apply_directory_specific_settings (standard_view, current_directory);

  /* We drop the model from the view as a simple optimization to speed up
   * the process of disconnecting the model data from the view.
   */
  g_object_set (G_OBJECT (gtk_bin_get_child (GTK_BIN (standard_view))), "model", NULL, NULL);

  /* open the new directory as folder */
  folder = thunar_folder_get_for_file (current_directory);

  /* connect the "loading" binding */
  standard_view->loading_binding =
    g_object_bind_property_full (folder,        "loading",
                                 standard_view, "loading",
                                 G_BINDING_SYNC_CREATE,
                                 NULL, NULL,
                                 standard_view,
                                 thunar_standard_view_loading_unbound);

  /* apply the new folder, ignore removal of any old files */
  g_signal_handler_block (standard_view->model, standard_view->priv->row_deleted_id);
  thunar_list_model_set_folder (standard_view->model, folder, NULL);
  g_signal_handler_unblock (standard_view->model, standard_view->priv->row_deleted_id);
  g_object_unref (G_OBJECT (folder));

  /* reconnect our model to the view */
  g_object_set (G_OBJECT (gtk_bin_get_child (GTK_BIN (standard_view))), "model", standard_view->model, NULL);

  /* schedule a thumbnail timeout */
  /* NOTE: quickly after this we always trigger a size allocate wich will handle this */

  /* notify all listeners about the new/old current directory */
  g_object_notify_by_pspec (G_OBJECT (standard_view), standard_view_props[PROP_CURRENT_DIRECTORY]);

  /* update tab label and tooltip */
  g_object_notify_by_pspec (G_OBJECT (standard_view), standard_view_props[PROP_DISPLAY_NAME]);
  g_object_notify_by_pspec (G_OBJECT (standard_view), standard_view_props[PROP_FULL_PARSED_PATH]);

  /* restore the selection from the history */
  thunar_standard_view_restore_selection_from_history (standard_view);
}



static gboolean
thunar_standard_view_get_loading (ThunarView *view)
{
  return THUNAR_STANDARD_VIEW (view)->loading;
}



static gboolean
thunar_standard_view_get_searching (ThunarView *view)
{
  return THUNAR_STANDARD_VIEW (view)->priv->active_search;
}



static void
thunar_standard_view_set_loading (ThunarStandardView *standard_view,
                                  gboolean            loading)
{
  ThunarFile *file;
  GList      *new_files_path_list;
  GList      *selected_files;
  GFile      *first_file;
  ThunarFile *current_directory;

  loading = !!loading;

  /* check if we're already in that state */
  if (G_UNLIKELY (standard_view->loading == loading))
    return;

  /* apply the new state */
  standard_view->loading = loading;

  /* block or unblock the insert signal to avoid queueing thumbnail reloads */
  if (loading)
    g_signal_handler_block (standard_view->model, standard_view->priv->row_changed_id);
  else
    g_signal_handler_unblock (standard_view->model, standard_view->priv->row_changed_id);

  /* check if we have a path list from new_files pending */
  if (G_UNLIKELY (!loading && standard_view->priv->new_files_path_list != NULL))
    {
      /* remember and reset the new_files_path_list */
      new_files_path_list = standard_view->priv->new_files_path_list;
      standard_view->priv->new_files_path_list = NULL;

      /* and try again */
      thunar_standard_view_new_files (standard_view, new_files_path_list);

      /* cleanup */
      thunar_g_list_free_full (new_files_path_list);
    }

  /* check if we're done loading */
  if (!loading)
    {
      /* remember and reset the file list */
      selected_files = standard_view->priv->selected_files;
      standard_view->priv->selected_files = NULL;

      /* and try setting the selected files again */
      thunar_component_set_selected_files (THUNAR_COMPONENT (standard_view), selected_files);

      /* cleanup */
      thunar_g_list_free_full (selected_files);
    }

  /* check if we're done loading and have a scheduled scroll_to_file
   * scrolling after loading circumvents the scroll caused by gtk_tree_view_set_cell */
  if (THUNAR_IS_DETAILS_VIEW (standard_view))
    {
      if (G_UNLIKELY (!loading))
        {
          if (standard_view->priv->scroll_to_file != NULL)
            {
              /* remember and reset the scroll_to_file reference */
              file = standard_view->priv->scroll_to_file;
              standard_view->priv->scroll_to_file = NULL;

              /* and try again */
              thunar_view_scroll_to_file (THUNAR_VIEW (standard_view), file,
                                          standard_view->priv->scroll_to_select,
                                          standard_view->priv->scroll_to_use_align,
                                          standard_view->priv->scroll_to_row_align,
                                          standard_view->priv->scroll_to_col_align);

              /* cleanup */
              g_object_unref (G_OBJECT (file));
            }
          else
            {
              /* look for a first visible file in the hash table */
              current_directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));
              if (G_LIKELY (current_directory != NULL))
                {
                  first_file = g_hash_table_lookup (standard_view->priv->scroll_to_files, thunar_file_get_file (current_directory));
                  if (G_LIKELY (first_file != NULL))
                    {
                      file = thunar_file_cache_lookup (first_file);
                      if (G_LIKELY (file != NULL))
                        {
                          thunar_view_scroll_to_file (THUNAR_VIEW (standard_view), file, FALSE, TRUE, 0.0f, 0.0f);
                          g_object_unref (file);
                        }
                    }
                }
            }
        }
    }

  /* check if we're done loading and a thumbnail timeout or idle was requested */
  if (!loading && standard_view->priv->thumbnailing_scheduled)
    {
      /* We've just finished loading. It will probably take the user some time to
       * understand the contents of the folder before he will start interacting
       * with the view. So here we can safely schedule an idle function instead
       * of a timeout. */
      thunar_standard_view_schedule_thumbnail_idle (standard_view);
      standard_view->priv->thumbnailing_scheduled = FALSE;
    }

  /* notify listeners */
  g_object_freeze_notify (G_OBJECT (standard_view));
  g_object_notify_by_pspec (G_OBJECT (standard_view), standard_view_props[PROP_LOADING]);
  thunar_standard_view_update_statusbar_text (standard_view);
  g_object_thaw_notify (G_OBJECT (standard_view));
}



static const gchar*
thunar_standard_view_get_statusbar_text (ThunarView *view)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (view);
  GList              *items;

  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), NULL);

  /* generate the statusbar text on-demand */
  if (standard_view->priv->statusbar_text == NULL)
    {
      /* query the selected items (actually a list of GtkTreePath's) */
      items = THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->get_selected_items (standard_view);

      /* we display a loading text if no items are
       * selected and the view is loading
       */
      if (items == NULL && standard_view->loading)
        return _("Loading folder contents...");

      standard_view->priv->statusbar_text = thunar_list_model_get_statusbar_text (standard_view->model, items);
      g_list_free_full (items, (GDestroyNotify) gtk_tree_path_free);
    }

  return standard_view->priv->statusbar_text;
}



static gboolean
thunar_standard_view_get_show_hidden (ThunarView *view)
{
  return thunar_list_model_get_show_hidden (THUNAR_STANDARD_VIEW (view)->model);
}



static void
thunar_standard_view_set_show_hidden (ThunarView *view,
                                      gboolean    show_hidden)
{
  thunar_list_model_set_show_hidden (THUNAR_STANDARD_VIEW (view)->model, show_hidden);
}



static ThunarZoomLevel
thunar_standard_view_get_zoom_level (ThunarView *view)
{
  return THUNAR_STANDARD_VIEW (view)->priv->zoom_level;
}



static void
thunar_standard_view_set_zoom_level (ThunarView     *view,
                                     ThunarZoomLevel zoom_level)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (view);
  gboolean newThumbnailSize = FALSE;

  /* check if we have a new zoom-level here */
  if (G_LIKELY (standard_view->priv->zoom_level != zoom_level))
    {
      if (standard_view->priv->directory_specific_settings)
        {
          const gchar *zoom_level_name;

          /* save the zoom level name */
          zoom_level_name = thunar_zoom_level_string_from_value (zoom_level);
          if (zoom_level_name != NULL)
            {
              /* do not set it asynchronously to ensure the correct operation of thumbnails (check the commit message for more) */
              thunar_file_set_metadata_setting (standard_view->priv->current_directory, "zoom-level", zoom_level_name, FALSE);
            }
        }

      if (thunar_zoom_level_to_thumbnail_size (zoom_level) != thunar_zoom_level_to_thumbnail_size (standard_view->priv->zoom_level))
        newThumbnailSize = TRUE;

      standard_view->priv->zoom_level = zoom_level;

      g_object_notify_by_pspec (G_OBJECT (standard_view), standard_view_props[PROP_ZOOM_LEVEL]);

      if (newThumbnailSize)
        thunar_standard_view_reload (view, TRUE);
    }
}



static void
thunar_standard_view_reset_zoom_level (ThunarView *view)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (view);
  const gchar        *property_name;
  GParamSpec         *pspec;
  GValue              value = { 0, };

  /* determine the default zoom level from the preferences */
  property_name = THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->zoom_level_property_name;
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (standard_view->preferences), property_name);
  g_value_init (&value, THUNAR_TYPE_ZOOM_LEVEL);
  g_param_value_set_default (pspec, &value);
  g_object_set_property (G_OBJECT (view), "zoom-level", &value);
  g_value_unset (&value);
}



static void
thunar_standard_view_apply_directory_specific_settings (ThunarStandardView *standard_view,
                                                        ThunarFile         *directory)
{
  const gchar *sort_column_name;
  const gchar *sort_order_name;
  const gchar *zoom_level_name;
  ThunarColumn sort_column;
  GtkSortType  sort_order;
  gint         zoom_level;

  /* get the default sort column and sort order */
  g_object_get (G_OBJECT (standard_view->preferences), "last-sort-column", &sort_column, "last-sort-order", &sort_order, NULL);

  /* get the stored directory specific settings (if any) */
  sort_column_name = thunar_file_get_metadata_setting (directory, "sort-column");
  sort_order_name = thunar_file_get_metadata_setting (directory, "sort-order");
  zoom_level_name = thunar_file_get_metadata_setting (directory, "zoom-level");

  /* convert the sort column name to a value */
  if (sort_column_name != NULL)
    thunar_column_value_from_string (sort_column_name, &sort_column);

  /* Out of range ? Use the default value */
  if (sort_column >= THUNAR_N_VISIBLE_COLUMNS)
    sort_column = standard_view->priv->sort_column_default;

  /* convert the sort order name to a value */
  if (sort_order_name != NULL)
    {
      if (g_strcmp0 (sort_order_name, "GTK_SORT_DESCENDING") == 0)
        sort_order = GTK_SORT_DESCENDING;
      else if (g_strcmp0 (sort_order_name, "GTK_SORT_ASCENDING") == 0)
        sort_order = GTK_SORT_ASCENDING;
      else
        sort_order = standard_view->priv->sort_order_default;
    }

  /* convert the zoom level name to a value */
  if (zoom_level_name != NULL)
    {
      if (thunar_zoom_level_value_from_string (zoom_level_name, &zoom_level) == TRUE)
        thunar_standard_view_set_zoom_level (THUNAR_VIEW (standard_view), zoom_level);
    }

  /* thunar_standard_view_sort_column_changed saves the directory specific settings to the directory, but we do not
   * want that behaviour here so we disconnect the signal before calling gtk_tree_sortable_set_sort_column_id */
  g_signal_handlers_disconnect_by_func (G_OBJECT (standard_view->model),
                                        G_CALLBACK (thunar_standard_view_sort_column_changed),
                                        standard_view);

  /* apply the sort column and sort order */
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (standard_view->model), sort_column, sort_order);
  standard_view->priv->sort_column = sort_column;
  standard_view->priv->sort_order = sort_order;

  /* keep the currently selected files selected after the change */
  thunar_component_restore_selection (THUNAR_COMPONENT (standard_view));

  /* reconnect the signal */
  g_signal_connect (G_OBJECT (standard_view->model),
                    "sort-column-changed",
                    G_CALLBACK (thunar_standard_view_sort_column_changed),
                    standard_view);
}



static void
thunar_standard_view_set_directory_specific_settings (ThunarStandardView *standard_view,
                                                      gboolean            directory_specific_settings)
{
  /* save the setting */
  standard_view->priv->directory_specific_settings = directory_specific_settings;

  /* if there is no current directory then return  */
  if (standard_view->priv->current_directory == NULL)
    return;

  /* apply the appropriate settings */
  if (directory_specific_settings)
    {
      /* apply the directory specific settings (if any) */
      thunar_standard_view_apply_directory_specific_settings (standard_view, standard_view->priv->current_directory);
    }
  else /* apply the shared settings to the current view */
    {
      ThunarColumn  sort_column;
      GtkSortType   sort_order;

      /* apply the last sort column and sort order */
      g_object_get (G_OBJECT (standard_view->preferences), "last-sort-column", &sort_column, "last-sort-order", &sort_order, NULL);
      if (sort_column >= THUNAR_N_VISIBLE_COLUMNS)
        sort_column = THUNAR_COLUMN_NAME;
      gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (standard_view->model), sort_column, sort_order);
      standard_view->priv->sort_column = sort_column;
      standard_view->priv->sort_order = sort_order;
    }
}



static void
thunar_standard_view_reload (ThunarView *view,
                             gboolean    reload_info)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (view);
  ThunarFolder       *folder;
  ThunarFile         *file;

  /* determine the folder for the view model */
  folder = thunar_list_model_get_folder (standard_view->model);
  if (G_LIKELY (folder != NULL))
    {
      file = thunar_folder_get_corresponding_file (folder);

      if (thunar_file_exists (file))
          thunar_folder_reload (folder, reload_info);
      else
          thunar_standard_view_current_directory_destroy (file, standard_view);
    }

  /* if directory specific settings are enabled, apply them. the reload might have been triggered */
  /* specifically to ensure that any change in these settings is applied */
  if (standard_view->priv->directory_specific_settings)
    thunar_standard_view_apply_directory_specific_settings (standard_view, standard_view->priv->current_directory);

  /* schedule thumbnail reload update */
  if (!standard_view->priv->thumbnailing_scheduled)
    thunar_standard_view_schedule_thumbnail_idle (standard_view);
}



static gboolean
thunar_standard_view_get_visible_range (ThunarView  *view,
                                        ThunarFile **start_file,
                                        ThunarFile **end_file)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (view);
  GtkTreePath        *start_path;
  GtkTreePath        *end_path;
  GtkTreeIter         iter;

  /* determine the visible range as tree paths */
  if ((*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->get_visible_range) (standard_view, &start_path, &end_path))
    {
      /* determine the file for the start path */
      if (G_LIKELY (start_file != NULL))
        {
          gtk_tree_model_get_iter (GTK_TREE_MODEL (standard_view->model), &iter, start_path);
          *start_file = thunar_list_model_get_file (standard_view->model, &iter);
        }

      /* determine the file for the end path */
      if (G_LIKELY (end_file != NULL))
        {
          gtk_tree_model_get_iter (GTK_TREE_MODEL (standard_view->model), &iter, end_path);
          *end_file = thunar_list_model_get_file (standard_view->model, &iter);
        }

      /* release the tree paths */
      gtk_tree_path_free (start_path);
      gtk_tree_path_free (end_path);

      return TRUE;
    }

  return FALSE;
}



static void
thunar_standard_view_scroll_to_file (ThunarView *view,
                                     ThunarFile *file,
                                     gboolean    select_file,
                                     gboolean    use_align,
                                     gfloat      row_align,
                                     gfloat      col_align)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (view);
  GList               files;
  GList              *paths;

  /* release the previous scroll_to_file reference (if any) */
  if (G_UNLIKELY (standard_view->priv->scroll_to_file != NULL))
    {
      g_object_unref (G_OBJECT (standard_view->priv->scroll_to_file));
      standard_view->priv->scroll_to_file = NULL;
    }

  /* check if we're still loading */
  if (thunar_view_get_loading (view))
    {
      /* remember a reference for the new file and settings */
      standard_view->priv->scroll_to_file = THUNAR_FILE (g_object_ref (G_OBJECT (file)));
      standard_view->priv->scroll_to_select = select_file;
      standard_view->priv->scroll_to_use_align = use_align;
      standard_view->priv->scroll_to_row_align = row_align;
      standard_view->priv->scroll_to_col_align = col_align;
    }
  else
    {
      /* fake a file list */
      files.data = file;
      files.next = NULL;
      files.prev = NULL;

      /* determine the path for the file */
      paths = thunar_list_model_get_paths_for_files (standard_view->model, &files);
      if (G_LIKELY (paths != NULL))
        {
          /* scroll to the path */
          (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->scroll_to_path) (standard_view, paths->data, use_align, row_align, col_align);

          /* check if we should also alter the selection */
          if (G_UNLIKELY (select_file))
            {
              /* select only the file in question */
              (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->unselect_all) (standard_view);
              (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->select_path) (standard_view, paths->data);
            }

          /* cleanup */
          g_list_free_full (paths, (GDestroyNotify) gtk_tree_path_free);
        }
    }
}



static GdkDragAction
thunar_standard_view_get_dest_actions (ThunarStandardView *standard_view,
                                       GdkDragContext     *context,
                                       gint                x,
                                       gint                y,
                                       guint               timestamp,
                                       ThunarFile        **file_return)
{
  GdkDragAction actions = 0;
  GdkDragAction action = 0;
  GtkTreePath  *path;
  ThunarFile   *file;

  /* determine the file and path for the given coordinates */
  file = thunar_standard_view_get_drop_file (standard_view, x, y, &path);

  /* check if we can drop there */
  if (G_LIKELY (file != NULL) && G_LIKELY (standard_view->priv->search_query == NULL))
    {
      /* determine the possible drop actions for the file (and the suggested action if any) */
      actions = thunar_file_accepts_drop (file, standard_view->priv->drop_file_list, context, &action);
      if (G_LIKELY (actions != 0))
        {
          /* tell the caller about the file (if it's interested) */
          if (G_UNLIKELY (file_return != NULL))
            *file_return = THUNAR_FILE (g_object_ref (G_OBJECT (file)));
        }
    }

  /* reset path if we cannot drop */
  if (G_UNLIKELY (action == 0 && path != NULL))
    {
      gtk_tree_path_free (path);
      path = NULL;
    }

  /* setup the drop-file for the icon renderer, so the user
   * gets good visual feedback for the drop target.
   */
  g_object_set (G_OBJECT (standard_view->icon_renderer), "drop-file", (action != 0) ? file : NULL, NULL);

  /* do the view highlighting */
  if (standard_view->priv->drop_highlight != (path == NULL && action != 0))
    {
      standard_view->priv->drop_highlight = (path == NULL && action != 0);
      gtk_widget_queue_draw (GTK_WIDGET (standard_view));
    }

  /* do the item highlighting */
  (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->highlight_path) (standard_view, path);

  /* tell Gdk whether we can drop here */
  gdk_drag_status (context, action, timestamp);

  /* clean up */
  if (G_LIKELY (file != NULL))
    g_object_unref (G_OBJECT (file));
  if (G_LIKELY (path != NULL))
    gtk_tree_path_free (path);

  return actions;
}



static ThunarFile*
thunar_standard_view_get_drop_file (ThunarStandardView *standard_view,
                                    gint                x,
                                    gint                y,
                                    GtkTreePath       **path_return)
{
  GtkTreePath *path = NULL;
  GtkTreeIter  iter;
  ThunarFile  *file = NULL;

  /* determine the path for the given coordinates */
  path = (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->get_path_at_pos) (standard_view, x, y);
  if (G_LIKELY (path != NULL))
    {
      /* determine the file for the path */
      gtk_tree_model_get_iter (GTK_TREE_MODEL (standard_view->model), &iter, path);
      file = thunar_list_model_get_file (standard_view->model, &iter);

      /* we can only drop to directories and executable files */
      if (!thunar_file_is_directory (file) && !thunar_file_can_execute (file))
        {
          /* drop to the folder instead */
          g_object_unref (G_OBJECT (file));
          gtk_tree_path_free (path);
          path = NULL;
        }
    }

  /* if we don't have a path yet, we'll drop to the folder instead */
  if (G_UNLIKELY (path == NULL))
    {
      /* determine the current directory */
      file = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));
      if (G_LIKELY (file != NULL))
        g_object_ref (G_OBJECT (file));
    }

  /* return the path (if any) */
  if (G_LIKELY (path_return != NULL))
    *path_return = path;
  else if (G_LIKELY (path != NULL))
    gtk_tree_path_free (path);

  return file;
}



static gboolean
thunar_standard_view_update_statusbar_text_idle (gpointer data)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (data);

  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), FALSE);

THUNAR_THREADS_ENTER

  /* clear the current status text (will be recalculated on-demand) */
  g_free (standard_view->priv->statusbar_text);
  standard_view->priv->statusbar_text = NULL;

  standard_view->priv->statusbar_text_idle_id = 0;

  /* tell everybody that the statusbar text may have changed */
  g_object_notify_by_pspec (G_OBJECT (standard_view), standard_view_props[PROP_STATUSBAR_TEXT]);

THUNAR_THREADS_LEAVE

  return FALSE;
}



void
thunar_standard_view_update_statusbar_text (ThunarStandardView *standard_view)
{
  /* stop pending timeout */
  if (standard_view->priv->statusbar_text_idle_id != 0)
    g_source_remove (standard_view->priv->statusbar_text_idle_id);

  /* restart a new one, this way we avoid multiple update when
   * the user is pressing a key to scroll */
  standard_view->priv->statusbar_text_idle_id =
      g_timeout_add_full (G_PRIORITY_LOW, 50, thunar_standard_view_update_statusbar_text_idle,
                          standard_view, NULL);
}



/*
 * Find a fallback directory we can navigate to if the directory gets
 * deleted. It first tries the parent folders, and finally if none can
 * be found, the home folder. If the home folder cannot be accessed,
 * the error will be stored for use by the caller.
 */
static ThunarFile *
thunar_standard_view_get_fallback_directory (ThunarFile *directory,
                                             GError     *error)
{
  ThunarFile *new_directory = NULL;
  GFile      *path;
  GFile      *tmp;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (directory), NULL);

  /* determine the path of the directory */
  path = g_object_ref (thunar_file_get_file (directory));

  /* try to find a parent directory that still exists */
  while (new_directory == NULL)
    {
      /* check whether the directory exists */
      if (g_file_query_exists (path, NULL))
        {
          /* it does, try to load the file */
          new_directory = thunar_file_get (path, NULL);

          /* fall back to $HOME if loading the file failed */
          if (new_directory == NULL)
            break;
        }
      else
        {
          /* determine the parent of the directory */
          tmp = g_file_get_parent (path);

          /* if there's no parent this means that we've found no parent
           * that still exists at all. Fall back to $HOME then */
          if (tmp == NULL)
            break;

          /* free the old directory */
          g_object_unref (path);

          /* check the parent next */
          path = tmp;
        }
    }

  /* release last ref */
  if (path != NULL)
    g_object_unref (path);

  if (new_directory == NULL)
    {
      /* fall-back to the home directory */
      path = thunar_g_file_new_for_home ();
      new_directory = thunar_file_get (path, &error);
      g_object_unref (path);
    }

  return new_directory;
}



static void
thunar_standard_view_current_directory_destroy (ThunarFile         *current_directory,
                                                ThunarStandardView *standard_view)
{
  GtkWidget  *window;
  ThunarFile *new_directory = NULL;
  GError     *error = NULL;

  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  _thunar_return_if_fail (THUNAR_IS_FILE (current_directory));
  _thunar_return_if_fail (standard_view->priv->current_directory == current_directory);

  /* get a fallback directory (parents or home) we can navigate to */
  new_directory = thunar_standard_view_get_fallback_directory (current_directory, error);
  if (G_UNLIKELY (new_directory == NULL))
    {
      /* display an error to the user */
      thunar_dialogs_show_error (GTK_WIDGET (standard_view), error, _("Failed to open the home folder"));
      g_error_free (error);
      return;
    }

  /* let the parent window update all active and inactive views (tabs) */
  window = gtk_widget_get_toplevel (GTK_WIDGET (standard_view));
  thunar_window_update_directories (THUNAR_WINDOW (window),
                                    current_directory,
                                    new_directory);

  /* release the reference to the new directory */
  g_object_unref (new_directory);
}



static void
thunar_standard_view_current_directory_changed (ThunarFile         *current_directory,
                                                ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  _thunar_return_if_fail (THUNAR_IS_FILE (current_directory));
  _thunar_return_if_fail (standard_view->priv->current_directory == current_directory);

  /* update tab label and tooltip */
  g_object_notify_by_pspec (G_OBJECT (standard_view), standard_view_props[PROP_DISPLAY_NAME]);
  g_object_notify_by_pspec (G_OBJECT (standard_view), standard_view_props[PROP_FULL_PARSED_PATH]);

  /* directory is possibly moved, schedule a thumbnail update */
  thunar_standard_view_schedule_thumbnail_timeout (standard_view);
}



static gboolean
thunar_standard_view_select_all_files (ThunarView *view)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (view);

  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), FALSE);

  /* grab the focus to the view */
  gtk_widget_grab_focus (GTK_WIDGET (standard_view));

  /* select all files in the real view */
  (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->select_all) (standard_view);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_standard_view_select_by_pattern (ThunarView *view)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (view);
  GtkWidget          *window;
  GtkWidget          *dialog;
  GtkBox             *content_area;
  GtkGrid            *grid;
  GtkWidget          *label;
  GtkWidget          *entry;
  GtkWidget          *info_image;
  GtkWidget          *case_sensitive_button;
  GtkWidget          *match_diacritics_button;
  GList              *paths;
  GList              *lp;
  gint                response;
  const gchar        *pattern;
  gchar              *pattern_extended = NULL;
  gboolean            case_sensitive;
  gboolean            match_diacritics;
  gint                row = 0;

  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), FALSE);

  window = gtk_widget_get_toplevel (GTK_WIDGET (standard_view));
  /* TRANSLATORS: Dialog allowing selection by wildcard ("*.c", etc.) */
  dialog = gtk_dialog_new_with_buttons (C_("Select by Pattern dialog: title",
                                           "Select by Pattern"),
                                        GTK_WINDOW (window),
                                        GTK_DIALOG_MODAL
                                        | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        C_("Select by Pattern dialog: buttons",
                                           "_Cancel"), GTK_RESPONSE_CANCEL,
                                        C_("Select by Pattern dialog: buttons",
                                           "_Select"), GTK_RESPONSE_OK,
                                        NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 290, -1);

  content_area = GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog)));
  gtk_container_set_border_width (GTK_CONTAINER (content_area), 6);

  grid = GTK_GRID (gtk_grid_new ());
  g_object_set (G_OBJECT (grid), "column-spacing", 10, "row-spacing", 10, NULL);
  gtk_box_pack_start (content_area, GTK_WIDGET (grid), TRUE, TRUE, 10);
  gtk_widget_show (GTK_WIDGET (grid));

  label = gtk_label_new_with_mnemonic (C_("Select by Pattern dialog: labels: pattern entry textbox", "_Pattern:"));
  gtk_widget_set_tooltip_text (GTK_WIDGET (label), C_("Select by Pattern dialog: tooltips on label for pattern entry textbox", "Files whose name matches the wildcard pattern you enter will be selected in the main window."));
  gtk_grid_attach (grid, label, 0, row, 1, 1);
  gtk_widget_show (label);

  entry = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
  gtk_entry_set_width_chars (GTK_ENTRY (entry), 25);
  gtk_grid_attach_next_to (grid, entry, label, GTK_POS_RIGHT, 1, 1);
  gtk_widget_set_hexpand (entry, TRUE);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
  gtk_widget_show (entry);

  info_image = gtk_image_new_from_icon_name ("dialog-information", GTK_ICON_SIZE_MENU);
  /* TRANSLATORS: the * and ? characters are the ASCII wildcard special symbols, and they must not be localized. */
  gtk_widget_set_tooltip_text (GTK_WIDGET (info_image), C_("Select by Pattern dialog: tooltips: pattern entry text box", "? matches exactly one character,\n* matches any number of characters, including zero.\n\nFor example: *.txt, file??.png, pict\n\nWithout any * or ? wildcards, the pattern will match anywhere in a name. With wildcards, the pattern must match at both the start and the end of a name."));
  gtk_grid_attach_next_to (grid, info_image, entry, GTK_POS_RIGHT, 1, 1);
  gtk_widget_show (info_image);

  case_sensitive_button = gtk_check_button_new_with_mnemonic (C_("Select by Pattern dialog: labels: case sensitive checkbox", "C_ase sensitive"));
  gtk_widget_set_tooltip_text (GTK_WIDGET (case_sensitive_button), C_("Select by Pattern dialog: tooltips: case sensitive checkbox", "If enabled, letter case must match the pattern.\nExamp* would match Example.txt and not example.txt"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (case_sensitive_button), FALSE);
  gtk_grid_attach_next_to (grid, case_sensitive_button, entry, GTK_POS_BOTTOM, 1, 1);
  gtk_widget_show (case_sensitive_button);
 
  match_diacritics_button = gtk_check_button_new_with_mnemonic (C_("Select by Pattern dialog: labels: match diacritics checkbox", "_Match diacritics"));
  gtk_widget_set_tooltip_text (GTK_WIDGET (match_diacritics_button), C_("Select by Pattern dialog: tooltips: match diacritics checkbox", "If enabled, require accents to match the pattern.\nRés* would match Résumé.txt and not Resume.txt"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (match_diacritics_button), FALSE);
  gtk_grid_attach_next_to (grid, match_diacritics_button, case_sensitive_button, GTK_POS_BOTTOM, 1, 1);
  gtk_widget_show (match_diacritics_button);

  response = gtk_dialog_run (GTK_DIALOG (dialog));
  if (response == GTK_RESPONSE_OK)
    {
      /* get entered pattern */
      pattern = gtk_entry_get_text (GTK_ENTRY (entry));
      if (pattern != NULL
          && strchr (pattern, '*') == NULL
          && strchr (pattern, '?') == NULL)
        {
          /* make file matching pattern */
          pattern_extended = g_strdup_printf ("*%s*", pattern);
          pattern = pattern_extended;
        }

      /* get case sensitivity option */
      case_sensitive = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (case_sensitive_button));

      /* get ignore diacritics option */
      match_diacritics = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (match_diacritics_button));

      /* select all files that match pattern */
      paths = thunar_list_model_get_paths_for_pattern (standard_view->model, pattern, case_sensitive, match_diacritics);
      THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->unselect_all (standard_view);

      /* set the cursor and scroll to the first selected item */
      if (paths != NULL)
        THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->set_cursor (standard_view, g_list_last (paths)->data, FALSE);

      for (lp = paths; lp != NULL; lp = lp->next)
        {
          THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->select_path (standard_view, lp->data);
          gtk_tree_path_free (lp->data);
        }
      g_list_free (paths);
      g_free (pattern_extended);
    }

  gtk_widget_destroy (dialog);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_standard_view_selection_invert (ThunarView *view)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (view);

  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), FALSE);

  /* grab the focus to the view */
  gtk_widget_grab_focus (GTK_WIDGET (standard_view));

  /* invert all files in the real view */
  (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->selection_invert) (standard_view);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_standard_view_unselect_all_files (ThunarView *view)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (view);

  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), FALSE);

  /* grab the focus to the view */
  gtk_widget_grab_focus (GTK_WIDGET (standard_view));

  /* unselect all files in the real view */
  (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->unselect_all) (standard_view);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static GClosure*
thunar_standard_view_new_files_closure (ThunarStandardView *standard_view,
                                        GtkWidget          *source_view)
{
  _thunar_return_val_if_fail (source_view == NULL || THUNAR_IS_VIEW (source_view), NULL);

  /* drop any previous "new-files" closure */
  if (G_UNLIKELY (standard_view->priv->new_files_closure != NULL))
    {
      g_closure_invalidate (standard_view->priv->new_files_closure);
      g_closure_unref (standard_view->priv->new_files_closure);
    }

  /* set the remove view data we possibly need to reload */
  g_object_set_data (G_OBJECT (standard_view), I_("source-view"), source_view);

  /* allocate a new "new-files" closure */
  standard_view->priv->new_files_closure = g_cclosure_new_swap (G_CALLBACK (thunar_standard_view_new_files), standard_view, NULL);
  g_closure_ref (standard_view->priv->new_files_closure);
  g_closure_sink (standard_view->priv->new_files_closure);

  /* and return our new closure */
  return standard_view->priv->new_files_closure;
}



static void
thunar_standard_view_new_files (ThunarStandardView *standard_view,
                                GList              *path_list)
{
  ThunarFile*file;
  GList     *file_list = NULL;
  GList     *lp;
  GtkWidget *source_view;
  GFile     *parent_file;
  gboolean   belongs_here;

  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* release the previous "new-files" paths (if any) */
  if (G_UNLIKELY (standard_view->priv->new_files_path_list != NULL))
    {
      thunar_g_list_free_full (standard_view->priv->new_files_path_list);
      standard_view->priv->new_files_path_list = NULL;
    }

  /* check if the folder is currently being loaded */
  if (G_UNLIKELY (standard_view->loading))
    {
      /* schedule the "new-files" paths for later processing */
      standard_view->priv->new_files_path_list = thunar_g_list_copy_deep (path_list);
    }
  else if (G_LIKELY (path_list != NULL))
    {
      /* to check if we should reload */
      parent_file = thunar_file_get_file (standard_view->priv->current_directory);
      belongs_here = FALSE;

      /* determine the files for the paths */
      for (lp = path_list; lp != NULL; lp = lp->next)
        {
          file = thunar_file_cache_lookup (lp->data);
          if (G_LIKELY (file != NULL))
            file_list = g_list_prepend (file_list, file);
          else if (!belongs_here && g_file_has_parent (lp->data, parent_file))
            belongs_here = TRUE;
        }

      /* check if we have any new files here */
      if (G_LIKELY (file_list != NULL))
        {
          /* select the files */
          thunar_component_set_selected_files (THUNAR_COMPONENT (standard_view), file_list);

          /* release the file list */
          g_list_free_full (file_list, g_object_unref);

          /* grab the focus to the view widget */
          gtk_widget_grab_focus (gtk_bin_get_child (GTK_BIN (standard_view)));
        }
      else if (belongs_here)
        {
          /* thunar files are not created yet, try again later because we know
           * some of them belong in this directory, so eventually they
           * will get a ThunarFile */
          standard_view->priv->new_files_path_list = thunar_g_list_copy_deep (path_list);
        }
    }

  /* when performing dnd between 2 views, we force a reload on the source as well */
  source_view = g_object_get_data (G_OBJECT (standard_view), I_("source-view"));
  if (THUNAR_IS_VIEW (source_view))
    thunar_view_reload (THUNAR_VIEW (source_view), FALSE);
}



static gboolean
thunar_standard_view_button_release_event (GtkWidget          *view,
                                           GdkEventButton     *event,
                                           ThunarStandardView *standard_view)
{
  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), FALSE);
  _thunar_return_val_if_fail (standard_view->priv->drag_timer_id != 0, FALSE);

  /* cancel the pending drag timer */
  g_source_remove (standard_view->priv->drag_timer_id);

  /* fire up the context menu */
  thunar_standard_view_context_menu (standard_view);

  return TRUE;
}



static gboolean
thunar_standard_view_motion_notify_event (GtkWidget          *view,
                                          GdkEventMotion     *event,
                                          ThunarStandardView *standard_view)
{
  GtkTargetList  *target_list;

  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), FALSE);
  _thunar_return_val_if_fail (standard_view->priv->drag_timer_id != 0, FALSE);

  /* check if we passed the DnD threshold */
  if (gtk_drag_check_threshold (view, standard_view->priv->drag_x, standard_view->priv->drag_y, event->x, event->y))
    {
      /* cancel the drag timer, as we won't popup the menu anymore */
      g_source_remove (standard_view->priv->drag_timer_id);
      gdk_event_free (standard_view->priv->drag_timer_event);
      standard_view->priv->drag_timer_event = NULL;

      /* allocate the drag context */
      target_list = gtk_target_list_new (drag_targets, G_N_ELEMENTS (drag_targets));
      gtk_drag_begin_with_coordinates (view, target_list,
                                       GDK_ACTION_COPY |
                                       GDK_ACTION_MOVE |
                                       GDK_ACTION_LINK |
                                       GDK_ACTION_ASK,
                                       3, (GdkEvent *) event, -1, -1);
      gtk_target_list_unref (target_list);

      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_standard_view_scroll_event (GtkWidget          *view,
                                   GdkEventScroll     *event,
                                   ThunarStandardView *standard_view)
{
  GdkScrollDirection scrolling_direction;
  gboolean           misc_horizontal_wheel_navigates;

  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), FALSE);

  if (event->direction != GDK_SCROLL_SMOOTH)
    scrolling_direction = event->direction;
  else if (event->delta_y < 0)
    scrolling_direction = GDK_SCROLL_UP;
  else if (event->delta_y > 0)
    scrolling_direction = GDK_SCROLL_DOWN;
  else if (event->delta_x < 0)
    scrolling_direction = GDK_SCROLL_LEFT;
  else if (event->delta_x > 0)
    scrolling_direction = GDK_SCROLL_RIGHT;
  else
    {
      g_debug ("GDK_SCROLL_SMOOTH scrolling event with no delta happened");
      return FALSE;
    }

  if (G_UNLIKELY (scrolling_direction == GDK_SCROLL_LEFT || scrolling_direction == GDK_SCROLL_RIGHT))
    {
      /* check if we should use the horizontal mouse wheel for navigation */
      g_object_get (G_OBJECT (standard_view->preferences), "misc-horizontal-wheel-navigates", &misc_horizontal_wheel_navigates, NULL);
      if (G_UNLIKELY (misc_horizontal_wheel_navigates))
        {
          if (scrolling_direction == GDK_SCROLL_LEFT)
            thunar_history_action_back (standard_view->priv->history);
          else
            thunar_history_action_forward (standard_view->priv->history);
        }
    }

  /* zoom-in/zoom-out on control+mouse wheel */
  if ((event->state & GDK_CONTROL_MASK) != 0 && (scrolling_direction == GDK_SCROLL_UP || scrolling_direction == GDK_SCROLL_DOWN))
    {
      thunar_view_set_zoom_level (THUNAR_VIEW (standard_view),
          (scrolling_direction == GDK_SCROLL_UP)
          ? MIN (standard_view->priv->zoom_level + 1, THUNAR_ZOOM_N_LEVELS - 1)
          : MAX (standard_view->priv->zoom_level, 1) - 1);
      return TRUE;
    }

  /* next please... */
  return FALSE;
}



static gboolean
thunar_standard_view_key_press_event (GtkWidget          *view,
                                      GdkEventKey        *event,
                                      ThunarStandardView *standard_view)
{
  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), FALSE);

  /* need to catch "/" and "~" first, as the views would otherwise start interactive search */
  if ((event->keyval == GDK_KEY_slash || event->keyval == GDK_KEY_asciitilde || event->keyval == GDK_KEY_dead_tilde) && !(event->state & (~GDK_SHIFT_MASK & gtk_accelerator_get_default_mod_mask ())))
    {
      /* popup the location selector (in whatever way) */
      if (event->keyval == GDK_KEY_dead_tilde)
        g_signal_emit (G_OBJECT (standard_view), standard_view_signals[START_OPEN_LOCATION], 0, "~");
      else
        g_signal_emit (G_OBJECT (standard_view), standard_view_signals[START_OPEN_LOCATION], 0, event->string);

      return TRUE;
    }


  if (standard_view->accel_group != NULL && 
      xfce_gtk_handle_tab_accels (event, standard_view->accel_group, standard_view, thunar_standard_view_action_entries, THUNAR_STANDARD_VIEW_N_ACTIONS) == TRUE)
    return TRUE;

  return FALSE;
}



static gboolean
thunar_standard_view_drag_drop (GtkWidget          *view,
                                GdkDragContext     *context,
                                gint                x,
                                gint                y,
                                guint               timestamp,
                                ThunarStandardView *standard_view)
{
  ThunarFile *file = NULL;
  GdkAtom     target;
  guchar     *prop_text;
  GFile      *path;
  gchar      *uri = NULL;
  gint        prop_len;

  target = gtk_drag_dest_find_target (view, context, NULL);
  if (G_UNLIKELY (target == GDK_NONE))
    {
      /* we cannot handle the drag data */
      return FALSE;
    }
  else if (G_UNLIKELY (target == gdk_atom_intern_static_string ("XdndDirectSave0")))
    {
      /* determine the file for the drop position */
      file = thunar_standard_view_get_drop_file (standard_view, x, y, NULL);
      if (G_LIKELY (file != NULL))
        {
          /* determine the file name from the DnD source window */
          if (gdk_property_get (gdk_drag_context_get_source_window (context),
                                gdk_atom_intern_static_string ("XdndDirectSave0"),
                                gdk_atom_intern_static_string ("text/plain"), 0, 1024, FALSE, NULL, NULL,
                                &prop_len, &prop_text) && prop_text != NULL)
            {
              /* zero-terminate the string */
              prop_text = g_realloc (prop_text, prop_len + 1);
              prop_text[prop_len] = '\0';

              /* verify that the file name provided by the source is valid */
              if (G_LIKELY (*prop_text != '\0' && strchr ((const gchar *) prop_text, G_DIR_SEPARATOR) == NULL))
                {
                  /* allocate the relative path for the target */
                  path = g_file_resolve_relative_path (thunar_file_get_file (file),
                                                       (const gchar *)prop_text);

                  /* determine the new URI */
                  uri = g_file_get_uri (path);

                  /* setup the property */
                  gdk_property_change (gdk_drag_context_get_source_window (context),
                                       gdk_atom_intern_static_string ("XdndDirectSave0"),
                                       gdk_atom_intern_static_string ("text/plain"), 8,
                                       GDK_PROP_MODE_REPLACE, (const guchar *) uri,
                                       strlen (uri));

                  /* cleanup */
                  g_object_unref (path);
                  g_free (uri);
                }
              else
                {
                  /* tell the user that the file name provided by the X Direct Save source is invalid */
                  thunar_dialogs_show_error (GTK_WIDGET (standard_view), NULL, _("Invalid filename provided by XDS drag site"));
                }

              /* cleanup */
              g_free (prop_text);
            }

          /* release the file reference */
          g_object_unref (G_OBJECT (file));
        }

      /* if uri == NULL, we didn't set the property */
      if (G_UNLIKELY (uri == NULL))
        return FALSE;
    }

  /* set state so the drag-data-received knows that
   * this is really a drop this time.
   */
  standard_view->priv->drop_occurred = TRUE;

  /* request the drag data from the source (initiates
   * saving in case of XdndDirectSave).
   */
  gtk_drag_get_data (view, context, target, timestamp);

  /* we'll call gtk_drag_finish() later */
  return TRUE;
}



static void
tsv_reload_directory (GPid     pid,
                      gint     status,
                      gpointer user_data)
{
  GFileMonitor *monitor;
  GFile        *file;

  /* determine the path for the directory */
  file = g_file_new_for_uri (user_data);

  /* schedule a changed event for the directory */
  monitor = g_file_monitor (file, G_FILE_MONITOR_NONE, NULL, NULL);
  if (monitor != NULL)
    {
      g_file_monitor_emit_event (monitor, file, NULL, G_FILE_MONITOR_EVENT_CHANGED);
      g_object_unref (monitor);
    }

  g_object_unref (file);
}



static gboolean
thunar_standard_view_receive_xdnd_direct_save (GdkDragContext     *context,
                                               gint                x,
                                               gint                y,
                                               GtkSelectionData   *selection_data,
                                               ThunarStandardView *standard_view)
{
  gint         format;
  gint         length;
  ThunarFolder *folder;
  ThunarFile   *file = NULL;

  format = gtk_selection_data_get_format (selection_data);
  length = gtk_selection_data_get_length (selection_data);

  /* we don't handle XdndDirectSave stage (3), result "F" yet */
  if (G_UNLIKELY (format == 8 && length == 1 && gtk_selection_data_get_data (selection_data)[0] == 'F'))
    {
      /* indicate that we don't provide "F" fallback */
      gdk_property_change (gdk_drag_context_get_source_window (context),
                            gdk_atom_intern_static_string ("XdndDirectSave0"),
                            gdk_atom_intern_static_string ("text/plain"), 8,
                            GDK_PROP_MODE_REPLACE, (const guchar *) "", 0);
    }
  else if (G_LIKELY (format == 8 && length == 1 && gtk_selection_data_get_data (selection_data)[0] == 'S'))
    {
      /* XDS was successfull, so determine the file for the drop position */
      file = thunar_standard_view_get_drop_file (standard_view, x, y, NULL);
      if (G_LIKELY (file != NULL))
        {
          /* verify that we have a directory here */
          if (thunar_file_is_directory (file))
            {
              /* reload the folder corresponding to the file */
              folder = thunar_folder_get_for_file (file);
              thunar_folder_reload (folder, FALSE);
              g_object_unref (G_OBJECT (folder));
            }

          /* cleanup */
          g_object_unref (G_OBJECT (file));
        }
    }

    /* in either case, we succeed! */
    return TRUE;
}



static gboolean
thunar_standard_view_receive_netscape_url (GtkWidget          *view,
                                           GdkDragContext     *context,
                                           gint                x,
                                           gint                y,
                                           GtkSelectionData   *selection_data,
                                           ThunarStandardView *standard_view)
{
  gint         format;
  gint         length;
  gchar      **bits;
  ThunarFile  *file = NULL;
  gchar       *working_directory;
  gchar       *argv[11];
  gint         pid;
  gint         n = 0;
  GError      *error = NULL;
  GtkWidget   *toplevel;
  GdkScreen   *screen;
  char        *display = NULL;
  gboolean     succeed = FALSE;

  format = gtk_selection_data_get_format (selection_data);
  length = gtk_selection_data_get_length (selection_data);

  /* check if the format is valid and we have any data */
  if (G_UNLIKELY (format != 8 || length <= 0))
    return succeed;

  /* _NETSCAPE_URL looks like this: "$URL\n$TITLE" */
  bits = g_strsplit ((const gchar *) gtk_selection_data_get_data (selection_data), "\n", -1);
  if (G_UNLIKELY (g_strv_length (bits) != 2))
    {
      g_strfreev (bits);
      return succeed;
    }

  /* determine the file for the drop position */
  file = thunar_standard_view_get_drop_file (standard_view, x, y, NULL);
  if (G_LIKELY (file != NULL))
    {
      /* determine the absolute path to the target directory */
      working_directory = g_file_get_path (thunar_file_get_file (file));
      if (G_LIKELY (working_directory != NULL))
        {
          /* prepare the basic part of the command */
          argv[n++] = "exo-desktop-item-edit";
          argv[n++] = "--type=Link";
          argv[n++] = "--url";
          argv[n++] = bits[0];
          argv[n++] = "--name";
          argv[n++] = bits[1];

          /* determine the toplevel window */
          toplevel = gtk_widget_get_toplevel (view);
          if (toplevel != NULL && gtk_widget_is_toplevel (toplevel))
            {
#if defined(GDK_WINDOWING_X11)
              /* on X11, we can supply the parent window id here */
              argv[n++] = "--xid";
              argv[n++] = g_newa (gchar, 32);
              g_snprintf (argv[n - 1], 32, "%ld", (glong) GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (toplevel))));
#endif
            }

          /* terminate the parameter list */
          argv[n++] = "--create-new";
          argv[n++] = working_directory;
          argv[n++] = NULL;

          screen = gtk_widget_get_screen (GTK_WIDGET (view));

          if (screen != NULL)
            display = g_strdup (gdk_display_get_name (gdk_screen_get_display (screen)));

          /* try to run exo-desktop-item-edit */
          succeed = g_spawn_async (working_directory, argv, NULL,
                                    G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH,
                                    thunar_setup_display_cb, display, &pid, &error);

          if (G_LIKELY (succeed))
            {
              /* reload the directory when the command terminates */
              g_child_watch_add_full (G_PRIORITY_LOW, pid, tsv_reload_directory, working_directory, g_free);
            }
          else
            {
              /* display an error dialog to the user */
              thunar_dialogs_show_error (standard_view, error, _("Failed to create a link for the URL \"%s\""), bits[0]);
              g_free (working_directory);
              g_error_free (error);
            }

          g_free (display);
        }
      g_object_unref (G_OBJECT (file));
    }

  g_strfreev (bits);

  return succeed;
}



static gboolean
thunar_standard_view_receive_application_octet_stream (GdkDragContext     *context,
                                                       gint                x,
                                                       gint                y,
                                                       GtkSelectionData   *selection_data,
                                                       ThunarStandardView *standard_view)
{
  ThunarFile        *file = NULL;
  gchar             *filename;
  gchar             *filepath = NULL;
  const gchar       *content;
  gint               length;
  GFile             *dest;
  GFileOutputStream *out;

  if (gtk_selection_data_get_length (selection_data) <= 0)
    return FALSE;

  /* determine filename */
  if (gdk_property_get (gdk_drag_context_get_source_window (context),
                        gdk_atom_intern ("XdndDirectSave0", FALSE),
                        gdk_atom_intern ("text/plain", FALSE), 0, 1024,
                        FALSE, NULL, NULL, &length,
                        (guchar **) &filename) && length > 0)
    {
      filename = g_realloc (filename, length + 1);
      filename[length] = '\0';
    }
  else
      filename = g_strdup (_("Untitled document"));

  /* determine filepath */
  file = thunar_standard_view_get_drop_file (standard_view, x, y, NULL);
  if (G_LIKELY (file != NULL))
    {
      if (thunar_file_is_directory (file))
        {
          gchar* filename_temp;
          gchar* folder_path;

          folder_path = g_file_get_path (thunar_file_get_file (file));
          filename_temp = thunar_util_next_new_file_name (file, filename, THUNAR_NEXT_FILE_NAME_MODE_COPY);
          g_free (filename);
          filename = filename_temp;
          filepath = g_build_filename (folder_path, filename, NULL);

          g_free (folder_path);
        }

      g_object_unref (G_OBJECT (file));
    }

  if (G_LIKELY (filepath != NULL))
    {
      dest = g_file_new_for_path (filepath);
      out = g_file_create (dest, G_FILE_CREATE_NONE, NULL, NULL);

      if (out)
        {
          content = (const gchar *) gtk_selection_data_get_data (selection_data);
          length = gtk_selection_data_get_length (selection_data);

          if (g_output_stream_write_all (G_OUTPUT_STREAM (out), content, length, NULL, NULL, NULL))
            g_output_stream_close (G_OUTPUT_STREAM (out), NULL, NULL);

          g_object_unref(out);
        }

      g_object_unref (dest);
    }

  g_free (filename);
  g_free (filepath);

  return TRUE;
}



static gboolean
thunar_standard_view_receive_text_uri_list (GdkDragContext     *context,
                                            gint                x,
                                            gint                y,
                                            GtkSelectionData   *selection_data,
                                            guint               timestamp,
                                            ThunarStandardView *standard_view)
{
  GdkDragAction actions;
  GdkDragAction action;
  ThunarFile   *file = NULL;
  gboolean      succeed = FALSE;
  GtkWidget    *source_widget;
  GtkWidget    *source_view = NULL;

  /* determine the drop position */
  actions = thunar_standard_view_get_dest_actions (standard_view, context, x, y, timestamp, &file);
  if (G_LIKELY ((actions & (GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK)) != 0))
    {
      /* ask the user what to do with the drop data */
      action = (gdk_drag_context_get_selected_action (context) == GDK_ACTION_ASK)
              ? thunar_dnd_ask (GTK_WIDGET (standard_view), file, standard_view->priv->drop_file_list, actions)
              : gdk_drag_context_get_selected_action (context);

      /* perform the requested action */
      if (G_LIKELY (action != 0))
        {
          /* look if we can find the drag source widget */
          source_widget = gtk_drag_get_source_widget (context);
          if (source_widget != NULL)
            {
              /* if this is a source view, attach it to the view receiving
                * the data, see thunar_standard_view_new_files */
              source_view = gtk_widget_get_parent (source_widget);
              if (!THUNAR_IS_VIEW (source_view))
                source_view = NULL;
            }

          succeed = thunar_dnd_perform (GTK_WIDGET (standard_view), file, standard_view->priv->drop_file_list,
                                        action, thunar_standard_view_new_files_closure (standard_view, source_view));
        }
    }

  /* release the file reference */
  if (G_LIKELY (file != NULL))
    g_object_unref (G_OBJECT (file));

  return succeed;
}



static void
thunar_standard_view_drag_data_received (GtkWidget          *view,
                                         GdkDragContext     *context,
                                         gint                x,
                                         gint                y,
                                         GtkSelectionData   *selection_data,
                                         guint               info,
                                         guint               timestamp,
                                         ThunarStandardView *standard_view)
{
  gboolean succeed = FALSE;

  /* check if we don't already know the drop data */
  if (G_LIKELY (!standard_view->priv->drop_data_ready))
    {
      /* extract the URI list from the selection data (if valid) */
      if (info == TARGET_TEXT_URI_LIST && gtk_selection_data_get_format (selection_data) == 8 && gtk_selection_data_get_length (selection_data) > 0)
        standard_view->priv->drop_file_list = thunar_g_file_list_new_from_string ((gchar *) gtk_selection_data_get_data (selection_data));

      /* reset the state */
      standard_view->priv->drop_data_ready = TRUE;
    }

  /* check if the data was dropped */
  if (G_UNLIKELY (standard_view->priv->drop_occurred))
    {
      /* reset the state */
      standard_view->priv->drop_occurred = FALSE;

      /* check if we're doing XdndDirectSave */
      if (G_UNLIKELY (info == TARGET_XDND_DIRECT_SAVE0))
        succeed = thunar_standard_view_receive_xdnd_direct_save (context, x, y, selection_data, standard_view);
      else if (G_UNLIKELY (info == TARGET_NETSCAPE_URL))
        succeed = thunar_standard_view_receive_netscape_url (view, context, x, y, selection_data, standard_view);
      else if (G_UNLIKELY (info == TARGET_APPLICATION_OCTET_STREAM))
        succeed = thunar_standard_view_receive_application_octet_stream (context, x, y, selection_data, standard_view);
      else if (G_LIKELY (info == TARGET_TEXT_URI_LIST))
        succeed = thunar_standard_view_receive_text_uri_list (context, x, y, selection_data, timestamp, standard_view);

      /* tell the peer that we handled the drop */
      gtk_drag_finish (context, succeed, FALSE, timestamp);

      /* disable the highlighting and release the drag data */
      thunar_standard_view_drag_leave (view, context, timestamp, standard_view);
    }
}



static void
thunar_standard_view_drag_leave (GtkWidget          *widget,
                                 GdkDragContext     *context,
                                 guint               timestamp,
                                 ThunarStandardView *standard_view)
{
  /* reset the drop-file for the icon renderer */
  g_object_set (G_OBJECT (standard_view->icon_renderer), "drop-file", NULL, NULL);

  /* stop any running drag autoscroll timer */
  if (G_UNLIKELY (standard_view->priv->drag_scroll_timer_id != 0))
    g_source_remove (standard_view->priv->drag_scroll_timer_id);

  /* disable the drop highlighting around the view */
  if (G_LIKELY (standard_view->priv->drop_highlight))
    {
      standard_view->priv->drop_highlight = FALSE;
      gtk_widget_queue_draw (GTK_WIDGET (standard_view));
    }

  /* reset the "drop data ready" status and free the URI list */
  if (G_LIKELY (standard_view->priv->drop_data_ready))
    {
      thunar_g_list_free_full (standard_view->priv->drop_file_list);
      standard_view->priv->drop_file_list = NULL;
      standard_view->priv->drop_data_ready = FALSE;
    }

  /* disable the highlighting of the items in the view */
  (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->highlight_path) (standard_view, NULL);
}



static gboolean
thunar_standard_view_drag_motion (GtkWidget          *view,
                                  GdkDragContext     *context,
                                  gint                x,
                                  gint                y,
                                  guint               timestamp,
                                  ThunarStandardView *standard_view)
{
  GdkDragAction action = 0;
  GtkTreePath  *path;
  ThunarFile   *file = NULL;
  GdkAtom       target;

  /* request the drop data on-demand (if we don't have it already) */
  if (G_UNLIKELY (!standard_view->priv->drop_data_ready))
    {
      /* check if we can handle that drag data (yet?) */
      target = gtk_drag_dest_find_target (view, context, NULL);

      if (target == gdk_atom_intern_static_string ("XdndDirectSave0") ||
          target == gdk_atom_intern_static_string ("_NETSCAPE_URL") ||
          target == gdk_atom_intern_static_string ("application/octet-stream"))
        {
          /* determine the file for the given coordinates */
          file = thunar_standard_view_get_drop_file (standard_view, x, y, &path);

          /* check if we can save here */
          if (G_LIKELY (file != NULL
                        && thunar_file_is_local (file)
                        && thunar_file_is_directory (file)
                        && thunar_file_is_writable (file)))
            {
              action = gdk_drag_context_get_suggested_action (context);
            }

          /* reset path if we cannot drop */
          if (G_UNLIKELY (action == 0 && path != NULL))
            {
              gtk_tree_path_free (path);
              path = NULL;
            }

          /* do the view highlighting */
          if (standard_view->priv->drop_highlight != (path == NULL && action != 0))
            {
              standard_view->priv->drop_highlight = (path == NULL && action != 0);
              gtk_widget_queue_draw (GTK_WIDGET (standard_view));
            }

          /* setup drop-file for the icon renderer to highlight the target */
          g_object_set (G_OBJECT (standard_view->icon_renderer), "drop-file", (action != 0) ? file : NULL, NULL);

          /* do the item highlighting */
          (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->highlight_path) (standard_view, path);

          /* cleanup */
          if (G_LIKELY (file != NULL))
            g_object_unref (G_OBJECT (file));
          if (G_LIKELY (path != NULL))
            gtk_tree_path_free (path);
        }
      else
        {
          /* request the drag data from the source */
          if (target != GDK_NONE)
            gtk_drag_get_data (view, context, target, timestamp);
        }

      /* tell Gdk whether we can drop here */
      gdk_drag_status (context, action, timestamp);
    }
  else
    {
      /* check whether we can drop at (x,y) */
      thunar_standard_view_get_dest_actions (standard_view, context, x, y, timestamp, NULL);
    }

  /* start the drag autoscroll timer if not already running */
  if (G_UNLIKELY (standard_view->priv->drag_scroll_timer_id == 0))
    {
      /* schedule the drag autoscroll timer */
      standard_view->priv->drag_scroll_timer_id = g_timeout_add_full (G_PRIORITY_LOW, 50, thunar_standard_view_drag_scroll_timer,
                                                                      standard_view, thunar_standard_view_drag_scroll_timer_destroy);
    }

  return TRUE;
}



static void
thunar_standard_view_drag_begin (GtkWidget          *view,
                                 GdkDragContext     *context,
                                 ThunarStandardView *standard_view)
{
  ThunarFile      *file;
  GdkPixbuf       *icon;
  cairo_surface_t *surface;
  gint             size;
  gint             scale_factor;

  /* release the drag path list (just in case the drag-end wasn't fired before) */
  thunar_g_list_free_full (standard_view->priv->drag_g_file_list);

  /* query the list of selected URIs */
  standard_view->priv->drag_g_file_list = thunar_file_list_to_thunar_g_file_list (standard_view->priv->selected_files);
  if (G_LIKELY (standard_view->priv->drag_g_file_list != NULL))
    {
      /* determine the first selected file */
      file = thunar_file_get (standard_view->priv->drag_g_file_list->data, NULL);
      if (G_LIKELY (file != NULL))
        {
          /* generate an icon based on that file */
          g_object_get (G_OBJECT (standard_view->icon_renderer), "size", &size, NULL);
          scale_factor = gtk_widget_get_scale_factor (view);
          icon = thunar_icon_factory_load_file_icon (standard_view->icon_factory, file, THUNAR_FILE_ICON_STATE_DEFAULT, size * scale_factor);
          if (G_LIKELY (icon != NULL))
            {
              surface = gdk_cairo_surface_create_from_pixbuf (icon, scale_factor, gtk_widget_get_window (view));
              g_object_unref (G_OBJECT (icon));
              gtk_drag_set_icon_surface (context, surface);
              cairo_surface_destroy (surface);
            }

          /* release the file */
          g_object_unref (G_OBJECT (file));
        }
    }
}



static void
thunar_standard_view_drag_data_get (GtkWidget          *view,
                                    GdkDragContext     *context,
                                    GtkSelectionData   *selection_data,
                                    guint               info,
                                    guint               timestamp,
                                    ThunarStandardView *standard_view)
{
  gchar **uris;

  /* set the URI list for the drag selection */
  if (standard_view->priv->drag_g_file_list != NULL)
    {
      uris = thunar_g_file_list_to_stringv (standard_view->priv->drag_g_file_list);
      gtk_selection_data_set_uris (selection_data, uris);
      g_strfreev (uris);
    }
}



static void
thunar_standard_view_drag_data_delete (GtkWidget          *view,
                                       GdkDragContext     *context,
                                       ThunarStandardView *standard_view)
{
  /* make sure the default handler of ExoIconView/GtkTreeView is never run */
  g_signal_stop_emission_by_name (G_OBJECT (view), "drag-data-delete");
}



static void
thunar_standard_view_drag_end (GtkWidget          *view,
                               GdkDragContext     *context,
                               ThunarStandardView *standard_view)
{
  /* stop any running drag autoscroll timer */
  if (G_UNLIKELY (standard_view->priv->drag_scroll_timer_id != 0))
    g_source_remove (standard_view->priv->drag_scroll_timer_id);

  /* release the list of dragged URIs */
  thunar_g_list_free_full (standard_view->priv->drag_g_file_list);
  standard_view->priv->drag_g_file_list = NULL;
}



static gboolean
thunar_standard_view_restore_selection_idle (gpointer user_data)
{
  ThunarStandardView *standard_view = user_data;
  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;
  gdouble        h, v, hl, hu, vl, vu;

  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), FALSE);

  /* save the current scroll position and limits */
  hadjustment = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (standard_view));
  vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (standard_view));
  g_object_get (G_OBJECT (hadjustment), "value", &h, "lower", &hl, "upper", &hu, NULL);
  g_object_get (G_OBJECT (vadjustment), "value", &v, "lower", &vl, "upper", &vu, NULL);

  /* keep the current scroll position by setting the limits to the current value */
  g_object_set (G_OBJECT (hadjustment), "lower", h, "upper", h, NULL);
  g_object_set (G_OBJECT (vadjustment), "lower", v, "upper", v, NULL);

  /* restore the selection */
  thunar_component_restore_selection (THUNAR_COMPONENT (standard_view));
  standard_view->priv->restore_selection_idle_id = 0;

  /* unfreeze the scroll position */
  g_object_set (G_OBJECT (hadjustment), "value", h, "lower", hl, "upper", hu, NULL);
  g_object_set (G_OBJECT (vadjustment), "value", v, "lower", vl, "upper", vu, NULL);

  return FALSE;
}



static void
thunar_standard_view_rows_reordered (ThunarListModel    *model,
                                     GtkTreePath        *path,
                                     GtkTreeIter        *iter,
                                     gpointer            new_order,
                                     ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (model));
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  _thunar_return_if_fail (standard_view->model == model);

  /* ignore while searching */
  if (standard_view->priv->active_search == TRUE)
    return;

  /* the order of the paths might have changed, but the selection
   * stayed the same, so restore the selection of the proper files
   * after letting row changes accumulate a bit */
  if (standard_view->priv->restore_selection_idle_id == 0)
    standard_view->priv->restore_selection_idle_id =
      g_timeout_add (50, thunar_standard_view_restore_selection_idle, standard_view);
}



static void
thunar_standard_view_row_changed (ThunarListModel    *model,
                                  GtkTreePath        *path,
                                  GtkTreeIter        *iter,
                                  ThunarStandardView *standard_view)
{
  ThunarFile *file;

  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (model));
  _thunar_return_if_fail (path != NULL);
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  _thunar_return_if_fail (standard_view->model == model);

  /* ignore while searching */
  if (standard_view->priv->active_search == TRUE)
    return;

  if (standard_view->priv->thumbnail_request != 0)
    return;

  /* leave if this view is not suitable for generating thumbnails */
  if (!thunar_icon_factory_get_show_thumbnail (standard_view->icon_factory,
                                               standard_view->priv->current_directory))
    return;

  /* queue a thumbnail request */
  file = thunar_list_model_get_file (standard_view->model, iter);
  if (thunar_file_get_thumb_state (file) == THUNAR_FILE_THUMB_STATE_UNKNOWN)
    {
      thunar_standard_view_cancel_thumbnailing (standard_view);
      thunar_thumbnailer_queue_file (standard_view->priv->thumbnailer, file,
                                     &standard_view->priv->thumbnail_request, THUNAR_THUMBNAIL_SIZE_DEFAULT);
    }
  g_object_unref (G_OBJECT (file));
}



static void
thunar_standard_view_select_after_row_deleted (ThunarListModel    *model,
                                               GtkTreePath        *path,
                                               ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (path != NULL);
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* ignore while searching */
  if (standard_view->priv->active_search == TRUE)
    return;

  /* ignore if we have selected files */
  if (standard_view->priv->selected_files != NULL)
    return;

  /* select the path */
  (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->select_path) (standard_view, path);

  /* place the cursor on the first selected path (must be first for GtkTreeView) */
  (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->set_cursor) (standard_view, path, FALSE);
}



static void
thunar_standard_view_error (ThunarListModel    *model,
                            const GError       *error,
                            ThunarStandardView *standard_view)
{
  ThunarFile *file;

  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (model));
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  _thunar_return_if_fail (standard_view->model == model);

  /* determine the ThunarFile for the current directory */
  file = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));
  if (G_UNLIKELY (file == NULL))
    return;

  /* inform the user about the problem */
  thunar_dialogs_show_error (GTK_WIDGET (standard_view), error,
                             _("Failed to open directory \"%s\""),
                             thunar_file_get_display_name (file));
}



static void
thunar_standard_view_search_done (ThunarListModel    *model,
                                  ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (model));
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  _thunar_return_if_fail (standard_view->model == model);

  standard_view->priv->active_search = FALSE;

  /* notify listeners */
  g_object_notify_by_pspec (G_OBJECT (standard_view), standard_view_props[PROP_SEARCHING]);
}



static void
thunar_standard_view_sort_column_changed (GtkTreeSortable    *tree_sortable,
                                          ThunarStandardView *standard_view)
{
  GtkSortType      sort_order;
  gint             sort_column;

  _thunar_return_if_fail (GTK_IS_TREE_SORTABLE (tree_sortable));
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* keep the currently selected files selected after the change */
  thunar_component_restore_selection (THUNAR_COMPONENT (standard_view));

  /* determine the new sort column and sort order, and save them */
  if (gtk_tree_sortable_get_sort_column_id (tree_sortable, &sort_column, &sort_order))
    {
      thunar_standard_view_store_sort_column (standard_view);

      if (standard_view->priv->directory_specific_settings)
        {
          const gchar *sort_column_name;
          const gchar *sort_order_name;

          /* save the sort column name */
          sort_column_name = thunar_column_string_from_value (sort_column);
          if (sort_column_name != NULL)
            thunar_file_set_metadata_setting (standard_view->priv->current_directory, "sort-column", sort_column_name, TRUE);

          /* convert the sort order to a string */
          if (sort_order == GTK_SORT_ASCENDING)
            sort_order_name = "GTK_SORT_ASCENDING";
          if (sort_order == GTK_SORT_DESCENDING)
            sort_order_name = "GTK_SORT_DESCENDING";

          /* save the sort order */
          thunar_file_set_metadata_setting (standard_view->priv->current_directory, "sort-order", sort_order_name, TRUE);
        }
      else
        {
          /* remember the new values as default */
          g_object_set (G_OBJECT (standard_view->preferences),
                        "last-sort-column", sort_column,
                        "last-sort-order", sort_order,
                        NULL);
        }
    }
}



static void
thunar_standard_view_loading_unbound (gpointer user_data)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (user_data);

  /* we don't have any binding now */
  standard_view->loading_binding = NULL;

  /* reset the "loading" property */
  if (G_UNLIKELY (standard_view->loading))
    {
      standard_view->loading = FALSE;
      g_object_freeze_notify (G_OBJECT (standard_view));
      g_object_notify_by_pspec (G_OBJECT (standard_view), standard_view_props[PROP_LOADING]);
      thunar_standard_view_update_statusbar_text (standard_view);
      g_object_thaw_notify (G_OBJECT (standard_view));
    }
}



static gboolean
thunar_standard_view_drag_scroll_timer (gpointer user_data)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (user_data);
  GtkAdjustment      *adjustment;
  GdkWindow          *window;
  GdkSeat            *seat;
  GdkDevice          *pointer;
  gfloat              value;
  gint                offset;
  gint                y, x;
  gint                w, h;

THUNAR_THREADS_ENTER

  /* verify that we are realized */
  if (G_LIKELY (gtk_widget_get_realized (GTK_WIDGET (standard_view))))
    {
      /* determine pointer location and window geometry */
      window = gtk_widget_get_window (gtk_bin_get_child (GTK_BIN (standard_view)));
      seat = gdk_display_get_default_seat (gdk_display_get_default ());
      pointer = gdk_seat_get_pointer (seat);

      gdk_window_get_device_position (window, pointer, &x, &y, NULL);
      gdk_window_get_geometry (window, NULL, NULL, &w, &h);

      /* check if we are near the edge (vertical) */
      offset = y - (2 * 20);
      if (G_UNLIKELY (offset > 0))
        offset = MAX (y - (h - 2 * 20), 0);

      /* change the vertical adjustment appropriately */
      if (G_UNLIKELY (offset != 0))
        {
          /* determine the vertical adjustment */
          adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (standard_view));

          /* determine the new value */
          value = CLAMP (gtk_adjustment_get_value (adjustment) + 2 * offset, gtk_adjustment_get_lower (adjustment), gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_page_size (adjustment));

          /* apply the new value */
          gtk_adjustment_set_value (adjustment, value);
        }

      /* check if we are near the edge (horizontal) */
      offset = x - (2 * 20);
      if (G_UNLIKELY (offset > 0))
        offset = MAX (x - (w - 2 * 20), 0);

      /* change the horizontal adjustment appropriately */
      if (G_UNLIKELY (offset != 0))
        {
          /* determine the vertical adjustment */
          adjustment = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (standard_view));

          /* determine the new value */
          value = CLAMP (gtk_adjustment_get_value (adjustment) + 2 * offset, gtk_adjustment_get_lower (adjustment), gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_page_size (adjustment));

          /* apply the new value */
          gtk_adjustment_set_value (adjustment, value);
        }
    }

THUNAR_THREADS_LEAVE

  return TRUE;
}



static void
thunar_standard_view_drag_scroll_timer_destroy (gpointer user_data)
{
  THUNAR_STANDARD_VIEW (user_data)->priv->drag_scroll_timer_id = 0;
}



static gboolean
thunar_standard_view_drag_timer (gpointer user_data)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (user_data);

  /* fire up the context menu */
THUNAR_THREADS_ENTER
  thunar_standard_view_context_menu (standard_view);
THUNAR_THREADS_LEAVE

  return FALSE;
}



static void
thunar_standard_view_drag_timer_destroy (gpointer user_data)
{
  /* unregister the motion notify and button release event handlers (thread-safe) */
  g_signal_handlers_disconnect_by_func (gtk_bin_get_child (GTK_BIN (user_data)), thunar_standard_view_button_release_event, user_data);
  g_signal_handlers_disconnect_by_func (gtk_bin_get_child (GTK_BIN (user_data)), thunar_standard_view_motion_notify_event, user_data);

  /* reset the drag timer source id */
  THUNAR_STANDARD_VIEW (user_data)->priv->drag_timer_id = 0;
}



static void
thunar_standard_view_finished_thumbnailing (ThunarThumbnailer  *thumbnailer,
                                            guint               request,
                                            ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  if (standard_view->priv->thumbnail_request == request)
    standard_view->priv->thumbnail_request = 0;
}



static void
thunar_standard_view_thumbnailing_destroyed (gpointer data)
{
  ThunarStandardView *standard_view = data;

  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  standard_view->priv->thumbnail_source_id = 0;
}



static void
thunar_standard_view_cancel_thumbnailing (ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* check if we have a pending thumbnail timeout/idle handler */
  if (standard_view->priv->thumbnail_source_id > 0)
    g_source_remove (standard_view->priv->thumbnail_source_id);

  /* check if we have a pending thumbnail request */
  if (standard_view->priv->thumbnail_request > 0)
    {
      /* cancel the request */
      thunar_thumbnailer_dequeue (standard_view->priv->thumbnailer,
                                  standard_view->priv->thumbnail_request);
      standard_view->priv->thumbnail_request = 0;
    }
}



static void
thunar_standard_view_schedule_thumbnail_timeout (ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* delay creating the idle until the view has finished loading.
   * this is done because we only can tell the visible range reliably after
   * all items have been added and we've perhaps scrolled to the file remember
   * the last time */
  if (thunar_view_get_loading (THUNAR_VIEW (standard_view)))
    {
      standard_view->priv->thumbnailing_scheduled = TRUE;
      return;
    }

  /* cancel any pending thumbnail sources and requests */
  thunar_standard_view_cancel_thumbnailing (standard_view);

  /* schedule the timeout handler */
  g_assert (standard_view->priv->thumbnail_source_id == 0);
  standard_view->priv->thumbnail_source_id =
    g_timeout_add_full (G_PRIORITY_DEFAULT, 175, thunar_standard_view_request_thumbnails_lazy,
                        standard_view, thunar_standard_view_thumbnailing_destroyed);
}



static void
thunar_standard_view_schedule_thumbnail_idle (ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* delay creating the idle until the view has finished loading.
   * this is done because we only can tell the visible range reliably after
   * all items have been added, layouting has finished and we've perhaps
   * scrolled to the file remembered the last time */
  if (thunar_view_get_loading (THUNAR_VIEW (standard_view)))
    {
      standard_view->priv->thumbnailing_scheduled = TRUE;
      return;
    }

  /* cancel any pending thumbnail sources or requests */
  thunar_standard_view_cancel_thumbnailing (standard_view);

  /* schedule the timeout or idle handler */
  g_assert (standard_view->priv->thumbnail_source_id == 0);
  standard_view->priv->thumbnail_source_id =
    g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, thunar_standard_view_request_thumbnails,
                     standard_view, thunar_standard_view_thumbnailing_destroyed);
}



static gboolean
thunar_standard_view_request_thumbnails_real (ThunarStandardView *standard_view,
                                              gboolean            lazy_request)
{
  GtkTreePath *start_path;
  GtkTreePath *end_path;
  GtkTreePath *path;
  GtkTreeIter  iter;
  ThunarFile  *file;
  gboolean     valid_iter;
  GList       *visible_files = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_ICON_FACTORY (standard_view->icon_factory), FALSE);

  /* do nothing if we are not supposed to show thumbnails at all */
  if (!thunar_icon_factory_get_show_thumbnail (standard_view->icon_factory,
                                               standard_view->priv->current_directory))
    return FALSE;

  /* reschedule the source if we're still loading the folder */
  if (thunar_view_get_loading (THUNAR_VIEW (standard_view)))
    return TRUE;

  /* do nothing if we are already loading thumbnails */
  if (standard_view->priv->thumbnail_request != 0)
    return FALSE;

  /* compute visible item range */
  if ((*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->get_visible_range) (standard_view,
                                                                            &start_path,
                                                                            &end_path))
    {
      /* iterate over the range to collect all files */
      valid_iter = gtk_tree_model_get_iter (GTK_TREE_MODEL (standard_view->model),
                                            &iter, start_path);

      while (valid_iter)
        {
          /* prepend the file to the visible items list */
          file = thunar_list_model_get_file (standard_view->model, &iter);
          visible_files = g_list_prepend (visible_files, file);

          /* check if we've reached the end of the visible range */
          path = gtk_tree_model_get_path (GTK_TREE_MODEL (standard_view->model), &iter);
          if (gtk_tree_path_compare (path, end_path) != 0)
            {
              /* try to compute the next visible item */
              valid_iter =
                gtk_tree_model_iter_next (GTK_TREE_MODEL (standard_view->model), &iter);
            }
          else
            {
              /* we have reached the end, terminate the loop */
              valid_iter = FALSE;
            }

          /* release the tree path */
          gtk_tree_path_free (path);
        }

      /* queue a thumbnail request */
      thunar_thumbnailer_queue_files (standard_view->priv->thumbnailer,
                                      lazy_request, visible_files,
                                      &standard_view->priv->thumbnail_request,
                                      THUNAR_THUMBNAIL_SIZE_DEFAULT);

      /* release the file list */
      g_list_free_full (visible_files, g_object_unref);

      /* release the start and end path */
      gtk_tree_path_free (start_path);
      gtk_tree_path_free (end_path);
    }

  return FALSE;
}



static gboolean
thunar_standard_view_request_thumbnails (gpointer data)
{
  return thunar_standard_view_request_thumbnails_real (data, FALSE);
}



static gboolean
thunar_standard_view_request_thumbnails_lazy (gpointer data)
{
  return thunar_standard_view_request_thumbnails_real (data, TRUE);
}



static void
thunar_standard_view_thumbnail_mode_toggled (ThunarStandardView *standard_view,
                                             GParamSpec         *pspec,
                                             ThunarIconFactory  *icon_factory)
{
  GtkAdjustment *vadjustment;

  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  _thunar_return_if_fail (THUNAR_IS_ICON_FACTORY (icon_factory));
  _thunar_return_if_fail (standard_view->icon_factory == icon_factory);

  /* check whether the user wants us to generate thumbnails */
  if (thunar_icon_factory_get_show_thumbnail (icon_factory,
                                              standard_view->priv->current_directory))
    {
      /* get the vertical adjustment of the view */
      vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (standard_view));

      /* fake a scroll event to generate thumbnail requests */
      thunar_standard_view_scrolled (vadjustment, standard_view);
    }
  else
    {
      /* cancel any pending thumbnail requests */
      thunar_standard_view_cancel_thumbnailing (standard_view);
    }
}



static void
thunar_standard_view_scrolled (GtkAdjustment      *adjustment,
                               ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* ignore adjustment changes when the view is still loading */
  if (thunar_view_get_loading (THUNAR_VIEW (standard_view)))
    return;

  /* Try to load thumbnails for files which are now visible */
  thunar_standard_view_request_thumbnails_real (standard_view, TRUE);
}



static void
thunar_standard_view_size_allocate (ThunarStandardView *standard_view,
                                    GtkAllocation      *allocation)
{
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* ignore size changes when the view is still loading */
  if (thunar_view_get_loading (THUNAR_VIEW (standard_view)))
    return;

  /* Try to load thumbnails for files which are now visible */
  thunar_standard_view_request_thumbnails_real (standard_view, TRUE);
}



/**
 * thunar_standard_view_context_menu:
 * @standard_view : a #ThunarStandardView instance.
 *
 * Invoked by derived classes (and only by derived classes!) whenever the user
 * requests to open a context menu, e.g. by right-clicking on a file/folder or by
 * using one of the context menu shortcuts.
 **/
void
thunar_standard_view_context_menu (ThunarStandardView *standard_view)
{
  GtkWidget  *window;
  ThunarMenu *context_menu;
  GList      *selected_items;

  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* grab an additional reference on the view */
  g_object_ref (G_OBJECT (standard_view));

  selected_items = (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->get_selected_items) (standard_view);

  window = gtk_widget_get_toplevel (GTK_WIDGET (standard_view));

  context_menu = g_object_new (THUNAR_TYPE_MENU, "menu-type", THUNAR_MENU_TYPE_CONTEXT_STANDARD_VIEW,
                                                 "action_mgr", thunar_window_get_action_manager (THUNAR_WINDOW (window)), NULL);
  if (selected_items != NULL)
    {
      thunar_menu_add_sections (context_menu, THUNAR_MENU_SECTION_OPEN
                                            | THUNAR_MENU_SECTION_EDIT_LAUNCHER
                                            | THUNAR_MENU_SECTION_SENDTO
                                            | THUNAR_MENU_SECTION_CUT
                                            | THUNAR_MENU_SECTION_COPY_PASTE
                                            | THUNAR_MENU_SECTION_TRASH_DELETE
                                            | THUNAR_MENU_SECTION_EMPTY_TRASH
                                            | THUNAR_MENU_SECTION_REMOVE_FROM_RECENT
                                            | THUNAR_MENU_SECTION_RESTORE
                                            | THUNAR_MENU_SECTION_RENAME
                                            | THUNAR_MENU_SECTION_CUSTOM_ACTIONS
                                            | THUNAR_MENU_SECTION_PROPERTIES);
    }
  else /* right click on some empty space */
    {
      thunar_menu_add_sections (context_menu, THUNAR_MENU_SECTION_CREATE_NEW_FILES
                                            | THUNAR_MENU_SECTION_COPY_PASTE
                                            | THUNAR_MENU_SECTION_EMPTY_TRASH
                                            | THUNAR_MENU_SECTION_CUSTOM_ACTIONS);
      thunar_standard_view_append_menu_items (standard_view, GTK_MENU (context_menu), NULL);
      xfce_gtk_menu_append_separator (GTK_MENU_SHELL (context_menu));
      thunar_menu_add_sections (context_menu, THUNAR_MENU_SECTION_ZOOM
                                            | THUNAR_MENU_SECTION_PROPERTIES);
    }
  thunar_gtk_menu_hide_accel_labels (GTK_MENU (context_menu));
  gtk_widget_show_all (GTK_WIDGET (context_menu));
  thunar_window_redirect_menu_tooltips_to_statusbar (THUNAR_WINDOW (window), GTK_MENU (context_menu));

  /* if there is a drag_timer_event (long press), we use it */
  if (standard_view->priv->drag_timer_event != NULL)
    {
      thunar_gtk_menu_run_at_event (GTK_MENU (context_menu), standard_view->priv->drag_timer_event);
      gdk_event_free (standard_view->priv->drag_timer_event);
      standard_view->priv->drag_timer_event = NULL;
    }
  else
    thunar_gtk_menu_run (GTK_MENU (context_menu));

  g_list_free_full (selected_items, (GDestroyNotify) gtk_tree_path_free);

  /* release the additional reference on the view */
  g_object_unref (G_OBJECT (standard_view));
}



/**
 * thunar_standard_view_queue_popup:
 * @standard_view : a #ThunarStandardView.
 * @event         : the right click event.
 *
 * Schedules a context menu popup in response to
 * a right-click button event. Right-click events
 * need to be handled in a special way, as the
 * user may also start a drag using the right
 * mouse button and therefore this function
 * schedules a timer, which - once expired -
 * opens the context menu. If the user moves
 * the mouse prior to expiration, a right-click
 * drag (with #GDK_ACTION_ASK) will be started
 * instead.
 **/
void
thunar_standard_view_queue_popup (ThunarStandardView *standard_view,
                                  GdkEventButton     *event)
{
  GtkSettings *settings;
  GtkWidget   *view;
  gint         delay;

  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  _thunar_return_if_fail (event != NULL);

  /* check if we have already scheduled a drag timer */
  if (G_LIKELY (standard_view->priv->drag_timer_id == 0))
    {
      /* remember the new coordinates */
      standard_view->priv->drag_x = event->x;
      standard_view->priv->drag_y = event->y;

      /* figure out the real view */
      view = gtk_bin_get_child (GTK_BIN (standard_view));

      /* we use the menu popup delay here, note that we only use this to
       * allow higher values! see bug #3549 */
      settings = gtk_settings_get_for_screen (gtk_widget_get_screen (view));
      g_object_get (G_OBJECT (settings), "gtk-menu-popup-delay", &delay, NULL);

      /* schedule the timer */
      standard_view->priv->drag_timer_id = g_timeout_add_full (G_PRIORITY_LOW, MAX (225, delay), thunar_standard_view_drag_timer,
                                                               standard_view, thunar_standard_view_drag_timer_destroy);
      /* store current event data */
      standard_view->priv->drag_timer_event = gtk_get_current_event ();

      /* register the motion notify and the button release events on the real view */
      g_signal_connect (G_OBJECT (view), "button-release-event", G_CALLBACK (thunar_standard_view_button_release_event), standard_view);
      g_signal_connect (G_OBJECT (view), "motion-notify-event", G_CALLBACK (thunar_standard_view_motion_notify_event), standard_view);
    }
}



/**
 * thunar_standard_view_selection_changed:
 * @standard_view : a #ThunarStandardView instance.
 *
 * Called by derived classes (and only by derived classes!) whenever the file
 * selection changes.
 *
 * Note, that this is also called internally whenever the number of
 * files in the @standard_view<!---->s model changes.
 **/
void
thunar_standard_view_selection_changed (ThunarStandardView *standard_view)
{
  GtkTreeIter iter;
  GList      *lp, *selected_thunar_files;

  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* ignore while searching */
  if (standard_view->priv->active_search == TRUE)
    return;

  /* drop any existing "new-files" closure */
  if (G_UNLIKELY (standard_view->priv->new_files_closure != NULL))
    {
      g_closure_invalidate (standard_view->priv->new_files_closure);
      g_closure_unref (standard_view->priv->new_files_closure);
      standard_view->priv->new_files_closure = NULL;
    }

  /* release the previously selected files */
  thunar_g_list_free_full (standard_view->priv->selected_files);

  /* determine the new list of selected files (replacing GtkTreePath's with ThunarFile's) */
  selected_thunar_files = (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->get_selected_items) (standard_view);
  for (lp = selected_thunar_files; lp != NULL; lp = lp->next)
    {
      /* determine the iterator for the path */
      gtk_tree_model_get_iter (GTK_TREE_MODEL (standard_view->model), &iter, lp->data);

      /* release the tree path... */
      gtk_tree_path_free (lp->data);

      /* ...and replace it with the file */
      lp->data = thunar_list_model_get_file (standard_view->model, &iter);
    }

  /* and setup the new selected files list */
  standard_view->priv->selected_files = selected_thunar_files;

  /* update the statusbar text */
  thunar_standard_view_update_statusbar_text (standard_view);

  /* emit notification for "selected-files" */
  g_object_notify_by_pspec (G_OBJECT (standard_view), standard_view_props[PROP_SELECTED_FILES]);
}



/**
 * thunar_standard_view_set_history:
 * @standard_view : a #ThunarStandardView instance.
 * @history       : the #ThunarHistory to set.
 *
 * replaces the history of this #ThunarStandardView with the passed history
 **/
void
thunar_standard_view_set_history (ThunarStandardView *standard_view,
                                  ThunarHistory      *history)
{
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  _thunar_return_if_fail (history == NULL || THUNAR_IS_HISTORY (history));

  /* set the new history */
  g_object_unref (standard_view->priv->history);
  standard_view->priv->history = history;

  /* connect callback */
  g_signal_connect_swapped (G_OBJECT (history), "change-directory", G_CALLBACK (thunar_navigator_change_directory), standard_view);
}



/**
 * thunar_standard_view_get_history:
 * @standard_view : a #ThunarStandardView instance.
 *
 * returns the #ThunarHistory of this #ThunarStandardView
 *
 * Return value: (transfer none): The #ThunarHistory of this #ThunarStandardView
 **/
ThunarHistory*
thunar_standard_view_get_history (ThunarStandardView *standard_view)
{
  return standard_view->priv->history;
}



/**
 * thunar_standard_view_copy_history:
 * @standard_view : a #ThunarStandardView instance.
 *
 * returns a copy of the #ThunarHistory of this #ThunarStandardView
 * The caller has to release the passed history with g_object_unref() after use.
 *
 * Return value: (transfer full): A copy of the #ThunarHistory of this #ThunarStandardView
 **/
ThunarHistory *
thunar_standard_view_copy_history (ThunarStandardView *standard_view)
{
  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), NULL);

  return thunar_history_copy (standard_view->priv->history);
}



/**
 * thunar_standard_view_append_menu_items:
 * @standard_view : a #ThunarStandardView.
 * @menu          : the #GtkMenu to add the menu items.
 * @accel_group   : a #GtkAccelGroup to be used used for new menu items
 *
 * Appends widget-specific menu items to a #GtkMenu and connects them to the passed #GtkAccelGroup
 * The concrete implementation depends on the concrete widget which is implementing this view
 **/
void
thunar_standard_view_append_menu_items (ThunarStandardView *standard_view,
                                        GtkMenu            *menu,
                                        GtkAccelGroup      *accel_group)
{

  GtkWidget  *item;
  GtkWidget  *submenu;

  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_STANDARD_VIEW_ACTION_ARRANGE_ITEMS_MENU), NULL, GTK_MENU_SHELL (menu));
  submenu =  gtk_menu_new();
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_STANDARD_VIEW_ACTION_SORT_BY_NAME), G_OBJECT (standard_view),
                                                   standard_view->priv->sort_column == THUNAR_COLUMN_NAME, GTK_MENU_SHELL (submenu));
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_STANDARD_VIEW_ACTION_SORT_BY_SIZE), G_OBJECT (standard_view),
                                                   standard_view->priv->sort_column == THUNAR_COLUMN_SIZE, GTK_MENU_SHELL (submenu));
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_STANDARD_VIEW_ACTION_SORT_BY_TYPE), G_OBJECT (standard_view),
                                                   standard_view->priv->sort_column == THUNAR_COLUMN_TYPE, GTK_MENU_SHELL (submenu));
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_STANDARD_VIEW_ACTION_SORT_BY_MTIME), G_OBJECT (standard_view),
                                                   standard_view->priv->sort_column == THUNAR_COLUMN_DATE_MODIFIED, GTK_MENU_SHELL (submenu));
  if (thunar_file_is_trash (standard_view->priv->current_directory))
    xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_STANDARD_VIEW_ACTION_SORT_BY_DTIME), G_OBJECT (standard_view),
                                                     standard_view->priv->sort_column == THUNAR_COLUMN_DATE_DELETED, GTK_MENU_SHELL (submenu));
  xfce_gtk_menu_append_separator (GTK_MENU_SHELL (submenu));
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_STANDARD_VIEW_ACTION_SORT_ASCENDING), G_OBJECT (standard_view),
                                                   standard_view->priv->sort_order == GTK_SORT_ASCENDING, GTK_MENU_SHELL (submenu));
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_STANDARD_VIEW_ACTION_SORT_DESCENDING), G_OBJECT (standard_view),
                                                   standard_view->priv->sort_order == GTK_SORT_DESCENDING, GTK_MENU_SHELL (submenu));
  xfce_gtk_menu_append_separator (GTK_MENU_SHELL (submenu));
  xfce_gtk_menu_item_new_from_action_entry        (get_action_entry (THUNAR_STANDARD_VIEW_ACTION_SORT_ORDER_TOGGLE), G_OBJECT (standard_view),
                                                   GTK_MENU_SHELL (submenu));
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), GTK_WIDGET (submenu));
  gtk_widget_show (item);

  if (THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->append_menu_items != NULL)
    (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->append_menu_items) (standard_view, menu, accel_group);
}



/**
 * thunar_standard_view_append_menu_item:
 * @standard_view  : Instance of a  #ThunarStandardView
 * @menu           : #GtkMenuShell to which the item should be added
 * @action         : #ThunarStandardViewAction to select which item should be added
 *
 * Adds the selected, widget specific #GtkMenuItem to the passed #GtkMenuShell
 *
 * Return value: (transfer none): The added #GtkMenuItem
 **/
GtkWidget *
thunar_standard_view_append_menu_item (ThunarStandardView      *standard_view,
                                       GtkMenu                 *menu,
                                       ThunarStandardViewAction action)
{
  GtkWidget *item;

  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), NULL);

  item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (action), G_OBJECT (standard_view), GTK_MENU_SHELL (menu));

  if (action == THUNAR_STANDARD_VIEW_ACTION_UNSELECT_ALL_FILES)
    gtk_widget_set_sensitive (item, standard_view->priv->selected_files != NULL);

  return item;
}



/**
 * thunar_standard_view_connect_accelerators:
 * @standard_view : a #ThunarStandardView.
 *
 * Connects all accelerators and corresponding default keys of this widget to the global accelerator list
 * The concrete implementation depends on the concrete widget which is implementing this view
 **/
static void
thunar_standard_view_connect_accelerators (ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  if (standard_view->accel_group == NULL)
    return;

  /* can cause emissions of "accel-map::changed" which in turns calls thunar_window_reconnect_accelerators */
  xfce_gtk_accel_map_add_entries (thunar_standard_view_action_entries, G_N_ELEMENTS (thunar_standard_view_action_entries));
  xfce_gtk_accel_group_connect_action_entries (standard_view->accel_group,
                                               thunar_standard_view_action_entries,
                                               G_N_ELEMENTS (thunar_standard_view_action_entries),
                                               standard_view);

  /* as well append accelerators of derived widgets */
  if (THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->connect_accelerators != NULL)
    (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->connect_accelerators) (standard_view, standard_view->accel_group);
}



/**
 * thunar_standard_view_disconnect_accelerators:
 * @standard_view : a #ThunarStandardView.
 *
 * Disconnects all accelerators of this widget from the global accelerator list
 * The concrete implementation depends on the concrete widget which is implementing this view
 **/
static void
thunar_standard_view_disconnect_accelerators (ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  if (standard_view->accel_group == NULL)
    return;

  /* Dont listen to the accel keys defined by the action entries any more */
  xfce_gtk_accel_group_disconnect_action_entries (standard_view->accel_group,
                                               thunar_standard_view_action_entries,
                                               G_N_ELEMENTS (thunar_standard_view_action_entries));

  /* as well disconnect accelerators of derived widgets */
  if (THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->disconnect_accelerators != NULL)
    (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->disconnect_accelerators) (standard_view, standard_view->accel_group);

  /* and release the accel group */
  g_object_unref (standard_view->accel_group);
  standard_view->accel_group = NULL;
}



/**
 * _thunar_standard_view_open_on_middle_click:
 * @standard_view : a #ThunarStandardView.
 * @tree_path : the #GtkTreePath to open.
 * @event_state : The event_state of the pressed #GdkEventButton
 *
 * Method only should be used by child widgets.
 * The method will attempt to find a thunar file for the given #GtkTreePath and open it as window/tab, if it is a directory
 * Note that this method only should be used after pressing the middle-mouse button
 **/
void
_thunar_standard_view_open_on_middle_click (ThunarStandardView *standard_view,
                                            GtkTreePath        *tree_path,
                                            guint               event_state)
{
  GtkTreeIter          iter;
  ThunarFile          *file;
  gboolean             in_tab;
  GtkWidget           *window;
  ThunarActionManager *action_mgr;

  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* determine the file for the path */
  gtk_tree_model_get_iter (GTK_TREE_MODEL (standard_view->model), &iter, tree_path);
  file = thunar_list_model_get_file (standard_view->model, &iter);
  if (G_LIKELY (file != NULL))
    {
      if (thunar_file_is_directory (file))
        {
          /* lookup setting if we should open in a tab or a window */
          g_object_get (G_OBJECT (standard_view->preferences), "misc-middle-click-in-tab", &in_tab, NULL);

          /* holding ctrl inverts the action */
          if ((event_state & GDK_CONTROL_MASK) != 0)
              in_tab = !in_tab;

          window     = gtk_widget_get_toplevel (GTK_WIDGET (standard_view));
          action_mgr = thunar_window_get_action_manager (THUNAR_WINDOW (window));
          thunar_action_manager_open_selected_folders (action_mgr, in_tab);
        }
      /* release the file reference */
      g_object_unref (G_OBJECT (file));
    }
}



/**
 * thunar_standard_view_set_searching:
 * @standard_view : a #ThunarStandardView.
 * @search_query : the search string.
 *
 * If @search_query is not NULL a search is initialized for this view. If it is NULL and the view is displaying
 * the results of a previous search it reverts to its normal contents.
 **/
void
thunar_standard_view_set_searching (ThunarStandardView *standard_view,
                                    gchar              *search_query)
{
  /* can be called from a change in the path entry when the tab switches and a new directory is set
   * which in turn sets a new location (see thunar_window_notebook_switch_page) */
  if (standard_view == NULL)
    return;

  if (standard_view->priv->search_query != NULL && search_query == NULL)
    thunar_standard_view_search_done (standard_view->model, standard_view);

  /* save the new query (used for switching between views) */
  g_free (standard_view->priv->search_query);
  standard_view->priv->search_query = g_strdup (search_query);

  /* initiate the search */
  /* set_folder() can emit a large number of row-deleted signals for large folders,
   * to the extent it degrades performance: https://gitlab.xfce.org/xfce/thunar/-/issues/914 */
  g_signal_handler_block (standard_view->model, standard_view->priv->row_deleted_id);
  g_object_ref (G_OBJECT (thunar_list_model_get_folder (standard_view->model))); /* temporarily hold a reference so the folder doesn't get deleted */
  thunar_list_model_set_folder (standard_view->model, thunar_list_model_get_folder (standard_view->model), search_query);
  g_object_unref (G_OBJECT (thunar_list_model_get_folder (standard_view->model))); /* reference no longer needed */
  g_signal_handler_unblock (standard_view->model, standard_view->priv->row_deleted_id);

  /* change the display name in the tab */
  g_object_notify_by_pspec (G_OBJECT (standard_view), standard_view_props[PROP_DISPLAY_NAME]);


  if (search_query != NULL && g_strcmp0 (search_query, "") != 0)
    standard_view->priv->active_search = TRUE;
  else
    standard_view->priv->active_search = FALSE;

  /* notify listeners */
  g_object_notify_by_pspec (G_OBJECT (standard_view), standard_view_props[PROP_SEARCHING]);
}



gchar*
thunar_standard_view_get_search_query (ThunarStandardView *standard_view)
{
  return standard_view->priv->search_query;
}



void
thunar_standard_view_save_view_type (ThunarStandardView *standard_view,
                                     GType               type)
{
  standard_view->priv->type = type;
}



GType
thunar_standard_view_get_saved_view_type (ThunarStandardView *standard_view)
{
  return standard_view->priv->type;
}



XfceGtkActionEntry*
thunar_standard_view_get_action_entries (void)
{
  return thunar_standard_view_action_entries;
}



void
thunar_standard_view_queue_redraw (ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->queue_redraw) (standard_view);
}



static void
thunar_standard_view_highlight_option_changed (ThunarStandardView *standard_view)
{
  GtkWidget             *view = gtk_bin_get_child (GTK_BIN (standard_view));
  GtkCellLayout         *layout = NULL;
  GtkCellLayoutDataFunc  function = NULL;
  gboolean               show_highlight;

  if (GTK_IS_TREE_VIEW (view))
    layout = GTK_CELL_LAYOUT (gtk_tree_view_get_column (GTK_TREE_VIEW (view), THUNAR_COLUMN_NAME));
  else
    layout = GTK_CELL_LAYOUT (view);

  g_object_get (G_OBJECT (THUNAR_STANDARD_VIEW (standard_view)->preferences), "misc-highlighting-enabled", &show_highlight, NULL);

  if (show_highlight)
    function = (GtkCellLayoutDataFunc) THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->cell_layout_data_func;

  gtk_cell_layout_set_cell_data_func (layout,
                                      standard_view->icon_renderer,
                                      function, NULL, NULL);
  gtk_cell_layout_set_cell_data_func (layout,
                                      standard_view->name_renderer,
                                      function, NULL, NULL);
}



static void
thunar_standard_view_cell_layout_data_func (GtkCellLayout   *layout,
                                            GtkCellRenderer *cell,
                                            GtkTreeModel    *model,
                                            GtkTreeIter     *iter,
                                            gpointer         data)
{
  ThunarFile  *file = THUNAR_FILE (thunar_list_model_get_file (THUNAR_LIST_MODEL (model), iter));
  const gchar *background = NULL;
  const gchar *foreground = NULL;

  background = thunar_file_get_metadata_setting (file, "highlight-color-background");
  foreground = thunar_file_get_metadata_setting (file, "highlight-color-foreground");

  /* since this function is being used for both icon & name renderers;
   * we need to make sure the right properties are applied to the right renderers */
  if (THUNAR_IS_TEXT_RENDERER (cell))
    g_object_set (G_OBJECT (cell),
                  "foreground", foreground,
                  "highlight-color", background,
                  NULL);

  else if (THUNAR_IS_ICON_RENDERER (cell))
    g_object_set (G_OBJECT (cell),
                  "highlight-color", background,
                  NULL);

  else if (GTK_IS_CELL_RENDERER_TEXT (cell))
    g_object_set (G_OBJECT (cell),
                  "foreground", foreground,
                  "background", background,
                  NULL);

  else if (GTK_IS_CELL_RENDERER (cell))
    g_object_set (G_OBJECT (cell),
                  "cell-background", background,
                  NULL);

  else
    g_warn_if_reached ();

  g_object_unref (file);
}
