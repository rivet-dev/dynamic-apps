/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
 * Copyright (c) 2012      Nick Schermer <nick@xfce.org>
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

#include <thunar/thunar-navigator.h>
#include <thunar/thunar-application.h>
#include <thunar/thunar-browser.h>
#include <thunar/thunar-device-monitor.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-dnd.h>
#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-menu.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-shortcuts-icon-renderer.h>
#include <thunar/thunar-shortcuts-model.h>
#include <thunar/thunar-shortcuts-view.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
};

/* Identifiers for DnD target types */
enum
{
  GTK_TREE_MODEL_ROW,
  TEXT_URI_LIST,
};



static void           thunar_shortcuts_view_navigator_init               (ThunarNavigatorIface     *iface);
static void           thunar_shortcuts_view_finalize                     (GObject                  *object);
static void           thunar_shortcuts_view_get_property                 (GObject                  *object,
                                                                          guint                     prop_id,
                                                                          GValue                   *value,
                                                                          GParamSpec               *pspec);
static void           thunar_shortcuts_view_set_property                 (GObject                  *object,
                                                                          guint                     prop_id,
                                                                          const GValue             *value,
                                                                          GParamSpec               *pspec);
static void           thunar_shortcuts_view_scale_changed                (GObject                  *object,
                                                                          GParamSpec               *pspec,
                                                                          gpointer                  user_data);
static ThunarFile*    thunar_shortcuts_view_get_current_directory        (ThunarNavigator          *navigator);
static void           thunar_shortcuts_view_set_current_directory        (ThunarNavigator          *navigator,
                                                                          ThunarFile               *current_directory);
static gboolean       thunar_shortcuts_view_button_press_event           (GtkWidget                *widget,
                                                                          GdkEventButton           *event);
static gboolean       thunar_shortcuts_view_button_release_event         (GtkWidget                *widget,
                                                                          GdkEventButton           *event);
static gboolean       thunar_shortcuts_view_key_release_event            (GtkWidget                *widget,
                                                                          GdkEventKey              *event);
static void           thunar_shortcuts_view_drag_begin                   (GtkWidget                *widget,
                                                                          GdkDragContext           *context);
static void           thunar_shortcuts_view_drag_data_received           (GtkWidget                *widget,
                                                                          GdkDragContext           *context,
                                                                          gint                      x,
                                                                          gint                      y,
                                                                          GtkSelectionData         *selection_data,
                                                                          guint                     info,
                                                                          guint                     timestamp);
static gboolean       thunar_shortcuts_view_drag_drop                    (GtkWidget                *widget,
                                                                          GdkDragContext           *context,
                                                                          gint                      x,
                                                                          gint                      y,
                                                                          guint                     timestamp);
static gboolean       thunar_shortcuts_view_drag_motion                  (GtkWidget                *widget,
                                                                          GdkDragContext           *context,
                                                                          gint                      x,
                                                                          gint                      y,
                                                                          guint                     timestamp);
static void           thunar_shortcuts_view_drag_leave                   (GtkWidget                *widget,
                                                                          GdkDragContext           *context,
                                                                          guint                     timestamp);
static gboolean       thunar_shortcuts_view_popup_menu                   (GtkWidget                *widget);
static void           thunar_shortcuts_view_row_activated                (GtkTreeView              *tree_view,
                                                                          GtkTreePath              *path,
                                                                          GtkTreeViewColumn        *column);
static gboolean       thunar_shortcuts_view_selection_func               (GtkTreeSelection         *selection,
                                                                          GtkTreeModel             *model,
                                                                          GtkTreePath              *path,
                                                                          gboolean                  path_currently_selected,
                                                                          gpointer                  user_data);
static void           thunar_shortcuts_view_context_menu_visibility      (ThunarShortcutsView      *view,
                                                                          GdkEventButton           *event,
                                                                          GtkTreeModel             *model);
static void           thunar_shortcuts_view_context_menu                 (ThunarShortcutsView      *view,
                                                                          GdkEventButton           *event,
                                                                          GtkTreeModel             *model,
                                                                          GtkTreeIter              *iter);
static gboolean       thunar_shortcuts_view_remove_activated             (ThunarShortcutsView      *view,
                                                                          GtkWidget                *item);
static void           thunar_shortcuts_view_editing_canceled             (GtkCellRenderer          *renderer,
                                                                          ThunarShortcutsView      *view);
static gboolean       thunar_shortcuts_view_rename_activated             (ThunarShortcutsView      *view,
                                                                          GtkWidget                *item);
static void           thunar_shortcuts_view_renamed                      (GtkCellRenderer          *renderer,
                                                                          const gchar              *path_string,
                                                                          const gchar              *text,
                                                                          ThunarShortcutsView      *view);
static GdkDragAction  thunar_shortcuts_view_compute_drop_actions         (ThunarShortcutsView      *view,
                                                                          GdkDragContext           *context,
                                                                          gint                      x,
                                                                          gint                      y,
                                                                          GtkTreePath             **path_return,
                                                                          GdkDragAction            *action_return,
                                                                          GtkTreeViewDropPosition  *position_return);
static GtkTreePath   *thunar_shortcuts_view_compute_drop_position        (ThunarShortcutsView      *view,
                                                                          gint                      x,
                                                                          gint                      y);
static void           thunar_shortcuts_view_drop_uri_list                (ThunarShortcutsView      *view,
                                                                          GList                    *path_list,
                                                                          GtkTreePath              *dst_path);
static void           thunar_shortcuts_view_open                         (ThunarShortcutsView      *view,
                                                                          ThunarActionManagerFolderOpenAction open_in);
static void           thunar_shortcuts_view_eject                        (ThunarShortcutsView      *view);
static void           thunar_shortcuts_view_stop_spinner                 (ThunarShortcutsView      *view,
                                                                          ThunarDevice             *device,
                                                                          gpointer                  user_data);
static void           thunar_shortcuts_view_start_spinner                (ThunarShortcutsView      *view,
                                                                          ThunarDevice             *device,
                                                                          gpointer                  user_data);
static void           thunar_shortcuts_view_new_files_created            (ThunarShortcutsView      *view,
                                                                          GList                    *files_to_selected);



struct _ThunarShortcutsViewClass
{
  GtkTreeViewClass __parent__;
};

struct _ThunarShortcutsView
{
  GtkTreeView        __parent__;


  ThunarPreferences      *preferences;
  GtkCellRenderer        *icon_renderer;
  GtkCellRenderer        *padding_renderer;
  GtkTreeViewColumn      *column;
  gboolean                padding_enabled;

  ThunarxProviderFactory *provider_factory;

  /* the currently pressed mouse button, set in the
   * button-press-event handler if the associated
   * button-release-event should activate.
   */
  gint  pressed_button;
  guint pressed_eject_button : 1;

  /* drop site support */
  guint  drop_data_ready : 1; /* whether the drop data was received already */
  guint  drop_occurred : 1;
  GList *drop_file_list;      /* the list of URIs that are contained in the drop data */

  /* id of the signal used to queue a resize on the
   * column whenever the shortcuts icon size is changed.
   */
  gulong queue_resize_signal_id;

  /* used to create menu items for the context menu */
  ThunarActionManager *action_mgr;
  ThunarFile          *current_directory;
};



/* Target types for dragging from the shortcuts view */
static const GtkTargetEntry drag_targets[] = {
  { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, GTK_TREE_MODEL_ROW },
};

/* Target types for dropping into the shortcuts view */
static const GtkTargetEntry drop_targets[] = {
  { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, GTK_TREE_MODEL_ROW },
  { "text/uri-list", 0, TEXT_URI_LIST },
};

/* #XfceGtkActionEntrys provided by this widget */
typedef enum
{
  THUNAR_SHORTCUTS_VIEW_ACTION_RENAME_BOOKMARK,
  THUNAR_SHORTCUTS_VIEW_ACTION_REMOVE_BOOKMARK,
} ThunarShortcutsViewAction;

static XfceGtkActionEntry thunar_shortcuts_view_action_entries[] =
{
    { THUNAR_SHORTCUTS_VIEW_ACTION_RENAME_BOOKMARK, "<Actions>/ThunarShortcutsView/rename-bookmark", "", XFCE_GTK_MENU_ITEM,       N_("Re_name Shortcut"), NULL, "",            G_CALLBACK (thunar_shortcuts_view_rename_activated), },
    { THUNAR_SHORTCUTS_VIEW_ACTION_REMOVE_BOOKMARK, "<Actions>/ThunarShortcutsView/remove-bookmark", "", XFCE_GTK_IMAGE_MENU_ITEM, N_("_Remove Shortcut"), NULL, "list-remove", G_CALLBACK (thunar_shortcuts_view_remove_activated), },
};

#define get_action_entry(id) xfce_gtk_get_action_entry_by_id(thunar_shortcuts_view_action_entries,G_N_ELEMENTS(thunar_shortcuts_view_action_entries),id)



G_DEFINE_TYPE_WITH_CODE (ThunarShortcutsView, thunar_shortcuts_view, GTK_TYPE_TREE_VIEW,
    G_IMPLEMENT_INTERFACE (THUNAR_TYPE_NAVIGATOR, thunar_shortcuts_view_navigator_init))



static void
thunar_shortcuts_view_class_init (ThunarShortcutsViewClass *klass)
{
  GtkTreeViewClass *gtktree_view_class;
  GtkWidgetClass   *gtkwidget_class;
  GObjectClass     *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_shortcuts_view_finalize;
  gobject_class->get_property = thunar_shortcuts_view_get_property;
  gobject_class->set_property = thunar_shortcuts_view_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->button_press_event = thunar_shortcuts_view_button_press_event;
  gtkwidget_class->button_release_event = thunar_shortcuts_view_button_release_event;
  gtkwidget_class->key_release_event = thunar_shortcuts_view_key_release_event;
  gtkwidget_class->drag_begin = thunar_shortcuts_view_drag_begin;
  gtkwidget_class->drag_data_received = thunar_shortcuts_view_drag_data_received;
  gtkwidget_class->drag_drop = thunar_shortcuts_view_drag_drop;
  gtkwidget_class->drag_motion = thunar_shortcuts_view_drag_motion;
  gtkwidget_class->drag_leave = thunar_shortcuts_view_drag_leave;
  gtkwidget_class->popup_menu = thunar_shortcuts_view_popup_menu;

  gtktree_view_class = GTK_TREE_VIEW_CLASS (klass);
  gtktree_view_class->row_activated = thunar_shortcuts_view_row_activated;

  /* Override ThunarNavigator's properties */
  g_object_class_override_property (gobject_class, PROP_CURRENT_DIRECTORY, "current-directory");

  xfce_gtk_translate_action_entries (thunar_shortcuts_view_action_entries, G_N_ELEMENTS (thunar_shortcuts_view_action_entries));
}



static void
thunar_shortcuts_view_navigator_init (ThunarNavigatorIface *iface)
{
  iface->get_current_directory = thunar_shortcuts_view_get_current_directory;
  iface->set_current_directory = thunar_shortcuts_view_set_current_directory;
}



static void
thunar_shortcuts_view_init (ThunarShortcutsView *view)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer   *renderer;
  GtkTreeSelection  *selection;

  /* configure the tree view */
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (view), FALSE);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view), FALSE);
  gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (view), THUNAR_SHORTCUTS_MODEL_COLUMN_TOOLTIP);

  /* grab a reference on the provider factory */
  view->provider_factory = thunarx_provider_factory_get_default ();

  /* grab a reference on the preferences; be sure to redraw the view
   * whenever the "shortcuts-icon-emblems" preference changes.
   */
  view->preferences = thunar_preferences_get ();
  g_signal_connect_swapped (G_OBJECT (view->preferences), "notify::shortcuts-icon-emblems", G_CALLBACK (gtk_widget_queue_draw), view);

  /* allocate a single column for our renderers */
  column = view->column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "reorderable", FALSE,
                         "resizable", FALSE,
                         "expand", TRUE,
                         "spacing", 2,
                         NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

  /* queue a resize on the column whenever the icon size is changed */
  view->queue_resize_signal_id = g_signal_connect_swapped (G_OBJECT (view->preferences), "notify::shortcuts-icon-size",
                                                           G_CALLBACK (gtk_tree_view_column_queue_resize), column);

  /* header */
  renderer = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT,
                           "weight", PANGO_WEIGHT_BOLD,
                           "xpad", 6,
                           "ypad", 4,
                           "ellipsize", PANGO_ELLIPSIZE_END,
                           NULL);
  gtk_tree_view_column_pack_end (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "text", THUNAR_SHORTCUTS_MODEL_COLUMN_NAME,
                                       "visible", THUNAR_SHORTCUTS_MODEL_COLUMN_IS_HEADER,
                                       NULL);

  /* separator for indent */
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer), "xpad", 6, NULL);
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "visible", THUNAR_SHORTCUTS_MODEL_COLUMN_IS_ITEM,
                                       NULL);

  /* allocate the special icon renderer */
  view->icon_renderer = thunar_shortcuts_icon_renderer_new ();
  gtk_tree_view_column_pack_start (column, view->icon_renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, view->icon_renderer,
                                       "gicon", THUNAR_SHORTCUTS_MODEL_COLUMN_GICON,
                                       "file", THUNAR_SHORTCUTS_MODEL_COLUMN_FILE,
                                       "device", THUNAR_SHORTCUTS_MODEL_COLUMN_DEVICE,
                                       "visible", THUNAR_SHORTCUTS_MODEL_COLUMN_IS_ITEM,
                                       NULL);
  g_signal_connect (G_OBJECT (view), "notify::scale-factor", G_CALLBACK (thunar_shortcuts_view_scale_changed), NULL);

  /* sync the "emblems" property of the icon renderer with the "shortcuts-icon-emblems" preference
   * and the "size" property of the renderer with the "shortcuts-icon-size" preference.
   */
  g_object_bind_property (G_OBJECT (view->preferences), "shortcuts-icon-size", G_OBJECT (view->icon_renderer), "size", G_BINDING_SYNC_CREATE);
  g_object_bind_property (G_OBJECT (view->preferences), "shortcuts-icon-emblems", G_OBJECT (view->icon_renderer), "emblems", G_BINDING_SYNC_CREATE);

  /* allocate the text renderer (ellipsizing as required, but "File System" must fit) */
  renderer = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT,
                           "ellipsize", PANGO_ELLIPSIZE_END,
                           NULL);
  g_signal_connect (G_OBJECT (renderer), "edited", G_CALLBACK (thunar_shortcuts_view_renamed), view);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "text", THUNAR_SHORTCUTS_MODEL_COLUMN_NAME,
                                       "visible", THUNAR_SHORTCUTS_MODEL_COLUMN_IS_ITEM,
                                       NULL);

  /* spinner to indicate (un)mount/eject delay */
  renderer = gtk_cell_renderer_spinner_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "visible", THUNAR_SHORTCUTS_MODEL_COLUMN_BUSY,
                                       "active", THUNAR_SHORTCUTS_MODEL_COLUMN_BUSY,
                                       "pulse", THUNAR_SHORTCUTS_MODEL_COLUMN_BUSY_PULSE,
                                       NULL);

  /* allocate icon renderer for the eject symbol */
  renderer = gtk_cell_renderer_pixbuf_new ();
  g_object_set (renderer, "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE, "icon-name", "media-eject-symbolic", NULL);
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "visible", THUNAR_SHORTCUTS_MODEL_COLUMN_CAN_EJECT,
                                       NULL);

  /* padding for the eject symbol so it's not covered by scroll bar */
  view->padding_enabled = FALSE;
  view->padding_renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (view->padding_renderer), "xpad", 6, NULL);
  gtk_tree_view_column_pack_start (column, view->padding_renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, view->padding_renderer,
                                       "visible", THUNAR_SHORTCUTS_MODEL_COLUMN_CAN_EJECT,
                                       NULL);

  /* enable drag support for the shortcuts view (actually used to support reordering) */
  gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (view), GDK_BUTTON1_MASK, drag_targets,
                                          G_N_ELEMENTS (drag_targets), GDK_ACTION_MOVE);

  /* enable drop support for the shortcuts view (both internal reordering
   * and adding new shortcuts from other widgets)
   */
  gtk_drag_dest_set (GTK_WIDGET (view), 0, drop_targets, G_N_ELEMENTS (drop_targets),
                     GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_MOVE);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  gtk_tree_selection_set_select_function (selection, thunar_shortcuts_view_selection_func, NULL, NULL);

  view->action_mgr =  g_object_new (THUNAR_TYPE_ACTION_MANAGER, "widget", GTK_WIDGET (view), NULL);

  g_signal_connect_swapped (G_OBJECT (view->action_mgr), "change-directory", G_CALLBACK (thunar_navigator_change_directory), view);
  g_signal_connect_swapped (G_OBJECT (view->action_mgr), "open-new-tab", G_CALLBACK (thunar_navigator_open_new_tab), view);
  g_signal_connect_swapped (G_OBJECT (view->action_mgr), "device-operation-started", G_CALLBACK (thunar_shortcuts_view_start_spinner), view);
  g_signal_connect_swapped (G_OBJECT (view->action_mgr), "device-operation-finished", G_CALLBACK (thunar_shortcuts_view_stop_spinner), view);
  g_signal_connect_swapped (G_OBJECT (view->action_mgr), "new-files-created", G_CALLBACK (thunar_shortcuts_view_new_files_created), view);
  g_object_bind_property (G_OBJECT (view), "current-directory", G_OBJECT (view->action_mgr), "current-directory", G_BINDING_SYNC_CREATE);
}



static void
thunar_shortcuts_view_finalize (GObject *object)
{
  ThunarShortcutsView *view = THUNAR_SHORTCUTS_VIEW (object);

  /* release drop path list (if drag_leave wasn't called) */
  thunar_g_list_free_full (view->drop_file_list);

  /* release the provider factory */
  g_object_unref (G_OBJECT (view->provider_factory));

  /* disconnect the queue resize signal handler */
  g_signal_handler_disconnect (G_OBJECT (view->preferences), view->queue_resize_signal_id);

  /* reset the current-directory property */
  thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (view), NULL);

  /* release reference on the action manager */
  g_object_unref (view->action_mgr);

  /* disconnect from the preferences object */
  g_signal_handlers_disconnect_matched (G_OBJECT (view->preferences), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, view);
  g_object_unref (G_OBJECT (view->preferences));

  (*G_OBJECT_CLASS (thunar_shortcuts_view_parent_class)->finalize) (object);
}



static void
thunar_shortcuts_view_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      g_value_set_object (value, thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (object)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_shortcuts_view_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (object), g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_shortcuts_view_scale_changed (GObject    *object,
                                     GParamSpec *pspec,
                                     gpointer    user_data)
{
    gtk_widget_queue_draw (GTK_WIDGET (object));
}



static ThunarFile*
thunar_shortcuts_view_get_current_directory (ThunarNavigator *navigator)
{
  return THUNAR_SHORTCUTS_VIEW (navigator)->current_directory;
}



static void
thunar_shortcuts_view_set_current_directory (ThunarNavigator *navigator,
                                             ThunarFile      *current_directory)
{

  ThunarShortcutsView *view = THUNAR_SHORTCUTS_VIEW (navigator);

  /* check if we already use that directory */
  if (G_UNLIKELY (view->current_directory == current_directory))
    return;

  /* check if we have an active directory */
  if (G_LIKELY (view->current_directory != NULL))
    {
      /* disconnect from the previous current directory */
      g_object_unref (G_OBJECT (view->current_directory));
    }

  /* activate the new current directory */
  view->current_directory = current_directory;

  /* connect to the new current directory */
  if (G_LIKELY (current_directory != NULL))
    {
      /* take a reference on the directory */
      g_object_ref (G_OBJECT (current_directory));
    }

  /* notify listeners */
  g_object_notify (G_OBJECT (view), "current-directory");
}



static gboolean
thunar_shortcuts_view_button_press_event (GtkWidget      *widget,
                                          GdkEventButton *event)
{
  ThunarShortcutsView *view = THUNAR_SHORTCUTS_VIEW (widget);
  GtkTreeModel        *model;
  GtkTreePath         *path;
  GtkTreeIter          iter;
  gboolean             result;
  gboolean             can_eject;
  gboolean             button_clicked;
  gint                 icon_width, column_width;

  /* reset the pressed button state */
  view->pressed_button = -1;
  view->pressed_eject_button = 0;

  /* completely ignore double click events */
  if (event->type == GDK_2BUTTON_PRESS)
    return TRUE;

  /* let the widget process the event first (handles focussing and scrolling) */
  result = (*GTK_WIDGET_CLASS (thunar_shortcuts_view_parent_class)->button_press_event) (widget, event);

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));

  /* resolve the path at the cursor position */
  if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget), event->x, event->y, &path, NULL, NULL, NULL))
    {
      /* check if we should popup the context menu */
      if (G_UNLIKELY (event->button == 3))
        {
          /* determine the iterator for the path */
          if (gtk_tree_model_get_iter (model, &iter, path))
            {
              /* popup the context menu */
              thunar_shortcuts_view_context_menu (view, event, model, &iter);

              /* we effectively handled the event */
              result = TRUE;
            }
        }
      else if (event->button == 1 || event->button == 2)
        {
          /* check if we clicked the eject button area */
          column_width = gtk_tree_view_column_get_width (gtk_tree_view_get_column (GTK_TREE_VIEW (view), 0));
          gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &icon_width, NULL);

          if (gtk_get_locale_direction () == GTK_TEXT_DIR_LTR)
            button_clicked = event->x >= column_width - icon_width - (view->padding_enabled ? 16 : 3);
          else
            button_clicked = event->x <= icon_width + (view->padding_enabled ? 16 : 3);

          if (event->button == 1 && button_clicked)
            {
              /* check if that shortcut actually has an eject button */
              model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
              if (gtk_tree_model_get_iter (model, &iter, path))
                {
                  gtk_tree_model_get (model, &iter, THUNAR_SHORTCUTS_MODEL_COLUMN_CAN_EJECT, &can_eject, -1);
                  if (can_eject)
                    view->pressed_eject_button = 1;
                }
            }
          else
            gtk_tree_view_set_cursor (GTK_TREE_VIEW (widget), path, gtk_tree_view_get_column (GTK_TREE_VIEW (view), 0), FALSE);

          /* remember the button as pressed and handle it in the release handler */
          view->pressed_button = event->button;
        }

      /* release the path */
      gtk_tree_path_free (path);
    }
  else if (event->button == 3)
    {
      thunar_shortcuts_view_context_menu_visibility (view, event, model);
      result = TRUE;
    }

  return result;
}



static gboolean
thunar_shortcuts_view_button_release_event (GtkWidget      *widget,
                                            GdkEventButton *event)
{
  ThunarShortcutsView *view = THUNAR_SHORTCUTS_VIEW (widget);
  gboolean             in_tab;

  /* check if we have an event matching the pressed button state */
  if (G_LIKELY (view->pressed_button == (gint) event->button))
    {
      /* check if the eject button has been pressed */
      if (view->pressed_eject_button)
        {
          thunar_shortcuts_view_eject (view);
        }
      else if (G_LIKELY (event->button == 1))
        {
          /* button 1 opens in the same window */
          thunar_shortcuts_view_open (view, THUNAR_ACTION_MANAGER_CHANGE_DIRECTORY);
        }
      else if (G_UNLIKELY (event->button == 2))
        {
          /* button 2 opens in a new window or tab */
          g_object_get (view->preferences, "misc-middle-click-in-tab", &in_tab, NULL);

          /* holding ctrl inverts the action */
          if ((event->state & GDK_CONTROL_MASK) != 0)
            in_tab = !in_tab;

          thunar_shortcuts_view_open (view, in_tab ? THUNAR_ACTION_MANAGER_OPEN_AS_NEW_TAB : THUNAR_ACTION_MANAGER_OPEN_AS_NEW_WINDOW);
        }
    }

  /* reset the pressed button state */
  view->pressed_button = -1;
  view->pressed_eject_button = 0;

  /* call the parent's release event handler */
  return (*GTK_WIDGET_CLASS (thunar_shortcuts_view_parent_class)->button_release_event) (widget, event);
}



static gboolean
thunar_shortcuts_view_key_release_event (GtkWidget   *widget,
                                         GdkEventKey *event)
{
  ThunarShortcutsView *view = THUNAR_SHORTCUTS_VIEW (widget);

  /* work nicer with keyboard navigation */
  switch (event->keyval)
    {
    case GDK_KEY_Up:
    case GDK_KEY_Down:
    case GDK_KEY_KP_Up:
    case GDK_KEY_KP_Down:
      thunar_shortcuts_view_open (view, THUNAR_ACTION_MANAGER_CHANGE_DIRECTORY);

      /* keep focus on us */
      gtk_widget_grab_focus (widget);
      break;
    }

  /* call the parent's release event handler */
  return (*GTK_WIDGET_CLASS (thunar_shortcuts_view_parent_class)->key_release_event) (widget, event);
}



static void
thunar_shortcuts_view_drag_begin (GtkWidget      *widget,
                                  GdkDragContext *context)
{
  ThunarShortcutsView *view = THUNAR_SHORTCUTS_VIEW (widget);

  /* reset the pressed button state */
  view->pressed_button = -1;

  /* call the parent's drag_begin() handler */
  if (GTK_WIDGET_CLASS (thunar_shortcuts_view_parent_class)->drag_begin != NULL)
    (*GTK_WIDGET_CLASS (thunar_shortcuts_view_parent_class)->drag_begin) (widget, context);
}



static void
thunar_shortcuts_view_drag_data_received (GtkWidget        *widget,
                                          GdkDragContext   *context,
                                          gint              x,
                                          gint              y,
                                          GtkSelectionData *selection_data,
                                          guint             info,
                                          guint             timestamp)
{
  GtkTreeViewDropPosition position = GTK_TREE_VIEW_DROP_BEFORE;
  ThunarShortcutsView    *view = THUNAR_SHORTCUTS_VIEW (widget);
  GdkDragAction           actions;
  GdkDragAction           action;
  GtkTreeModel           *model;
  GtkTreePath            *path;
  GtkTreeIter             iter;
  ThunarFile             *file;
  gboolean                succeed = FALSE;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view));

  /* check if we don't already know the drop data */
  if (G_LIKELY (!view->drop_data_ready))
    {
      /* extract the URI list from the selection data (if valid) */
      if (info == TEXT_URI_LIST && gtk_selection_data_get_format (selection_data) == 8 && gtk_selection_data_get_length (selection_data) > 0)
        view->drop_file_list = thunar_g_file_list_new_from_string ((const gchar *) gtk_selection_data_get_data (selection_data));

      /* reset the state */
      view->drop_data_ready = TRUE;
    }

  /* check if the data was droppped */
  if (G_UNLIKELY (view->drop_occurred))
    {
      /* reset the state */
      view->drop_occurred = FALSE;

      /* verify that we only handle text/uri-list here */
      if (G_LIKELY (info == TEXT_URI_LIST))
        {
          /* determine the drop actions */
          actions = thunar_shortcuts_view_compute_drop_actions (view, context, x, y, &path, &action, &position);
          if (G_LIKELY (actions != 0))
            {
              /* check if we should add a shortcut */
              if (position == GTK_TREE_VIEW_DROP_BEFORE || position == GTK_TREE_VIEW_DROP_AFTER)
                {
                  /* if position is "after", we need to advance the path,
                   * as the drop_uri_list() will insert "at" the path.
                   */
                  if (position == GTK_TREE_VIEW_DROP_AFTER)
                    gtk_tree_path_next (path);

                  /* just add the required shortcuts then */
                  thunar_shortcuts_view_drop_uri_list (view, view->drop_file_list, path);
                }
              else if (G_LIKELY ((actions & (GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK)) != 0))
                {
                  /* get the shortcuts model */
                  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));

                  /* determine the iterator for the path */
                  if (gtk_tree_model_get_iter (model, &iter, path))
                    {
                      /* determine the file for the iter */
                      gtk_tree_model_get (model, &iter, THUNAR_SHORTCUTS_MODEL_COLUMN_FILE, &file, -1);
                      if (G_LIKELY (file != NULL))
                        {
                          /* ask the user what to do with the drop data */
                          if (G_UNLIKELY (action == GDK_ACTION_ASK))
                            action = thunar_dnd_ask (widget, file, view->drop_file_list, actions);

                          /* perform the requested action */
                          if (G_LIKELY (action != 0))
                            {
                              /* really perform the drop :-) */
                              succeed = thunar_dnd_perform (widget, file, view->drop_file_list, action, NULL);
                            }

                          /* release the file */
                          g_object_unref (G_OBJECT (file));
                        }
                    }
                }

              /* release the tree path */
              gtk_tree_path_free (path);
            }
        }

      /* disable the drop highlighting */
      thunar_shortcuts_view_drag_leave (widget, context, timestamp);

      /* tell the peer that we handled the drop */
      gtk_drag_finish (context, succeed, FALSE, timestamp);
    }
  else
    {
      gdk_drag_status (context, 0, timestamp);
    }
}



static gboolean
thunar_shortcuts_view_drag_drop (GtkWidget      *widget,
                                 GdkDragContext *context,
                                 gint            x,
                                 gint            y,
                                 guint           timestamp)
{
  ThunarShortcutsView *view = THUNAR_SHORTCUTS_VIEW (widget);
  GtkTreeSelection    *selection;
  GtkTreeModel        *model;
  GtkTreePath         *dst_path;
  GtkTreePath         *src_path;
  GtkTreeIter          iter;
  GdkAtom              target;
  GtkTreeModel        *child_model;
  GtkTreePath         *child_dst_path;
  GtkTreePath         *child_src_path;

  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view), FALSE);

  /* determine the drop target */
  target = gtk_drag_dest_find_target (widget, context, NULL);
  if (G_LIKELY (target == gdk_atom_intern_static_string ("text/uri-list")))
    {
      /* set state so the drag-data-received handler
       * knows that this is really a drop this time.
       */
      view->drop_occurred = TRUE;

      /* request the drag data from the source. */
      gtk_drag_get_data (widget, context, target, timestamp);
    }
  else if (target == gdk_atom_intern_static_string ("GTK_TREE_MODEL_ROW"))
    {
      /* compute the drop position */
      dst_path = thunar_shortcuts_view_compute_drop_position (view, x, y);

      /* determine the source path */
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
      if (gtk_tree_selection_get_selected (selection, &model, &iter))
        {
          /* get the source path */
          src_path = gtk_tree_model_get_path (model, &iter);

          /* if we drop downwards, correct the drop destination */
          if (gtk_tree_path_compare (src_path, dst_path) < 0)
            gtk_tree_path_prev (dst_path);

          /* convert iters */
          src_path = gtk_tree_model_get_path (model, &iter);
          child_src_path = gtk_tree_model_filter_convert_path_to_child_path (GTK_TREE_MODEL_FILTER (model), src_path);
          child_dst_path = gtk_tree_model_filter_convert_path_to_child_path (GTK_TREE_MODEL_FILTER (model), dst_path);
          gtk_tree_path_free (src_path);

          child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));

          /* perform the move */
          thunar_shortcuts_model_move (THUNAR_SHORTCUTS_MODEL (child_model), child_src_path, child_dst_path);

          gtk_tree_path_free (child_src_path);
          gtk_tree_path_free (child_dst_path);

          /* make sure the new position is selectde */
          gtk_tree_selection_select_path (selection, dst_path);
        }

      /* release the dst path */
      gtk_tree_path_free (dst_path);

      /* finish the dnd operation */
      gtk_drag_finish (context, TRUE, FALSE, timestamp);
    }
  else
    {
      /* we cannot handle the drop */
      return FALSE;
    }

  return TRUE;
}



static gboolean
thunar_shortcuts_view_drag_motion (GtkWidget      *widget,
                                   GdkDragContext *context,
                                   gint            x,
                                   gint            y,
                                   guint           timestamp)
{
  GtkTreeViewDropPosition position = GTK_TREE_VIEW_DROP_BEFORE;
  ThunarShortcutsView    *view = THUNAR_SHORTCUTS_VIEW (widget);
  GdkDragAction           action = 0;
  GtkTreeModel           *model;
  GtkTreePath            *path = NULL;
  GdkAtom                 target;

  /* reset the "drop-file" of the icon renderer */
  g_object_set (G_OBJECT (view->icon_renderer), "drop-file", NULL, NULL);

  /* determine the drag target */
  target = gtk_drag_dest_find_target (widget, context, NULL);
  if (target == gdk_atom_intern_static_string ("text/uri-list"))
    {
      /* request the drop data on-demand (if we don't have it already) */
      if (G_UNLIKELY (!view->drop_data_ready))
        {
          /* request the drag data from the source */
          gtk_drag_get_data (widget, context, target, timestamp);

          /* gdk_drag_status() will be called by drag_data_received */
          return TRUE;
        }
      else
        {
          /* compute the drop position */
          thunar_shortcuts_view_compute_drop_actions (view, context, x, y, &path, &action, &position);
        }
    }
  else if (target == gdk_atom_intern_static_string ("GTK_TREE_MODEL_ROW"))
    {
      /* check the action that should be performed */
      if (gdk_drag_context_get_suggested_action (context) == GDK_ACTION_LINK || (gdk_drag_context_get_actions (context) & GDK_ACTION_LINK) != 0)
        action = GDK_ACTION_LINK;
      else if (gdk_drag_context_get_suggested_action (context) == GDK_ACTION_COPY || (gdk_drag_context_get_actions (context) & GDK_ACTION_COPY) != 0)
        action = GDK_ACTION_COPY;
      else if (gdk_drag_context_get_suggested_action (context) == GDK_ACTION_MOVE || (gdk_drag_context_get_actions (context) & GDK_ACTION_MOVE) != 0)
        action = GDK_ACTION_MOVE;
      else
        return FALSE;

      /* compute the drop position for the coordinates */
      path = thunar_shortcuts_view_compute_drop_position (view, x, y);
      if (path == NULL)
        return FALSE;

      /* check if path is about to append to the model */
      model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
      if (gtk_tree_path_get_indices (path)[0] >= gtk_tree_model_iter_n_children (model, NULL))
        {
          /* set the position to "after" and move the path to
           * point to the previous row instead; required to
           * get the highlighting in GtkTreeView correct.
           */
          position = GTK_TREE_VIEW_DROP_AFTER;
          gtk_tree_path_prev (path);
        }
    }
  else
    {
      /* we cannot handle the drop */
      return FALSE;
    }

  /* check if we have a drop path */
  if (G_LIKELY (path != NULL))
    {
      /* highlight the appropriate row */
      gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (view), path, position);
      gtk_tree_path_free (path);
    }
  else
    {
      /* remove highlight */
      gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (view), NULL, position);
    }

  /* tell Gdk whether we can drop here */
  gdk_drag_status (context, action, timestamp);

  return TRUE;
}



static void
thunar_shortcuts_view_drag_leave (GtkWidget      *widget,
                                  GdkDragContext *context,
                                  guint           timestamp)
{
  ThunarShortcutsView *view = THUNAR_SHORTCUTS_VIEW (widget);

  /* reset the "drop-file" of the icon renderer */
  g_object_set (G_OBJECT (view->icon_renderer), "drop-file", NULL, NULL);

  /* reset the "drop data ready" status and free the URI list */
  if (G_LIKELY (view->drop_data_ready))
    {
      thunar_g_list_free_full (view->drop_file_list);
      view->drop_data_ready = FALSE;
      view->drop_file_list = NULL;
    }

  /* schedule a repaint to make sure the special drop icon for the target row
   * is reset to its default (https://bugzilla.xfce.org/show_bug.cgi?id=2498).
   */
  gtk_widget_queue_draw (widget);

  /* call the parent's handler */
  (*GTK_WIDGET_CLASS (thunar_shortcuts_view_parent_class)->drag_leave) (widget, context, timestamp);
}



static gboolean
thunar_shortcuts_view_popup_menu (GtkWidget *widget)
{
  ThunarShortcutsView *view = THUNAR_SHORTCUTS_VIEW (widget);
  GtkTreeSelection    *selection;
  GtkTreeModel        *model;
  GtkTreeIter          iter;

  /* determine the selection for the tree view */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

  /* determine the selected row */
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      /* popup the context menu */
      thunar_shortcuts_view_context_menu (view, NULL, model, &iter);
      return TRUE;
    }
  else if (GTK_WIDGET_CLASS (thunar_shortcuts_view_parent_class)->popup_menu != NULL)
    {
      /* call the parent's "popup-menu" handler */
      return (*GTK_WIDGET_CLASS (thunar_shortcuts_view_parent_class)->popup_menu) (widget);
    }

  return FALSE;
}



static void
thunar_shortcuts_view_row_activated (GtkTreeView       *tree_view,
                                     GtkTreePath       *path,
                                     GtkTreeViewColumn *column)
{
  ThunarShortcutsView *view = THUNAR_SHORTCUTS_VIEW (tree_view);

  /* call the parent's "row-activated" handler */
  if (GTK_TREE_VIEW_CLASS (thunar_shortcuts_view_parent_class)->row_activated != NULL)
    (*GTK_TREE_VIEW_CLASS (thunar_shortcuts_view_parent_class)->row_activated) (tree_view, path, column);

  /* open the selected shortcut */
  thunar_shortcuts_view_open (view, THUNAR_ACTION_MANAGER_CHANGE_DIRECTORY);
}



static gboolean
thunar_shortcuts_view_selection_func (GtkTreeSelection *selection,
                                      GtkTreeModel     *model,
                                      GtkTreePath      *path,
                                      gboolean          path_currently_selected,
                                      gpointer          user_data)
{
  GtkTreeIter iter;
  gboolean    is_header;

  /* don't allow selecting headers */
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, THUNAR_SHORTCUTS_MODEL_COLUMN_IS_HEADER, &is_header, -1);
  return !is_header;
}



static void
thunar_shortcuts_view_context_menu_visibility_toggled (GtkCheckMenuItem *item,
                                                       GtkTreeModel     *model)
{
  gboolean             hidden;
  GtkTreePath         *path;
  GtkTreeRowReference *row;

  _thunar_return_if_fail (GTK_IS_CHECK_MENU_ITEM (item));
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));

  row = g_object_get_data (G_OBJECT (item), I_("thunar-shortcuts-row"));
  path = gtk_tree_row_reference_get_path (row);
  if (G_LIKELY (path != NULL))
    {
      hidden = !gtk_check_menu_item_get_active (item);
      thunar_shortcuts_model_set_hidden (THUNAR_SHORTCUTS_MODEL (model), path, hidden);
      gtk_tree_path_free (path);
    }
}



static void
thunar_shortcuts_view_context_menu_visibility (ThunarShortcutsView *view,
                                               GdkEventButton      *event,
                                               GtkTreeModel        *model)
{
  GtkTreeIter          iter;
  guint                mask = 0;
  ThunarShortcutGroup  group;
  GtkWidget           *menu;
  GtkWidget           *mi;
  gchar               *label;
  GtkTreeSelection    *selection;
  gboolean             visible;
  GtkTreePath         *path;
  GtkTreeModel        *child_model;
  GtkWidget           *submenu;

  /* unselect the items */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  gtk_tree_selection_unselect_all (GTK_TREE_SELECTION (selection));

  /* prepare the popup menu */
  menu = submenu = gtk_menu_new ();

  /* process all items below the header */
  child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
  if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (child_model), &iter))
      return;
  path = gtk_tree_model_get_path (child_model, &iter);
  do
    {
      /* get all the info */
      gtk_tree_model_get (GTK_TREE_MODEL (child_model), &iter,
                          THUNAR_SHORTCUTS_MODEL_COLUMN_GROUP, &group,
                          THUNAR_SHORTCUTS_MODEL_COLUMN_NAME, &label,
                          THUNAR_SHORTCUTS_MODEL_COLUMN_VISIBLE, &visible,
                          -1);

      if ((group & THUNAR_SHORTCUT_GROUP_HEADER) != 0)
        {
          /* get the mask of the group */
          if ((group & THUNAR_SHORTCUT_GROUP_DEVICES) != 0)
            mask = THUNAR_SHORTCUT_GROUP_DEVICES;
          else if ((group & THUNAR_SHORTCUT_GROUP_PLACES) != 0)
            mask = THUNAR_SHORTCUT_GROUP_PLACES;
          else if ((group & THUNAR_SHORTCUT_GROUP_NETWORK) != 0)
            mask = THUNAR_SHORTCUT_GROUP_NETWORK;
          else
            _thunar_assert_not_reached ();

          /* create menu items */
          mi = gtk_menu_item_new_with_label (label);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
          gtk_widget_show (mi);

          submenu = gtk_menu_new ();
          gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi), submenu);
        }
      else if ((group & mask) != 0)
        {
          mi = gtk_check_menu_item_new_with_label (label);
          gtk_menu_shell_append (GTK_MENU_SHELL (submenu), mi);
          gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mi), visible);
          g_object_set_data_full (G_OBJECT (mi), I_("thunar-shortcuts-row"),
                                  gtk_tree_row_reference_new (child_model, path),
                                  (GDestroyNotify) gtk_tree_row_reference_free);
          g_signal_connect (G_OBJECT (mi), "toggled",
            G_CALLBACK (thunar_shortcuts_view_context_menu_visibility_toggled), child_model);
          gtk_widget_show (mi);
        }

      g_free (label);
      gtk_tree_path_next (path);
    }
  while (gtk_tree_model_iter_next (child_model, &iter));

  gtk_tree_path_free (path);

  /* run the menu (taking over the floating reference on menu) */
  thunar_gtk_menu_run (GTK_MENU (menu));
}



static void
thunar_shortcuts_view_context_menu (ThunarShortcutsView *view,
                                    GdkEventButton      *event,
                                    GtkTreeModel        *model,
                                    GtkTreeIter         *iter)
{
  GtkTreePath         *path;
  ThunarFile          *file;
  GtkWidget           *item;
  GtkWidget           *window;
  gboolean             mutable;
  ThunarDevice        *device;
  ThunarShortcutGroup  group;
  gboolean             is_header;
  GFile               *location;
  GList               *files;
  ThunarMenu          *context_menu;

  /* check if this is an item menu or a header menu */
  gtk_tree_model_get (model, iter, THUNAR_SHORTCUTS_MODEL_COLUMN_IS_HEADER, &is_header, -1);
  if (is_header)
    {
      thunar_shortcuts_view_context_menu_visibility (view, event, model);
      return;
    }

  /* determine the tree path for the given iter */
  path = gtk_tree_model_get_path (model, iter);
  if (G_UNLIKELY (path == NULL))
    return;

  /* check whether the shortcut at the given path is mutable */
  gtk_tree_model_get (model, iter,
                      THUNAR_SHORTCUTS_MODEL_COLUMN_FILE, &file,
                      THUNAR_SHORTCUTS_MODEL_COLUMN_DEVICE, &device,
                      THUNAR_SHORTCUTS_MODEL_COLUMN_LOCATION, &location,
                      THUNAR_SHORTCUTS_MODEL_COLUMN_MUTABLE, &mutable,
                      THUNAR_SHORTCUTS_MODEL_COLUMN_GROUP, &group,
                      -1);

  context_menu = g_object_new (THUNAR_TYPE_MENU, "menu-type", THUNAR_MENU_TYPE_CONTEXT_SHORTCUTS_VIEW,
                                                 "action_mgr", view->action_mgr,
                                                 "force-section-open", TRUE, NULL);

  /* try to determine the ThunarFile of the location, if not set */
  if (location != NULL && file == NULL)
    {
      file = thunar_file_get (location, NULL);
      if (file != NULL && thunar_file_exists (file) == FALSE)
        {
          g_object_unref (G_OBJECT (file));
          file = NULL;
        }
    }

  /* try to determine the ThunarFile of the device, if not set */
  if (device != NULL && file == NULL && thunar_device_is_mounted (device))
    {
      GFile* mount_point = thunar_device_get_root (device);
      if (G_LIKELY (mount_point != NULL))
        {
          file = thunar_file_get (mount_point, NULL);
          g_object_unref (mount_point);
        }
    }

  if (device != NULL && !thunar_device_is_mounted (device))
    {
      g_object_set (G_OBJECT (view->action_mgr), "selected-device", device, NULL);
      thunar_action_manager_append_menu_item (view->action_mgr, GTK_MENU_SHELL (context_menu), THUNAR_ACTION_MANAGER_ACTION_OPEN, TRUE);
      thunar_action_manager_append_menu_item (view->action_mgr, GTK_MENU_SHELL (context_menu), THUNAR_ACTION_MANAGER_ACTION_OPEN_IN_TAB, TRUE);
      thunar_action_manager_append_menu_item (view->action_mgr, GTK_MENU_SHELL (context_menu), THUNAR_ACTION_MANAGER_ACTION_OPEN_IN_WINDOW, TRUE);
      xfce_gtk_menu_append_separator (GTK_MENU_SHELL (context_menu));
      thunar_menu_add_sections (context_menu, THUNAR_MENU_SECTION_MOUNTABLE);
      if (thunar_device_is_mounted (device))
        thunar_menu_add_sections (context_menu, THUNAR_MENU_SECTION_PROPERTIES);
    }
  else if (file != NULL)
    {
      files = g_list_append (NULL, file);
      g_object_set (G_OBJECT (view->action_mgr), "current-directory", file, NULL);
      thunar_action_manager_set_selection (view->action_mgr, files, device, NULL);
      g_list_free (files);
      if (thunar_g_file_is_trash    (thunar_file_get_file (file)) ||
          thunar_g_file_is_computer (thunar_file_get_file (file)) ||
          thunar_g_file_is_network  (thunar_file_get_file (file)))
        {
          thunar_action_manager_append_menu_item (view->action_mgr, GTK_MENU_SHELL (context_menu), THUNAR_ACTION_MANAGER_ACTION_OPEN, TRUE);
          thunar_action_manager_append_menu_item (view->action_mgr, GTK_MENU_SHELL (context_menu), THUNAR_ACTION_MANAGER_ACTION_OPEN_IN_TAB, TRUE);
          thunar_action_manager_append_menu_item (view->action_mgr, GTK_MENU_SHELL (context_menu), THUNAR_ACTION_MANAGER_ACTION_OPEN_IN_WINDOW, TRUE);
          xfce_gtk_menu_append_separator (GTK_MENU_SHELL (context_menu));
          thunar_menu_add_sections (context_menu, THUNAR_MENU_SECTION_EMPTY_TRASH);
        }
      else
        {
          thunar_menu_add_sections (context_menu, THUNAR_MENU_SECTION_OPEN);
          thunar_menu_add_sections (context_menu, THUNAR_MENU_SECTION_SENDTO
                                                | THUNAR_MENU_SECTION_CREATE_NEW_FILES
                                                | THUNAR_MENU_SECTION_COPY_PASTE
                                                | THUNAR_MENU_SECTION_CUSTOM_ACTIONS);
        }

      thunar_menu_add_sections (context_menu, THUNAR_MENU_SECTION_MOUNTABLE);
      thunar_menu_add_sections (context_menu, THUNAR_MENU_SECTION_PROPERTIES);
    }
  else if (location != NULL)
    {
      g_object_set (G_OBJECT (view->action_mgr), "selected-location", location, NULL);
      thunar_action_manager_append_menu_item (view->action_mgr, GTK_MENU_SHELL (context_menu), THUNAR_ACTION_MANAGER_ACTION_OPEN, TRUE);
      thunar_action_manager_append_menu_item (view->action_mgr, GTK_MENU_SHELL (context_menu), THUNAR_ACTION_MANAGER_ACTION_OPEN_IN_TAB, TRUE);
      thunar_action_manager_append_menu_item (view->action_mgr, GTK_MENU_SHELL (context_menu), THUNAR_ACTION_MANAGER_ACTION_OPEN_IN_WINDOW, TRUE);
    }
  else
      g_error("Unknown type");

  if (group == THUNAR_SHORTCUT_GROUP_PLACES_BOOKMARKS)
  {
      xfce_gtk_menu_append_separator (GTK_MENU_SHELL (context_menu));
      item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_SHORTCUTS_VIEW_ACTION_REMOVE_BOOKMARK), G_OBJECT (model), GTK_MENU_SHELL (context_menu));
      g_object_set_data_full (G_OBJECT (item), I_("thunar-shortcuts-row"),
                              gtk_tree_row_reference_new (model, path),
                              (GDestroyNotify) gtk_tree_row_reference_free);
      item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_SHORTCUTS_VIEW_ACTION_RENAME_BOOKMARK), G_OBJECT (view), GTK_MENU_SHELL (context_menu));
      g_object_set_data_full (G_OBJECT (item), I_("thunar-shortcuts-row"),
                              gtk_tree_row_reference_new (model, path),
                              (GDestroyNotify) gtk_tree_row_reference_free);
  }

  thunar_gtk_menu_hide_accel_labels (GTK_MENU (context_menu));
  gtk_widget_show_all (GTK_WIDGET (context_menu));
  window = gtk_widget_get_toplevel (GTK_WIDGET (view));
  thunar_window_redirect_menu_tooltips_to_statusbar (THUNAR_WINDOW (window), GTK_MENU (context_menu));
  thunar_gtk_menu_run (GTK_MENU (context_menu));

  /* return the focus to the current folder */
  thunar_shortcuts_view_select_by_file (view, view->current_directory);

  /* clean up */
  if (G_LIKELY (file != NULL))
    g_object_unref (G_OBJECT (file));
  if (G_UNLIKELY (device != NULL))
    g_object_unref (G_OBJECT (device));
  gtk_tree_path_free (path);
}



static gboolean
thunar_shortcuts_view_remove_activated (ThunarShortcutsView *view,
                                        GtkWidget           *item)
{
  GtkTreeRowReference *row;
  GtkTreeModel        *child_model;
  GtkTreePath         *path;
  GtkTreePath         *child_path;

  row = g_object_get_data (G_OBJECT (item), I_("thunar-shortcuts-row"));
  path = gtk_tree_row_reference_get_path (row);
  if (G_LIKELY (path != NULL))
    {
      child_path = gtk_tree_model_filter_convert_path_to_child_path (GTK_TREE_MODEL_FILTER (view), path);
      gtk_tree_path_free (path);

      child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (view));
      thunar_shortcuts_model_remove (THUNAR_SHORTCUTS_MODEL (child_model), child_path);
      gtk_tree_path_free (child_path);
    }

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static void
thunar_shortcuts_view_editing_canceled (GtkCellRenderer     *renderer,
                                        ThunarShortcutsView *view)
{
  g_object_set (G_OBJECT (renderer), "editable", FALSE, NULL);

  g_signal_handlers_disconnect_by_func (G_OBJECT (renderer),
                                       G_CALLBACK (thunar_shortcuts_view_editing_canceled),
                                       view);
}



static gboolean
thunar_shortcuts_view_rename_activated (ThunarShortcutsView *view,
                                        GtkWidget           *item)
{
  GtkTreeRowReference *row;
  GtkTreeViewColumn   *column;
  GtkCellRenderer     *renderer;
  GtkTreePath         *path;
  GList               *renderers;

  row = g_object_get_data (G_OBJECT (item), I_("thunar-shortcuts-row"));
  path = gtk_tree_row_reference_get_path (row);
  if (G_LIKELY (path != NULL))
    {
      /* determine the text renderer */
      column = gtk_tree_view_get_column (GTK_TREE_VIEW (view), 0);
      renderers = gtk_cell_layout_get_cells  (GTK_CELL_LAYOUT (column));
      renderer = g_list_nth_data (renderers, 3);

      /* make sure the text renderer is editable */
      g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);

      /* set up signals for the cell renderer */
      g_signal_connect (G_OBJECT (renderer), "editing-canceled",
                        G_CALLBACK (thunar_shortcuts_view_editing_canceled), view);

      /* tell the tree view to start editing the given row */
      gtk_tree_view_set_cursor_on_cell (GTK_TREE_VIEW (view), path, column, renderer, TRUE);

      /* cleanup */
      gtk_tree_path_free (path);
      g_list_free (renderers);
    }

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static void
thunar_shortcuts_view_renamed (GtkCellRenderer     *renderer,
                               const gchar         *path_string,
                               const gchar         *text,
                               ThunarShortcutsView *view)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  GtkTreeModel *child_model;
  GtkTreeIter   child_iter;

  /* reset the editable flag */
  g_object_set (G_OBJECT (renderer), "editable", FALSE, NULL);

  /* perform the rename */
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
  if (gtk_tree_model_get_iter_from_string (model, &iter, path_string))
    {
      child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
      gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &child_iter, &iter);
      thunar_shortcuts_model_rename (THUNAR_SHORTCUTS_MODEL (child_model), &child_iter, text);
    }
}



static GdkDragAction
thunar_shortcuts_view_compute_drop_actions (ThunarShortcutsView     *view,
                                            GdkDragContext          *context,
                                            gint                     x,
                                            gint                     y,
                                            GtkTreePath            **path_return,
                                            GdkDragAction           *action_return,
                                            GtkTreeViewDropPosition *position_return)
{
  GtkTreeViewColumn *column;
  GdkDragAction      actions = 0;
  GdkRectangle       cell_area;
  GtkTreeModel      *model;
  GtkTreePath       *path;
  GtkTreeIter        iter;
  ThunarFile        *file;
  gint               cell_y;
  GList             *lp;
  guint              n;

  /* determine the shortcuts model */
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));

  /* determine the path for x/y */
  if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (view), x, y, &path, &column, NULL, &cell_y))
    {
      /* determine the background area of the column */
      gtk_tree_view_get_background_area (GTK_TREE_VIEW (view), path, column, &cell_area);

      /* check if 1/6th inside the cell and the path is within the model */
      if ((cell_y > (cell_area.height / 6) && cell_y < (cell_area.height - cell_area.height / 6))
          && gtk_tree_model_get_iter (model, &iter, path))
        {
          /* determine the file for the iterator */
          gtk_tree_model_get (model, &iter, THUNAR_SHORTCUTS_MODEL_COLUMN_FILE, &file, -1);
          if (G_LIKELY (file != NULL))
            {
              /* check if the file accepts the drop */
              actions = thunar_file_accepts_drop (file, view->drop_file_list, context, action_return);
              if (G_LIKELY (actions != 0))
                {
                  /* we can drop into this location */
                  *position_return = GTK_TREE_VIEW_DROP_INTO_OR_AFTER;
                  *path_return = gtk_tree_path_copy (path);

                  /* display a drop icon for the file */
                  g_object_set (G_OBJECT (view->icon_renderer), "drop-file", file, NULL);
                }

              /* release the file */
              g_object_unref (G_OBJECT (file));
            }
        }

      /* release the path */
      gtk_tree_path_free (path);
    }

  /* check if we cannot drop into the shortcut */
  if (G_UNLIKELY (actions == 0))
    {
      /* check if any of the paths is not directory but only up to 100, if
       * someone drags too many directories it may slowdown or even freeze
       * Thunar while moving the cursor.
       */
      for (lp = view->drop_file_list, n = 0; lp != NULL && n < 100; lp = lp->next, ++n)
        {
          GFileInfo *info;
          gboolean   is_directory;

          info = g_file_query_info (lp->data,
                                    G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                    G_FILE_QUERY_INFO_NONE,
                                    NULL, NULL);

          if (G_UNLIKELY (info == NULL))
            return 0;

          is_directory = g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY;
          g_object_unref (info);

          if (!is_directory)
            return 0;
        }

      /* check the action that should be performed */
      if (gdk_drag_context_get_suggested_action (context) == GDK_ACTION_LINK || (gdk_drag_context_get_actions (context) & GDK_ACTION_LINK) != 0)
        actions = GDK_ACTION_LINK;
      else if (gdk_drag_context_get_suggested_action (context) == GDK_ACTION_COPY || (gdk_drag_context_get_actions (context) & GDK_ACTION_COPY) != 0)
        actions = GDK_ACTION_COPY;
      else if (gdk_drag_context_get_suggested_action (context) == GDK_ACTION_MOVE || (gdk_drag_context_get_actions (context) & GDK_ACTION_MOVE) != 0)
        actions = GDK_ACTION_MOVE;
      else
        return 0;

      /* but maybe we can add as shortcut */
      path = thunar_shortcuts_view_compute_drop_position (view, x, y);

      if (path == NULL)
        return 0;

      /* check if path is about to append to the model */
      if (gtk_tree_path_get_indices (path)[0] >= gtk_tree_model_iter_n_children (model, NULL))
        {
          /* set the position to "after" and move the path to
           * point to the previous row instead; required to
           * get the highlighting in GtkTreeView correct.
           */
          *position_return = GTK_TREE_VIEW_DROP_AFTER;
          gtk_tree_path_prev (path);
        }
      else
        {
          /* drop before the path */
          *position_return = GTK_TREE_VIEW_DROP_BEFORE;
        }

      /* got it */
      *action_return = actions;
      *path_return = path;
    }

  return actions;
}



static GtkTreePath*
thunar_shortcuts_view_compute_drop_position (ThunarShortcutsView *view,
                                             gint                 x,
                                             gint                 y)
{
  GtkTreeViewColumn *column;
  GtkTreeModel      *model;
  GdkRectangle       area;
  GtkTreePath       *path;
  gint               n_rows;
  gboolean           result;
  GtkTreePath       *child_path;
  GtkTreeModel      *child_model;

  _thunar_return_val_if_fail (gtk_tree_view_get_model (GTK_TREE_VIEW (view)) != NULL, NULL);
  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view), NULL);

  /* query the number of rows in the model */
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
  n_rows = gtk_tree_model_iter_n_children (model, NULL);

  if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (view), x, y,
                                     &path, &column, &x, &y))
    {
      /* determine the exact path of the row the user is trying to drop
       * (taking into account the relative y position)
       */
      gtk_tree_view_get_background_area (GTK_TREE_VIEW (view), path, column, &area);
      if (y >= area.height / 2)
        gtk_tree_path_next (path);

      child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));

      /* find a suitable drop path (we cannot drop into the default shortcuts list) */
      for (; gtk_tree_path_get_indices (path)[0] < n_rows; gtk_tree_path_next (path))
        {
          child_path = gtk_tree_model_filter_convert_path_to_child_path (GTK_TREE_MODEL_FILTER (model), path);
          result = thunar_shortcuts_model_drop_possible (THUNAR_SHORTCUTS_MODEL (child_model), child_path);
          gtk_tree_path_free (child_path);
          if (result)
            return path;
        }

      gtk_tree_path_free (path);
    }

  return NULL;
}



static void
thunar_shortcuts_view_drop_uri_list (ThunarShortcutsView *view,
                                     GList               *path_list,
                                     GtkTreePath         *dst_path)
{
  GtkTreeModel *model;
  GtkTreePath  *path;
  ThunarFile   *file;
  GError       *error = NULL;
  GList        *lp;
  GtkTreeModel *child_model;
  GtkTreePath  *child_path;

  /* take a copy of the destination path */
  path = gtk_tree_path_copy (dst_path);

  /* process the URIs one-by-one and stop on error */
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
  child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
  for (lp = path_list; lp != NULL; lp = lp->next)
    {
      file = thunar_file_get (lp->data, &error);
      if (G_UNLIKELY (file == NULL))
        break;

      /* make sure, that only directories gets added to the shortcuts list */
      if (G_UNLIKELY (!thunar_file_is_directory (file)))
        {
          g_set_error (&error, G_FILE_ERROR, G_FILE_ERROR_NOTDIR,
                       _("The path \"%s\" does not refer to a directory"),
                       thunar_file_get_display_name (file));
          g_object_unref (file);
          break;
        }

      child_path = gtk_tree_model_filter_convert_path_to_child_path (GTK_TREE_MODEL_FILTER (model), path);
      thunar_shortcuts_model_add (THUNAR_SHORTCUTS_MODEL (child_model), child_path, file);
      gtk_tree_path_free (child_path);

      g_object_unref (file);
      gtk_tree_path_next (path);
    }

  /* release the tree path copy */
  gtk_tree_path_free (path);

  if (G_UNLIKELY (error != NULL))
    {
      /* display an error message to the user */
      thunar_dialogs_show_error (GTK_WIDGET (view), error, _("Failed to add new shortcut"));

      /* release the error */
      g_error_free (error);
    }
}



static void
thunar_shortcuts_view_open (ThunarShortcutsView                 *view,
                            ThunarActionManagerFolderOpenAction  open_in)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  ThunarFile       *file;
  GList            *files;
  ThunarDevice     *device;
  GFile            *location;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view));

  /* determine the selected item */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

  /* avoid dealing with invalid selections */
  if (!GTK_IS_TREE_SELECTION (selection))
    return;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      /* determine the file for the shortcut at the given tree iterator */
      gtk_tree_model_get (model, &iter,
                          THUNAR_SHORTCUTS_MODEL_COLUMN_FILE, &file,
                          THUNAR_SHORTCUTS_MODEL_COLUMN_DEVICE, &device,
                          THUNAR_SHORTCUTS_MODEL_COLUMN_LOCATION, &location,
                          -1);

      if (device != NULL)
         {
            g_object_set (G_OBJECT (view->action_mgr), "selected-device", device, NULL);
         }
       else if (file != NULL)
         {
           files = g_list_append (NULL, file);
           thunar_action_manager_set_selection (view->action_mgr, files, device, NULL);
           g_list_free (files);
         }
       else if (location != 0)
         {
           g_object_set (G_OBJECT (view->action_mgr), "selected-location", location, NULL);
         }

      thunar_action_manager_activate_selected_files (view->action_mgr, (ThunarActionManagerFolderOpenAction) open_in, NULL);

      /* return the focus to the current folder, unless the folder changed */
      if (open_in != THUNAR_ACTION_MANAGER_CHANGE_DIRECTORY)
        thunar_shortcuts_view_select_by_file (view, view->current_directory);

      if (file != NULL)
        g_object_unref (file);

      if (device != NULL)
        g_object_unref (device);

      if (location != NULL)
        g_object_unref (location);
    }
}



static void
thunar_shortcuts_view_start_spinner (ThunarShortcutsView *view,
                                    ThunarDevice *device,
                                    gpointer      user_data)
{
  GtkTreeModel        *model;
  GtkTreeModel        *child_model;

  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view));

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
  child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
  thunar_shortcuts_model_set_busy (THUNAR_SHORTCUTS_MODEL (child_model), device, TRUE);
}



static void
thunar_shortcuts_view_stop_spinner (ThunarShortcutsView *view,
                                    ThunarDevice *device,
                                    gpointer      user_data)
{
  GtkTreeModel        *model;
  GtkTreeModel        *child_model;

  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view));

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
  child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
  thunar_shortcuts_model_set_busy (THUNAR_SHORTCUTS_MODEL (child_model), device, FALSE);

  /* return the focus to the current folder */
  thunar_shortcuts_view_select_by_file (view, view->current_directory);
}



static void
thunar_shortcuts_view_eject (ThunarShortcutsView *view)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  ThunarDevice     *device;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view));

  /* determine the selected item */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      /* determine the device/mount for the shortcut at the given tree iterator */
      gtk_tree_model_get (model, &iter, THUNAR_SHORTCUTS_MODEL_COLUMN_DEVICE, &device, -1);

      if (G_LIKELY (THUNAR_IS_DEVICE (device)))
        {
          g_object_set (G_OBJECT (view->action_mgr), "selected-device", device, NULL);
          thunar_action_manager_action_eject (view->action_mgr);
          g_object_unref (G_OBJECT (device));
        }
    }

  /* return the focus to the current folder */
  thunar_shortcuts_view_select_by_file (view, view->current_directory);
}



/**
 * thunar_shortcuts_view_new:
 *
 * Allocates a new #ThunarShortcutsView instance and associates
 * it with the default #ThunarShortcutsModel instance.
 *
 * Return value: the newly allocated #ThunarShortcutsView instance.
 **/
GtkWidget*
thunar_shortcuts_view_new (void)
{
  ThunarShortcutsModel *model;
  GtkWidget            *view;
  GtkTreeModel         *filter_model;

  model = thunar_shortcuts_model_get_default ();
  filter_model = gtk_tree_model_filter_new (GTK_TREE_MODEL (model), NULL);
  gtk_tree_model_filter_set_visible_column (GTK_TREE_MODEL_FILTER (filter_model), THUNAR_SHORTCUTS_MODEL_COLUMN_VISIBLE);
  g_object_unref (G_OBJECT (model));

  view = g_object_new (THUNAR_TYPE_SHORTCUTS_VIEW, "model", filter_model, NULL);
  g_object_unref (G_OBJECT (filter_model));

  return view;
}



/**
 * thunar_shortcuts_view_select_by_file:
 * @view : a #ThunarShortcutsView instance.
 * @file : a #ThunarFile instance.
 *
 * Looks up the first shortcut that refers to @file in @view and selects it.
 * If @file is not present in the underlying #ThunarShortcutsModel, no
 * shortcut will be selected afterwards.
 **/
void
thunar_shortcuts_view_select_by_file (ThunarShortcutsView *view,
                                      ThunarFile          *file)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  GtkTreeModel     *child_model;
  GtkTreeIter       child_iter;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  /* get the selection */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
  child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));

  /* try to lookup a tree iter for the given file */
  if (thunar_shortcuts_model_iter_for_file (THUNAR_SHORTCUTS_MODEL (child_model), file, &child_iter)
      && gtk_tree_model_filter_convert_child_iter_to_iter (GTK_TREE_MODEL_FILTER (model), &iter, &child_iter))
    gtk_tree_selection_select_iter (selection, &iter);
  else
     gtk_tree_selection_unselect_all (selection);
}



void
thunar_shortcuts_view_toggle_padding (ThunarShortcutsView *view,
                                      gboolean             enable)
{
  gtk_tree_view_column_set_attributes (view->column, view->padding_renderer,
                                       "visible", enable ?
                                          THUNAR_SHORTCUTS_MODEL_COLUMN_CAN_EJECT :
                                          THUNAR_SHORTCUTS_MODEL_COLUMN_HIDDEN,
                                       NULL);

  view->padding_enabled = enable;
}



static void
thunar_shortcuts_view_new_files_created (ThunarShortcutsView *view,
                                         GList               *files_to_selected)
{
  ThunarFile *file;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view));

  if (files_to_selected == NULL)
    return;

  /* Just pick the first file */
  file = thunar_file_get (files_to_selected->data, NULL);

  /* And open it, if it is a directory (It always should be a directory for shortcuts-view) */
  if (G_LIKELY (file != NULL))
    {
      if (G_LIKELY (thunar_file_is_directory (file)))
        thunar_navigator_change_directory (THUNAR_NAVIGATOR (view), file);
      g_object_unref (file);
    }
}
