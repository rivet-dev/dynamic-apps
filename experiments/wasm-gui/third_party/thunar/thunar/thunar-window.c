/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include <gdk/gdkkeysyms.h>

#include <libxfce4util/libxfce4util.h>

#include <thunar/thunar-action-manager.h>
#include <thunar/thunar-application.h>
#include <thunar/thunar-browser.h>
#include <thunar/thunar-clipboard-manager.h>
#include <thunar/thunar-compact-view.h>
#include <thunar/thunar-details-view.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-shortcuts-pane.h>
#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-history.h>
#include <thunar/thunar-icon-view.h>
#include <thunar/thunar-job-operation-history.h>
#include <thunar/thunar-location-buttons.h>
#include <thunar/thunar-location-entry.h>
#include <thunar/thunar-marshal.h>
#include <thunar/thunar-menu.h>
#include <thunar/thunar-pango-extensions.h>
#include <thunar/thunar-preferences-dialog.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-util.h>
#include <thunar/thunar-statusbar.h>
#include <thunar/thunar-tree-pane.h>
#include <thunar/thunar-window.h>
#include <thunar/thunar-device-monitor.h>
#include <thunar/thunar-toolbar-editor.h>
#include <thunar/thunar-thumbnailer.h>

#include <glib.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
  PROP_ZOOM_LEVEL,
  PROP_DIRECTORY_SPECIFIC_SETTINGS,
};

/* Signal identifiers */
enum
{
  BACK,
  RELOAD,
  TOGGLE_SIDEPANE,
  TOGGLE_MENUBAR,
  ZOOM_IN,
  ZOOM_OUT,
  ZOOM_RESET,
  TAB_CHANGE,
  LAST_SIGNAL,
};

/* Trash infobar response ids */
enum
{
    EMPTY,
    RESTORE
};

struct _ThunarBookmark
{
  GFile *g_file;
  gchar *name;
};
typedef struct _ThunarBookmark ThunarBookmark;

typedef struct
{
    ThunarWindow *window;
    gchar        *action_name;
} UCAActivation;



static void      thunar_window_screen_changed             (GtkWidget              *widget,
                                                           GdkScreen              *old_screen,
                                                           gpointer                userdata);
static void      thunar_window_dispose                    (GObject                *object);
static void      thunar_window_finalize                   (GObject                *object);
static gboolean  thunar_window_delete                     (GtkWidget              *widget,
                                                           GdkEvent               *event,
                                                           gpointer                data);
static void      thunar_window_get_property               (GObject                *object,
                                                           guint                   prop_id,
                                                           GValue                 *value,
                                                           GParamSpec             *pspec);
static void      thunar_window_set_property               (GObject                *object,
                                                           guint                   prop_id,
                                                           const GValue           *value,
                                                           GParamSpec             *pspec);
static gboolean  thunar_window_reload                     (ThunarWindow           *window,
                                                           gboolean                reload_info);
static gboolean  thunar_window_toggle_sidepane            (ThunarWindow           *window);
static gboolean  thunar_window_zoom_in                    (ThunarWindow           *window);
static gboolean  thunar_window_zoom_out                   (ThunarWindow           *window);
static gboolean  thunar_window_zoom_reset                 (ThunarWindow           *window);
static gboolean  thunar_window_tab_change                 (ThunarWindow           *window,
                                                           gint                    nth);
static void      thunar_window_realize                    (GtkWidget              *widget);
static void      thunar_window_unrealize                  (GtkWidget              *widget);
static gboolean  thunar_window_configure_event            (GtkWidget              *widget,
                                                           GdkEventConfigure      *event);
static void      thunar_window_notebook_switch_page       (GtkWidget              *notebook,
                                                           GtkWidget              *page,
                                                           guint                   page_num,
                                                           ThunarWindow           *window);
static void      thunar_window_notebook_page_added        (GtkWidget              *notebook,
                                                           GtkWidget              *page,
                                                           guint                   page_num,
                                                           ThunarWindow           *window);
static void      thunar_window_notebook_page_removed      (GtkWidget              *notebook,
                                                           GtkWidget              *page,
                                                           guint                   page_num,
                                                           ThunarWindow           *window);
static gboolean  thunar_window_notebook_button_press_event(GtkWidget              *notebook,
                                                           GdkEventButton         *event,
                                                           ThunarWindow           *window);
static gboolean  thunar_window_notebook_popup_menu        (GtkWidget              *notebook,
                                                           ThunarWindow           *window);
static gpointer  thunar_window_notebook_create_window     (GtkWidget              *notebook,
                                                           GtkWidget              *page,
                                                           gint                    x,
                                                           gint                    y,
                                                           ThunarWindow           *window);
static void      thunar_window_notebook_update_title       (GtkWidget              *label);
static GtkWidget*thunar_window_notebook_insert_page       (ThunarWindow           *window,
                                                           ThunarFile             *directory,
                                                           GType                   view_type,
                                                           gint                    position,
                                                           ThunarHistory          *history);
static void      thunar_window_notebook_select_current_page(ThunarWindow           *window);

static GtkWidget*thunar_window_paned_notebooks_add        (ThunarWindow           *window);
static void      thunar_window_paned_notebooks_switch     (ThunarWindow           *window);
static void      thunar_window_paned_notebooks_indicate_focus (ThunarWindow       *window,
                                                           GtkWidget              *notebook);
static gboolean  thunar_window_split_view_is_active       (ThunarWindow           *window);

static void      thunar_window_update_location_bar_visible(ThunarWindow           *window);
static void      thunar_window_install_sidepane           (ThunarWindow           *window,
                                                           GType                   type);
static void      thunar_window_start_open_location        (ThunarWindow           *window,
                                                           const gchar            *initial_text);
static void      thunar_window_resume_search              (ThunarWindow           *window,
                                                           const gchar            *initial_text);
static gint      thunar_window_reset_view_type_idle       (gpointer                window_ptr);
static gboolean  thunar_window_action_open_new_tab        (ThunarWindow           *window,
                                                           GtkWidget              *menu_item);
static gboolean  thunar_window_action_open_new_window     (ThunarWindow           *window,
                                                           GtkWidget              *menu_item);
static gboolean  thunar_window_action_detach_tab          (ThunarWindow           *window,
                                                           GtkWidget              *menu_item);
static gboolean  thunar_window_action_close_all_windows   (ThunarWindow           *window,
                                                           GtkWidget              *menu_item);
static gboolean  thunar_window_action_close_tab           (ThunarWindow           *window,
                                                           GtkWidget              *menu_item);
static gboolean  thunar_window_action_close_window        (ThunarWindow           *window,
                                                           GtkWidget              *menu_item);
static gboolean  thunar_window_action_undo                (ThunarWindow           *window,
                                                           GtkWidget              *menu_item);
static gboolean  thunar_window_action_redo                (ThunarWindow           *window,
                                                           GtkWidget              *menu_item);
static gboolean  thunar_window_action_preferences         (ThunarWindow           *window,
                                                           GtkWidget              *menu_item);
static gboolean  thunar_window_action_reload              (ThunarWindow           *window,
                                                           GtkWidget              *menu_item);
static gboolean  thunar_window_action_toggle_split_view   (ThunarWindow           *window);
static gboolean  thunar_window_action_switch_next_tab     (ThunarWindow           *window);
static gboolean  thunar_window_action_switch_previous_tab (ThunarWindow           *window);
static gboolean  thunar_window_action_locationbar_entry   (ThunarWindow           *window);
static gboolean  thunar_window_action_locationbar_buttons (ThunarWindow           *window);
static gboolean  thunar_window_action_shortcuts_changed   (ThunarWindow           *window);
static gboolean  thunar_window_action_tree_changed        (ThunarWindow           *window);
static gboolean  thunar_window_action_image_preview       (ThunarWindow           *window);
static gboolean  thunar_window_action_statusbar_changed   (ThunarWindow           *window);
static void      thunar_window_action_menubar_update      (ThunarWindow           *window);
static gboolean  thunar_window_action_menubar_changed     (ThunarWindow           *window);
static gboolean  thunar_window_action_detailed_view       (ThunarWindow           *window);
static gboolean  thunar_window_action_icon_view           (ThunarWindow           *window);
static gboolean  thunar_window_action_compact_view        (ThunarWindow           *window);
static gboolean  thunar_window_action_show_toolbar_editor (ThunarWindow           *window);
static void      thunar_window_replace_view               (ThunarWindow           *window,
                                                           GtkWidget              *view,
                                                           GType                   view_type);
static void      thunar_window_action_view_changed        (ThunarWindow           *window,
                                                           GType                   view_type);
static gboolean  thunar_window_action_go_up               (ThunarWindow           *window);
static gboolean  thunar_window_action_back                (ThunarWindow           *window);
static gboolean  thunar_window_action_forward             (ThunarWindow           *window);
static gboolean  thunar_window_action_open_home           (ThunarWindow           *window);
static gboolean  thunar_window_action_open_desktop        (ThunarWindow           *window);
static gboolean  thunar_window_action_open_computer       (ThunarWindow           *window);
static gboolean  thunar_window_action_open_templates      (ThunarWindow           *window);
static gboolean  thunar_window_action_open_file_system    (ThunarWindow           *window);
static gboolean  thunar_window_action_open_recent         (ThunarWindow           *window);
static gboolean  thunar_window_action_open_trash          (ThunarWindow           *window);
static gboolean  thunar_window_action_open_network        (ThunarWindow           *window);
static void      thunar_window_action_open_bookmark       (GFile                  *g_file);
static gboolean  thunar_window_action_open_location       (ThunarWindow           *window);
static gboolean  thunar_window_action_contents            (ThunarWindow           *window);
static gboolean  thunar_window_action_about               (ThunarWindow           *window);
static gboolean  thunar_window_action_show_hidden         (ThunarWindow           *window);
static gboolean  thunar_window_action_show_highlight      (ThunarWindow           *window);
static gboolean  thunar_window_propagate_key_event        (GtkWindow              *window,
                                                           GdkEvent               *key_event,
                                                           gpointer                user_data);
static gboolean  thunar_window_after_propagate_key_event  (GtkWindow              *window,
                                                           GdkEvent               *key_event,
                                                           gpointer                user_data);
static gboolean  thunar_window_action_open_file_menu      (ThunarWindow           *window);
static void      thunar_window_current_directory_changed  (ThunarFile             *current_directory,
                                                           ThunarWindow           *window);
static void      thunar_window_menu_item_selected         (ThunarWindow           *window,
                                                           GtkWidget              *menu_item);
static void      thunar_window_menu_item_deselected       (ThunarWindow           *window,
                                                           GtkWidget              *menu_item);
static void      thunar_window_notify_loading             (ThunarView             *view,
                                                           GParamSpec             *pspec,
                                                           ThunarWindow           *window);
static void      thunar_window_device_pre_unmount         (ThunarDeviceMonitor    *device_monitor,
                                                           ThunarDevice           *device,
                                                           GFile                  *root_file,
                                                           ThunarWindow           *window);
static void      thunar_window_device_changed             (ThunarDeviceMonitor    *device_monitor,
                                                           ThunarDevice           *device,
                                                           ThunarWindow           *window);
static gboolean  thunar_window_save_paned                 (ThunarWindow           *window);
static gboolean  thunar_window_save_paned_notebooks       (ThunarWindow           *window);
static gboolean  thunar_window_save_geometry_timer        (gpointer                user_data);
static void      thunar_window_save_geometry_timer_destroy(gpointer                user_data);
static void      thunar_window_set_zoom_level             (ThunarWindow           *window,
                                                           ThunarZoomLevel         zoom_level);
static void      thunar_window_update_window_icon         (ThunarWindow           *window);
static void      thunar_window_create_menu                (ThunarWindow           *window,
                                                           ThunarWindowAction      action,
                                                           GCallback               cb_update_menu);
static void      thunar_window_select_files               (ThunarWindow           *window,
                                                           GList                  *files_to_select);
static void      thunar_window_update_file_menu           (ThunarWindow           *window,
                                                           GtkWidget              *menu);
static void      thunar_window_update_edit_menu           (ThunarWindow           *window,
                                                           GtkWidget              *menu);
static void      thunar_window_update_view_menu           (ThunarWindow           *window,
                                                           GtkWidget              *menu);
static void      thunar_window_update_go_menu             (ThunarWindow           *window,
                                                           GtkWidget              *menu);
static void      thunar_window_update_bookmarks_menu      (ThunarWindow           *window,
                                                           GtkWidget              *menu);
static void      thunar_window_update_help_menu           (ThunarWindow           *window,
                                                           GtkWidget              *menu);
static void      thunar_window_binding_create             (ThunarWindow           *window,
                                                           gpointer                src_object,
                                                           const gchar            *src_prop,
                                                           gpointer                dst_object,
                                                           const                   gchar *dst_prop,
                                                           GBindingFlags           flags);
static gboolean  thunar_window_history_clicked            (GtkWidget              *button,
                                                           GdkEventButton         *event,
                                                           ThunarWindow           *window);
static gboolean  thunar_window_toolbar_button_clicked     (GtkWidget              *button,
                                                           GdkEventButton         *event,
                                                           ThunarWindow           *window);
static gboolean  thunar_window_open_parent_clicked        (GtkWidget              *button,
                                                           GdkEventButton         *event,
                                                           ThunarWindow           *window);
static gboolean  thunar_window_open_home_clicked          (GtkWidget              *button,
                                                           GdkEventButton         *event,
                                                           ThunarWindow           *window);
static gboolean  thunar_window_button_press_event         (GtkWidget              *view,
                                                           GdkEventButton         *event,
                                                           ThunarWindow           *window);
static void      thunar_window_history_changed            (ThunarWindow           *window);
static void      thunar_window_update_bookmarks           (ThunarWindow           *window);
static void      thunar_window_free_bookmarks             (ThunarWindow           *window);
static void      thunar_window_menu_add_bookmarks         (ThunarWindow           *window,
                                                           GtkMenuShell           *view_menu);
static gboolean   thunar_window_check_uca_key_activation                 (ThunarWindow           *window,
                                                                          GdkEventKey            *key_event,
                                                                          gpointer                user_data);
static void       thunar_window_set_directory_specific_settings          (ThunarWindow           *window,
                                                                          gboolean                directory_specific_settings);
static GType      thunar_window_view_type_for_directory                  (ThunarWindow           *window,
                                                                          ThunarFile             *directory);
static gboolean   thunar_window_action_clear_directory_specific_settings (ThunarWindow           *window);
static void       thunar_window_trash_infobar_clicked                    (GtkInfoBar             *info_bar,
                                                                          gint                    response_id,
                                                                          ThunarWindow           *window);
static void       thunar_window_update_embedded_image_preview            (ThunarWindow           *window);
static void       thunar_window_update_standalone_image_preview          (ThunarWindow           *window);
static void       thunar_window_selection_changed                        (ThunarWindow           *window);
static void       thunar_window_finished_thumbnailing                    (ThunarWindow           *window,
                                                                          guint                   request,
                                                                          ThunarThumbnailer      *thumbnailer);
static void       thunar_window_recent_reload                            (GtkRecentManager       *recent_manager,
                                                                          ThunarWindow           *window);
static void       thunar_window_catfish_dialog_configure                 (GtkWidget              *entry);
static gboolean   thunar_window_paned_notebooks_update_orientation       (ThunarWindow           *window);
static void       thunar_window_location_toolbar_create                  (ThunarWindow           *window);
static void       thunar_window_update_location_toolbar                  (GFileMonitor           *monitor,
                                                                          GFile                  *file,
                                                                          GFile                  *other_file,
                                                                          GFileMonitorEvent       event_type,
                                                                          ThunarWindow           *window);
static void       thunar_window_location_toolbar_add_ucas                (ThunarWindow           *window);
GtkWidget*        thunar_window_location_toolbar_add_uca                 (ThunarWindow           *window,
                                                                          GObject                *thunarx_menu_item);
static void       thunar_window_location_toolbar_load_last_order         (ThunarWindow           *window);
static gboolean   thunar_window_location_toolbar_load_visibility         (ThunarWindow           *window);
static guint      thunar_window_toolbar_item_count                       (ThunarWindow           *window);
static GtkWidget* thunar_window_create_toolbar_item_from_action          (ThunarWindow           *window,
                                                                          ThunarWindowAction      action,
                                                                          guint                   item_order);
static GtkWidget* thunar_window_create_toolbar_toggle_item_from_action   (ThunarWindow           *window,
                                                                          ThunarWindowAction      action,
                                                                          gboolean                active,
                                                                          guint                   item_order);
static gboolean   thunar_window_image_preview_mode_changed               (ThunarWindow           *window);
static void       image_preview_update                                   (GtkWidget              *parent,
                                                                          GtkAllocation          *allocation,
                                                                          GtkWidget              *image);



struct _ThunarWindowClass
{
  GtkWindowClass __parent__;

  /* internal action signals */
  gboolean (*reload)          (ThunarWindow *window,
                               gboolean      reload_info);
  gboolean (*zoom_in)         (ThunarWindow *window);
  gboolean (*zoom_out)        (ThunarWindow *window);
  gboolean (*zoom_reset)      (ThunarWindow *window);
  gboolean (*tab_change)      (ThunarWindow *window,
                               gint          idx);
};

struct _ThunarWindow
{
  GtkWindow __parent__;

  /* support for custom preferences actions */
  ThunarxProviderFactory    *provider_factory;
  GList                     *thunarx_preferences_providers;

  GFile                     *bookmark_file;
  GList                     *bookmarks;
  GFileMonitor              *bookmark_monitor;

  ThunarClipboardManager   *clipboard;

  ThunarPreferences         *preferences;

  /* to be able to change folder on "device-pre-unmount" if required */
  ThunarDeviceMonitor       *device_monitor;

  GtkWidget                 *grid;
  GtkWidget                 *menubar;
  gboolean                   menubar_visible;
  GtkWidget                *spinner;
  GtkWidget                 *paned;
  GtkWidget                 *paned_right;
  GtkWidget                 *sidepane_box;
  GtkWidget                 *sidepane;
  GtkWidget                 *sidepane_preview_image;
  GtkWidget                 *right_pane_box;
  GtkWidget                 *right_pane_grid;
  GtkWidget                 *right_pane_preview_image;
  GtkWidget                 *right_pane_image_label;
  GtkWidget                 *right_pane_size_label;
  GtkWidget                 *view_box;
  GtkWidget                 *trash_infobar;
  GtkWidget                 *trash_infobar_restore_button;
  GtkWidget                 *trash_infobar_empty_button;

  /* split view panes */
  GtkWidget                 *paned_notebooks;
  GtkWidget                 *notebook_selected;
  GtkWidget                 *notebook_left;
  GtkWidget                 *notebook_right;

  GtkWidget                 *view;
  GtkWidget                 *statusbar;

  /* search */
  GtkWidget                 *catfish_search_button;
  gchar                     *search_query;
  gboolean                   is_searching;
  gboolean                   ignore_next_search_update;

  GType                      view_type;
  GSList                    *view_bindings;
  guint                      reset_view_type_idle_id;

  /* support for two different styles of location bars */
  GtkWidget                 *location_bar;
  GtkWidget                 *location_toolbar;
  GFileMonitor              *uca_file_monitor;
  GFile                     *uca_file;

  /* we need to maintain pointers to be able to toggle sensitivity */
  GtkWidget                 *location_toolbar_item_back;
  GtkWidget                 *location_toolbar_item_forward;
  GtkWidget                 *location_toolbar_item_parent;
  GtkWidget                 *location_toolbar_item_home;
  GtkWidget                 *location_toolbar_item_undo;
  GtkWidget                 *location_toolbar_item_redo;
  GtkWidget                 *location_toolbar_item_zoom_in;
  GtkWidget                 *location_toolbar_item_zoom_out;
  GtkWidget                 *location_toolbar_item_search;
  GtkWidget                 *location_toolbar_item_view_menubar;

  ThunarActionManager       *action_mgr;

  gulong                     signal_handler_id_history_changed;

  ThunarFile                *current_directory;
  GtkAccelGroup             *accel_group;

  /* zoom-level support */
  ThunarZoomLevel            zoom_level;

  gboolean                   show_hidden;

  gboolean                   directory_specific_settings;

  /* support to remember window geometry */
  guint                      save_geometry_timer_id;

  /* support to toggle side pane using F9,
   * see the toggle_sidepane() function.
   */
  GType                      toggle_sidepane_type;

  /* Image Preview thumbnail generation */
  ThunarThumbnailer         *thumbnailer;
  guint                      thumbnail_request;
  GdkPixbuf                 *preview_image_pixbuf;

  /* Reference to the global job operation history */
  ThunarJobOperationHistory *job_operation_history;
};



static XfceGtkActionEntry thunar_window_action_entries[] =
{
    { THUNAR_WINDOW_ACTION_FILE_MENU,                      "<Actions>/ThunarWindow/file-menu",                       "",                     XFCE_GTK_MENU_ITEM,       N_ ("_File"),                  NULL, NULL, NULL,},
    { THUNAR_WINDOW_ACTION_NEW_TAB,                        "<Actions>/ThunarWindow/new-tab",                         "<Primary>t",           XFCE_GTK_IMAGE_MENU_ITEM, N_ ("New _Tab"),               N_ ("Open a new tab for the displayed location"),                                    "tab-new",                 G_CALLBACK (thunar_window_action_open_new_tab),       },
    { THUNAR_WINDOW_ACTION_NEW_WINDOW,                     "<Actions>/ThunarWindow/new-window",                      "<Primary>n",           XFCE_GTK_IMAGE_MENU_ITEM, N_ ("New _Window"),            N_ ("Open a new Thunar window for the displayed location"),                          "window-new",              G_CALLBACK (thunar_window_action_open_new_window),    },
    { THUNAR_WINDOW_ACTION_DETACH_TAB,                     "<Actions>/ThunarWindow/detach-tab",                      "",                     XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Detac_h Tab"),            N_ ("Open current folder in a new window"),                                          NULL,                      G_CALLBACK (thunar_window_action_detach_tab),         },
    { THUNAR_WINDOW_ACTION_CLOSE_TAB,                      "<Actions>/ThunarWindow/close-tab",                       "<Primary>w",           XFCE_GTK_IMAGE_MENU_ITEM, N_ ("C_lose Tab"),             N_ ("Close this folder"),                                                            "window-close",            G_CALLBACK (thunar_window_action_close_tab),          },
    { THUNAR_WINDOW_ACTION_CLOSE_WINDOW,                   "<Actions>/ThunarWindow/close-window",                    "<Primary>q",           XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Close Window"),          N_ ("Close this window"),                                                            "application-exit",        G_CALLBACK (thunar_window_action_close_window),       },
    { THUNAR_WINDOW_ACTION_CLOSE_ALL_WINDOWS,              "<Actions>/ThunarWindow/close-all-windows",               "<Primary><Shift>w",    XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Close _All Windows"),     N_ ("Close all Thunar windows"),                                                     NULL,                      G_CALLBACK (thunar_window_action_close_all_windows),  },

    { THUNAR_WINDOW_ACTION_EDIT_MENU,                      "<Actions>/ThunarWindow/edit-menu",                       "",                     XFCE_GTK_MENU_ITEM,       N_ ("_Edit"),                  NULL,                                                                                NULL,                      NULL,                                                 },
    { THUNAR_WINDOW_ACTION_UNDO,                           "<Actions>/ThunarActionManager/undo",                     "<Primary>Z",           XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Undo"),                  N_ ("Undo the latest operation"),                                                    "edit-undo-symbolic",      G_CALLBACK (thunar_window_action_undo),               },
    { THUNAR_WINDOW_ACTION_REDO,                           "<Actions>/ThunarActionManager/redo",                     "<Primary><shift>Z",    XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Redo"),                  N_ ("Redo the latest operation"),                                                    "edit-redo-symbolic",      G_CALLBACK (thunar_window_action_redo),               },
    { THUNAR_WINDOW_ACTION_PREFERENCES,                    "<Actions>/ThunarWindow/preferences",                     "",                     XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Pr_eferences..."),        N_ ("Edit Thunars Preferences"),                                                     "preferences-system",      G_CALLBACK (thunar_window_action_preferences),        },

    { THUNAR_WINDOW_ACTION_VIEW_MENU,                      "<Actions>/ThunarWindow/view-menu",                       "",                     XFCE_GTK_MENU_ITEM,       N_ ("_View"),                  NULL,                                                                                NULL,                      NULL,                                                 },
    { THUNAR_WINDOW_ACTION_RELOAD,                         "<Actions>/ThunarWindow/reload",                          "<Primary>r",           XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Reload"),                N_ ("Reload the current folder"),                                                    "view-refresh-symbolic",   G_CALLBACK (thunar_window_action_reload),             },
    { THUNAR_WINDOW_ACTION_RELOAD_ALT,                     "<Actions>/ThunarWindow/reload-alt",                      "F5",                   XFCE_GTK_IMAGE_MENU_ITEM, NULL,                          NULL,                                                                                NULL,                      G_CALLBACK (thunar_window_action_reload),             },
    { THUNAR_WINDOW_ACTION_VIEW_SPLIT,                     "<Actions>/ThunarWindow/toggle-split-view",               "F3",                   XFCE_GTK_CHECK_MENU_ITEM, N_ ("Spl_it View"),            N_ ("Open/Close Split View"),                                                        "view-dual-symbolic",      G_CALLBACK (thunar_window_action_toggle_split_view),  },
    { THUNAR_WINDOW_ACTION_VIEW_LOCATION_SELECTOR_MENU,    "<Actions>/ThunarWindow/view-location-selector-menu",     "",                     XFCE_GTK_MENU_ITEM,       N_ ("_Location Selector"),     NULL,                                                                                NULL,                      NULL,                                                 },
    { THUNAR_WINDOW_ACTION_VIEW_LOCATION_SELECTOR_ENTRY,   "<Actions>/ThunarWindow/view-location-selector-entry",    "",                     XFCE_GTK_CHECK_MENU_ITEM, N_ ("_Entry Style"),           N_ ("Traditional entry showing the current path"),                                   NULL,                      G_CALLBACK (thunar_window_action_locationbar_entry),  },
    { THUNAR_WINDOW_ACTION_VIEW_LOCATION_SELECTOR_BUTTONS, "<Actions>/ThunarWindow/view-location-selector-buttons",  "",                     XFCE_GTK_CHECK_MENU_ITEM, N_ ("_Buttons Style"),         N_ ("Modern approach with buttons that correspond to folders"),                      NULL,                      G_CALLBACK (thunar_window_action_locationbar_buttons),},
    { THUNAR_WINDOW_ACTION_VIEW_SIDE_PANE_MENU,            "<Actions>/ThunarWindow/view-side-pane-menu",             "",                     XFCE_GTK_MENU_ITEM,       N_ ("_Side Pane"),             NULL,                                                                                NULL,                      NULL,                                                 },
    { THUNAR_WINDOW_ACTION_VIEW_SIDE_PANE_SHORTCUTS,       "<Actions>/ThunarWindow/view-side-pane-shortcuts",        "<Primary>b",           XFCE_GTK_CHECK_MENU_ITEM, N_ ("_Shortcuts"),             N_ ("Toggles the visibility of the shortcuts pane"),                                 NULL,                      G_CALLBACK (thunar_window_action_shortcuts_changed),  },
    { THUNAR_WINDOW_ACTION_VIEW_SIDE_PANE_TREE,            "<Actions>/ThunarWindow/view-side-pane-tree",             "<Primary>e",           XFCE_GTK_CHECK_MENU_ITEM, N_ ("_Tree"),                  N_ ("Toggles the visibility of the tree pane"),                                      NULL,                      G_CALLBACK (thunar_window_action_tree_changed),       },
    { THUNAR_WINDOW_ACTION_TOGGLE_SIDE_PANE,               "<Actions>/ThunarWindow/toggle-side-pane",                "F9",                   XFCE_GTK_MENU_ITEM,       NULL,                          NULL,                                                                                NULL,                      G_CALLBACK (thunar_window_toggle_sidepane),           },
    { THUNAR_WINDOW_ACTION_TOGGLE_IMAGE_PREVIEW,           "<Actions>/ThunarWindow/toggle-image-preview",            "",                     XFCE_GTK_CHECK_MENU_ITEM, N_ ("Image Preview"),          N_ ("Change the visibility of this window's image preview"),                         NULL,                      G_CALLBACK (thunar_window_action_image_preview),  },
    { THUNAR_WINDOW_ACTION_VIEW_STATUSBAR,                 "<Actions>/ThunarWindow/view-statusbar",                  "",                     XFCE_GTK_CHECK_MENU_ITEM, N_ ("St_atusbar"),             N_ ("Change the visibility of this window's statusbar"),                             NULL,                      G_CALLBACK (thunar_window_action_statusbar_changed),  },
    { THUNAR_WINDOW_ACTION_VIEW_MENUBAR,                   "<Actions>/ThunarWindow/view-menubar",                    "<Primary>m",           XFCE_GTK_CHECK_MENU_ITEM, N_ ("_Menubar"),               N_ ("Change the visibility of this window's menubar"),                               "open-menu-symbolic",      G_CALLBACK (thunar_window_action_menubar_changed),    },
    { THUNAR_WINDOW_ACTION_CONFIGURE_TOOLBAR,              "<Actions>/ThunarWindow/view-configure-toolbar",          "",                     XFCE_GTK_MENU_ITEM ,      N_ ("Configure _Toolbar..."),  N_ ("Configure the toolbar"),                                                        NULL,                      G_CALLBACK (thunar_window_action_show_toolbar_editor),},
    { THUNAR_WINDOW_ACTION_CLEAR_DIRECTORY_SPECIFIC_SETTINGS,"<Actions>/ThunarWindow/clear-directory-specific-settings","",                  XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Clear Saved _Folder View Settings"), N_ ("Delete saved view settings for this folder"),                        NULL,                      G_CALLBACK (thunar_window_action_clear_directory_specific_settings), },
    { THUNAR_WINDOW_ACTION_SHOW_HIDDEN,                    "<Actions>/ThunarWindow/show-hidden",                     "<Primary>h",           XFCE_GTK_CHECK_MENU_ITEM, N_ ("Show _Hidden Files"),     N_ ("Toggles the display of hidden files in the current window"),                    NULL,                      G_CALLBACK (thunar_window_action_show_hidden),        },
    { THUNAR_WINDOW_ACTION_SHOW_HIGHLIGHT,                 "<Actions>/ThunarWindow/show-highlight",                  "",                     XFCE_GTK_CHECK_MENU_ITEM, N_ ("Show File Hi_ghlight"),   N_ ("Toggles the display of file highlight which can be configured in the file specific property dialog"), NULL,G_CALLBACK (thunar_window_action_show_highlight),     },
    { THUNAR_WINDOW_ACTION_ZOOM_IN,                        "<Actions>/ThunarWindow/zoom-in",                         "<Primary>plus",        XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Zoom I_n"),               N_ ("Show the contents in more detail"),                                             "zoom-in-symbolic",        G_CALLBACK (thunar_window_zoom_in),                   },
    { THUNAR_WINDOW_ACTION_ZOOM_IN_ALT_1,                  "<Actions>/ThunarWindow/zoom-in-alt1",                    "<Primary>KP_Add",      XFCE_GTK_IMAGE_MENU_ITEM, NULL,                          NULL,                                                                                NULL,                      G_CALLBACK (thunar_window_zoom_in),                   },
    { THUNAR_WINDOW_ACTION_ZOOM_IN_ALT_2,                  "<Actions>/ThunarWindow/zoom-in-alt2",                    "<Primary>equal",       XFCE_GTK_IMAGE_MENU_ITEM, NULL,                          NULL,                                                                                NULL,                      G_CALLBACK (thunar_window_zoom_in),                   },
    { THUNAR_WINDOW_ACTION_ZOOM_OUT,                       "<Actions>/ThunarWindow/zoom-out",                        "<Primary>minus",       XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Zoom _Out"),              N_ ("Show the contents in less detail"),                                             "zoom-out-symbolic",       G_CALLBACK (thunar_window_zoom_out),                  },
    { THUNAR_WINDOW_ACTION_ZOOM_OUT_ALT,                   "<Actions>/ThunarWindow/zoom-out-alt",                    "<Primary>KP_Subtract", XFCE_GTK_IMAGE_MENU_ITEM, NULL,                          NULL,                                                                                NULL,                      G_CALLBACK (thunar_window_zoom_out),                  },
    { THUNAR_WINDOW_ACTION_ZOOM_RESET,                     "<Actions>/ThunarWindow/zoom-reset",                      "<Primary>0",           XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Normal Si_ze"),           N_ ("Show the contents at the normal size"),                                         "zoom-original-symbolic",  G_CALLBACK (thunar_window_zoom_reset),                },
    { THUNAR_WINDOW_ACTION_ZOOM_RESET_ALT,                 "<Actions>/ThunarWindow/zoom-reset-alt",                  "<Primary>KP_0",        XFCE_GTK_IMAGE_MENU_ITEM, NULL,                          NULL,                                                                                NULL,                      G_CALLBACK (thunar_window_zoom_reset),                },
    { THUNAR_WINDOW_ACTION_VIEW_AS_ICONS,                  "<Actions>/ThunarWindow/view-as-icons",                   "<Primary>1",           XFCE_GTK_RADIO_MENU_ITEM, N_ ("_Icon View"),             N_ ("Display folder content in an icon view"),                                       "view-grid-symbolic",      G_CALLBACK (thunar_window_action_icon_view),          },
    { THUNAR_WINDOW_ACTION_VIEW_AS_DETAILED_LIST,          "<Actions>/ThunarWindow/view-as-detailed-list",           "<Primary>2",           XFCE_GTK_RADIO_MENU_ITEM, N_ ("_List View"),             N_ ("Display folder content in a detailed list view"),                               "view-list-symbolic",      G_CALLBACK (thunar_window_action_detailed_view),      },
    { THUNAR_WINDOW_ACTION_VIEW_AS_COMPACT_LIST,           "<Actions>/ThunarWindow/view-as-compact-list",            "<Primary>3",           XFCE_GTK_RADIO_MENU_ITEM, N_ ("_Compact View"),          N_ ("Display folder content in a compact list view"),                                "view-compact-symbolic",   G_CALLBACK (thunar_window_action_compact_view),       },

    { THUNAR_WINDOW_ACTION_GO_MENU,                        "<Actions>/ThunarWindow/go-menu",                         "",                     XFCE_GTK_MENU_ITEM,       N_ ("_Go"),                    NULL,                                                                                NULL,                      NULL                                                  },
    { THUNAR_WINDOW_ACTION_BOOKMARKS_MENU,                 "<Actions>/ThunarWindow/bookmarks-menu",                  "",                     XFCE_GTK_MENU_ITEM,       N_ ("_Bookmarks"),             NULL,                                                                                NULL,                      NULL                                                  },
    { THUNAR_WINDOW_ACTION_OPEN_FILE_SYSTEM,               "<Actions>/ThunarWindow/open-file-system",                "",                     XFCE_GTK_IMAGE_MENU_ITEM, N_ ("File System"),            N_ ("Browse the file system"),                                                       "drive-harddisk",          G_CALLBACK (thunar_window_action_open_file_system),   },
    { THUNAR_WINDOW_ACTION_OPEN_HOME,                      "<Actions>/ThunarWindow/open-home",                       "<Alt>Home",            XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Home"),                  N_ ("Go to the home folder"),                                                        "go-home-symbolic",        G_CALLBACK (thunar_window_action_open_home),          },
    { THUNAR_WINDOW_ACTION_OPEN_DESKTOP,                   "<Actions>/ThunarWindow/open-desktop",                    "",                     XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Desktop"),                N_ ("Go to the desktop folder"),                                                     "user-desktop",            G_CALLBACK (thunar_window_action_open_desktop),       },
    { THUNAR_WINDOW_ACTION_OPEN_COMPUTER,                  "<Actions>/ThunarWindow/open-computer",                   "",                     XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Computer"),               N_ ("Browse all local and remote disks and folders accessible from this computer"),  "computer",                G_CALLBACK (thunar_window_action_open_computer),      },
    { THUNAR_WINDOW_ACTION_OPEN_RECENT,                    "<Actions>/ThunarWindow/open-recent",                     "",                     XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Recent"),                 N_ ("Display recently used files"),                                                  "document-open-recent",    G_CALLBACK (thunar_window_action_open_recent),        },
    { THUNAR_WINDOW_ACTION_OPEN_TRASH,                     "<Actions>/ThunarWindow/open-trash",                      "",                     XFCE_GTK_IMAGE_MENU_ITEM, N_ ("T_rash"),                 N_ ("Display the contents of the trash can"),                                        NULL,                      G_CALLBACK (thunar_window_action_open_trash),         },
    { THUNAR_WINDOW_ACTION_OPEN_PARENT,                    "<Actions>/ThunarWindow/open-parent",                     "<Alt>Up",              XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Open _Parent"),           N_ ("Open the parent folder"),                                                       "go-up-symbolic",          G_CALLBACK (thunar_window_action_go_up),              },
    { THUNAR_WINDOW_ACTION_OPEN_LOCATION,                  "<Actions>/ThunarWindow/open-location",                   "<Primary>l",           XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Open Location..."),      N_ ("Specify a location to open"),                                                   NULL,                      G_CALLBACK (thunar_window_action_open_location),      },
    { THUNAR_WINDOW_ACTION_OPEN_LOCATION_ALT,              "<Actions>/ThunarWindow/open-location-alt",               "<Alt>d",               XFCE_GTK_MENU_ITEM,       NULL,                          NULL,                                                                                NULL,                      G_CALLBACK (thunar_window_action_open_location),      },
    { THUNAR_WINDOW_ACTION_OPEN_TEMPLATES,                 "<Actions>/ThunarWindow/open-templates",                  "",                     XFCE_GTK_IMAGE_MENU_ITEM, N_("T_emplates"),              N_ ("Go to the templates folder"),                                                   "text-x-generic-template", G_CALLBACK (thunar_window_action_open_templates),     },
    { THUNAR_WINDOW_ACTION_OPEN_NETWORK,                   "<Actions>/ThunarWindow/open-network",                    "",                     XFCE_GTK_IMAGE_MENU_ITEM, N_("B_rowse Network"),         N_ ("Browse local network connections"),                                             "network-workgroup",       G_CALLBACK (thunar_window_action_open_network),       },

    { THUNAR_WINDOW_ACTION_HELP_MENU,                      "<Actions>/ThunarWindow/contents/help-menu",              "",                     XFCE_GTK_MENU_ITEM      , N_ ("_Help"),                  NULL, NULL, NULL},
    { THUNAR_WINDOW_ACTION_CONTENTS,                       "<Actions>/ThunarWindow/contents",                        "F1",                   XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Contents"),              N_ ("Display Thunar user manual"),                                                   "help-browser",            G_CALLBACK (thunar_window_action_contents),            },
    { THUNAR_WINDOW_ACTION_ABOUT,                          "<Actions>/ThunarWindow/about",                           "",                     XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_About"),                 N_ ("Display information about Thunar"),                                             "help-about",              G_CALLBACK (thunar_window_action_about),               },
    { THUNAR_WINDOW_ACTION_BACK,                           "<Actions>/ThunarStandardView/back",                      "<Alt>Left",            XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Back"),                   N_ ("Go to the previous visited folder"),                                            "go-previous-symbolic",    G_CALLBACK (thunar_window_action_back),                },
    { THUNAR_WINDOW_ACTION_BACK_ALT,                       "<Actions>/ThunarStandardView/back-alt",                  "BackSpace",            XFCE_GTK_IMAGE_MENU_ITEM, NULL,                          NULL,                                                                                NULL,                      G_CALLBACK (thunar_window_action_back),                },
    { THUNAR_WINDOW_ACTION_FORWARD,                        "<Actions>/ThunarStandardView/forward",                   "<Alt>Right",           XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Forward"),                N_ ("Go to the next visited folder"),                                                "go-next-symbolic",        G_CALLBACK (thunar_window_action_forward),             },
    { THUNAR_WINDOW_ACTION_SWITCH_PREV_TAB,                "<Actions>/ThunarWindow/switch-previous-tab",             "<Primary>Page_Up",     XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Previous Tab"),          N_ ("Switch to Previous Tab"),                                                       "go-previous",             G_CALLBACK (thunar_window_action_switch_previous_tab), },
    { THUNAR_WINDOW_ACTION_SWITCH_NEXT_TAB,                "<Actions>/ThunarWindow/switch-next-tab",                 "<Primary>Page_Down",   XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Next Tab"),              N_ ("Switch to Next Tab"),                                                           "go-next",                 G_CALLBACK (thunar_window_action_switch_next_tab),     },
    { THUNAR_WINDOW_ACTION_SEARCH,                         "<Actions>/ThunarWindow/search",                          "<Primary>f",           XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Search for Files..."),   N_ ("Search for a specific file in the current folder and Recent"),                  "system-search-symbolic",  G_CALLBACK (thunar_window_action_search),              },
    { THUNAR_WINDOW_ACTION_CANCEL_SEARCH,                  "<Actions>/ThunarWindow/cancel-search",                   "Escape",               XFCE_GTK_MENU_ITEM,       N_ ("Cancel search for files"),NULL,                                                                                "",                        G_CALLBACK (thunar_window_action_cancel_search),       },
    { 0,                                                   "<Actions>/ThunarWindow/open-file-menu",                  "F10",                  0,                        NULL,                          NULL,                                                                                NULL,                      G_CALLBACK (thunar_window_action_open_file_menu),      },
};

#define get_action_entry(id) xfce_gtk_get_action_entry_by_id(thunar_window_action_entries,G_N_ELEMENTS(thunar_window_action_entries),id)



static guint window_signals[LAST_SIGNAL];



G_DEFINE_TYPE_WITH_CODE (ThunarWindow, thunar_window, GTK_TYPE_WINDOW,
                         G_IMPLEMENT_INTERFACE (THUNAR_TYPE_BROWSER, NULL))



static void
thunar_window_class_init (ThunarWindowClass *klass)
{
  GtkWidgetClass *gtkwidget_class;
  GtkBindingSet  *binding_set;
  GObjectClass   *gobject_class;
  guint           i;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_window_dispose;
  gobject_class->finalize = thunar_window_finalize;
  gobject_class->get_property = thunar_window_get_property;
  gobject_class->set_property = thunar_window_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->realize = thunar_window_realize;
  gtkwidget_class->unrealize = thunar_window_unrealize;
  gtkwidget_class->configure_event = thunar_window_configure_event;

  klass->reload = thunar_window_reload;
  klass->zoom_in = thunar_window_zoom_in;
  klass->zoom_out = thunar_window_zoom_out;
  klass->zoom_reset = thunar_window_zoom_reset;
  klass->tab_change = thunar_window_tab_change;

  xfce_gtk_translate_action_entries (thunar_window_action_entries, G_N_ELEMENTS (thunar_window_action_entries));

  /**
   * ThunarWindow:current-directory:
   *
   * The directory currently displayed within this #ThunarWindow
   * or %NULL.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_CURRENT_DIRECTORY,
                                   g_param_spec_object ("current-directory",
                                                        "current-directory",
                                                        "current-directory",
                                                        THUNAR_TYPE_FILE,
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarWindow:zoom-level:
   *
   * The #ThunarZoomLevel applied to the #ThunarView currently
   * shown within this window.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ZOOM_LEVEL,
                                   g_param_spec_enum ("zoom-level",
                                                      "zoom-level",
                                                      "zoom-level",
                                                      THUNAR_TYPE_ZOOM_LEVEL,
                                                      THUNAR_ZOOM_LEVEL_100_PERCENT,
                                                      EXO_PARAM_READWRITE));

  /**
   * ThunarWindow:directory-specific-settings:
   *
   * Whether to use directory specific settings.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_DIRECTORY_SPECIFIC_SETTINGS,
                                   g_param_spec_boolean ("directory-specific-settings",
                                                         "directory-specific-settings",
                                                         "directory-specific-settings",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarWindow::reload:
   * @window : a #ThunarWindow instance.
   *
   * Emitted whenever the user requests to reload the contents
   * of the currently displayed folder.
   **/
  window_signals[RELOAD] =
    g_signal_new (I_("reload"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (ThunarWindowClass, reload),
                  g_signal_accumulator_true_handled, NULL,
                  _thunar_marshal_BOOLEAN__BOOLEAN,
                  G_TYPE_BOOLEAN, 1,
                  G_TYPE_BOOLEAN);

  /**
   * ThunarWindow::zoom-in:
   * @window : a #ThunarWindow instance.
   *
   * Emitted whenever the user requests to zoom in. This
   * is an internal signal used to bind the action to keys.
   **/
  window_signals[ZOOM_IN] =
    g_signal_new (I_("zoom-in"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (ThunarWindowClass, zoom_in),
                  g_signal_accumulator_true_handled, NULL,
                  _thunar_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN, 0);

  /**
   * ThunarWindow::zoom-out:
   * @window : a #ThunarWindow instance.
   *
   * Emitted whenever the user requests to zoom out. This
   * is an internal signal used to bind the action to keys.
   **/
  window_signals[ZOOM_OUT] =
    g_signal_new (I_("zoom-out"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (ThunarWindowClass, zoom_out),
                  g_signal_accumulator_true_handled, NULL,
                  _thunar_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN, 0);

  /**
   * ThunarWindow::zoom-reset:
   * @window : a #ThunarWindow instance.
   *
   * Emitted whenever the user requests reset the zoom level.
   * This is an internal signal used to bind the action to keys.
   **/
  window_signals[ZOOM_RESET] =
    g_signal_new (I_("zoom-reset"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (ThunarWindowClass, zoom_reset),
                  g_signal_accumulator_true_handled, NULL,
                  _thunar_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN, 0);

  /**
   * ThunarWindow::tab-change:
   * @window : a #ThunarWindow instance.
   * @idx    : tab index,
   *
   * Emitted whenever the user uses a Alt+N combination to
   * switch tabs.
   **/
  window_signals[TAB_CHANGE] =
    g_signal_new (I_("tab-change"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (ThunarWindowClass, tab_change),
                  g_signal_accumulator_true_handled, NULL,
                  _thunar_marshal_BOOLEAN__INT,
                  G_TYPE_BOOLEAN, 1,
                  G_TYPE_INT);

  /* setup the key bindings for the windows */
  binding_set = gtk_binding_set_by_class (klass);

  /* setup the key bindings for Alt+N */
  for (i = 0; i < 10; i++)
    {
      gtk_binding_entry_add_signal (binding_set, GDK_KEY_0 + i, GDK_MOD1_MASK,
                                    "tab-change", 1, G_TYPE_UINT, i - 1);
    }
}



static void
thunar_window_paned_notebooks_destroy (GtkWidget    *paned_notebooks,
                                       GtkWidget    *widget,
                                       ThunarWindow *window)
{
  if (window->notebook_left)
    {
      gtk_widget_destroy (window->notebook_left);
      window->notebook_left = NULL;
    }
  if (window->notebook_right)
    {
      gtk_widget_destroy (window->notebook_right);
      window->notebook_right = NULL;
    }
}



static void
thunar_window_init (ThunarWindow *window)
{
  GtkWidget             *label;
  GtkWidget             *infobar;
  GtkWidget             *item;
  GtkWidget             *event_box;
  gboolean               last_menubar_visible;
  gchar                 *last_location_bar;
  gchar                 *last_side_pane;
  gchar                 *uca_path;
  gchar                 *catfish_path;
  GType                  type;
  gint                   last_separator_position;
  gint                   last_window_width;
  gint                   last_window_height;
  gboolean               last_window_maximized;
  gboolean               last_statusbar_visible;
  gboolean               last_image_preview_visible;
  ThunarImagePreviewMode misc_image_preview_mode;
  gint                   max_paned_position;
  GtkStyleContext       *context;

  /* unset the view type */
  window->view_type = G_TYPE_NONE;

  /* grab a reference on the provider factory and load the providers*/
  window->provider_factory = thunarx_provider_factory_get_default ();
  window->thunarx_preferences_providers = thunarx_provider_factory_list_providers (window->provider_factory, THUNARX_TYPE_PREFERENCES_PROVIDER);

  /* grab a reference on the preferences */
  window->preferences = thunar_preferences_get ();

  window->thumbnailer = thunar_thumbnailer_get ();
  g_signal_connect_swapped (G_OBJECT (window->thumbnailer), "request-finished", G_CALLBACK (thunar_window_finished_thumbnailing), window);

  window->accel_group = gtk_accel_group_new ();
  xfce_gtk_accel_map_add_entries (thunar_window_action_entries, G_N_ELEMENTS (thunar_window_action_entries));
  xfce_gtk_accel_group_connect_action_entries (window->accel_group,
                                               thunar_window_action_entries,
                                               G_N_ELEMENTS (thunar_window_action_entries),
                                               window);

  gtk_window_add_accel_group (GTK_WINDOW (window), window->accel_group);

  /* get all properties for init */
  g_object_get (G_OBJECT (window->preferences),
                "last-show-hidden", &window->show_hidden,
                "last-window-width", &last_window_width,
                "last-window-height", &last_window_height,
                "last-window-maximized", &last_window_maximized,
                "last-menubar-visible", &last_menubar_visible,
                "last-separator-position", &last_separator_position,
                "last-location-bar", &last_location_bar,
                "last-side-pane", &last_side_pane,
                "last-statusbar-visible", &last_statusbar_visible,
                "last-image-preview-visible", &last_image_preview_visible,
                "misc-image-preview-mode", &misc_image_preview_mode,
                NULL);

  /* update the visual on screen_changed events */
  g_signal_connect (window, "screen-changed", G_CALLBACK (thunar_window_screen_changed), NULL);

  /* invoke the thunar_window_screen_changed function to initially set the best possible visual.*/
  thunar_window_screen_changed (GTK_WIDGET (window), NULL, NULL);

  /* set up a handler to confirm exit when there are multiple tabs open  */
  g_signal_connect (window, "delete-event", G_CALLBACK (thunar_window_delete), NULL);

  /* connect to the volume monitor */
  window->device_monitor = thunar_device_monitor_get ();
  g_signal_connect (window->device_monitor, "device-pre-unmount", G_CALLBACK (thunar_window_device_pre_unmount), window);
  g_signal_connect (window->device_monitor, "device-removed", G_CALLBACK (thunar_window_device_changed), window);
  g_signal_connect (window->device_monitor, "device-changed", G_CALLBACK (thunar_window_device_changed), window);

  /* Catch key events before accelerators get processed */
  g_signal_connect (window, "key-press-event", G_CALLBACK (thunar_window_propagate_key_event), NULL);
  g_signal_connect (window, "key-release-event", G_CALLBACK (thunar_window_propagate_key_event), NULL);
  g_signal_connect_after (window, "key-release-event", G_CALLBACK (thunar_window_after_propagate_key_event), NULL);

  window->action_mgr = g_object_new (THUNAR_TYPE_ACTION_MANAGER, "widget", GTK_WIDGET (window), NULL);

  g_object_bind_property (G_OBJECT (window), "current-directory", G_OBJECT (window->action_mgr), "current-directory", G_BINDING_SYNC_CREATE);
  g_signal_connect_swapped (G_OBJECT (window->action_mgr), "change-directory", G_CALLBACK (thunar_window_set_current_directory), window);
  g_signal_connect_swapped (G_OBJECT (window->action_mgr), "open-new-tab", G_CALLBACK (thunar_window_notebook_open_new_tab), window);
  g_signal_connect_swapped (G_OBJECT (window->action_mgr), "new-files-created", G_CALLBACK (thunar_window_show_and_select_files), window);
  thunar_action_manager_append_accelerators (window->action_mgr, window->accel_group);

  /* determine the default window size from the preferences */
  gtk_window_set_default_size (GTK_WINDOW (window), last_window_width, last_window_height);

  /* restore the maxized state of the window */
  if (G_UNLIKELY (last_window_maximized))
    gtk_window_maximize (GTK_WINDOW (window));

  /* add thunar style class for easier theming */
  context = gtk_widget_get_style_context (GTK_WIDGET (window));
  gtk_style_context_add_class (context, "thunar");

  window->grid = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (window), window->grid);
  gtk_widget_show (window->grid);

  /* build the menubar */
  window->menubar = gtk_menu_bar_new ();
  thunar_window_create_menu (window, THUNAR_WINDOW_ACTION_FILE_MENU, G_CALLBACK (thunar_window_update_file_menu));
  thunar_window_create_menu (window, THUNAR_WINDOW_ACTION_EDIT_MENU, G_CALLBACK (thunar_window_update_edit_menu));
  thunar_window_create_menu (window, THUNAR_WINDOW_ACTION_VIEW_MENU, G_CALLBACK (thunar_window_update_view_menu));
  thunar_window_create_menu (window, THUNAR_WINDOW_ACTION_GO_MENU, G_CALLBACK (thunar_window_update_go_menu));
  thunar_window_create_menu (window, THUNAR_WINDOW_ACTION_BOOKMARKS_MENU, G_CALLBACK (thunar_window_update_bookmarks_menu));
  thunar_window_create_menu (window, THUNAR_WINDOW_ACTION_HELP_MENU, G_CALLBACK (thunar_window_update_help_menu));
  gtk_widget_show_all (window->menubar);

  window->menubar_visible = last_menubar_visible;
  if (last_menubar_visible == FALSE)
    gtk_widget_hide (window->menubar);
  gtk_widget_set_hexpand (window->menubar, TRUE);
  gtk_grid_attach (GTK_GRID (window->grid), window->menubar, 0, 0, 1, 1);

  /* append the menu item for the spinner */
  item = gtk_menu_item_new ();
  gtk_widget_set_sensitive (GTK_WIDGET (item), FALSE);
  g_object_set (G_OBJECT (item), "right-justified", TRUE, NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (window->menubar), item);
  gtk_widget_show (item);

  /* Required if F10 is pushed while the menu is hidden */
  g_signal_connect_swapped (G_OBJECT (window->menubar), "deactivate", G_CALLBACK (thunar_window_action_menubar_update), window);

  /* place the spinner into the menu item */
  window->spinner = gtk_spinner_new ();
  gtk_container_add (GTK_CONTAINER (item), window->spinner);
  gtk_widget_set_visible (GTK_WIDGET (window->spinner), TRUE);

  /* check if we need to add the root warning */
  if (G_UNLIKELY (geteuid () == 0))
    {
      /* add the bar for the root warning */
      infobar = gtk_info_bar_new ();
      gtk_info_bar_set_message_type (GTK_INFO_BAR (infobar), GTK_MESSAGE_WARNING);
      gtk_widget_set_hexpand (infobar, TRUE);
      gtk_grid_attach (GTK_GRID (window->grid), infobar, 0, 2, 1, 1);
      gtk_widget_show (infobar);

      /* add the label with the root warning */
      label = gtk_label_new (_("Warning: you are using the root account. You may harm your system."));
      gtk_container_add (GTK_CONTAINER (gtk_info_bar_get_content_area (GTK_INFO_BAR (infobar))), label);
      gtk_widget_show (label);
    }

  /* main paned widget */
  window->paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_container_set_border_width (GTK_CONTAINER (window->paned), 0);
  gtk_widget_set_hexpand (window->paned, TRUE);
  gtk_widget_set_vexpand (window->paned, TRUE);
  gtk_grid_attach (GTK_GRID (window->grid), window->paned, 0, 4, 1, 1);
  gtk_widget_show (window->paned);

  /* left sidepane */
  window->sidepane_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_paned_pack1 (GTK_PANED (window->paned), window->sidepane_box, FALSE, FALSE);
  gtk_widget_show (window->sidepane_box);

  window->sidepane_preview_image = gtk_image_new_from_file ("");
  gtk_widget_set_margin_top (window->sidepane_preview_image, 10);
  gtk_widget_set_margin_bottom (window->sidepane_preview_image, 10);
  gtk_box_pack_end (GTK_BOX (window->sidepane_box), window->sidepane_preview_image, FALSE, TRUE, 0);
  if (last_image_preview_visible == TRUE && misc_image_preview_mode == THUNAR_IMAGE_PREVIEW_MODE_EMBEDDED)
    gtk_widget_show (window->sidepane_preview_image);

  g_signal_connect (G_OBJECT (window->sidepane_box), "size-allocate", G_CALLBACK (image_preview_update), window->sidepane_preview_image);

  /* determine the last separator position and apply it to the paned view */
  gtk_paned_set_position (GTK_PANED (window->paned), last_separator_position);
  g_signal_connect_swapped (window->paned, "accept-position", G_CALLBACK (thunar_window_save_paned), window);
  g_signal_connect_swapped (window->paned, "button-release-event", G_CALLBACK (thunar_window_save_paned), window);

  /* nested paned widget */
  window->paned_right = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_container_set_border_width (GTK_CONTAINER (window->paned_right), 0);
  gtk_widget_set_hexpand (window->paned_right, TRUE);
  gtk_widget_set_vexpand (window->paned_right, TRUE);
  gtk_widget_show (window->paned_right);
  gtk_paned_pack2 (GTK_PANED (window->paned), window->paned_right, TRUE, FALSE);

  /* right sidepane */
  window->right_pane_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_widget_set_margin_top (window->right_pane_box, 20);
  gtk_paned_pack2 (GTK_PANED (window->paned_right), window->right_pane_box, TRUE, FALSE);

  window->right_pane_preview_image = gtk_image_new_from_file ("");
  window->thumbnail_request = 0;
  gtk_widget_set_size_request (window->right_pane_preview_image, 276, -1); /* large thumbnail size + 20 */
  gtk_box_pack_start (GTK_BOX (window->right_pane_box), window->right_pane_preview_image, FALSE, TRUE, 0);

  window->right_pane_grid = gtk_grid_new ();
  gtk_widget_set_halign (window->right_pane_grid, GTK_ALIGN_CENTER);
  gtk_widget_set_vexpand (window->right_pane_grid, TRUE);
  gtk_widget_set_hexpand (window->right_pane_grid, TRUE);
  gtk_box_pack_end (GTK_BOX (window->right_pane_box), window->right_pane_grid, FALSE, TRUE, 0);

  window->right_pane_image_label = gtk_label_new ("");
  gtk_grid_attach (GTK_GRID (window->right_pane_grid), window->right_pane_image_label, 0, 1, 2, 1);

  gtk_grid_attach (GTK_GRID (window->right_pane_grid), gtk_label_new (_("Size: ")), 0, 2, 1, 1);
  window->right_pane_size_label = gtk_label_new ("");
  gtk_grid_attach (GTK_GRID (window->right_pane_grid), window->right_pane_size_label, 1, 2, 1, 1);

  gtk_widget_show_all (window->right_pane_box);

  if (last_image_preview_visible == FALSE || misc_image_preview_mode == THUNAR_IMAGE_PREVIEW_MODE_EMBEDDED)
    gtk_widget_hide(window->right_pane_box);

  g_signal_connect (G_OBJECT (window->right_pane_box), "size-allocate", G_CALLBACK (image_preview_update), window->right_pane_preview_image);

  g_signal_connect_swapped (window->preferences, "notify::misc-image-preview-mode", G_CALLBACK (thunar_window_image_preview_mode_changed), window);

  window->preview_image_pixbuf = NULL;

  /* split view: Create panes where the two notebooks */
  window->paned_notebooks = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  g_signal_connect_swapped (window->preferences, "notify::misc-vertical-split-pane", G_CALLBACK (thunar_window_paned_notebooks_update_orientation), window);
  thunar_window_paned_notebooks_update_orientation (window);

  window->view_box = gtk_grid_new ();
  gtk_paned_pack1 (GTK_PANED (window->paned_right), window->view_box, TRUE, FALSE);
  gtk_widget_show (window->view_box);

  gtk_widget_add_events (window->paned_notebooks, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_BUTTON_PRESS_MASK);
  gtk_grid_attach (GTK_GRID (window->view_box), window->paned_notebooks, 0, 0, 1, 1);
  gtk_widget_show (window->paned_notebooks);

  g_object_get (window->paned_right, "max-position", &max_paned_position, NULL);
  gtk_paned_set_position (GTK_PANED (window->paned_right), max_paned_position);

  /** close notebooks on window-remove signal because later on window property
   *  pointers are broken.
   **/
  g_signal_connect (G_OBJECT (window), "remove", G_CALLBACK (thunar_window_paned_notebooks_destroy), window);

  /* add first notebook and select it*/
  window->notebook_selected = thunar_window_paned_notebooks_add(window);

  /* get a reference of the global job operation history */
  window->job_operation_history = thunar_job_operation_history_get_default ();

  window->location_toolbar = NULL;
  thunar_window_location_toolbar_create (window);

  uca_path = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, "Thunar/uca.xml", TRUE);
  window->uca_file         = g_file_new_for_path (uca_path);
  window->uca_file_monitor = g_file_monitor_file (window->uca_file, G_FILE_MONITOR_NONE, NULL, NULL);
  if (G_LIKELY (window->uca_file_monitor != NULL))
    g_signal_connect (window->uca_file_monitor, "changed", G_CALLBACK (thunar_window_update_location_toolbar), window);
  g_free (uca_path);

  /* setup setting the location bar visibility on-demand */
  g_signal_connect_object (G_OBJECT (window->preferences), "notify::last-location-bar", G_CALLBACK (thunar_window_update_location_bar_visible), window, G_CONNECT_SWAPPED);
  thunar_window_update_location_bar_visible (window);

  /* update window icon whenever preferences change */
  g_signal_connect_object (G_OBJECT (window->preferences), "notify::misc-change-window-icon", G_CALLBACK (thunar_window_update_window_icon), window, G_CONNECT_SWAPPED);

  /* determine the selected side pane */
  if (g_strcmp0 (last_side_pane, g_type_name (THUNAR_TYPE_SHORTCUTS_PANE)) == 0)
    type = THUNAR_TYPE_SHORTCUTS_PANE;
  else if (g_strcmp0 (last_side_pane, g_type_name (THUNAR_TYPE_TREE_PANE)) == 0)
    type = THUNAR_TYPE_TREE_PANE;
  else
    type = G_TYPE_NONE;
  thunar_window_install_sidepane (window, type);
  g_free (last_side_pane);

  /* synchronise the "directory-specific-settings" property with the global "misc-directory-specific-settings" property */
  g_object_bind_property (G_OBJECT (window->preferences), "misc-directory-specific-settings", G_OBJECT (window), "directory-specific-settings", G_BINDING_SYNC_CREATE);

  /* setup the `Show More...` button that appears at the bottom of the view after a search is completed */
  catfish_path =  g_find_program_in_path ("catfish");
  if (catfish_path != NULL)
    {
      /* TRANSLATORS: `Catfish' is a software package, please don't translate it. */
      window->catfish_search_button = gtk_button_new_with_label (_("Search with Catfish..."));
      gtk_grid_attach (GTK_GRID (window->view_box), window->catfish_search_button, 0, 1, 1, 1);
      g_signal_connect_swapped (window->catfish_search_button, "clicked", G_CALLBACK (thunar_window_catfish_dialog_configure), window);
      gtk_widget_hide (window->catfish_search_button);
      g_free (catfish_path);
    }
  else
    window->catfish_search_button = NULL;

  window->trash_infobar = gtk_info_bar_new ();
  gtk_grid_attach (GTK_GRID (window->view_box), window->trash_infobar, 0, 2, 1, 1);
  window->trash_infobar_restore_button = gtk_info_bar_add_button (GTK_INFO_BAR (window->trash_infobar), _("Restore Selected Items"), RESTORE);
  window->trash_infobar_empty_button = gtk_info_bar_add_button (GTK_INFO_BAR (window->trash_infobar), _("Empty Trash"), EMPTY);
  g_signal_connect (window->trash_infobar,
                    "response",
                    G_CALLBACK (thunar_window_trash_infobar_clicked),
                    G_OBJECT (window));

  /* setup a new statusbar */
  event_box = gtk_event_box_new ();
  window->statusbar = thunar_statusbar_new ();
  thunar_statusbar_append_accelerators (THUNAR_STATUSBAR (window->statusbar), window->accel_group);
  gtk_widget_set_hexpand (window->statusbar, TRUE);
  gtk_container_add (GTK_CONTAINER (event_box), window->statusbar);
  gtk_grid_attach (GTK_GRID (window->view_box), event_box, 0, 3, 1, 1);
  if (last_statusbar_visible)
    gtk_widget_show (window->statusbar);
  gtk_widget_show (event_box);

  thunar_statusbar_setup_event (THUNAR_STATUSBAR (window->statusbar), event_box);
  if (G_LIKELY (window->view != NULL))
    thunar_window_binding_create (window, window->view, "statusbar-text", window->statusbar, "text", G_BINDING_SYNC_CREATE);

  /* ensure that all the view types are registered */
  g_type_ensure (THUNAR_TYPE_ICON_VIEW);
  g_type_ensure (THUNAR_TYPE_DETAILS_VIEW);
  g_type_ensure (THUNAR_TYPE_COMPACT_VIEW);

  /* load the bookmarks file and monitor */
  window->bookmarks = NULL;
  window->bookmark_file = thunar_g_file_new_for_bookmarks ();
  window->bookmark_monitor = g_file_monitor_file (window->bookmark_file, G_FILE_MONITOR_NONE, NULL, NULL);
  if (G_LIKELY (window->bookmark_monitor != NULL))
      g_signal_connect_swapped (window->bookmark_monitor, "changed", G_CALLBACK (thunar_window_update_bookmarks), window);

  /* initial load of the bookmarks */
  thunar_window_update_bookmarks (window);

  /* update recent */
  g_signal_connect (G_OBJECT (gtk_recent_manager_get_default()), "changed", G_CALLBACK (thunar_window_recent_reload), window);

  window->search_query = NULL;
  window->reset_view_type_idle_id = 0;
}


static void
thunar_window_screen_changed (GtkWidget *widget,
                              GdkScreen *old_screen,
                              gpointer   userdata)
{
  GdkScreen *screen = gdk_screen_get_default ();
  GdkVisual *visual = gdk_screen_get_rgba_visual (screen);

  if (visual == NULL || !gdk_screen_is_composited (screen))
    visual = gdk_screen_get_system_visual (screen);

  gtk_widget_set_visual (GTK_WIDGET (widget), visual);
}



static void
hash_table_entry_show_and_select_files (gpointer key, gpointer list, gpointer application)
{
  ThunarFile *original_dir  = NULL;
  GList      *window_list   = NULL;

  _thunar_return_if_fail (key != NULL);
  _thunar_return_if_fail (list != NULL);
  _thunar_return_if_fail (application != NULL);

  /* open directory */
  original_dir = thunar_file_get_for_uri (key, NULL);
  thunar_application_open_window (application, original_dir, NULL, NULL, FALSE);

  /* select files */
  window_list = thunar_application_get_windows (application);
  window_list = g_list_last (window_list); /* this will be the topmost Window */
  thunar_window_select_files (THUNAR_WINDOW (window_list->data), list);

  /* free memory */
  g_list_free (window_list);
  g_object_unref (original_dir);
}



/**
 * thunar_window_show_and_select_files:
 * @window            : a #ThunarWindow instance.
 * @files_to_select   : a list of #GFile<!---->s
 *
 * Visually selects the files, given by the list. If the files are being restored from the trash folder
 * new tabs are opened and then the files are selected.
 **/
void
thunar_window_show_and_select_files (ThunarWindow *window,
                                     GList        *files_to_select)
{
  gboolean      restore_and_show_in_progress;
  ThunarFile   *first_file;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  restore_and_show_in_progress = thunar_file_is_trash (window->current_directory) && thunar_g_file_is_trashed (files_to_select->data) == FALSE;
  if (restore_and_show_in_progress)
    {
      thunar_window_open_files_in_location (window, files_to_select);
    }
  else
    {
      thunar_window_select_files (window, files_to_select);
    }

  /* scroll to first file */
  if (files_to_select != NULL)
    {
      first_file = thunar_file_get (files_to_select->data, NULL);
      thunar_view_scroll_to_file (THUNAR_VIEW (window->view), first_file, FALSE, TRUE, 0.1f, 0.1f);
      g_object_unref (first_file);
    }
}



void
thunar_window_open_files_in_location (ThunarWindow *window,
                                      GList        *files_to_select)
{
  ThunarApplication *application;
  GHashTable        *restore_show_table; /* <string, GList<GFile*>> */
  gchar             *original_uri;
  GFile             *original_dir_file;
  gchar             *original_dir_path;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* prepare hashtable */
  restore_show_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (void(*) (void*))thunar_g_list_free_full);
  for (GList *lp = files_to_select; lp != NULL; lp = lp->next)
    {
      original_dir_file = g_file_get_parent (lp->data);
      original_uri =  g_file_get_uri (lp->data);
      original_dir_path = g_file_get_uri (original_dir_file);

      if (g_hash_table_contains (restore_show_table, original_dir_path) == FALSE)
        {
          GList *list = g_list_prepend (NULL, g_file_new_for_commandline_arg (original_uri));
          g_hash_table_insert (restore_show_table, original_dir_path, list);
        }
      else
        {
          GList *list = g_hash_table_lookup (restore_show_table, original_dir_path);
          list = g_list_append (list, g_file_new_for_commandline_arg (original_uri));
        }

      g_free (original_uri);
      g_object_unref (original_dir_file);
    }
  /* open tabs and show files */
  application = thunar_application_get();
  g_hash_table_foreach (restore_show_table, hash_table_entry_show_and_select_files, application);
  /* free memory */
  g_hash_table_destroy (restore_show_table);
  g_object_unref (application);
}



/**
 * thunar_window_select_files:
 * @window            : a #ThunarWindow instance.
 * @files_to_select   : a list of #GFile<!---->s
 *
 * Visually selects the files, given by the list
 **/
static void
thunar_window_select_files (ThunarWindow *window,
                            GList        *files_to_select)
{
  GList        *thunar_files = NULL;
  ThunarFolder *thunar_folder;

  /* If possible, reload the current directory to make sure new files got added to the view */
  thunar_folder = thunar_folder_get_for_file (window->current_directory);
  if (thunar_folder != NULL)
    {
      thunar_folder_reload (thunar_folder, FALSE);
      g_object_unref (thunar_folder);
    }

  for (GList *lp = files_to_select; lp != NULL; lp = lp->next)
    thunar_files = g_list_append (thunar_files, thunar_file_get (G_FILE (lp->data), NULL));
  thunar_view_set_selected_files (THUNAR_VIEW (window->view), thunar_files);
  g_list_free_full (thunar_files, g_object_unref);
}



static void
thunar_window_create_menu (ThunarWindow       *window,
                           ThunarWindowAction  action,
                           GCallback           cb_update_menu)
{
  GtkWidget *item;
  GtkWidget *submenu;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (action), G_OBJECT (window), GTK_MENU_SHELL (window->menubar));

  submenu = g_object_new (THUNAR_TYPE_MENU, "menu-type", THUNAR_MENU_TYPE_WINDOW, "action_mgr", window->action_mgr, NULL);
  gtk_menu_set_accel_group (GTK_MENU (submenu), window->accel_group);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), GTK_WIDGET (submenu));
  g_signal_connect_swapped (G_OBJECT (submenu), "show", G_CALLBACK (cb_update_menu), window);
}



static void
thunar_window_update_file_menu (ThunarWindow *window,
                                GtkWidget    *menu)
{
  GtkWidget  *item;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  thunar_gtk_menu_clean (GTK_MENU (menu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_NEW_TAB), G_OBJECT (window), GTK_MENU_SHELL (menu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_NEW_WINDOW), G_OBJECT (window), GTK_MENU_SHELL (menu));
  xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
  thunar_menu_add_sections (THUNAR_MENU (menu), THUNAR_MENU_SECTION_OPEN
                                              | THUNAR_MENU_SECTION_EDIT_LAUNCHER
                                              | THUNAR_MENU_SECTION_SENDTO
                                              | THUNAR_MENU_SECTION_CREATE_NEW_FILES
                                              | THUNAR_MENU_SECTION_EMPTY_TRASH
                                              | THUNAR_MENU_SECTION_CUSTOM_ACTIONS
                                              | THUNAR_MENU_SECTION_PROPERTIES);
  xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
  item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_DETACH_TAB), G_OBJECT (window), GTK_MENU_SHELL (menu));
  gtk_widget_set_sensitive (item, gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook_selected)) > 1);
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_CLOSE_ALL_WINDOWS), G_OBJECT (window), GTK_MENU_SHELL (menu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_CLOSE_TAB), G_OBJECT (window), GTK_MENU_SHELL (menu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_CLOSE_WINDOW), G_OBJECT (window), GTK_MENU_SHELL (menu));

  gtk_widget_show_all (GTK_WIDGET (menu));
  thunar_window_redirect_menu_tooltips_to_statusbar (window, GTK_MENU (menu));
}



static void
thunar_window_update_edit_menu (ThunarWindow *window,
                                GtkWidget    *menu)
{
  GtkWidget       *gtk_menu_item;
  GList           *thunarx_menu_items;
  GList           *pp, *lp;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  thunar_gtk_menu_clean (GTK_MENU (menu));

  gtk_menu_item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_UNDO), G_OBJECT (window), GTK_MENU_SHELL (menu));
  gtk_widget_set_sensitive (gtk_menu_item, thunar_job_operation_history_can_undo ());
  gtk_menu_item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_REDO), G_OBJECT (window), GTK_MENU_SHELL (menu));
  gtk_widget_set_sensitive (gtk_menu_item, thunar_job_operation_history_can_redo ());
  xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));

  thunar_menu_add_sections (THUNAR_MENU (menu), THUNAR_MENU_SECTION_CUT
                                              | THUNAR_MENU_SECTION_COPY_PASTE
                                              | THUNAR_MENU_SECTION_TRASH_DELETE);
  if (window->view != NULL)
    {
      thunar_standard_view_append_menu_item (THUNAR_STANDARD_VIEW (window->view),
                                             GTK_MENU (menu), THUNAR_STANDARD_VIEW_ACTION_SELECT_ALL_FILES);
      thunar_standard_view_append_menu_item (THUNAR_STANDARD_VIEW (window->view),
                                             GTK_MENU (menu), THUNAR_STANDARD_VIEW_ACTION_SELECT_BY_PATTERN);
      thunar_standard_view_append_menu_item (THUNAR_STANDARD_VIEW (window->view),
                                             GTK_MENU (menu), THUNAR_STANDARD_VIEW_ACTION_INVERT_SELECTION);
      thunar_standard_view_append_menu_item (THUNAR_STANDARD_VIEW (window->view),
                                             GTK_MENU (menu), THUNAR_STANDARD_VIEW_ACTION_UNSELECT_ALL_FILES);
    }
  xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
  thunar_menu_add_sections (THUNAR_MENU (menu), THUNAR_MENU_SECTION_DUPLICATE
                                              | THUNAR_MENU_SECTION_MAKELINK
                                              | THUNAR_MENU_SECTION_RENAME
                                              | THUNAR_MENU_SECTION_RESTORE);

  /* determine the available preferences providers */
  if (G_LIKELY (window->thunarx_preferences_providers != NULL))
    {
      /* add menu items from all providers */
      for (pp = window->thunarx_preferences_providers; pp != NULL; pp = pp->next)
        {
          /* determine the available menu items for the provider */
          thunarx_menu_items = thunarx_preferences_provider_get_menu_items (THUNARX_PREFERENCES_PROVIDER (pp->data), GTK_WIDGET (window));
          for (lp = thunarx_menu_items; lp != NULL; lp = lp->next)
            {
                gtk_menu_item = thunar_gtk_menu_thunarx_menu_item_new (lp->data, GTK_MENU_SHELL (menu));

                /* Each thunarx_menu_item will be destroyed together with its related gtk_menu_item */
                g_signal_connect_swapped (G_OBJECT (gtk_menu_item), "destroy", G_CALLBACK (g_object_unref), lp->data);
            }

          /* release the list */
          g_list_free (thunarx_menu_items);
        }
    }

  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_PREFERENCES), G_OBJECT (window), GTK_MENU_SHELL (menu));
  gtk_widget_show_all (GTK_WIDGET (menu));

  thunar_window_redirect_menu_tooltips_to_statusbar (window, GTK_MENU (menu));
}



static void
thunar_window_update_view_menu (ThunarWindow *window,
                                GtkWidget    *menu)
{
  GtkWidget  *item;
  GtkWidget  *sub_items;
  gchar      *last_location_bar;
  gboolean    image_preview_visible;
  gboolean    highlight_enabled;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  thunar_gtk_menu_clean (GTK_MENU (menu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_RELOAD), G_OBJECT (window), GTK_MENU_SHELL (menu));
  xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_VIEW_SPLIT), G_OBJECT (window), thunar_window_split_view_is_active (window), GTK_MENU_SHELL (menu));
  xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
  item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_VIEW_LOCATION_SELECTOR_MENU), G_OBJECT (window), GTK_MENU_SHELL (menu));
  sub_items =  gtk_menu_new();
  gtk_menu_set_accel_group (GTK_MENU (sub_items), window->accel_group);
  g_object_get (window->preferences, "last-location-bar", &last_location_bar, NULL);
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_VIEW_LOCATION_SELECTOR_BUTTONS), G_OBJECT (window),
                                                   (g_strcmp0 (last_location_bar, g_type_name (THUNAR_TYPE_LOCATION_BUTTONS)) == 0), GTK_MENU_SHELL (sub_items));
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_VIEW_LOCATION_SELECTOR_ENTRY), G_OBJECT (window),
                                                   (g_strcmp0 (last_location_bar, g_type_name (THUNAR_TYPE_LOCATION_ENTRY)) == 0), GTK_MENU_SHELL (sub_items));
  g_free (last_location_bar);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), GTK_WIDGET (sub_items));
  item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_VIEW_SIDE_PANE_MENU), G_OBJECT (window), GTK_MENU_SHELL (menu));
  sub_items =  gtk_menu_new();
  gtk_menu_set_accel_group (GTK_MENU (sub_items), window->accel_group);
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_VIEW_SIDE_PANE_SHORTCUTS), G_OBJECT (window),
                                                   thunar_window_has_shortcut_sidepane (window), GTK_MENU_SHELL (sub_items));
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_VIEW_SIDE_PANE_TREE), G_OBJECT (window),
                                                   thunar_window_has_tree_view_sidepane (window), GTK_MENU_SHELL (sub_items));
  xfce_gtk_menu_append_separator (GTK_MENU_SHELL (sub_items));
  image_preview_visible = gtk_widget_get_visible (window->right_pane_box) || gtk_widget_get_visible (window->sidepane_preview_image);
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_TOGGLE_IMAGE_PREVIEW), G_OBJECT (window),
                                                   image_preview_visible, GTK_MENU_SHELL (sub_items));
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), GTK_WIDGET (sub_items));
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_VIEW_STATUSBAR), G_OBJECT (window),
                                                   gtk_widget_get_visible (window->statusbar), GTK_MENU_SHELL (menu));
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_VIEW_MENUBAR), G_OBJECT (window),
                                                   window->menubar_visible, GTK_MENU_SHELL (menu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_CONFIGURE_TOOLBAR), G_OBJECT (window), GTK_MENU_SHELL (menu));
  xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
  if (window->directory_specific_settings)
    {
      item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_CLEAR_DIRECTORY_SPECIFIC_SETTINGS),
                                                       G_OBJECT (window), GTK_MENU_SHELL (menu));
      gtk_widget_set_sensitive (item, thunar_file_has_directory_specific_settings (window->current_directory));
    }
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_SHOW_HIDDEN), G_OBJECT (window),
                                                   window->show_hidden, GTK_MENU_SHELL (menu));
  if (thunar_g_vfs_metadata_is_supported ())
    {
      g_object_get (G_OBJECT (window->preferences), "misc-highlighting-enabled", &highlight_enabled, NULL);
      xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_SHOW_HIGHLIGHT), G_OBJECT (window),
                                                       highlight_enabled, GTK_MENU_SHELL (menu));
    }
  xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
  if (window->view != NULL)
    thunar_standard_view_append_menu_items (THUNAR_STANDARD_VIEW (window->view), GTK_MENU (menu), window->accel_group);
  xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
  thunar_window_append_menu_item (window, GTK_MENU_SHELL (menu), THUNAR_WINDOW_ACTION_ZOOM_IN);
  thunar_window_append_menu_item (window, GTK_MENU_SHELL (menu), THUNAR_WINDOW_ACTION_ZOOM_OUT);
  thunar_window_append_menu_item (window, GTK_MENU_SHELL (menu), THUNAR_WINDOW_ACTION_ZOOM_RESET);
  xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
  item = xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_VIEW_AS_ICONS),
                                                 G_OBJECT (window), window->view_type == THUNAR_TYPE_ICON_VIEW, GTK_MENU_SHELL (menu));
  if (window->is_searching == TRUE)
    gtk_widget_set_sensitive (item, FALSE);
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_VIEW_AS_DETAILED_LIST),
                                                   G_OBJECT (window), window->view_type == THUNAR_TYPE_DETAILS_VIEW, GTK_MENU_SHELL (menu));
  item = xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_VIEW_AS_COMPACT_LIST),
                                                          G_OBJECT (window), window->view_type == THUNAR_TYPE_COMPACT_VIEW, GTK_MENU_SHELL (menu));
  if (window->is_searching == TRUE)
    gtk_widget_set_sensitive (item, FALSE);

  gtk_widget_show_all (GTK_WIDGET (menu));

  thunar_window_redirect_menu_tooltips_to_statusbar (window, GTK_MENU (menu));
}



static void
thunar_window_update_go_menu (ThunarWindow *window,
                              GtkWidget    *menu)
{
  gchar                    *icon_name;
  const XfceGtkActionEntry *action_entry;
  ThunarHistory            *history = NULL;
  GtkWidget                *item;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  if (window->view != NULL)
    history = thunar_standard_view_get_history (THUNAR_STANDARD_VIEW (window->view));

  thunar_gtk_menu_clean (GTK_MENU (menu));
  item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_OPEN_PARENT), G_OBJECT (window), GTK_MENU_SHELL (menu));
  gtk_widget_set_sensitive (item, !thunar_g_file_is_root (thunar_file_get_file (window->current_directory)));
  item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_BACK), G_OBJECT (window), GTK_MENU_SHELL (menu));
  if (history != NULL)
    gtk_widget_set_sensitive (item, thunar_history_has_back (history));
  item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_FORWARD), G_OBJECT (window), GTK_MENU_SHELL (menu));
  if (history != NULL)
    gtk_widget_set_sensitive (item, thunar_history_has_forward (history));
  xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_OPEN_COMPUTER), G_OBJECT (window), GTK_MENU_SHELL (menu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_OPEN_HOME), G_OBJECT (window), GTK_MENU_SHELL (menu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_OPEN_DESKTOP), G_OBJECT (window), GTK_MENU_SHELL (menu));
  if (thunar_g_vfs_is_uri_scheme_supported ("recent"))
    xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_OPEN_RECENT), G_OBJECT (window), GTK_MENU_SHELL (menu));
  if (thunar_g_vfs_is_uri_scheme_supported ("trash"))
    {
      GFile      *gfile;
      ThunarFile *trash_folder;

      /* try to connect to the trash bin */
      gfile = thunar_g_file_new_for_trash ();
      if (gfile != NULL)
        {
          trash_folder = thunar_file_get (gfile, NULL);
          if (trash_folder != NULL)
            {
              action_entry = get_action_entry (THUNAR_WINDOW_ACTION_OPEN_TRASH);
              if (action_entry != NULL)
                {
                  if (thunar_file_get_item_count (trash_folder) > 0)
                    icon_name = "user-trash-full";
                  else
                    icon_name = "user-trash";
                  xfce_gtk_image_menu_item_new_from_icon_name (action_entry->menu_item_label_text, action_entry->menu_item_tooltip_text,
                                                               action_entry->accel_path, action_entry->callback, G_OBJECT (window), icon_name, GTK_MENU_SHELL (menu));
                }
              g_object_unref (trash_folder);
            }
          g_object_unref (gfile);
        }
    }
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_OPEN_TEMPLATES), G_OBJECT (window), GTK_MENU_SHELL (menu));
  xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_OPEN_FILE_SYSTEM), G_OBJECT (window), GTK_MENU_SHELL (menu));
  xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_OPEN_NETWORK), G_OBJECT (window), GTK_MENU_SHELL (menu));
  xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_OPEN_LOCATION), G_OBJECT (window), GTK_MENU_SHELL (menu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_SEARCH), G_OBJECT (window), GTK_MENU_SHELL (menu));
  gtk_widget_show_all (GTK_WIDGET (menu));

  thunar_window_redirect_menu_tooltips_to_statusbar (window, GTK_MENU (menu));
}



static void
thunar_window_update_bookmarks_menu (ThunarWindow *window,
                                     GtkWidget    *menu)
{
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  thunar_gtk_menu_clean (GTK_MENU (menu));

  thunar_action_manager_append_menu_item (window->action_mgr, GTK_MENU_SHELL (menu),THUNAR_ACTION_MANAGER_ACTION_SENDTO_SHORTCUTS, FALSE);
  xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
  thunar_window_menu_add_bookmarks (window, GTK_MENU_SHELL (menu));

  gtk_widget_show_all (GTK_WIDGET (menu));

  thunar_window_redirect_menu_tooltips_to_statusbar (window, GTK_MENU (menu));
}



static void
thunar_window_update_help_menu (ThunarWindow *window,
                                GtkWidget    *menu)
{
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  thunar_gtk_menu_clean (GTK_MENU (menu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_CONTENTS), G_OBJECT (window), GTK_MENU_SHELL (menu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_ABOUT), G_OBJECT (window), GTK_MENU_SHELL (menu));
  gtk_widget_show_all (GTK_WIDGET (menu));

  thunar_window_redirect_menu_tooltips_to_statusbar (window, GTK_MENU (menu));
}



static void
thunar_window_dispose (GObject *object)
{
  ThunarWindow  *window = THUNAR_WINDOW (object);

  /* indicate that history items are out of use */
  window->location_toolbar_item_back = NULL;
  window->location_toolbar_item_forward = NULL;

  if (window->accel_group != NULL)
    {
      gtk_accel_group_disconnect (window->accel_group, NULL);
      gtk_window_remove_accel_group (GTK_WINDOW (window), window->accel_group);
      g_object_unref (window->accel_group);
      window->accel_group = NULL;
    }

  /* destroy the save geometry timer source */
  if (G_UNLIKELY (window->save_geometry_timer_id != 0))
    g_source_remove (window->save_geometry_timer_id);

  /* disconnect from the current-directory */
  thunar_window_set_current_directory (window, NULL);

  (*G_OBJECT_CLASS (thunar_window_parent_class)->dispose) (object);
}



static void
thunar_window_finalize (GObject *object)
{
  ThunarWindow *window = THUNAR_WINDOW (object);

  thunar_window_free_bookmarks (window);
  g_list_free_full (window->thunarx_preferences_providers, g_object_unref);
  g_free (window->search_query);

  /* disconnect from the volume monitor */
  g_signal_handlers_disconnect_matched (window->device_monitor, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, window);
  g_object_unref (window->device_monitor);

  g_signal_handlers_disconnect_by_data (window->action_mgr, window);
  g_object_unref (window->action_mgr);

  if (window->bookmark_file != NULL)
    g_object_unref (window->bookmark_file);

  if (window->bookmark_monitor != NULL)
    {
      g_file_monitor_cancel (window->bookmark_monitor);
      g_object_unref (window->bookmark_monitor);
    }

  /* detach from the file monitor */
  if (window->uca_file_monitor != NULL)
    {
      g_file_monitor_cancel (window->uca_file_monitor);
      g_object_unref (window->uca_file_monitor);
      g_object_unref (window->uca_file);
    }

  /* release our reference on the provider factory */
  g_object_unref (window->provider_factory);

  /* release the preferences reference */
  g_signal_handlers_disconnect_by_data (window->preferences, window);
  g_object_unref (window->preferences);

  g_signal_handlers_disconnect_by_data (window->thumbnailer, window);
  g_object_unref (window->thumbnailer);

  /* disconnect signal from GtkRecentManager */
  g_signal_handlers_disconnect_by_data (G_OBJECT (gtk_recent_manager_get_default()), window);

  g_object_unref (window->job_operation_history);

  (*G_OBJECT_CLASS (thunar_window_parent_class)->finalize) (object);
}



static gboolean thunar_window_delete (GtkWidget *widget,
                                      GdkEvent  *event,
                                      gpointer   data )
{
  gboolean      confirm_close_multiple_tabs, do_not_ask_again, restore_tabs;
  gint          response, n_tabs, n_tabsl = 0, n_tabsr = 0;
  gint          current_page_left = 0, current_page_right = 0;
  ThunarWindow *window = THUNAR_WINDOW (widget);
  gchar       **tab_uris_left;
  gchar       **tab_uris_right;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (widget),FALSE);

  if (window->is_searching == TRUE)
    thunar_window_action_cancel_search (window);

  if (window->reset_view_type_idle_id != 0)
    {
      thunar_window_reset_view_type_idle (window);
      g_source_remove (window->reset_view_type_idle_id);
    }

  if (window->notebook_left)
    n_tabsl = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook_left));
  if (window->notebook_right)
    n_tabsr = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook_right));
  n_tabs = n_tabsl + n_tabsr;

  /* save open tabs */
  g_object_get (G_OBJECT (window->preferences), "last-restore-tabs", &restore_tabs, NULL);
  if (restore_tabs)
    {
      tab_uris_left = g_new0 (gchar *, n_tabsl + 1);
      for (int i = 0; i < n_tabsl; i++)
        {
          ThunarNavigator *view = THUNAR_NAVIGATOR (gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook_left), i));
          gchar *uri = g_file_get_uri (thunar_file_get_file (thunar_navigator_get_current_directory (view)));
          tab_uris_left[i] = g_strdup (uri);
          g_free (uri);
        }

      tab_uris_right = g_new0 (gchar *, n_tabsr + 1);
      for (int i = 0; i < n_tabsr; i++)
        {
          ThunarNavigator *view = THUNAR_NAVIGATOR (gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook_right), i));
          gchar *uri = g_file_get_uri (thunar_file_get_file (thunar_navigator_get_current_directory (view)));
          tab_uris_right[i] = g_strdup (uri);
          g_free (uri);
        }

      if (window->notebook_left)
        current_page_left = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook_left));
      if (window->notebook_right)
        current_page_right = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook_right));

      g_object_set (G_OBJECT (window->preferences), "last-tabs-left", tab_uris_left, NULL);
      g_object_set (G_OBJECT (window->preferences), "last-tabs-right", tab_uris_right, NULL);

      if (current_page_left > -1)
        g_object_set (G_OBJECT (window->preferences), "last-focused-tab-left", current_page_left, NULL);
      if (current_page_right > -1)
        g_object_set (G_OBJECT (window->preferences), "last-focused-tab-right", current_page_right, NULL);

      g_strfreev (tab_uris_left);
      g_strfreev (tab_uris_right);
  }

  /* if we don't have muliple tabs in one of the notebooks then just exit */
  if (thunar_window_split_view_is_active (window))
    {
      if (n_tabs < 3)
        return FALSE;
    }
  else
    {
      if (n_tabs < 2)
        return FALSE;
    }

  /* check if the user has disabled confirmation of closing multiple tabs, and just exit if so */
  g_object_get (G_OBJECT (window->preferences),
                "misc-confirm-close-multiple-tabs", &confirm_close_multiple_tabs,
                NULL);
  if(!confirm_close_multiple_tabs)
    return FALSE;

  /* ask the user for confirmation */
  do_not_ask_again = FALSE;
  response = xfce_dialog_confirm_close_tabs (GTK_WINDOW (widget), n_tabs, TRUE, &do_not_ask_again);

  /* if the user requested not to be asked again, store this preference */
  if (response != GTK_RESPONSE_CANCEL && do_not_ask_again)
    g_object_set (G_OBJECT (window->preferences),
                  "misc-confirm-close-multiple-tabs", FALSE, NULL);

  if(response == GTK_RESPONSE_YES)
    return FALSE;

  /* close active tab in active notebook */
  if(response == GTK_RESPONSE_CLOSE)
    gtk_notebook_remove_page (GTK_NOTEBOOK (window->notebook_selected), gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook_selected)));
  return TRUE;
}



static void
thunar_window_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  ThunarWindow *window = THUNAR_WINDOW (object);

  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      g_value_set_object (value, thunar_window_get_current_directory (window));
      break;

    case PROP_ZOOM_LEVEL:
      g_value_set_enum (value, window->zoom_level);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_window_set_property (GObject            *object,
                            guint               prop_id,
                            const GValue       *value,
                            GParamSpec         *pspec)
{
  ThunarWindow *window = THUNAR_WINDOW (object);

  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      thunar_window_set_current_directory (window, g_value_get_object (value));
      break;

    case PROP_ZOOM_LEVEL:
      thunar_window_set_zoom_level (window, g_value_get_enum (value));
      break;

    case PROP_DIRECTORY_SPECIFIC_SETTINGS:
      thunar_window_set_directory_specific_settings (window, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gboolean
thunar_window_reload (ThunarWindow *window,
                      gboolean      reload_info)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* force the view to reload */
  if (G_LIKELY (window->view != NULL))
    {
      thunar_view_reload (THUNAR_VIEW (window->view), reload_info);
      return TRUE;
    }

  return FALSE;
}



/**
 * thunar_window_has_shortcut_sidepane:
 * @window : a #ThunarWindow instance.
 *
 * Return value: True, if this window is running a shortcut sidepane
 **/
gboolean
thunar_window_has_shortcut_sidepane (ThunarWindow *window)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* check if a side pane is currently active */
  if (G_LIKELY (window->sidepane != NULL))
    return G_OBJECT_TYPE (window->sidepane) == THUNAR_TYPE_SHORTCUTS_PANE;
  return FALSE;
}



/**
 * thunar_window_has_tree_view_sidepane:
 * @window : a #ThunarWindow instance.
 *
 * Return value: True, if this window is running a tree_view sidepane
 **/
gboolean
thunar_window_has_tree_view_sidepane (ThunarWindow *window)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* check if a side pane is currently active */
  if (G_LIKELY (window->sidepane != NULL))
    return G_OBJECT_TYPE (window->sidepane) == THUNAR_TYPE_TREE_PANE;
  return FALSE;
}



/**
 * thunar_window_get_sidepane:
 * @window : a #ThunarWindow instance.
 *
 * Return value: (transfer none): The #ThunarSidePane of this window, or NULL if not available
 **/
GtkWidget* thunar_window_get_sidepane (ThunarWindow *window)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);
  return GTK_WIDGET (window->sidepane);
}



static gboolean
thunar_window_toggle_sidepane (ThunarWindow *window)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* check if a side pane is currently active */
  if (G_LIKELY (window->sidepane != NULL))
    {
      /* determine the currently active side pane type */
      window->toggle_sidepane_type = G_OBJECT_TYPE (window->sidepane);
      thunar_window_install_sidepane (window, G_TYPE_NONE);
    }
  else
    {
      /* check if we have a previously remembered toggle type */
      if (window->toggle_sidepane_type == THUNAR_TYPE_TREE_PANE || window->toggle_sidepane_type == THUNAR_TYPE_SHORTCUTS_PANE)
          thunar_window_install_sidepane (window, window->toggle_sidepane_type);
    }

  return TRUE;
}



static gboolean
thunar_window_zoom_in (ThunarWindow *window)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* check if we can still zoom in */
  if (G_LIKELY (window->zoom_level < THUNAR_ZOOM_N_LEVELS - 1))
    thunar_window_set_zoom_level (window, window->zoom_level + 1);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_zoom_out (ThunarWindow *window)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* check if we can still zoom out */
  if (G_LIKELY (window->zoom_level > 0))
    thunar_window_set_zoom_level (window, window->zoom_level - 1);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_zoom_reset (ThunarWindow *window)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* tell the view to reset it's zoom level */
  if (G_LIKELY (window->view != NULL))
    {
      thunar_view_reset_zoom_level (THUNAR_VIEW (window->view));
      return TRUE;
    }

  return TRUE;
}



static gboolean
thunar_window_tab_change (ThunarWindow *window,
                          gint          nth)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* Alt+0 is 10th tab */
  thunar_window_notebook_set_current_tab (window, nth == -1 ? 9 : nth);

  return TRUE;
}



static gboolean
thunar_window_action_switch_next_tab (ThunarWindow *window)
{
  gint current_page;
  gint new_page;
  gint pages;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook_selected));
  pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook_selected));
  new_page = (current_page + 1) % pages;

  thunar_window_notebook_set_current_tab (window, new_page);

  return TRUE;
}



static gboolean
thunar_window_action_switch_previous_tab (ThunarWindow *window)
{
  gint current_page;
  gint new_page;
  gint pages;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook_selected));
  pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook_selected));
  new_page = (current_page - 1) % pages;

  thunar_window_notebook_set_current_tab (window, new_page);

  return TRUE;
}



static void
thunar_window_clipboard_manager_changed (GtkWidget *widget)
{
  ThunarWindow *window = THUNAR_WINDOW (widget);

  /* check if the content actually can be pasted into thunar,
   * in order to do not redraw the view if just some text is copied.
   */
  if (thunar_clipboard_manager_get_can_paste (window->clipboard) && G_LIKELY (window->view != NULL))
    thunar_standard_view_queue_redraw (THUNAR_STANDARD_VIEW (window->view));
}



static void
thunar_window_realize (GtkWidget *widget)
{
  ThunarWindow *window = THUNAR_WINDOW (widget);

  /* let the GtkWidget class perform the realize operation */
  (*GTK_WIDGET_CLASS (thunar_window_parent_class)->realize) (widget);

  /* connect to the clipboard manager of the new display and be sure to redraw the window
   * whenever the clipboard contents change to make sure we always display up2date state.
   */
  window->clipboard = thunar_clipboard_manager_get_for_display (gtk_widget_get_display (widget));
  g_signal_connect_swapped (G_OBJECT (window->clipboard), "changed",
                            G_CALLBACK (thunar_window_clipboard_manager_changed), widget);
}



static void
thunar_window_unrealize (GtkWidget *widget)
{
  ThunarWindow *window = THUNAR_WINDOW (widget);

  /* disconnect from the clipboard manager */
  g_signal_handlers_disconnect_by_func (G_OBJECT (window->clipboard), thunar_window_clipboard_manager_changed, widget);

  /* let the GtkWidget class unrealize the window */
  (*GTK_WIDGET_CLASS (thunar_window_parent_class)->unrealize) (widget);

  /* drop the reference on the clipboard manager, we do this after letting the GtkWidget class
   * unrealise the window to prevent the clipboard being disposed during the unrealize  */
  g_object_unref (G_OBJECT (window->clipboard));
}



static gboolean
thunar_window_configure_event (GtkWidget         *widget,
                               GdkEventConfigure *event)
{
  ThunarWindow *window = THUNAR_WINDOW (widget);
  GtkAllocation widget_allocation;

  gtk_widget_get_allocation (widget, &widget_allocation);

  /* check if we have a new dimension here */
  if (widget_allocation.width != event->width || widget_allocation.height != event->height)
    {
      /* drop any previous timer source */
      if (window->save_geometry_timer_id != 0)
        g_source_remove (window->save_geometry_timer_id);

      /* check if we should schedule another save timer */
      if (gtk_widget_get_visible (widget))
        {
          /* save the geometry one second after the last configure event */
          window->save_geometry_timer_id = g_timeout_add_seconds_full (G_PRIORITY_LOW, 1, thunar_window_save_geometry_timer,
                                                                       window, thunar_window_save_geometry_timer_destroy);
        }
    }

  /* let Gtk+ handle the configure event */
  return (*GTK_WIDGET_CLASS (thunar_window_parent_class)->configure_event) (widget, event);
}



static void
thunar_window_binding_destroyed (gpointer data,
                                 GObject  *binding)
{
  ThunarWindow *window = THUNAR_WINDOW (data);

  if (window->view_bindings != NULL)
    window->view_bindings = g_slist_remove (window->view_bindings, binding);
}



static void
thunar_window_binding_create (ThunarWindow *window,
                              gpointer src_object,
                              const gchar *src_prop,
                              gpointer dst_object,
                              const gchar *dst_prop,
                              GBindingFlags flags)
{
  GBinding *binding;

  _thunar_return_if_fail (G_IS_OBJECT (src_object));
  _thunar_return_if_fail (G_IS_OBJECT (dst_object));

  binding = g_object_bind_property (G_OBJECT (src_object), src_prop,
                                    G_OBJECT (dst_object), dst_prop,
                                    flags);

  g_object_weak_ref (G_OBJECT (binding), thunar_window_binding_destroyed, window);
  window->view_bindings = g_slist_prepend (window->view_bindings, binding);
}



static void
thunar_window_notebook_switch_page (GtkWidget    *notebook,
                                    GtkWidget    *page,
                                    guint         page_num,
                                    ThunarWindow *window)
{
  GSList        *view_bindings;
  ThunarFile    *current_directory;
  ThunarHistory *history;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  _thunar_return_if_fail (THUNAR_IS_VIEW (page));

  /* leave if nothing changed or tab from other split-view is selected as
   * thunar_window_notebook_select_current_page() is going to take care of that */
  if ((window->view == page) || (window->notebook_selected != notebook))
    return;

  /* Use accelerators only on the current active tab */
  if (window->view != NULL)
    g_object_set (G_OBJECT (window->view), "accel-group", NULL, NULL);

  if (G_LIKELY (window->view != NULL))
    {
      /* disconnect from previous history */
      if (window->signal_handler_id_history_changed != 0)
        {
          history = thunar_standard_view_get_history (THUNAR_STANDARD_VIEW (window->view));
          g_signal_handler_disconnect (history, window->signal_handler_id_history_changed);
          window->signal_handler_id_history_changed = 0;
        }

      /* unset view during switch */
      window->view = NULL;
    }

  /* disconnect existing bindings */
  view_bindings = window->view_bindings;
  window->view_bindings = NULL;
  g_slist_free_full (view_bindings, g_object_unref);

  /* update the directory of the current window */
  current_directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (page));
  thunar_window_set_current_directory (window, current_directory);

  /* add stock bindings */
  thunar_window_binding_create (window, window, "current-directory", page, "current-directory", G_BINDING_DEFAULT);
  thunar_window_binding_create (window, page, "loading", window->spinner, "active", G_BINDING_SYNC_CREATE);
  thunar_window_binding_create (window, page, "searching", window->spinner, "active", G_BINDING_SYNC_CREATE);
  thunar_window_binding_create (window, page, "selected-files", window->action_mgr, "selected-files", G_BINDING_SYNC_CREATE);
  thunar_window_binding_create (window, page, "zoom-level", window, "zoom-level", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  /* connect to the sidepane (if any) */
  if (G_LIKELY (window->sidepane != NULL))
    {
      thunar_window_binding_create (window, page, "selected-files",
                                    window->sidepane, "selected-files",
                                    G_BINDING_SYNC_CREATE);
    }

  /* connect to the statusbar (if any) */
  if (G_LIKELY (window->statusbar != NULL))
    {
      thunar_window_binding_create (window, page, "statusbar-text",
                                    window->statusbar, "text",
                                    G_BINDING_SYNC_CREATE);
    }

  if (window->view != NULL)
    g_signal_handlers_disconnect_by_func (window->view, thunar_window_selection_changed, window);

  /* activate new view */
  window->view = page;
  window->view_type = G_TYPE_FROM_INSTANCE (page);

  /* connect accelerators for the view, we need to do this after window->view has been set,
   * see thunar_window_reconnect_accelerators and thunar_standard_view_connect_accelerators */
  g_object_set (G_OBJECT (page), "accel-group", window->accel_group, NULL);

  g_signal_connect_swapped (G_OBJECT (window->view), "notify::selected-files",
                            G_CALLBACK (thunar_window_selection_changed), window);

  /* remember the last view type if directory specific settings are not enabled */
  if (!window->directory_specific_settings && window->view_type != G_TYPE_NONE)
    g_object_set (G_OBJECT (window->preferences), "last-view", g_type_name (window->view_type), NULL);

  /* connect to the new history */
  history = thunar_standard_view_get_history (THUNAR_STANDARD_VIEW (window->view));
  if (history != NULL)
    {
      window->signal_handler_id_history_changed = g_signal_connect_swapped (G_OBJECT (history), "history-changed", G_CALLBACK (thunar_window_history_changed), window);
      thunar_window_history_changed (window);
    }

  /* update the selection */
  thunar_standard_view_selection_changed (THUNAR_STANDARD_VIEW (page));

  gtk_widget_grab_focus (page);

  /* Set trash infobar's `empty trash` button sensitivity, if required */
  if (thunar_file_is_trash (window->current_directory))
    gtk_widget_set_sensitive (window->trash_infobar_empty_button, thunar_file_get_item_count (window->current_directory) > 0);

  /* if the view has an ongoing search operation take that into account, otherwise cancel the current search (if there is one) */
  if (thunar_standard_view_get_search_query (THUNAR_STANDARD_VIEW (page)) != NULL)
    {
      gchar *str = g_strjoin (NULL, SEARCH_PREFIX, thunar_standard_view_get_search_query (THUNAR_STANDARD_VIEW (page)), NULL);
      thunar_window_resume_search (window, str);
      g_free (str);
    }
  else if (window->search_query != NULL)
    thunar_window_action_cancel_search (window);
}



static void
thunar_window_notebook_show_tabs (ThunarWindow *window)
{
  gboolean   always_show_tabs;
  gint       n_pages = 0;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (window->notebook_left || window->notebook_right);

  g_object_get (G_OBJECT (window->preferences), "misc-always-show-tabs", &always_show_tabs, NULL);

  /* check both notebooks, maybe not the selected one get clicked */
  if (window->notebook_left)
    n_pages += gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook_left));
  if (window->notebook_right)
    n_pages += gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook_right));

  if (thunar_window_split_view_is_active (window))
    {
      gtk_notebook_set_show_tabs (GTK_NOTEBOOK (window->notebook_left), n_pages > 2 || always_show_tabs);
      gtk_notebook_set_show_tabs (GTK_NOTEBOOK (window->notebook_right), n_pages > 2 || always_show_tabs);
    }
  else
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK (window->notebook_selected), n_pages > 1 || always_show_tabs);
}



static void
thunar_window_history_changed (ThunarWindow *window)
{
  ThunarHistory *history;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  if (window->view == NULL)
    return;

  history = thunar_standard_view_get_history (THUNAR_STANDARD_VIEW (window->view));
  if (history == NULL)
    return;

  if (window->location_toolbar_item_back != NULL)
    gtk_widget_set_sensitive (window->location_toolbar_item_back, thunar_history_has_back (history));

  if (window->location_toolbar_item_forward != NULL)
    gtk_widget_set_sensitive (window->location_toolbar_item_forward, thunar_history_has_forward (history));
}



static void
thunar_window_notebook_page_added (GtkWidget    *notebook,
                                   GtkWidget    *page,
                                   guint         page_num,
                                   ThunarWindow *window)
{
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  _thunar_return_if_fail (THUNAR_IS_VIEW (page));
  _thunar_return_if_fail (window->notebook_selected == notebook);

  /* connect signals */
  g_signal_connect (G_OBJECT (page), "notify::loading", G_CALLBACK (thunar_window_notify_loading), window);
  g_signal_connect_swapped (G_OBJECT (page), "start-open-location", G_CALLBACK (thunar_window_start_open_location), window);
  g_signal_connect_swapped (G_OBJECT (page), "change-directory", G_CALLBACK (thunar_window_set_current_directory), window);
  g_signal_connect_swapped (G_OBJECT (page), "open-new-tab", G_CALLBACK (thunar_window_notebook_open_new_tab), window);

  /* update tab visibility */
  thunar_window_notebook_show_tabs (window);

  /* set default type if not set yet */
  if (window->view_type == G_TYPE_NONE)
    window->view_type = G_OBJECT_TYPE (page);
}



static void
thunar_window_notebook_page_removed (GtkWidget    *notebook,
                                     GtkWidget    *page,
                                     guint         page_num,
                                     ThunarWindow *window)
{
  gint       n_pages;
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  _thunar_return_if_fail (THUNAR_IS_VIEW (page));
  _thunar_return_if_fail (window->notebook_left == notebook || window->notebook_right == notebook);
  
  /* drop connected signals */
  g_signal_handlers_disconnect_matched (page, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, window);

  n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook));
  if (n_pages == 0)
    {
      if (thunar_window_split_view_is_active (window))
        {
          /* select the other notebook if the current gets closed */
          if (notebook == window->notebook_selected)
            thunar_window_paned_notebooks_switch (window);

          thunar_window_action_toggle_split_view (window);
        }
      else
        {
          /* destroy the window */
          gtk_widget_destroy (GTK_WIDGET (window));
        }
    }
  else
    {
      /* page from the other notebook was removed */
      if (notebook != window->notebook_selected)
        thunar_window_paned_notebooks_switch (window);
      else
        /* this page removed -> select next page */
        thunar_window_notebook_select_current_page (window);

      /* update tab visibility */
      thunar_window_notebook_show_tabs (window);
    }
}



static gboolean
thunar_window_notebook_button_press_event (GtkWidget      *notebook,
                                           GdkEventButton *event,
                                           ThunarWindow   *window)
{
  gint           page_num = 0;
  GtkWidget     *page;
  GtkWidget     *label_box;
  GtkAllocation  alloc;
  gint           x, y;
  gboolean       close_tab;

  if ((event->button == 2 || event->button == 3)
      && event->type == GDK_BUTTON_PRESS)
    {
      /* get real window coordinates */
      gdk_window_get_position (event->window, &x, &y);
      x += event->x;
      y += event->y;

      /* lookup the clicked tab */
      while ((page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), page_num)) != NULL)
        {
          label_box = gtk_notebook_get_tab_label (GTK_NOTEBOOK (notebook), page);
          gtk_widget_get_allocation (label_box, &alloc);

          if (x >= alloc.x && x < alloc.x + alloc.width
              && y >= alloc.y && y < alloc.y + alloc.height)
            break;

          page_num++;
        }

      /* leave if no tab could be found */
      if (page == NULL)
        return FALSE;

      if (event->button == 2)
        {
          /* check if we should close the tab */
          g_object_get (window->preferences, "misc-tab-close-middle-click", &close_tab, NULL);
          if (close_tab)
            gtk_widget_destroy (page);
        }
      else if (event->button == 3)
        {
          /* update the current tab before we show the menu */
          gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), page_num);

          /* show the tab menu */
          thunar_window_notebook_popup_menu (notebook, window);
        }

      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_window_notebook_popup_menu (GtkWidget    *notebook,
                                   ThunarWindow *window)
{
  GtkWidget *menu;

  menu = gtk_menu_new ();
  gtk_menu_set_accel_group (GTK_MENU (menu), window->accel_group);
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_NEW_TAB), G_OBJECT (window), GTK_MENU_SHELL (menu));
  xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_DETACH_TAB), G_OBJECT (window), GTK_MENU_SHELL (menu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_SWITCH_PREV_TAB), G_OBJECT (window), GTK_MENU_SHELL (menu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_SWITCH_NEXT_TAB), G_OBJECT (window), GTK_MENU_SHELL (menu));
  xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_CLOSE_TAB), G_OBJECT (window), GTK_MENU_SHELL (menu));
  gtk_widget_show_all (menu);
  thunar_gtk_menu_run (GTK_MENU (menu));
  return TRUE;
}



static gpointer
thunar_window_notebook_create_window (GtkWidget    *notebook,
                                      GtkWidget    *page,
                                      gint          x,
                                      gint          y,
                                      ThunarWindow *window)
{
  GtkWidget         *new_window;
  ThunarApplication *application;
  gint               width, height;
  GdkMonitor        *monitor;
  GdkScreen         *screen;
  GdkRectangle       geo;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), NULL);
  _thunar_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);
  _thunar_return_val_if_fail (window->notebook_selected == notebook, NULL);
  _thunar_return_val_if_fail (THUNAR_IS_VIEW (page), NULL);

  /* do nothing if this window has only 1 tab */
  if (gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook)) < 2)
    return NULL;

  /* create new window */
  application = thunar_application_get ();
  screen = gtk_window_get_screen (GTK_WINDOW (window));
  new_window = thunar_application_open_window (application, NULL, screen, NULL, TRUE);
  g_object_unref (application);

  /* make sure the new window has the same size */
  gtk_window_get_size (GTK_WINDOW (window), &width, &height);
  gtk_window_resize (GTK_WINDOW (new_window), width, height);

  /* move the window to the drop position */
  if (x >= 0 && y >= 0)
    {
      /* get the monitor geometry */
      monitor = gdk_display_get_monitor_at_point (gdk_display_get_default (), x, y);
      gdk_monitor_get_geometry (monitor, &geo);

      /* calculate window position, but keep it on the current monitor */
      x = CLAMP (x - width / 2, geo.x, geo.x + geo.width - width);
      y = CLAMP (y - height / 2, geo.y, geo.y + geo.height - height);

      /* move the window */
      gtk_window_move (GTK_WINDOW (new_window), MAX (0, x), MAX (0, y));
    }

  /* insert page in new notebook */
  return THUNAR_WINDOW (new_window)->notebook_selected;
}



static void
thunar_window_notebook_update_title (GtkWidget *label)
{
  ThunarWindow  *window;
  GtkWidget     *view;
  GBinding      *binding;
  gboolean       show_full_path;

  window = g_object_get_data (G_OBJECT (label), "window");
  view = g_object_get_data (G_OBJECT (label), "view");
  binding = g_object_get_data (G_OBJECT (label), "binding");

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* set tab title according to window preferences */
  g_object_get (G_OBJECT (window->preferences), "misc-full-path-in-tab-title", &show_full_path, NULL);

  if (binding != NULL)
    g_binding_unbind (binding);

  if (show_full_path)
  {
    binding = g_object_bind_property (G_OBJECT (view), "full-parsed-path", G_OBJECT (label), "label", G_BINDING_SYNC_CREATE);
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_START);
  }
  else
  {
    binding = g_object_bind_property (G_OBJECT (view), "display-name", G_OBJECT (label), "label", G_BINDING_SYNC_CREATE);
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  }

  g_object_set_data (G_OBJECT (label), "binding", binding);

  return;
}



static GtkWidget*
thunar_window_notebook_insert_page (ThunarWindow  *window,
                                    ThunarFile    *directory,
                                    GType          view_type,
                                    gint           position,
                                    ThunarHistory *history)
{
  GtkWidget      *view;
  GtkWidget      *label;
  GtkWidget      *label_box;
  GtkWidget      *button;
  GtkWidget      *spinner;
  GtkWidget      *icon;
  ThunarColumn    sort_column;
  GtkSortType     sort_order;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), NULL);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (directory), NULL);
  _thunar_return_val_if_fail (view_type != G_TYPE_NONE, NULL);
  _thunar_return_val_if_fail (history == NULL || THUNAR_IS_HISTORY (history), NULL);

  /* Figure out which sort settings to use */
  if (window->view == NULL)
    g_object_get (G_OBJECT (window->preferences), "last-sort-column", &sort_column, "last-sort-order", &sort_order, NULL);
  else
    g_object_get (window->view, "sort-column", &sort_column, "sort-order", &sort_order, NULL);

  /* allocate and setup a new view */
  view = g_object_new (view_type, "current-directory", directory,
                                  "sort-column-default", sort_column,
                                  "sort-order-default", sort_order, NULL);
  thunar_view_set_show_hidden (THUNAR_VIEW (view), window->show_hidden);

  gtk_widget_show (view);

  /* set the history of the view if a history is provided */
  if (history != NULL)
    {
      thunar_standard_view_set_history (THUNAR_STANDARD_VIEW (view), history);
    }

  label_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  label = gtk_label_new (NULL);

  g_object_set_data (G_OBJECT (label), "window", window);
  g_object_set_data (G_OBJECT (label), "view", view);
  g_object_set_data (G_OBJECT (label), "binding", NULL);
  thunar_window_notebook_update_title (label);

  g_signal_connect_object (window->preferences, "notify::misc-full-path-in-tab-title", G_CALLBACK (thunar_window_notebook_update_title), label, G_CONNECT_SWAPPED);

  g_object_bind_property (G_OBJECT (view), "full-parsed-path", G_OBJECT (label), "tooltip-text", G_BINDING_SYNC_CREATE);
  gtk_widget_set_has_tooltip (label, TRUE);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_widget_set_margin_start (GTK_WIDGET(label), 3);
  gtk_widget_set_margin_end (GTK_WIDGET(label), 3);
  gtk_widget_set_margin_top (GTK_WIDGET(label), 3);
  gtk_widget_set_margin_bottom (GTK_WIDGET(label), 3);
  gtk_label_set_single_line_mode (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (label_box), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  spinner = gtk_spinner_new ();
  gtk_box_pack_start (GTK_BOX (label_box), spinner, FALSE, FALSE, 0);
  gtk_widget_show (spinner);

  g_object_bind_property (G_OBJECT (spinner), "active",
                          G_OBJECT (spinner), "visible",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (G_OBJECT (view), "searching",
                          G_OBJECT (spinner), "active",
                          G_BINDING_SYNC_CREATE);

  button = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (label_box), button, FALSE, FALSE, 0);
  gtk_widget_set_can_default (button, FALSE);
  gtk_widget_set_focus_on_click (button, FALSE);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_widget_set_tooltip_text (button, _("Close tab"));
  g_signal_connect_swapped (G_OBJECT (button), "clicked", G_CALLBACK (gtk_widget_destroy), view);
  gtk_widget_show (button);

  icon = gtk_image_new_from_icon_name ("window-close", GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (button), icon);
  gtk_widget_show (icon);

  /* insert the new page */
  gtk_notebook_insert_page (GTK_NOTEBOOK (window->notebook_selected), view, label_box, position);

  /* set tab child properties */
  gtk_container_child_set (GTK_CONTAINER (window->notebook_selected), view, "tab-expand", TRUE, NULL);
  gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (window->notebook_selected), view, TRUE);
  gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (window->notebook_selected), view, TRUE);

  return view;
}



static void
thunar_window_notebook_select_current_page (ThunarWindow *window)
{
  gint       current_page_n;
  GtkWidget *current_page;

  _thunar_return_if_fail (window->notebook_selected != NULL);

  current_page_n = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook_selected));
  current_page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook_selected), current_page_n);
  thunar_window_notebook_switch_page (window->notebook_selected, current_page, current_page_n, window);
}



static GtkWidget*
thunar_window_paned_notebooks_add (ThunarWindow *window)
{
  GtkWidget *notebook;
  _thunar_return_val_if_fail(THUNAR_IS_WINDOW (window), NULL);
  _thunar_return_val_if_fail (!thunar_window_split_view_is_active (window), NULL);

  notebook = gtk_notebook_new ();
  gtk_widget_set_hexpand (notebook, TRUE);
  gtk_widget_set_vexpand (notebook, TRUE);
  g_signal_connect (G_OBJECT (notebook), "switch-page", G_CALLBACK (thunar_window_notebook_switch_page), window);
  g_signal_connect (G_OBJECT (notebook), "page-added", G_CALLBACK (thunar_window_notebook_page_added), window);
  g_signal_connect (G_OBJECT (notebook), "page-removed", G_CALLBACK (thunar_window_notebook_page_removed), window);
  g_signal_connect_after (G_OBJECT (notebook), "button-press-event", G_CALLBACK (thunar_window_notebook_button_press_event), window);
  g_signal_connect (G_OBJECT (notebook), "popup-menu", G_CALLBACK (thunar_window_notebook_popup_menu), window);
  g_signal_connect (G_OBJECT (notebook), "create-window", G_CALLBACK (thunar_window_notebook_create_window), window);

  gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);
  gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);
  gtk_container_set_border_width (GTK_CONTAINER (notebook), 0);
  gtk_notebook_set_group_name (GTK_NOTEBOOK (notebook), "thunar-tabs");
  gtk_widget_show (notebook);
  if (window->notebook_left == NULL)
    {
      gtk_paned_pack1 (GTK_PANED (window->paned_notebooks), notebook, TRUE, FALSE);
      window->notebook_left = notebook;
    }
  else if (window->notebook_right == NULL)
    {
      gtk_paned_pack2 (GTK_PANED (window->paned_notebooks), notebook, TRUE, FALSE);
      window->notebook_right = notebook;
    }
  return notebook;
}



static void
thunar_window_paned_notebooks_switch (ThunarWindow *window)
{
  GtkWidget *new_curr_notebook = NULL;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (thunar_window_split_view_is_active (window));

  if (window->notebook_selected == window->notebook_left)
    new_curr_notebook = window->notebook_right;
  else if (window->notebook_selected == window->notebook_right)
    new_curr_notebook = window->notebook_left;

  if (new_curr_notebook)
    {
      thunar_window_paned_notebooks_indicate_focus (window, new_curr_notebook);

      /* select and activate selected notebook */
      window->notebook_selected = new_curr_notebook;
      thunar_window_notebook_select_current_page (window);
    }
}



void
thunar_window_focus_view (ThunarWindow *window,
                          GtkWidget    *view)
{
  GtkWidget  *selected_notebook;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  if (!thunar_window_split_view_is_active (window))
    return;

  selected_notebook = gtk_widget_get_ancestor (view, GTK_TYPE_NOTEBOOK);
  if (selected_notebook == window->notebook_selected)
    return;

  thunar_window_paned_notebooks_switch (window);
}



static void
thunar_window_paned_notebooks_indicate_focus (ThunarWindow *window,
                                              GtkWidget    *notebook)
{
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  _thunar_return_if_fail (thunar_window_split_view_is_active (window));

  gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), TRUE);
  if (notebook == window->notebook_left)
    gtk_notebook_set_show_border (GTK_NOTEBOOK (window->notebook_right), FALSE);

  if (notebook == window->notebook_right)
    gtk_notebook_set_show_border (GTK_NOTEBOOK (window->notebook_left), FALSE);

  gtk_widget_grab_focus (notebook);
}



static gboolean
thunar_window_split_view_is_active (ThunarWindow *window)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);
  return (window->notebook_left && window->notebook_right);
}

static gboolean
thunar_window_paned_notebooks_update_orientation (ThunarWindow *window)
{
  gboolean vertical_split_panes;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  g_object_get (G_OBJECT (window->preferences), "misc-vertical-split-pane", &vertical_split_panes, NULL);

  if (vertical_split_panes)
    gtk_orientable_set_orientation (GTK_ORIENTABLE (window->paned_notebooks), GTK_ORIENTATION_VERTICAL);
  else
    gtk_orientable_set_orientation (GTK_ORIENTABLE (window->paned_notebooks), GTK_ORIENTATION_HORIZONTAL);

  return TRUE;
}


void
thunar_window_notebook_add_new_tab (ThunarWindow        *window,
                                    ThunarFile          *directory,
                                    ThunarNewTabBehavior behavior)
{
  ThunarHistory *history = NULL;
  GtkWidget     *view;
  gint           page_num;
  GType          view_type;
  gboolean       switch_to_new_tab;

  if (thunar_file_is_directory (directory) == FALSE)
    {
      char *uri = thunar_file_dup_uri (directory);
      g_warning ("Skipping to add tab. The passed URI is not a directory: %s", uri);
      g_free (uri);
      return;
    }

  /* save the history of the current view */
  if (THUNAR_IS_STANDARD_VIEW (window->view))
    {
      history = thunar_standard_view_copy_history (THUNAR_STANDARD_VIEW (window->view));

      /* history is updated only on 'change-directory' signal. */
      /* For inserting a new tab, we need to update it manually */
      if (G_LIKELY (history))
        thunar_history_add (history, directory);
    }

  /* find the correct view type */
  view_type = thunar_window_view_type_for_directory (window, directory);

  /* insert the new view */
  page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook_selected));
  view = thunar_window_notebook_insert_page (window, directory, view_type, page_num + 1, history);

  /* switch to the new view */
  g_object_get (G_OBJECT (window->preferences), "misc-switch-to-new-tab", &switch_to_new_tab, NULL);
  if ((behavior == THUNAR_NEW_TAB_BEHAVIOR_FOLLOW_PREFERENCE && switch_to_new_tab == TRUE)
    || behavior == THUNAR_NEW_TAB_BEHAVIOR_SWITCH)
    {
      page_num = gtk_notebook_page_num (GTK_NOTEBOOK (window->notebook_selected), view);
      thunar_window_notebook_set_current_tab (window, page_num);
    }

  /* take focus on the new view */
  gtk_widget_grab_focus (view);
}



void
thunar_window_notebook_open_new_tab (ThunarWindow *window,
                                      ThunarFile  *directory)
{
  thunar_window_notebook_add_new_tab (window, directory, THUNAR_NEW_TAB_BEHAVIOR_FOLLOW_PREFERENCE);
}



/**
 * thunar_window_notebook_toggle_split_view:
 * @window      : a #ThunarWindow instance.
 *
 * Toggles the split-view functionality for @window.
 **/
void
thunar_window_notebook_toggle_split_view (ThunarWindow *window)
{
  thunar_window_action_toggle_split_view (window);
}



/**
 * thunar_window_notebook_remove_tab:
 * @window      : a #ThunarWindow instance.
 * @tab         : the page index as a #gint.
 *
 * Removes @tab page from the currently selected notebook.
 **/
void
thunar_window_notebook_remove_tab (ThunarWindow *window,
                                   gint          tab)
{
  gtk_notebook_remove_page (GTK_NOTEBOOK (window->notebook_selected), tab);
}



/**
 * thunar_window_notebook_set_current_tab:
 * @window      : a #ThunarWindow instance.
 * @tab         : the page index as a #gint.
 *
 * Switches to the @tab page in the currently selected notebook.
 **/
void
thunar_window_notebook_set_current_tab (ThunarWindow *window,
                                        gint          tab)
{
  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook_selected), tab);
}



void
thunar_window_update_directories (ThunarWindow *window,
                                  ThunarFile   *old_directory,
                                  ThunarFile   *new_directory)
{
  GtkWidget  *view;
  ThunarFile *directory;
  gint        n;
  gint        n_pages;
  gint        active_page;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (THUNAR_IS_FILE (old_directory));
  _thunar_return_if_fail (THUNAR_IS_FILE (new_directory));

  n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook_selected));
  if (G_UNLIKELY (n_pages == 0))
    return;

  active_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook_selected));

  for (n = 0; n < n_pages; n++)
    {
      /* get the view */
      view = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook_selected), n);
      if (! THUNAR_IS_NAVIGATOR (view))
        continue;

      /* get the directory of the view */
      directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (view));
      if (! THUNAR_IS_FILE (directory))
        continue;

      /* if it matches the old directory, change to the new one */
      if (directory == old_directory)
        {
          if (n == active_page)
            thunar_navigator_change_directory (THUNAR_NAVIGATOR (view), new_directory);
          else
            thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (view), new_directory);
        }
    }
}



static void
thunar_window_update_location_bar_visible (ThunarWindow *window)
{
  gchar *last_location_bar = NULL;

  g_object_get (window->preferences, "last-location-bar", &last_location_bar, NULL);

  if (g_strcmp0 (last_location_bar, g_type_name (G_TYPE_NONE)) == 0)
    {
      gtk_widget_hide (window->location_toolbar);
      if (window->view != NULL)
        gtk_widget_grab_focus (window->view);
    }
  else
    gtk_widget_show (window->location_toolbar);

  g_free (last_location_bar);
}



static void
thunar_window_update_window_icon (ThunarWindow *window)
{
  gboolean      change_window_icon;
  GtkIconTheme *icon_theme;
  const gchar  *icon_name = "folder";

  g_object_get (window->preferences, "misc-change-window-icon", &change_window_icon, NULL);

  if (change_window_icon)
    {
      icon_theme = gtk_icon_theme_get_for_screen (gtk_window_get_screen (GTK_WINDOW (window)));
      icon_name = thunar_file_get_icon_name (window->current_directory,
                                             THUNAR_FILE_ICON_STATE_DEFAULT,
                                             icon_theme);
    }

  gtk_window_set_icon_name (GTK_WINDOW (window), icon_name);
}



static void
thunar_window_install_sidepane (ThunarWindow *window,
                                GType         type)
{
  GtkStyleContext *context;

  _thunar_return_if_fail (type == G_TYPE_NONE || g_type_is_a (type, THUNAR_TYPE_SIDE_PANE));
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* drop the previous side pane (if any) */
  if (G_UNLIKELY (window->sidepane != NULL))
    {
      gtk_widget_destroy (window->sidepane);
      window->sidepane = NULL;
    }

  /* check if we have a new sidepane widget */
  if (G_LIKELY (type != G_TYPE_NONE))
    {
      /* allocate the new side pane widget */
      window->sidepane = g_object_new (type, NULL);
      gtk_widget_set_size_request (window->sidepane, 0, -1);
      g_object_bind_property (G_OBJECT (window), "current-directory", G_OBJECT (window->sidepane), "current-directory", G_BINDING_SYNC_CREATE);
      g_signal_connect_swapped (G_OBJECT (window->sidepane), "change-directory", G_CALLBACK (thunar_window_set_current_directory), window);
      g_signal_connect_swapped (G_OBJECT (window->sidepane), "open-new-tab", G_CALLBACK (thunar_window_notebook_open_new_tab), window);
      context = gtk_widget_get_style_context (window->sidepane);
      gtk_style_context_add_class (context, "sidebar");
      gtk_box_pack_start (GTK_BOX (window->sidepane_box), window->sidepane, TRUE, TRUE, 0);
      gtk_widget_show (window->sidepane);
      gtk_widget_show (window->sidepane_box);

      /* connect the side pane widget to the view (if any) */
      if (G_LIKELY (window->view != NULL))
        thunar_window_binding_create (window, window->view, "selected-files", window->sidepane, "selected-files", G_BINDING_SYNC_CREATE);

      /* apply show_hidden config to tree pane */
      if (type == THUNAR_TYPE_TREE_PANE)
        thunar_side_pane_set_show_hidden (THUNAR_SIDE_PANE (window->sidepane), window->show_hidden);
    }
  else
    gtk_widget_hide (window->sidepane_box);

  /* remember the setting */
  if (gtk_widget_get_visible (GTK_WIDGET (window)))
    g_object_set (G_OBJECT (window->preferences), "last-side-pane", g_type_name (type), NULL);
}



static gchar*
thunar_window_bookmark_get_accel_path (GFile *bookmark_file)
{
  GChecksum    *checksum;
  gchar        *uri;
  gchar        *accel_path;
  const gchar  *unique_name;

  _thunar_return_val_if_fail (G_IS_FILE (bookmark_file), NULL);

  /* create unique id based on the uri */
  uri = g_file_get_uri (bookmark_file);
  checksum = g_checksum_new (G_CHECKSUM_MD5);
  g_checksum_update (checksum, (const guchar *) uri, strlen (uri));
  unique_name = g_checksum_get_string (checksum);
  accel_path = g_strconcat("<Actions>/ThunarBookmarks/", unique_name, NULL);

  g_free (uri);
  g_checksum_free (checksum);
  return accel_path;
}



static void
thunar_window_menu_add_bookmarks (ThunarWindow *window,
                                  GtkMenuShell *view_menu)
{
  GList          *lp;
  ThunarBookmark *bookmark;
  ThunarFile     *thunar_file;
  gchar          *parse_name;
  gchar          *accel_path;
  gchar          *tooltip;
  gchar          *name;
  GtkIconTheme   *icon_theme;
  const gchar    *icon_name;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  for (lp = window->bookmarks; lp != NULL; lp = lp->next)
    {
      bookmark = lp->data;
      name = g_strdup (bookmark->name);
      accel_path = thunar_window_bookmark_get_accel_path (bookmark->g_file);
      parse_name = g_file_get_parse_name (bookmark->g_file);
      tooltip = g_strdup_printf (_("Open the location \"%s\""), parse_name);
      g_free (parse_name);

      if (g_file_has_uri_scheme (bookmark->g_file, "file"))
        {
          thunar_file = thunar_file_get (bookmark->g_file, NULL);

          if (name == NULL)
            {
              if (thunar_file != NULL)
                name = g_strdup (thunar_file_get_display_name (thunar_file));
              else
                name = g_file_get_basename (bookmark->g_file);
            }

          icon_theme = gtk_icon_theme_get_for_screen (gtk_window_get_screen (GTK_WINDOW (window)));
          icon_name = thunar_file == NULL ? "folder" : thunar_file_get_icon_name (thunar_file, THUNAR_FILE_ICON_STATE_DEFAULT, icon_theme);

          if (thunar_file != NULL)
            g_object_unref (thunar_file);
        }
      else
        {
          if (name == NULL)
            name = thunar_g_file_get_display_name_remote (bookmark->g_file);

          icon_name = "folder-remote";
        }

      xfce_gtk_image_menu_item_new_from_icon_name (name, tooltip, accel_path, G_CALLBACK (thunar_window_action_open_bookmark), G_OBJECT (bookmark->g_file), icon_name, view_menu);

      g_free (name);
      g_free (tooltip);
      g_free (accel_path);
    }
}



static ThunarBookmark *
thunar_window_bookmark_add (ThunarWindow *window,
                            GFile        *g_file,
                            const gchar  *name)
{
  ThunarBookmark *bookmark;

  bookmark = g_slice_new0 (ThunarBookmark);
  bookmark->g_file = g_object_ref (g_file);
  bookmark->name = g_strdup (name);

  window->bookmarks = g_list_append (window->bookmarks, bookmark);
  return bookmark;
}



static void
thunar_window_free_bookmarks (ThunarWindow *window)
{
  GList          *lp;
  ThunarBookmark *bookmark;

  for (lp = window->bookmarks; lp != NULL; lp = lp->next)
    {
      bookmark = lp->data;
      g_object_unref (bookmark->g_file);
      g_free (bookmark->name);
      g_slice_free (ThunarBookmark, lp->data);
    }
  g_list_free (window->bookmarks);
  window->bookmarks = NULL;
}



static void
thunar_window_update_bookmark (GFile       *g_file,
                               const gchar *name,
                               gint         line_num,
                               gpointer     user_data)
{
  ThunarWindow      *window = THUNAR_WINDOW (user_data);
  gchar             *accel_path;
  XfceGtkActionEntry entry[1];

  _thunar_return_if_fail (G_IS_FILE (g_file));
  _thunar_return_if_fail (name == NULL || g_utf8_validate (name, -1, NULL));
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* Add the bookmark to our internal list of bookmarks */
  thunar_window_bookmark_add (window, g_file, name);
  
  /* Add ref to window to each g_file, will be needed in callback */
  g_object_set_data_full (G_OBJECT (g_file), I_("thunar-window"), window, NULL);

  /* Build a minimal XfceGtkActionEntry in order to be able to use the methods below */
  accel_path = thunar_window_bookmark_get_accel_path (g_file);
  entry[0].accel_path = accel_path;
  entry[0].callback = G_CALLBACK (thunar_window_action_open_bookmark);
  entry[0].default_accelerator = "";

  /* Add entry, so that the bookmark can loaded/saved to acceels.scm (will be skipped if already available)*/
  xfce_gtk_accel_map_add_entries (entry, G_N_ELEMENTS (entry));

  /* Link action with callback */
  xfce_gtk_accel_group_disconnect_action_entries (window->accel_group, entry, G_N_ELEMENTS (entry));
  xfce_gtk_accel_group_connect_action_entries (window->accel_group, entry, G_N_ELEMENTS (entry), g_file);
  g_free (accel_path);
}



static void
thunar_window_update_bookmarks (ThunarWindow *window)
{
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  thunar_window_free_bookmarks (window);

  /* re-create our internal bookmarks according to the bookmark file */
  thunar_util_load_bookmarks (window->bookmark_file,
                              thunar_window_update_bookmark,
                              window);
}



static void
thunar_window_start_open_location (ThunarWindow *window,
                                   const gchar  *initial_text)
{
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* setup a search if required */
  if (initial_text != NULL && thunar_util_is_a_search_query (initial_text) == TRUE)
    {
      GType view_type;
      /* workaround the slowness of ExoIconView */
      view_type = window->view_type;
      thunar_window_action_detailed_view (window);
      thunar_standard_view_save_view_type (THUNAR_STANDARD_VIEW (window->view), view_type); /* save it in the new view */

      /* if directory specific settings are enabled, save the view type for this directory */
      if (window->directory_specific_settings)
        thunar_file_set_metadata_setting (window->current_directory, "view-type", g_type_name (view_type), TRUE);
      /* end of workaround */

      /* temporary show the location toolbar, even if it is normally hidden */
      gtk_widget_show (window->location_toolbar);
      thunar_location_bar_request_entry (THUNAR_LOCATION_BAR (window->location_bar), initial_text, FALSE);

      thunar_window_update_search (window);
      window->is_searching = TRUE;
      thunar_action_manager_set_searching (window->action_mgr, TRUE);

      /* the check is useless as long as the workaround is in place */
      if (THUNAR_IS_DETAILS_VIEW (window->view))
        thunar_details_view_set_location_column_visible (THUNAR_DETAILS_VIEW (window->view), TRUE);
    }
  else /* location edit */
    {
      gchar *last_location_bar = NULL;
      gboolean temporary_till_focus_lost = FALSE;

      /* If the style is already 'location entry', we dont need to care about 'focus-lost' */
      g_object_get (window->preferences, "last-location-bar", &last_location_bar, NULL);
      if (g_strcmp0 (last_location_bar, g_type_name (THUNAR_TYPE_LOCATION_ENTRY)) != 0)
        temporary_till_focus_lost = TRUE;
      g_free (last_location_bar);

      thunar_window_action_cancel_search (window);

      /* temporary show the location toolbar, even if it is normally hidden */
      gtk_widget_show (window->location_toolbar);
      thunar_location_bar_request_entry (THUNAR_LOCATION_BAR (window->location_bar), initial_text, temporary_till_focus_lost);
    }
}



static void
thunar_window_resume_search (ThunarWindow *window,
                             const gchar  *initial_text)
{
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* when setting up the location entry do not resent the search query to the standard view, there is a search ongoing */
  window->ignore_next_search_update = TRUE;

  /* continue searching */
  window->is_searching = TRUE;

  /* temporary show the location toolbar, even if it is normally hidden */
  gtk_widget_show (window->location_toolbar);
  thunar_location_bar_request_entry (THUNAR_LOCATION_BAR (window->location_bar), initial_text, FALSE);

  /* change to search UI and options */
  g_free (window->search_query);
  window->search_query = thunar_location_bar_get_search_query (THUNAR_LOCATION_BAR (window->location_bar));
  if (window->catfish_search_button != NULL)
    gtk_widget_show (window->catfish_search_button);
  thunar_action_manager_set_searching (window->action_mgr, TRUE);

  /* the check is useless as long as the workaround is in place */
  if (THUNAR_IS_DETAILS_VIEW (window->view))
    thunar_details_view_set_location_column_visible (THUNAR_DETAILS_VIEW (window->view), TRUE);

  g_signal_handlers_block_by_func (G_OBJECT (window->location_toolbar_item_search), thunar_window_action_search, window);
  gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (window->location_toolbar_item_search), TRUE);
  g_signal_handlers_unblock_by_func (G_OBJECT (window->location_toolbar_item_search), thunar_window_action_search, window);
}



void
thunar_window_update_search (ThunarWindow *window)
{
  if (window->ignore_next_search_update == TRUE)
    {
      window->ignore_next_search_update = FALSE;
      return;
    }
    
  g_free (window->search_query);
  window->search_query = thunar_location_bar_get_search_query (THUNAR_LOCATION_BAR (window->location_bar));
  thunar_standard_view_set_searching (THUNAR_STANDARD_VIEW (window->view), window->search_query);
  if (window->search_query != NULL && g_strcmp0 (window->search_query, "") != 0 && window->catfish_search_button != NULL)
    gtk_widget_show (window->catfish_search_button);
  else if (window->catfish_search_button != NULL)
    gtk_widget_hide (window->catfish_search_button);
}



static gint
thunar_window_reset_view_type_idle (gpointer window_ptr)
{
  ThunarWindow *window = window_ptr;
  /* null check for the same reason as thunar_standard_view_set_searching */
  if (window->view != NULL)
    {
      if (thunar_standard_view_get_saved_view_type (THUNAR_STANDARD_VIEW (window->view)) != 0)
        thunar_window_action_view_changed (window, thunar_standard_view_get_saved_view_type (THUNAR_STANDARD_VIEW (window->view)));

      thunar_standard_view_save_view_type (THUNAR_STANDARD_VIEW (window->view), 0);
    }

  return G_SOURCE_REMOVE;
}



static void
thunar_window_reset_view_type_idle_destroyed (gpointer data)
{
  _thunar_return_if_fail (THUNAR_IS_WINDOW (data));

  THUNAR_WINDOW (data)->reset_view_type_idle_id = 0;
}




gboolean
thunar_window_action_cancel_search (ThunarWindow *window)
{
  _thunar_return_val_if_fail (THUNAR_IS_LOCATION_BAR (window->location_bar), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  if (window->is_searching == FALSE)
    return FALSE;

  thunar_location_bar_cancel_search (THUNAR_LOCATION_BAR (window->location_bar));
  thunar_standard_view_set_searching (THUNAR_STANDARD_VIEW (window->view), NULL);
  thunar_action_manager_set_searching (window->action_mgr, FALSE);
  if (window->catfish_search_button != NULL)
    gtk_widget_hide (window->catfish_search_button);

  g_free (window->search_query);
  window->search_query = NULL;

  if (THUNAR_IS_DETAILS_VIEW (window->view))
    {
      gboolean is_recent = thunar_file_is_recent (window->current_directory);

      thunar_details_view_set_recency_column_visible (THUNAR_DETAILS_VIEW (window->view), is_recent);
      thunar_details_view_set_location_column_visible (THUNAR_DETAILS_VIEW (window->view), is_recent);
    }

  window->is_searching = FALSE;

  if (window->reset_view_type_idle_id == 0)
    {
      window->reset_view_type_idle_id = g_idle_add_full (G_PRIORITY_LOW, thunar_window_reset_view_type_idle, window,
                                                         thunar_window_reset_view_type_idle_destroyed);
    }

  g_signal_handlers_block_by_func (G_OBJECT (window->location_toolbar_item_search), thunar_window_action_search, window);
  gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (window->location_toolbar_item_search), FALSE);
  g_signal_handlers_unblock_by_func (G_OBJECT (window->location_toolbar_item_search), thunar_window_action_search, window);
  
  /* bring back the original location bar style (relevant if the bar is hidden) */
  thunar_window_update_location_bar_visible (window);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_open_new_tab (ThunarWindow *window,
                                   GtkWidget    *menu_item)
{
  /* open new tab with current directory as default */
  thunar_window_notebook_add_new_tab (window, thunar_window_get_current_directory (window), THUNAR_NEW_TAB_BEHAVIOR_SWITCH);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_open_new_window (ThunarWindow *window,
                                      GtkWidget    *menu_item)
{
  ThunarApplication *application;
  ThunarHistory     *history;
  ThunarWindow      *new_window;
  ThunarFile        *start_file;
  gboolean           misc_open_new_windows_in_split_view;

  /* popup a new window */
  application = thunar_application_get ();
  new_window = THUNAR_WINDOW (thunar_application_open_window (application, window->current_directory,
                                                              gtk_widget_get_screen (GTK_WIDGET (window)), NULL, TRUE));
  g_object_unref (G_OBJECT (application));

  g_object_get (G_OBJECT (window->preferences), "misc-open-new-windows-in-split-view", &misc_open_new_windows_in_split_view, NULL);
  if (misc_open_new_windows_in_split_view)
    thunar_window_notebook_toggle_split_view (new_window);

  /* if we have no origin view we are done */
  if (window->view == NULL)
    return TRUE;

  /* let the view of the new window inherit the history of the origin view */
  history = thunar_standard_view_copy_history (THUNAR_STANDARD_VIEW (window->view));
  if (history != NULL)
    {
      thunar_standard_view_set_history (THUNAR_STANDARD_VIEW (new_window->view), history);
      thunar_window_history_changed (new_window);

      /* connect the new window to the new history */
      new_window->signal_handler_id_history_changed = g_signal_connect_swapped (G_OBJECT (history), "history-changed", G_CALLBACK (thunar_window_history_changed), new_window);
    }

  /* determine the first visible file in the current window */
  if (thunar_view_get_visible_range (THUNAR_VIEW (window->view), &start_file, NULL))
    {
      /* scroll the new window to the same file */
      thunar_view_scroll_to_file (THUNAR_VIEW (new_window->view), start_file, FALSE, TRUE, 0.1f, 0.1f);

      /* release the file reference */
      g_object_unref (G_OBJECT (start_file));
    }

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_detach_tab (ThunarWindow *window,
                                 GtkWidget    *menu_item)
{
  GtkWidget *notebook;
  GtkWidget *label;
  GtkWidget *view = window->view;

  _thunar_return_val_if_fail (THUNAR_IS_VIEW (view), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* create a new window */
  notebook = thunar_window_notebook_create_window (window->notebook_selected, view, -1, -1, window);
  if (notebook == NULL)
    return TRUE;

  /* get the current label */
  label = gtk_notebook_get_tab_label (GTK_NOTEBOOK (window->notebook_selected), view);
  _thunar_return_val_if_fail (GTK_IS_WIDGET (label), FALSE);

  /* ref object so they don't destroy when removed from the container */
  g_object_ref (label);
  g_object_ref (view);

  /* remove view from the current notebook */
  gtk_container_remove (GTK_CONTAINER (window->notebook_selected), view);

  /* insert in the new notebook */
  gtk_notebook_insert_page (GTK_NOTEBOOK (notebook), view, label, 0);

  /* set tab child properties */
  gtk_container_child_set (GTK_CONTAINER (notebook), view, "tab-expand", TRUE, NULL);
  gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (notebook), view, TRUE);
  gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (notebook), view, TRUE);

  /* release */
  g_object_unref (label);
  g_object_unref (view);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_close_all_windows (ThunarWindow *window,
                                        GtkWidget    *menu_item)
{
  ThunarApplication *application;
  GList             *windows;

  /* query the list of currently open windows */
  application = thunar_application_get ();
  windows = thunar_application_get_windows (application);
  g_object_unref (G_OBJECT (application));

  /* destroy all open windows */
  g_list_free_full (windows, (GDestroyNotify) gtk_widget_destroy);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_close_tab (ThunarWindow *window,
                                GtkWidget    *menu_item)
{
  gboolean response;

  if (thunar_window_split_view_is_active (window))
    {
      if (window->view != NULL)
         gtk_widget_destroy (window->view);
    }
  else
    {
      if (gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook_selected)) == 1)
        {
          response = thunar_window_delete (GTK_WIDGET (window), NULL, NULL);
          if (response == FALSE)
            gtk_widget_destroy (GTK_WIDGET (window));
        }
      else if (window->view != NULL)
        gtk_widget_destroy (window->view);
    }

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_close_window (ThunarWindow *window,
                                   GtkWidget    *menu_item)
{
  gboolean response;

  response = thunar_window_delete (GTK_WIDGET (window), NULL, NULL);
  if (response == FALSE)
    gtk_widget_destroy (GTK_WIDGET (window));

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_undo (ThunarWindow *window,
                           GtkWidget    *menu_item)
{
  thunar_job_operation_history_undo ();
  return TRUE; /* return value required in case of shortcut activation, in order to signal that the accel key got handled */
}



static gboolean
thunar_window_action_redo (ThunarWindow *window,
                           GtkWidget    *menu_item)
{
  thunar_job_operation_history_redo ();
  return TRUE; /* return value required in case of shortcut activation, in order to signal that the accel key got handled */
}



static gboolean
thunar_window_action_preferences (ThunarWindow *window,
                                  GtkWidget    *menu_item)
{
  GtkWidget         *dialog;
  ThunarApplication *application;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* allocate and display a preferences dialog */;
  dialog = thunar_preferences_dialog_new (GTK_WINDOW (window));
  gtk_widget_show (dialog);

  /* ...and let the application take care of it */
  application = thunar_application_get ();
  thunar_application_take_window (application, GTK_WINDOW (dialog));
  g_object_unref (G_OBJECT (application));

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_reload (ThunarWindow *window,
                             GtkWidget    *menu_item)
{
  gboolean result;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* force the view to reload */
  g_signal_emit (G_OBJECT (window), window_signals[RELOAD], 0, TRUE, &result);

  /* update the location bar to show the current directory */
  if (window->location_bar != NULL)
    g_object_notify (G_OBJECT (window->location_bar), "current-directory");

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_toggle_split_view (ThunarWindow *window)
{

  ThunarFile    *directory;
  ThunarHistory *history = NULL;
  gint           page_num, last_splitview_separator_position;
  GType          view_type;
  GtkAllocation  allocation;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);
  _thunar_return_val_if_fail (window->view_type != G_TYPE_NONE, FALSE);

  if (thunar_window_split_view_is_active (window))
    {
      if (window->notebook_selected == window->notebook_left)
        {
          gtk_widget_destroy (window->notebook_right);
          window->notebook_right = NULL;
        }
      else if (window->notebook_selected == window->notebook_right)
        {
          gtk_widget_destroy (window->notebook_left);
          window->notebook_left = NULL;
        }
      gtk_notebook_set_show_border (GTK_NOTEBOOK (window->notebook_selected), FALSE);
    }
  else
    {
      GtkWidget* old_notebook = window->notebook_selected;
      window->notebook_selected = thunar_window_paned_notebooks_add (window);
      directory = thunar_window_get_current_directory (window);

      /* save the history of the current view */
      if (THUNAR_IS_STANDARD_VIEW (window->view))
        history = thunar_standard_view_copy_history (THUNAR_STANDARD_VIEW (window->view));

      /* find the correct view type */
      view_type = thunar_window_view_type_for_directory (window, directory);

      /* insert the new view */
      page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook_selected));
      thunar_window_notebook_insert_page (window, directory, view_type, page_num+1, history);

      /* Prevent notebook expand on tab creation */
      g_object_get (G_OBJECT (window->preferences), "last-splitview-separator-position", &last_splitview_separator_position, NULL);
      if (last_splitview_separator_position == 0)
        {
          gtk_widget_get_allocation (GTK_WIDGET (window->paned_notebooks), &allocation);
          last_splitview_separator_position = (gint)(allocation.width/2);
        }

      gtk_paned_set_position (GTK_PANED (window->paned_notebooks), last_splitview_separator_position);
  
      /* Keep focus on the old notebook */
      thunar_window_paned_notebooks_indicate_focus (window, old_notebook);

      g_signal_connect_swapped (window->paned, "accept-position", G_CALLBACK (thunar_window_save_paned_notebooks), window);
      g_signal_connect_swapped (window->paned, "button-release-event", G_CALLBACK (thunar_window_save_paned_notebooks), window);
    }

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_locationbar_entry (ThunarWindow *window)
{
  gchar    *last_location_bar;
  gboolean  entry_checked;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  g_object_get (window->preferences, "last-location-bar", &last_location_bar, NULL);
  entry_checked = (g_strcmp0 (last_location_bar, g_type_name (THUNAR_TYPE_LOCATION_ENTRY)) == 0);
  g_free (last_location_bar);

  thunar_window_action_cancel_search (window);

  if (entry_checked)
    g_object_set (window->preferences, "last-location-bar", g_type_name (G_TYPE_NONE), NULL);
  else
    g_object_set (window->preferences, "last-location-bar", g_type_name (THUNAR_TYPE_LOCATION_ENTRY), NULL);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_locationbar_buttons (ThunarWindow *window)
{
  gchar    *last_location_bar;
  gboolean  buttons_checked;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  g_object_get (window->preferences, "last-location-bar", &last_location_bar, NULL);
  buttons_checked = (g_strcmp0 (last_location_bar, g_type_name (THUNAR_TYPE_LOCATION_BUTTONS)) == 0);
  g_free (last_location_bar);

  thunar_window_action_cancel_search (window);

  if (buttons_checked)
    g_object_set (window->preferences, "last-location-bar", g_type_name (G_TYPE_NONE), NULL);
  else
    g_object_set (window->preferences, "last-location-bar", g_type_name (THUNAR_TYPE_LOCATION_BUTTONS), NULL);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_shortcuts_changed (ThunarWindow *window)
{
  GType type = G_TYPE_NONE;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  if (thunar_window_has_shortcut_sidepane (window) == FALSE)
    type = THUNAR_TYPE_SHORTCUTS_PANE;

  thunar_window_install_sidepane (window, type);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_tree_changed (ThunarWindow *window)
{
  GType type = G_TYPE_NONE;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  if (thunar_window_has_tree_view_sidepane (window) == FALSE)
    type = THUNAR_TYPE_TREE_PANE;

  thunar_window_install_sidepane (window, type);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_image_preview (ThunarWindow *window)
{
  ThunarImagePreviewMode misc_image_preview_mode;
  gboolean               image_preview_visible;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  g_object_get (window->preferences, "misc-image-preview-mode", &misc_image_preview_mode, NULL);

  image_preview_visible = gtk_widget_get_visible (window->right_pane_box) || gtk_widget_get_visible (window->sidepane_preview_image);

  if (misc_image_preview_mode == THUNAR_IMAGE_PREVIEW_MODE_EMBEDDED)
    {
      gtk_widget_set_visible (window->sidepane_preview_image, !image_preview_visible);
      gtk_widget_set_visible (window->right_pane_box, FALSE);
    }
  else
    {
      gtk_widget_set_visible (window->sidepane_preview_image, FALSE);
      gtk_widget_set_visible (window->right_pane_box, !image_preview_visible);
    }

  g_object_set (G_OBJECT (window->preferences), "last-image-preview-visible", !image_preview_visible, NULL);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



gboolean
thunar_window_image_preview_mode_changed (ThunarWindow *window)
{
  ThunarImagePreviewMode misc_image_preview_mode;
  gboolean               last_image_preview_visible;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  g_object_get (window->preferences,
                "last-image-preview-visible", &last_image_preview_visible,
                "misc-image-preview-mode", &misc_image_preview_mode,
                NULL);

  gtk_widget_set_visible (window->sidepane_preview_image, last_image_preview_visible && misc_image_preview_mode == THUNAR_IMAGE_PREVIEW_MODE_EMBEDDED);
  gtk_widget_set_visible (window->right_pane_box, last_image_preview_visible && misc_image_preview_mode == THUNAR_IMAGE_PREVIEW_MODE_STANDALONE);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static void
image_preview_update (GtkWidget     *parent,
                      GtkAllocation *allocation,
                      GtkWidget     *image)
{
  ThunarWindow    *window = THUNAR_WINDOW (gtk_widget_get_toplevel (parent));
  GdkPixbuf       *scaled_preview;
  cairo_surface_t *surface;
  gint             new_size;
  gint             scale_factor;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* there is no image to preview */
  if (window->preview_image_pixbuf == NULL)
    return;


  if (allocation != NULL)
    {
      new_size = allocation->width < allocation->height ?
          allocation->width - 50 :
          allocation->height - 50;
    }
  else
    {
      GtkAllocation alloc;
      gtk_widget_get_allocation (parent, &alloc);
      if (alloc.width - 50 <= 0) /* the widget doesn't have any allocated space */
        return;
      new_size = alloc.width < alloc.height ?
          alloc.width - 50 :
          alloc.height - 50;
    }

  scale_factor = gtk_widget_get_scale_factor (parent);
  scaled_preview = exo_gdk_pixbuf_scale_ratio (window->preview_image_pixbuf, new_size * scale_factor);
  surface = gdk_cairo_surface_create_from_pixbuf (scaled_preview, scale_factor, gtk_widget_get_window (parent));
  gtk_image_set_from_surface (GTK_IMAGE (image), surface);

  g_object_unref (G_OBJECT (scaled_preview));
  cairo_surface_destroy (surface);
}



static gboolean
thunar_window_action_statusbar_changed (ThunarWindow *window)
{
  gboolean statusbar_visible;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  statusbar_visible = gtk_widget_get_visible (window->statusbar);

  gtk_widget_set_visible (window->statusbar, !statusbar_visible);

  g_object_set (G_OBJECT (window->preferences), "last-statusbar-visible", !statusbar_visible, NULL);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static void
thunar_window_action_menubar_update (ThunarWindow *window)
{
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  gtk_widget_set_visible (window->menubar, window->menubar_visible);
}



static gboolean
thunar_window_action_menubar_changed (ThunarWindow *window)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* Dont rely on 'gtk_widget_set_visible', it will be wrong when the menubar is accessed via F10  */
  window->menubar_visible = !window->menubar_visible;

  gtk_widget_set_visible (window->menubar, window->menubar_visible);
  gtk_widget_set_visible (window->location_toolbar_item_view_menubar, !window->menubar_visible);

  g_object_set (G_OBJECT (window->preferences), "last-menubar-visible", window->menubar_visible, NULL);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_show_toolbar_editor (ThunarWindow *window)
{
  thunar_show_toolbar_editor (GTK_WIDGET (window), window->location_toolbar);
  return TRUE;
}



static gboolean
thunar_window_action_clear_directory_specific_settings (ThunarWindow *window)
{
  GType       view_type;
  gboolean    result;

  /* clear the settings */
  thunar_file_clear_directory_specific_settings (window->current_directory);

  /* get the correct view type for the current directory */
  view_type = thunar_window_view_type_for_directory (window, window->current_directory);

  /* force the view to reload so that any changes to the settings are applied */
  g_signal_emit (G_OBJECT (window), window_signals[RELOAD], 0, TRUE, &result);

  /* replace the active view with a new one of the correct type */
  thunar_window_replace_view (window, window->view, view_type);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_detailed_view (ThunarWindow *window)
{
  thunar_window_action_view_changed (window, THUNAR_TYPE_DETAILS_VIEW);
  thunar_details_view_set_date_deleted_column_visible (THUNAR_DETAILS_VIEW (window->view),
                                                       thunar_file_is_trash (window->current_directory));
  thunar_details_view_set_recency_column_visible (THUNAR_DETAILS_VIEW (window->view),
                                                  thunar_file_is_recent (window->current_directory));

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_icon_view (ThunarWindow *window)
{
  if (window->is_searching == FALSE)
    thunar_window_action_view_changed (window, THUNAR_TYPE_ICON_VIEW);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_compact_view (ThunarWindow *window)
{
  if (window->is_searching == FALSE)
    thunar_window_action_view_changed (window, THUNAR_TYPE_COMPACT_VIEW);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static void
thunar_window_replace_view (ThunarWindow *window,
                            GtkWidget    *view,
                            GType         view_type)
{
  ThunarFile     *file = NULL;
  ThunarFile     *current_directory = NULL;
  GtkWidget      *new_view;
  ThunarHistory  *history = NULL;
  ThunarJob      *job = NULL;
  GList          *selected_thunar_files = NULL;
  gint            page_num;
  gboolean        is_current_view;

  _thunar_return_if_fail (view_type != G_TYPE_NONE);

  /* if the view already has the correct type then just return */
  if (view != NULL && G_TYPE_FROM_INSTANCE (view) == view_type)
    return;

  /* is the view we are replacing the active view?
   * (note that this will be true if both view and window->view are NULL) */
  is_current_view = (view == window->view);

  /* save some settings from the old view for the new view */
  if (view != NULL)
    {
      /* disconnect from previous history if the old view is the active view */
      if (is_current_view && window->signal_handler_id_history_changed != 0)
        {
          history = thunar_standard_view_get_history (THUNAR_STANDARD_VIEW (view));
          g_signal_handler_disconnect (history, window->signal_handler_id_history_changed);
          window->signal_handler_id_history_changed = 0;
        }

      /* get first visible file in the old view */
      if (!thunar_view_get_visible_range (THUNAR_VIEW (view), &file, NULL))
        file = NULL;

      /* store the active directory from the old view */
      current_directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (view));
      if (current_directory != NULL)
        g_object_ref (current_directory);

      /* remember the file selection from the old view */
      selected_thunar_files = thunar_g_list_copy_deep (thunar_component_get_selected_files (THUNAR_COMPONENT (view)));

      /* save the history of the current view */
      history = NULL;
      if (THUNAR_IS_STANDARD_VIEW (view))
        {
          history = thunar_standard_view_copy_history (THUNAR_STANDARD_VIEW (view));

          /* Transfer ownership of the search-job to the new view. It is the new view's responsibility to cancel the search. */
          job = thunar_list_model_get_job (THUNAR_STANDARD_VIEW (view)->model);
        }
    }

  if (is_current_view)
    window->view_type = view_type;

  /* if we have not got a current directory from the old view, use the window's current directory */
  if (current_directory == NULL && window->current_directory != NULL)
    current_directory = g_object_ref (window->current_directory);

  _thunar_assert (current_directory != NULL);

  /* find where to insert the new view */
  if (view != NULL)
    page_num = gtk_notebook_page_num (GTK_NOTEBOOK (window->notebook_selected), view);
  else
    page_num = -1;

  /* insert the new view */
  new_view = thunar_window_notebook_insert_page (window, current_directory, view_type, page_num + 1, history);

  /* if we are replacing the active view, make the new view the active view */
  if (is_current_view)
    {
      /* switch to the new view */
      page_num = gtk_notebook_page_num (GTK_NOTEBOOK (window->notebook_selected), new_view);
      thunar_window_notebook_set_current_tab (window, page_num);

      /* take focus on the new view */
      gtk_widget_grab_focus (new_view);
    }

  /* scroll to the previously visible file in the old view */
  if (G_UNLIKELY (file != NULL))
    thunar_view_scroll_to_file (THUNAR_VIEW (new_view), file, FALSE, TRUE, 0.0f, 0.0f);

  /* destroy the old view */
  if (view != NULL)
    gtk_widget_destroy (view);

  /* restore the file selection */
  thunar_component_set_selected_files (THUNAR_COMPONENT (new_view), selected_thunar_files);
  thunar_g_list_free_full (selected_thunar_files);

  /* remember the last view type if this is the active view and directory specific settings are not enabled */
  if (is_current_view && !window->directory_specific_settings && gtk_widget_get_visible (GTK_WIDGET (window)) && view_type != G_TYPE_NONE)
    g_object_set (G_OBJECT (window->preferences), "last-view", g_type_name (view_type), NULL);

  /* release the file references */
  if (G_UNLIKELY (file != NULL))
    g_object_unref (G_OBJECT (file));
  if (G_UNLIKELY (current_directory != NULL))
    g_object_unref (G_OBJECT (current_directory));

  /* connect to the new history if this is the active view */
  if (is_current_view)
    {
      history = thunar_standard_view_get_history (THUNAR_STANDARD_VIEW (new_view));
      window->signal_handler_id_history_changed = g_signal_connect_swapped (G_OBJECT (history),
                                                                            "history-changed",
                                                                            G_CALLBACK (thunar_window_history_changed),
                                                                            window);
    }

  thunar_list_model_set_job (THUNAR_STANDARD_VIEW (new_view)->model, job);
}



static void
thunar_window_action_view_changed (ThunarWindow *window,
                                   GType         view_type)
{
  thunar_window_replace_view (window, window->view, view_type);

  /* if directory specific settings are enabled, save the view type for this directory */
  if (window->directory_specific_settings)
    thunar_file_set_metadata_setting (window->current_directory, "view-type", g_type_name (view_type), TRUE);
}



static gboolean
thunar_window_action_go_up (ThunarWindow *window)
{
  ThunarFile *parent;
  GError     *error = NULL;

  parent = thunar_file_get_parent (window->current_directory, &error);
  if (G_LIKELY (parent != NULL))
    {
      thunar_window_set_current_directory (window, parent);
      g_object_unref (G_OBJECT (parent));
    }
  else
    {
      /* the root folder '/' has no parent. In this special case we do not need a dialog */
      if (error->code != G_FILE_ERROR_NOENT)
        {
          thunar_dialogs_show_error (GTK_WIDGET (window), error, _("Failed to open parent folder"));
        }
      g_error_free (error);
    }

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_back (ThunarWindow *window)
{
  ThunarHistory *history;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  history = thunar_standard_view_get_history (THUNAR_STANDARD_VIEW (window->view));
  thunar_history_action_back (history);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_forward (ThunarWindow *window)
{
  ThunarHistory *history;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  history = thunar_standard_view_get_history (THUNAR_STANDARD_VIEW (window->view));
  thunar_history_action_forward (history);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_open_home (ThunarWindow *window)
{
  GFile         *home;
  ThunarFile    *home_file;
  GError        *error = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* determine the path to the home directory */
  home = thunar_g_file_new_for_home ();

  /* determine the file for the home directory */
  home_file = thunar_file_get (home, &error);
  if (G_UNLIKELY (home_file == NULL))
    {
      /* display an error to the user */
      thunar_dialogs_show_error (GTK_WIDGET (window), error, _("Failed to open the home folder"));
      g_error_free (error);
    }
  else
    {
      /* open the home folder */
      thunar_window_set_current_directory (window, home_file);
      g_object_unref (G_OBJECT (home_file));
    }

  /* release our reference on the home path */
  g_object_unref (home);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_open_user_folder (ThunarWindow   *window,
                                GUserDirectory  thunar_user_dir,
                                const gchar    *default_name)
{
  ThunarFile  *user_file = NULL;
  gboolean     result = FALSE;
  GError      *error = NULL;
  GFile       *home_dir;
  GFile       *user_dir;
  const gchar *path;
  gint         response;
  GtkWidget   *dialog;
  gchar       *parse_name;

  path = g_get_user_special_dir (thunar_user_dir);
  home_dir = thunar_g_file_new_for_home ();

  /* check if there is an entry in user-dirs.dirs */
  if (G_LIKELY (path != NULL))
    {
      user_dir = g_file_new_for_path (path);

      /* if equal to home, leave */
      if (g_file_equal (user_dir, home_dir))
        goto is_homedir;
    }
  else
    {
      /* build a name */
      user_dir = g_file_resolve_relative_path (home_dir, default_name);
    }

  /* try to load the user dir */
  user_file = thunar_file_get (user_dir, NULL);

  /* check if the directory exists */
  if (G_UNLIKELY (user_file == NULL || !thunar_file_exists (user_file)))
    {
      /* release the instance if it does not exist */
      if (user_file != NULL)
        {
          g_object_unref (user_file);
          user_file = NULL;
        }

      /* ask the user to create the directory */
      parse_name = g_file_get_parse_name (user_dir);
      dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_QUESTION,
                                       GTK_BUTTONS_YES_NO,
                                       _("The directory \"%s\" does not exist. Do you want to create it?"),
                                       parse_name);
      gtk_window_set_title (GTK_WINDOW (dialog), _("Create directory"));
      gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);
      response = gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      g_free (parse_name);

      if (response == GTK_RESPONSE_YES
          && g_file_make_directory_with_parents (user_dir, NULL, &error))
        {
          /* try again */
          user_file = thunar_file_get (user_dir, &error);
        }
    }

  if (G_LIKELY (user_file != NULL))
    {
      /* open the folder */
      thunar_window_set_current_directory (window, user_file);
      g_object_unref (G_OBJECT (user_file));
      result = TRUE;
    }
  else if (error != NULL)
    {
      parse_name = g_file_get_parse_name (user_dir);
      thunar_dialogs_show_error (GTK_WIDGET (window), error, _("Failed to open directory \"%s\""), parse_name);
      g_free (parse_name);
      g_error_free (error);
    }

  is_homedir:

  g_object_unref (user_dir);
  g_object_unref (home_dir);

  return result;
}



static gboolean
thunar_window_action_open_desktop (ThunarWindow *window)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  thunar_window_open_user_folder (window, G_USER_DIRECTORY_DESKTOP, "Desktop");

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_open_computer (ThunarWindow *window)
{
  GFile         *computer;
  ThunarFile    *computer_file;
  GError        *error = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* determine the computer location */
  computer = thunar_g_file_new_for_computer();

  /* determine the file for this location */
  computer_file = thunar_file_get (computer, &error);
  if (G_UNLIKELY (computer_file == NULL))
    {
      /* display an error to the user */
      thunar_dialogs_show_error (GTK_WIDGET (window), error, _("Failed to browse the computer"));
      g_error_free (error);
    }
  else
    {
      /* open the computer location */
      thunar_window_set_current_directory (window, computer_file);
      g_object_unref (G_OBJECT (computer_file));
    }

  /* release our reference on the location itself */
  g_object_unref (computer);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_open_templates (ThunarWindow *window)
{
  GtkWidget     *dialog;
  GtkWidget     *button;
  GtkWidget     *label;
  GtkWidget     *image;
  GtkWidget     *hbox;
  GtkWidget     *vbox;
  gboolean       show_about_templates;
  gboolean       success;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  success = thunar_window_open_user_folder (window, G_USER_DIRECTORY_TEMPLATES, "Templates");

  /* check whether we should display the "About Templates" dialog */
  g_object_get (G_OBJECT (window->preferences),
                "misc-show-about-templates", &show_about_templates,
                NULL);

  if (G_UNLIKELY(show_about_templates && success))
    {
      /* display the "About Templates" dialog */
      dialog = gtk_dialog_new_with_buttons (_("About Templates"), GTK_WINDOW (window),
                                            GTK_DIALOG_DESTROY_WITH_PARENT,
                                            _("_OK"), GTK_RESPONSE_OK,
                                            NULL);

      gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);
      gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), hbox, TRUE, TRUE, 0);
      gtk_widget_show (hbox);

      image = gtk_image_new_from_icon_name ("dialog-information", GTK_ICON_SIZE_DIALOG);
      gtk_widget_set_halign (image, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (image, GTK_ALIGN_START);
      gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
      gtk_widget_show (image);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 18);
      gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
      gtk_widget_show (vbox);

      label = gtk_label_new (_("All files in this folder will appear in the \"Create Document\" menu."));
      gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
      gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_big_bold ());
      gtk_label_set_line_wrap (GTK_LABEL (label), FALSE);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
      gtk_widget_show (label);

      label = gtk_label_new (_("If you frequently create certain kinds "
                             " of documents, make a copy of one and put it in this "
                             "folder. Thunar will add an entry for this document in the"
                             " \"Create Document\" menu.\n\n"
                             "You can then select the entry from the \"Create Document\" "
                             "menu and a copy of the document will be created in the "
                             "directory you are viewing."));
      gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
      gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
      gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
      gtk_widget_show (label);

      button = gtk_check_button_new_with_mnemonic (_("Do _not display this message again"));
      g_object_bind_property (G_OBJECT (window->preferences), "misc-show-about-templates", G_OBJECT (button), "active", G_BINDING_INVERT_BOOLEAN | G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
      gtk_widget_show (button);

      gtk_window_set_default_size (GTK_WINDOW (dialog), 600, -1);

      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
    }

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_open_file_system (ThunarWindow *window)
{
  GFile         *root;
  ThunarFile    *root_file;
  GError        *error = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* determine the path to the root directory */
  root = thunar_g_file_new_for_root ();

  /* determine the file for the root directory */
  root_file = thunar_file_get (root, &error);
  if (G_UNLIKELY (root_file == NULL))
    {
      /* display an error to the user */
      thunar_dialogs_show_error (GTK_WIDGET (window), error, _("Failed to open the file system root folder"));
      g_error_free (error);
    }
  else
    {
      /* open the root folder */
      thunar_window_set_current_directory (window, root_file);
      g_object_unref (G_OBJECT (root_file));
    }

  /* release our reference on the home path */
  g_object_unref (root);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_open_recent (ThunarWindow *window)
{
  GFile      *recent;
  ThunarFile *recent_file;
  GError     *error = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* determine the path to `Recent` */
  recent = thunar_g_file_new_for_recent ();

  /* determine the file for `Recent` */
  recent_file = thunar_file_get (recent, &error);
  if (G_UNLIKELY (recent_file == NULL))
    {
      /* display an error to the user */
      thunar_dialogs_show_error (GTK_WIDGET (window), error, _("Failed to display `Recent`"));
      g_error_free (error);
    }
  else
    {
      /* open the `Recent` folder */
      thunar_window_set_current_directory (window, recent_file);
      g_object_unref (G_OBJECT (recent_file));
    }

  /* release our reference on the `Recent` path */
  g_object_unref (recent);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_open_trash (ThunarWindow *window)
{
  GFile      *trash_bin;
  ThunarFile *trash_bin_file;
  GError     *error = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* determine the path to the trash bin */
  trash_bin = thunar_g_file_new_for_trash ();

  /* determine the file for the trash bin */
  trash_bin_file = thunar_file_get (trash_bin, &error);
  if (G_UNLIKELY (trash_bin_file == NULL))
    {
      /* display an error to the user */
      thunar_dialogs_show_error (GTK_WIDGET (window), error, _("Failed to display the contents of the trash can"));
      g_error_free (error);
    }
  else
    {
      /* open the trash folder */
      thunar_window_set_current_directory (window, trash_bin_file);
      g_object_unref (G_OBJECT (trash_bin_file));
    }

  /* release our reference on the trash bin path */
  g_object_unref (trash_bin);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_open_network (ThunarWindow *window)
{
  ThunarFile *network_file;
  GError     *error = NULL;
  GFile      *network;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* determine the network root location */
  network = thunar_g_file_new_for_network();

  /* determine the file for this location */
  network_file = thunar_file_get (network, &error);
  if (G_UNLIKELY (network_file == NULL))
    {
      /* display an error to the user */
      thunar_dialogs_show_error (GTK_WIDGET (window), error, _("Failed to browse the network"));
      g_error_free (error);
    }
  else
    {
      /* open the network root location */
      thunar_window_set_current_directory (window, network_file);
      g_object_unref (G_OBJECT (network_file));
    }

  /* release our reference on the location itself */
  g_object_unref (network);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_check_uca_key_activation (ThunarWindow *window,
                                        GdkEventKey  *key_event,
                                        gpointer      user_data)
{
  if (thunar_action_manager_check_uca_key_activation (window->action_mgr, key_event))
    return GDK_EVENT_STOP;
  return GDK_EVENT_PROPAGATE;
}



static gboolean
thunar_window_propagate_key_event (GtkWindow* window,
                                   GdkEvent  *key_event,
                                   gpointer   user_data)
{
  GtkWidget    *focused_widget;
  ThunarWindow *thunar_window = THUNAR_WINDOW (window);

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), GDK_EVENT_PROPAGATE);

  focused_widget = gtk_window_get_focus (window);

  /* Turn the accelerator priority around globally,
   * so that the focused widget always gets the accels first.
   * Implementing this cleanly while maintaining some wanted accels
   * (like Ctrl+N and exo accels) is a lot of work. So we resort to
   * only priorize GtkEditable, because that is the easiest way to
   * fix the right-ahead problem. */
  if (focused_widget != NULL && GTK_IS_EDITABLE (focused_widget))
    return gtk_window_propagate_key_event (window, (GdkEventKey *) key_event);

  /* GTK ignores the Tab accelerator by default, handle it manually */
  if (xfce_gtk_handle_tab_accels ((GdkEventKey *) key_event, thunar_window->accel_group, thunar_window, thunar_window_action_entries, THUNAR_WINDOW_N_ACTIONS) == TRUE)
    return TRUE;

  /* ThunarActionManager doesn't handle it own shortcuts, so ThunarWindow will handle any Tab-accelerated actions */
  if (xfce_gtk_handle_tab_accels ((GdkEventKey *) key_event, thunar_window->accel_group, thunar_window->action_mgr, thunar_action_manager_get_action_entries (), THUNAR_ACTION_MANAGER_N_ACTIONS) == TRUE)
    return TRUE;

  /* ThunarStatusbar doesn't handle it own shortcuts, so ThunarWindow will handle any Tab-accelerated actions */
  if (xfce_gtk_handle_tab_accels ((GdkEventKey *) key_event, thunar_window->accel_group, thunar_window->statusbar, thunar_statusbar_get_action_entries (), THUNAR_STATUS_BAR_N_ACTIONS) == TRUE)
    return TRUE;

  return GDK_EVENT_PROPAGATE;
}



static gboolean
thunar_window_after_propagate_key_event (GtkWindow* window,
                                         GdkEvent  *key_event,
                                         gpointer   user_data)
{
  GtkWidget    *focused_widget;
  ThunarWindow *thunar_window = THUNAR_WINDOW (window);

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), GDK_EVENT_PROPAGATE);

  focused_widget = gtk_window_get_focus (window);

  /* After 'tab' was preassed, we might need to update the selected notebook */
  if (thunar_window_split_view_is_active (thunar_window))
    {
      if (thunar_window->notebook_left != NULL && gtk_widget_is_ancestor (focused_widget, thunar_window->notebook_left))
        {
          thunar_window->notebook_selected = thunar_window->notebook_left;
          thunar_window_notebook_select_current_page (thunar_window);
        }
      if(thunar_window->notebook_right != NULL && gtk_widget_is_ancestor (focused_widget, thunar_window->notebook_right))
        {
          thunar_window->notebook_selected = thunar_window->notebook_right;
          thunar_window_notebook_select_current_page (thunar_window);
        }
    }

  return GDK_EVENT_PROPAGATE;
}



static void
thunar_window_action_open_bookmark (GFile *g_file)
{
  ThunarWindow *window;

  window = g_object_get_data (G_OBJECT (g_file), I_("thunar-window"));

  g_object_set (G_OBJECT (window->action_mgr), "selected-location", g_file, NULL);
  thunar_action_manager_activate_selected_files (window->action_mgr, THUNAR_ACTION_MANAGER_CHANGE_DIRECTORY, NULL);
}



static gboolean
thunar_window_action_open_location (ThunarWindow *window)
{
  /* just use the "start-open-location" callback */
  thunar_window_start_open_location (window, NULL);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_contents (ThunarWindow *window)
{
  /* display the documentation index */
  xfce_dialog_show_help (GTK_WINDOW (window), "thunar", NULL, NULL);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_about (ThunarWindow *window)
{
  /* just popup the about dialog */
  thunar_dialogs_show_about (GTK_WINDOW (window), PACKAGE_NAME,
                             _("Thunar is a fast and easy to use file manager\n"
                               "for the Xfce Desktop Environment."));

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_show_hidden (ThunarWindow *window)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  window->show_hidden = !window->show_hidden;
  gtk_container_foreach (GTK_CONTAINER (window->notebook_selected), (GtkCallback) (void (*)(void)) thunar_view_set_show_hidden, GINT_TO_POINTER (window->show_hidden));

  if (G_LIKELY (window->sidepane != NULL))
    thunar_side_pane_set_show_hidden (THUNAR_SIDE_PANE (window->sidepane), window->show_hidden);

  g_object_set (G_OBJECT (window->preferences), "last-show-hidden", window->show_hidden, NULL);

  /* restart any active search */
  if (G_UNLIKELY (window->is_searching))
    thunar_window_update_search (window);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_show_highlight (ThunarWindow *window)
{
  gboolean highlight_enabled;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  g_object_get (G_OBJECT (window->preferences), "misc-highlighting-enabled", &highlight_enabled, NULL);
  g_object_set (G_OBJECT (window->preferences), "misc-highlighting-enabled", !highlight_enabled, NULL);

  /* refresh the view to refresh the cell renderer drawings */
  thunar_window_action_reload (window, NULL);

  return TRUE;
}



gboolean
thunar_window_action_search (ThunarWindow *window)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  if (window->is_searching == FALSE)
    {
      /* initiate search */
      thunar_window_start_open_location (window, SEARCH_PREFIX);
      g_signal_handlers_block_by_func (G_OBJECT (window->location_toolbar_item_search), thunar_window_action_search, window);
      gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (window->location_toolbar_item_search), TRUE);
      g_signal_handlers_unblock_by_func (G_OBJECT (window->location_toolbar_item_search), thunar_window_action_search, window);
    }
  else
    thunar_window_action_cancel_search (window);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_window_action_open_file_menu (ThunarWindow *window)
{
  gboolean  ret;
  GList    *children;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* In case the menubar is hidden, we make it visible (e.g. when F10 is pressed) */
  gtk_widget_set_visible (window->menubar, TRUE);

  children = gtk_container_get_children (GTK_CONTAINER (window->menubar));
  g_signal_emit_by_name (children->data, "button-press-event", NULL, &ret);
  g_list_free (children);
  gtk_menu_shell_select_first (GTK_MENU_SHELL (window->menubar), TRUE);

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static void
thunar_window_current_directory_changed (ThunarFile   *current_directory,
                                         ThunarWindow *window)
{
  gboolean      show_full_path;
  gchar        *title;
  gchar        *parse_name = NULL;
  const gchar  *name;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (THUNAR_IS_FILE (current_directory));
  _thunar_return_if_fail (window->current_directory == current_directory);

  /* get name of directory or full path */
  g_object_get (G_OBJECT (window->preferences), "misc-full-path-in-window-title", &show_full_path, NULL);
  if (G_UNLIKELY (show_full_path))
    name = parse_name = g_file_get_parse_name (thunar_file_get_file (current_directory));
  else
    name = thunar_file_get_display_name (current_directory);

  /* set window title */
  title = g_strdup_printf ("%s - %s", name, "Thunar");
  gtk_window_set_title (GTK_WINDOW (window), title);
  g_free (title);
  g_free (parse_name);

  /* set window icon */
  thunar_window_update_window_icon (window);
}



static void
thunar_window_menu_item_selected (ThunarWindow *window,
                                  GtkWidget    *menu_item)
{
  gchar *tooltip;
  guint  id;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* we can only display tooltips if we have a statusbar */
  if (G_LIKELY (window->statusbar != NULL))
    {
      tooltip = gtk_widget_get_tooltip_text (menu_item);
      if (G_LIKELY (tooltip != NULL))
        {
          /* push to the statusbar */
          id = gtk_statusbar_get_context_id (GTK_STATUSBAR (window->statusbar), "Menu tooltip");
          gtk_statusbar_push (GTK_STATUSBAR (window->statusbar), id, tooltip);
          g_free (tooltip);
        }
    }
}



static void
thunar_window_menu_item_deselected (ThunarWindow *window,
                                    GtkWidget    *menu_item)
{
  guint id;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* we can only undisplay tooltips if we have a statusbar */
  if (G_LIKELY (window->statusbar != NULL))
    {
      /* drop the last tooltip from the statusbar */
      id = gtk_statusbar_get_context_id (GTK_STATUSBAR (window->statusbar), "Menu tooltip");
      gtk_statusbar_pop (GTK_STATUSBAR (window->statusbar), id);
    }
}



static void
thunar_window_notify_loading (ThunarView   *view,
                              GParamSpec   *pspec,
                              ThunarWindow *window)
{
  GdkCursor *cursor;

  _thunar_return_if_fail (THUNAR_IS_VIEW (view));
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  if (gtk_widget_get_realized (GTK_WIDGET (window))
      && window->view == GTK_WIDGET (view))
    {
      /* setup the proper cursor */
      if (thunar_view_get_loading (view))
        {
          cursor = gdk_cursor_new_for_display (gtk_widget_get_display (GTK_WIDGET (view)), GDK_WATCH);
          gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (window)), cursor);
          g_object_unref (cursor);
        }
      else
        {
          gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (window)), NULL);
        }

      /* Set trash infobar's `empty trash` button sensitivity, if required */
      if (thunar_file_is_trash (window->current_directory))
        gtk_widget_set_sensitive (window->trash_infobar_empty_button, thunar_file_get_item_count (window->current_directory) > 0);
    }
}



static void
thunar_window_device_pre_unmount (ThunarDeviceMonitor *device_monitor,
                                  ThunarDevice        *device,
                                  GFile               *root_file,
                                  ThunarWindow        *window)
{
  _thunar_return_if_fail (THUNAR_IS_DEVICE_MONITOR (device_monitor));
  _thunar_return_if_fail (window->device_monitor == device_monitor);
  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));
  _thunar_return_if_fail (G_IS_FILE (root_file));
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* nothing to do if we don't have a current directory */
  if (G_UNLIKELY (window->current_directory == NULL))
    return;

  /* check if the file is the current directory or an ancestor of the current directory */
  if (g_file_equal (thunar_file_get_file (window->current_directory), root_file)
      || thunar_file_is_gfile_ancestor (window->current_directory, root_file))
    {
      /* change to the home folder */
      thunar_window_action_open_home (window);
    }
}



static void
thunar_window_device_changed (ThunarDeviceMonitor *device_monitor,
                              ThunarDevice        *device,
                              ThunarWindow        *window)
{
  GFile *root_file;

  _thunar_return_if_fail (THUNAR_IS_DEVICE_MONITOR (device_monitor));
  _thunar_return_if_fail (window->device_monitor == device_monitor);
  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  if (thunar_device_is_mounted (device))
    return;

  root_file = thunar_device_get_root (device);
  if (root_file != NULL)
    {
      thunar_window_device_pre_unmount (device_monitor, device, root_file, window);
      g_object_unref (root_file);
    }
}



static gboolean
thunar_window_save_paned (ThunarWindow *window)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  g_object_set (G_OBJECT (window->preferences), "last-separator-position",
                gtk_paned_get_position (GTK_PANED (window->paned)), NULL);

  /* for button release event */
  return FALSE;
}



static gboolean
thunar_window_save_paned_notebooks (ThunarWindow *window)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  g_object_set (G_OBJECT (window->preferences), "last-splitview-separator-position",
                gtk_paned_get_position (GTK_PANED (window->paned_notebooks)), NULL);

  /* for button release event */
  return FALSE;
}



static gboolean
thunar_window_save_geometry_timer (gpointer user_data)
{
  GdkWindowState state;
  ThunarWindow  *window = THUNAR_WINDOW (user_data);
  gboolean       remember_geometry;
  gint           width;
  gint           height;

THUNAR_THREADS_ENTER

  /* check if we should remember the window geometry */
  g_object_get (G_OBJECT (window->preferences), "misc-remember-geometry", &remember_geometry, NULL);
  if (G_LIKELY (remember_geometry))
    {
      /* check if the window is still visible */
      if (gtk_widget_get_visible (GTK_WIDGET (window)))
        {
          /* determine the current state of the window */
          state = gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (window)));

          /* don't save geometry for maximized or fullscreen windows */
          if ((state & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN)) == 0)
            {
              /* determine the current width/height of the window... */
              gtk_window_get_size (GTK_WINDOW (window), &width, &height);

              /* ...and remember them as default for new windows */
              g_object_set (G_OBJECT (window->preferences), "last-window-width", width, "last-window-height", height,
                            "last-window-maximized", FALSE, NULL);
            }
          else
            {
              /* only store that the window is full screen */
              g_object_set (G_OBJECT (window->preferences), "last-window-maximized", TRUE, NULL);
            }
        }
    }

THUNAR_THREADS_LEAVE

  return FALSE;
}



static void
thunar_window_save_geometry_timer_destroy (gpointer user_data)
{
  THUNAR_WINDOW (user_data)->save_geometry_timer_id = 0;
}



/**
 * thunar_window_set_zoom_level:
 * @window     : a #ThunarWindow instance.
 * @zoom_level : the new zoom level for @window.
 *
 * Sets the zoom level for @window to @zoom_level.
 **/
void
thunar_window_set_zoom_level (ThunarWindow   *window,
                              ThunarZoomLevel zoom_level)
{
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (zoom_level < THUNAR_ZOOM_N_LEVELS);

  /* check if we have a new zoom level */
  if (G_LIKELY (window->zoom_level != zoom_level))
    {
      /* remember the new zoom level */
      window->zoom_level = zoom_level;

      /* notify listeners */
      g_object_notify (G_OBJECT (window), "zoom-level");
    }

  /* check if we can still zoom in and set the sensitivity of the button */
  gtk_widget_set_sensitive (window->location_toolbar_item_zoom_in, (window->zoom_level < THUNAR_ZOOM_N_LEVELS - 1));
  /* check if we can still zoom out and set the sensitivity of the button */
  gtk_widget_set_sensitive (window->location_toolbar_item_zoom_out, (window->zoom_level > 0));
}



/**
 * thunar_window_set_directory_specific_settings:
 * @window                      : a #ThunarWindow instance.
 * @directory_specific_settings : whether to use directory specific settings in @window.
 *
 * Toggles the use of directory specific settings in @window according to @directory_specific_settings.
 **/
void
thunar_window_set_directory_specific_settings (ThunarWindow *window,
                                               gboolean      directory_specific_settings)
{
  GList      *tabs, *lp;
  ThunarFile *directory;
  GType       view_type;
  gchar      *type_name;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* reset to the default view type if we are turning directory specific settings off */
  if (!directory_specific_settings && window->directory_specific_settings)
    {
      /* determine the default view type */
      g_object_get (G_OBJECT (window->preferences), "default-view", &type_name, NULL);
      view_type = g_type_from_name (type_name);
      g_free (type_name);

      /* set the last view type */
      if (!g_type_is_a (view_type, G_TYPE_NONE) && !g_type_is_a (view_type, G_TYPE_INVALID))
        g_object_set (G_OBJECT (window->preferences), "last-view", g_type_name (view_type), NULL);
    }

  /* save the setting */
  window->directory_specific_settings = directory_specific_settings;

  /* get all of the window's tabs */
  tabs = gtk_container_get_children (GTK_CONTAINER (window->notebook_selected));

  /* replace each tab with a tab of the correct view type */
  for (lp = tabs; lp != NULL; lp = lp->next)
    {
      if (!THUNAR_IS_STANDARD_VIEW (lp->data))
        continue;

      directory = thunar_navigator_get_current_directory (lp->data);

      if (!THUNAR_IS_FILE (directory))
        continue;

      /* find the correct view type for the new view */
      view_type = thunar_window_view_type_for_directory (window, directory);

      /* replace the old view with a new one */
      thunar_window_replace_view (window, lp->data, view_type);
    }

  g_list_free (tabs);
}



/**
 * thunar_window_get_current_directory:
 * @window : a #ThunarWindow instance.
 *
 * Queries the #ThunarFile instance, which represents the directory
 * currently displayed within @window. %NULL is returned if @window
 * is not currently associated with any directory.
 *
 * Return value: the directory currently displayed within @window or %NULL.
 **/
ThunarFile*
thunar_window_get_current_directory (ThunarWindow *window)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), NULL);
  return window->current_directory;
}



/**
 * thunar_window_set_current_directory:
 * @window            : a #ThunarWindow instance.
 * @current_directory : the new directory or %NULL.
 **/
void
thunar_window_set_current_directory (ThunarWindow *window,
                                     ThunarFile   *current_directory)
{
  gboolean is_trash;
  gboolean is_recent;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (current_directory == NULL || THUNAR_IS_FILE (current_directory));

  /* check if we already display the requested directory */
  if (G_UNLIKELY (window->current_directory == current_directory))
    return;

  /* disconnect from the previously active directory */
  if (G_LIKELY (window->current_directory != NULL))
    {
      /* disconnect signals and release reference */
      g_signal_handlers_disconnect_by_func (G_OBJECT (window->current_directory), thunar_window_current_directory_changed, window);
      g_object_unref (G_OBJECT (window->current_directory));
    }

  /* connect to the new directory */
  if (G_LIKELY (current_directory != NULL))
    {
      GType  type;
      gchar *type_name;
      gint   num_pages;

      /* take a reference on the file */
      g_object_ref (G_OBJECT (current_directory));

      num_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook_selected));

      /* if the window is new, get the default-view type and set it as the last-view type (if it is a valid type)
       * so that it will be used as the initial view type for directories with no saved directory specific view type */
      if (num_pages == 0)
        {
          /* determine the default view type */
          g_object_get (G_OBJECT (window->preferences), "default-view", &type_name, NULL);
          type = g_type_from_name (type_name);
          g_free (type_name);

          /* set the last view type to the default view type if there is a default view type */
          if (!g_type_is_a (type, G_TYPE_NONE) && !g_type_is_a (type, G_TYPE_INVALID))
            g_object_set (G_OBJECT (window->preferences), "last-view", g_type_name (type), NULL);
        }

      type = thunar_window_view_type_for_directory (window, current_directory);

      if (num_pages == 0) /* create a new view if the window is new */
        {
          window->current_directory = current_directory;
          thunar_window_replace_view (window, window->view, type);
        }
      else /* change the view type if necessary, and set the current directory */
        {
          if (window->view != NULL && window->view_type != type)
            thunar_window_replace_view (window, window->view, type);

          window->current_directory = current_directory;
        }

      /* connect the "changed"/"destroy" signals */
      g_signal_connect (G_OBJECT (current_directory), "changed", G_CALLBACK (thunar_window_current_directory_changed), window);

      /* update window icon and title */
      thunar_window_current_directory_changed (current_directory, window);

      if (G_LIKELY (window->view != NULL))
        {
          /* grab the focus to the main view */
          gtk_widget_grab_focus (window->view);
        }

      thunar_window_history_changed (window);
      gtk_widget_set_sensitive (window->location_toolbar_item_parent, !thunar_g_file_is_root (thunar_file_get_file (current_directory)));
    }

  /* tell everybody that we have a new "current-directory",
   * we do this first so other widgets display the new
   * state already while the folder view is loading.
   */
  g_object_notify (G_OBJECT (window), "current-directory");

  /* show/hide date_deleted column/sortBy in the trash directory */
  if (current_directory == NULL)
    return;

  is_trash = thunar_file_is_trash (current_directory);
  is_recent = thunar_file_is_recent (current_directory);
  if (is_trash)
    gtk_widget_show (window->trash_infobar);
  else
    gtk_widget_hide (window->trash_infobar);

  if (THUNAR_IS_DETAILS_VIEW (window->view) == FALSE)
    return;
  thunar_details_view_set_date_deleted_column_visible (THUNAR_DETAILS_VIEW (window->view), is_trash);
  thunar_details_view_set_recency_column_visible (THUNAR_DETAILS_VIEW (window->view), is_recent);
  thunar_details_view_set_location_column_visible (THUNAR_DETAILS_VIEW (window->view), is_recent);
}



GList*
thunar_window_get_directories (ThunarWindow *window,
                               gint         *active_page)
{
  gint         n;
  gint         n_pages;
  GList       *uris = NULL;
  GtkWidget   *view;
  ThunarFile  *directory;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), NULL);

  n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook_selected));
  if (G_UNLIKELY (n_pages == 0))
    return NULL;

  for (n = 0; n < n_pages; n++)
    {
      /* get the view */
      view = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook_selected), n);
      if (THUNAR_IS_NAVIGATOR (view) == FALSE)
        continue;

      /* get the directory of the view */
      directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (view));
      if (THUNAR_IS_FILE (directory) == FALSE)
        continue;

      uris = g_list_append (uris, thunar_file_dup_uri (directory));
    }

  /* selected tab */
  if (active_page != NULL)
    *active_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook_selected));

  return uris;
}



gboolean
thunar_window_set_directories (ThunarWindow   *window,
                               gchar         **uris,
                               gint            active_page)
{
  ThunarFile *directory;
  guint       n;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);
  _thunar_return_val_if_fail (uris != NULL, FALSE);

  for (n = 0; uris[n] != NULL; n++)
    {
      /* check if the string looks like an uri */
      if (!g_uri_is_valid (uris[n], G_URI_FLAGS_NONE, NULL))
        continue;

      /* get the file for the uri */
      directory = thunar_file_get_for_uri (uris[n], NULL);
      if (G_UNLIKELY (directory == NULL))
        continue;

      /* open the directory in a new notebook */
      if (thunar_file_is_directory (directory))
        {
          if (gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook_selected)) == 0)
            thunar_window_set_current_directory (window, directory);
          else
            thunar_window_notebook_open_new_tab (window, directory);
        }

      g_object_unref (G_OBJECT (directory));
    }

  /* select the page */
  thunar_window_notebook_set_current_tab (window, active_page);

  /* we succeeded if new pages have been opened */
  return gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook_selected)) > 0;
}



/**
 * thunar_window_get_action_entry:
 * @window  : Instance of a  #ThunarWindow
 * @action  : #ThunarWindowAction for which the #XfceGtkActionEntry is requested
 *
 * returns a reference to the requested #XfceGtkActionEntry
 *
 * Return value: (transfer none): The reference to the #XfceGtkActionEntry
 **/
const XfceGtkActionEntry*
thunar_window_get_action_entry (ThunarWindow       *window,
                                ThunarWindowAction  action)
{
  return get_action_entry (action);
}



/**
 * thunar_window_append_menu_item:
 * @window  : Instance of a  #ThunarWindow
 * @menu    : #GtkMenuShell to which the item should be added
 * @action  : #ThunarWindowAction to select which item should be added
 *
 * Adds the selected, widget specific #GtkMenuItem to the passed #GtkMenuShell
 **/
void
thunar_window_append_menu_item (ThunarWindow       *window,
                                GtkMenuShell       *menu,
                                ThunarWindowAction  action)
{
  GtkWidget *item;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (action), G_OBJECT (window), menu);

  if (action == THUNAR_WINDOW_ACTION_ZOOM_IN)
    gtk_widget_set_sensitive (item, G_LIKELY (window->zoom_level < THUNAR_ZOOM_N_LEVELS - 1));
  if (action == THUNAR_WINDOW_ACTION_ZOOM_OUT)
    gtk_widget_set_sensitive (item, G_LIKELY (window->zoom_level > 0));
}



/**
 * thunar_window_get_action_manager:
 * @window : a #ThunarWindow instance.
 *
 * Return value: (transfer none): The single #ThunarActionManager of this #ThunarWindow
 **/
ThunarActionManager*
thunar_window_get_action_manager (ThunarWindow *window)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), NULL);

  return window->action_mgr;
}



static void
thunar_window_redirect_menu_tooltips_to_statusbar_recursive (GtkWidget    *menu_item,
                                                             ThunarWindow *window)
{
  GtkWidget  *submenu;

  if (GTK_IS_MENU_ITEM (menu_item))
    {
      submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (menu_item));
      if (submenu != NULL)
          gtk_container_foreach (GTK_CONTAINER (submenu), (GtkCallback) (void (*)(void)) thunar_window_redirect_menu_tooltips_to_statusbar_recursive, window);

      /* this disables to show the tooltip on hover */
      gtk_widget_set_has_tooltip (menu_item, FALSE);

      /* These method will put the tooltip on the statusbar */
      g_signal_connect_swapped (G_OBJECT (menu_item), "select", G_CALLBACK (thunar_window_menu_item_selected), window);
      g_signal_connect_swapped (G_OBJECT (menu_item), "deselect", G_CALLBACK (thunar_window_menu_item_deselected), window);
    }
}



/**
 * thunar_window_redirect_menu_tooltips_to_statusbar:
 * @window : a #ThunarWindow instance.
 * @menu   : #GtkMenu for which all tooltips should be shown in the statusbar
 *
 * All tooltips of the provided #GtkMenu and any submenu will not be shown directly any more.
 * Instead they will be shown in the status bar of the passed #ThunarWindow
 **/
void
thunar_window_redirect_menu_tooltips_to_statusbar (ThunarWindow *window, GtkMenu *menu)
{
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (GTK_IS_MENU (menu));

  gtk_container_foreach (GTK_CONTAINER (menu), (GtkCallback) (void (*)(void)) thunar_window_redirect_menu_tooltips_to_statusbar_recursive, window);
}



static gboolean
thunar_window_button_press_event (GtkWidget      *view,
                                  GdkEventButton *event,
                                  ThunarWindow   *window)
{
  const XfceGtkActionEntry* action_entry;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  if (event->type == GDK_BUTTON_PRESS)
    {
      if (G_UNLIKELY (event->button == 8))
        {
          action_entry = get_action_entry (THUNAR_WINDOW_ACTION_BACK);
          ((void(*)(GtkWindow*))action_entry->callback)(GTK_WINDOW (window));
          return GDK_EVENT_STOP;
        }
      if (G_UNLIKELY (event->button == 9))
        {
          action_entry = get_action_entry (THUNAR_WINDOW_ACTION_FORWARD);
          ((void(*)(GtkWindow*))action_entry->callback)(GTK_WINDOW (window));
          return GDK_EVENT_STOP;
        }
    }

  return GDK_EVENT_PROPAGATE;
}



static gboolean
thunar_window_history_clicked (GtkWidget      *button,
                               GdkEventButton *event,
                               ThunarWindow   *window)
{
  ThunarHistory *history    = NULL;
  ThunarFile    *directory  = NULL;
  gboolean       open_in_tab;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  history = thunar_standard_view_get_history (THUNAR_STANDARD_VIEW (window->view));
  if (event->button == 3)
    {
      if (button == window->location_toolbar_item_back)
        thunar_history_show_menu (history, THUNAR_HISTORY_MENU_BACK, button);
      else if (button == window->location_toolbar_item_forward)
        thunar_history_show_menu (history, THUNAR_HISTORY_MENU_FORWARD, button);
      else
        g_warning ("This button is not able to spawn a history menu");
    }
  else if (event->button == 2)
    {
      /* middle click to open a new tab/window */
      g_object_get (window->preferences, "misc-middle-click-in-tab", &open_in_tab, NULL);
      if (button == window->location_toolbar_item_back)
          directory = thunar_history_peek_back (history);
      else if (button == window->location_toolbar_item_forward)
          directory = thunar_history_peek_forward (history);

      if (directory == NULL)
        return FALSE;

      if (open_in_tab)
        thunar_window_notebook_open_new_tab (window, directory);
      else
        {
          ThunarApplication *application = thunar_application_get ();
          thunar_application_open_window (application, directory, NULL, NULL, TRUE);
          g_object_unref (G_OBJECT (application));
        }
      g_object_unref (directory);
    }

  return FALSE;
}



static gboolean
thunar_window_toolbar_button_clicked (GtkWidget      *button,
                                      GdkEventButton *event,
                                      ThunarWindow   *window)
{
  GtkWidget *menu;

  if (event->button == 3)
    {
      menu = gtk_menu_new ();
      xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_CONFIGURE_TOOLBAR), G_OBJECT (window), GTK_MENU_SHELL (menu));
      gtk_widget_show_all (menu);

      /* run the menu (takes over the floating of menu) */
      thunar_gtk_menu_run (GTK_MENU (menu));
    }

  return TRUE;
}



static gboolean
thunar_window_open_parent_clicked (GtkWidget      *button,
                                   GdkEventButton *event,
                                   ThunarWindow   *window)
{
  ThunarFile    *directory  = NULL;
  gboolean       open_in_tab;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  g_object_get (window->preferences, "misc-middle-click-in-tab", &open_in_tab, NULL);

  if (event->button == 2)
    {
      /* middle click to open a new tab/window */
      directory = thunar_file_get_parent (window->current_directory, NULL);
      if (G_LIKELY (directory == NULL))
        return FALSE;

      if (open_in_tab)
        thunar_window_notebook_open_new_tab (window, directory);
      else
        {
          ThunarApplication *application = thunar_application_get ();
          thunar_application_open_window (application, directory, NULL, NULL, TRUE);
          g_object_unref (G_OBJECT (application));
        }
      g_object_unref (directory);
    }

  return FALSE;
}



static gboolean
thunar_window_open_home_clicked   (GtkWidget      *button,
                                   GdkEventButton *event,
                                   ThunarWindow   *window)
{
  ThunarFile    *directory  = NULL;
  gint           page_num;
  gboolean       open_in_tab;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  g_object_get (window->preferences, "misc-middle-click-in-tab", &open_in_tab, NULL);

  if (event->button == 2)
    {
      /* switch to the new tab, go to the home directory, return to the old tab */
      if (open_in_tab)
        {
          page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook_selected));
          thunar_window_notebook_add_new_tab (window, window->current_directory, THUNAR_NEW_TAB_BEHAVIOR_SWITCH);
          thunar_window_action_open_home (window);
          thunar_window_notebook_set_current_tab (window, page_num);
        }
      else
        {
          ThunarApplication *application = thunar_application_get ();
          ThunarWindow *newWindow = THUNAR_WINDOW (thunar_application_open_window (application, directory, NULL, NULL, TRUE));
          thunar_window_action_open_home (newWindow);
          g_object_unref (G_OBJECT (application));
        }
   }

 return FALSE;
}



/**
 * thunar_window_view_type_for_directory:
 * @window      : a #ThunarWindow instance.
 * @directory   : #ThunarFile representing the directory
 *
 * Return value: the #GType representing the view type which
 * @window would use to display @directory.
 **/
GType
thunar_window_view_type_for_directory (ThunarWindow *window,
                                       ThunarFile   *directory)
{
  GType  type = G_TYPE_NONE;
  gchar *type_name;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), G_TYPE_NONE);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (directory), G_TYPE_NONE);

  /* if the  directory has a saved view type and directory specific view types are enabled, we use it */
  if (window->directory_specific_settings)
    {
      const gchar *dir_spec_type_name;

      dir_spec_type_name = thunar_file_get_metadata_setting (directory, "view-type");
      if (dir_spec_type_name != NULL)
        type = g_type_from_name (dir_spec_type_name);
    }

  /* if there is no saved view type for the directory or directory specific view types are not enabled,
   * we use the last view type */
  if (g_type_is_a (type, G_TYPE_NONE) || g_type_is_a (type, G_TYPE_INVALID))
    {
      /* determine the last view type */
      g_object_get (G_OBJECT (window->preferences), "last-view", &type_name, NULL);
      type = g_type_from_name (type_name);
      g_free (type_name);
    }

  /* fallback view type, in case nothing was set */
  if (g_type_is_a (type, G_TYPE_NONE) || g_type_is_a (type, G_TYPE_INVALID))
    type = THUNAR_TYPE_ICON_VIEW;

  return type;
}



static void
thunar_window_trash_infobar_clicked (GtkInfoBar   *info_bar,
                                     gint          response_id,
                                     ThunarWindow *window)
{
  switch (response_id)
    {
      case EMPTY:
        thunar_action_manager_action_empty_trash (window->action_mgr);
        break;
      case RESTORE:
        thunar_action_manager_action_restore (window->action_mgr);
        break;
      default:
        g_return_if_reached();
    }
}



static void thunar_window_update_embedded_image_preview (ThunarWindow *window)
{
  ThunarImagePreviewMode misc_image_preview_mode;
  gboolean               last_image_preview_visible;

  g_object_get (window->preferences,
                "last-image-preview-visible", &last_image_preview_visible,
                "misc-image-preview-mode", &misc_image_preview_mode,
                NULL);

  if (window->preview_image_pixbuf != NULL)
    {
      image_preview_update (window->sidepane_box, NULL, window->sidepane_preview_image);
      if (last_image_preview_visible == TRUE && misc_image_preview_mode == THUNAR_IMAGE_PREVIEW_MODE_EMBEDDED)
        gtk_widget_show (window->sidepane_preview_image);
      else
        gtk_widget_hide (window->sidepane_preview_image);
    }
  else
    gtk_widget_hide (window->sidepane_preview_image);
}



static void thunar_window_update_standalone_image_preview (ThunarWindow *window)
{
  GList *selected_files = thunar_view_get_selected_files (THUNAR_VIEW (window->view));

  if (window->preview_image_pixbuf != NULL)
    {
      gchar *file_size = thunar_file_get_size_string (selected_files->data);

      image_preview_update (window->right_pane_box, NULL, window->right_pane_preview_image);
      gtk_label_set_text (GTK_LABEL (window->right_pane_image_label), thunar_file_get_display_name (selected_files->data));
      gtk_label_set_text (GTK_LABEL (window->right_pane_size_label), file_size);

      g_free (file_size);
    }
  else
    {
      gtk_image_clear (GTK_IMAGE (window->right_pane_preview_image));
      gtk_label_set_text (GTK_LABEL (window->right_pane_image_label), "No available thumbnail...");
      gtk_label_set_text (GTK_LABEL (window->right_pane_size_label), "-");
    }
}



/**
 * thunar_window_selection_changed:
 * @window      : a #ThunarWindow instance.
 *
 * Used to set the `sensitive` value of the `Restore` button in the trash infobar and to
 * update the image previews.
 **/
static void
thunar_window_selection_changed (ThunarWindow *window)
{
  gboolean  last_image_preview_visible;
  GList    *selected_files = thunar_view_get_selected_files (THUNAR_VIEW (window->view));

  /* butttons specific to the Trash location */
  if (g_list_length (selected_files) > 0)
    gtk_widget_set_sensitive (window->trash_infobar_restore_button, TRUE);
  else
    gtk_widget_set_sensitive (window->trash_infobar_restore_button, FALSE);

  /* image preview sidepane */
  /* check if we have a pending thumbnail request (for the image preview) */
  if (window->thumbnail_request != 0)
    {
      /* cancel the request */
      thunar_thumbnailer_dequeue (window->thumbnailer, window->thumbnail_request);
      window->thumbnail_request = 0;
    }

  /* clear image previews */
  if (window->preview_image_pixbuf != NULL)
    {
      g_object_unref (window->preview_image_pixbuf);
      window->preview_image_pixbuf = NULL;
    }

  /* only request new preview thumbnails if the user wants image previews */
  g_object_get (G_OBJECT (window->preferences),
                "last-image-preview-visible", &last_image_preview_visible,
                NULL);

  /* get or request a thumbnail */
  if ((last_image_preview_visible == TRUE) && (g_list_length (selected_files) == 1))
    {
      gchar *path = thunar_file_get_thumbnail_path_forced (selected_files->data, THUNAR_THUMBNAIL_SIZE_XX_LARGE);
      if (path == NULL) /* request the creation of the thumbnail if it doesn't exist */
        thunar_thumbnailer_queue_file (window->thumbnailer, selected_files->data, &window->thumbnail_request, THUNAR_THUMBNAIL_SIZE_XX_LARGE);
      else /* display the thumbnail */
        window->preview_image_pixbuf = gdk_pixbuf_new_from_file (path, NULL);
      g_free (path);
    }

  /* update previews */
  thunar_window_update_embedded_image_preview (window);
  thunar_window_update_standalone_image_preview (window);
}



static void
thunar_window_finished_thumbnailing (ThunarWindow       *window,
                                     guint               request,
                                     ThunarThumbnailer  *thumbnailer)
{
  GList *selected_files = NULL;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  if (window->thumbnail_request != request)
    return;

  window->thumbnail_request = 0;

  /* update image previews */
  selected_files = thunar_view_get_selected_files (THUNAR_VIEW (window->view));
  if (g_list_length (selected_files) == 1)
    {
      /* there is no guarantee that the thumbnail will exist, the type of the selected file might be unsupported by the thumbnailer */
      gchar *path = thunar_file_get_thumbnail_path_forced (selected_files->data, THUNAR_THUMBNAIL_SIZE_XX_LARGE);
      if (path == NULL)
        return;

      window->preview_image_pixbuf = gdk_pixbuf_new_from_file (path, NULL);
      thunar_window_update_embedded_image_preview (window);
      thunar_window_update_standalone_image_preview (window);

      g_free (path);
    }
}



static void
thunar_window_recent_reload (GtkRecentManager *recent_manager,
                             ThunarWindow     *window)
{
  _thunar_return_if_fail (window != NULL);
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  if (thunar_file_is_in_recent (window->current_directory))
    thunar_window_action_reload (window, NULL);
}



static void
thunar_window_catfish_dialog_configure (GtkWidget *entry)
{
  GError    *err = NULL;
  gchar     *argv[4];
  GdkScreen *screen;
  char      *display = NULL;

  /* prepare the argument vector */
  argv[0] = (gchar *) "catfish";
  argv[1] = THUNAR_WINDOW (entry)->search_query;
  argv[2] = (gchar *) "--start";
  argv[3] = NULL;

  screen = gtk_widget_get_screen (entry);

  if (screen != NULL)
    display = g_strdup (gdk_display_get_name (gdk_screen_get_display (screen)));

  /* invoke the configuration interface of Catfish */
  if (!g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, thunar_setup_display_cb, display, NULL, &err))
    {
      thunar_dialogs_show_error (entry, err, _("Failed to launch search with Catfish"));
      g_error_free (err);
    }

  g_free (display);
}



void
thunar_window_update_statusbar (ThunarWindow *window)
{
  thunar_standard_view_update_statusbar_text (THUNAR_STANDARD_VIEW (window->view));
}



XfceGtkActionEntry*
thunar_window_get_action_entries (void)
{
  return thunar_window_action_entries;
}



/**
 * thunar_window_reconnect_accelerators:
 * @window      : a #ThunarWindow instance.
 *
 * Used to recreate the accelerator group when the accelerator map changes. This way the open windows can use the
 * updated shortcuts.
 **/
void
thunar_window_reconnect_accelerators (ThunarWindow *window)
{
  /* the window has not been properly initialized yet */
  if (window->accel_group == NULL)
    return;

  /* delete previous accel_group */
  gtk_accel_group_disconnect (window->accel_group, NULL);
  gtk_window_remove_accel_group (GTK_WINDOW (window), window->accel_group);
  g_object_unref (window->accel_group);

  /* create new accel_group */
  window->accel_group = gtk_accel_group_new ();
  xfce_gtk_accel_map_add_entries (thunar_window_action_entries, G_N_ELEMENTS (thunar_window_action_entries));
  xfce_gtk_accel_group_connect_action_entries (window->accel_group,
                                               thunar_window_action_entries,
                                               G_N_ELEMENTS (thunar_window_action_entries),
                                               window);
  thunar_action_manager_append_accelerators (window->action_mgr, window->accel_group);
  thunar_statusbar_append_accelerators (THUNAR_STATUSBAR (window->statusbar), window->accel_group);
  thunar_window_update_bookmarks (window);

  gtk_window_add_accel_group (GTK_WINDOW (window), window->accel_group);

  /* update page accelarators */
  if (window->view != NULL)
    g_object_set (G_OBJECT (window->view), "accel-group", window->accel_group, NULL);
}



static void
uca_activation_callback_free (gpointer  data,
                              GClosure *closure)
{
  g_free (((UCAActivation*) data)->action_name);
  g_slice_free (UCAActivation, data);
}



static void
thunar_window_check_activate_toolbar_uca (UCAActivation *data)
{
  ThunarWindow           *window              = data->window;
  gchar                  *action_name         = data->action_name;
  ThunarxProviderFactory *provider_factory;
  GList                  *providers;
  GList                  *thunarx_menu_items  = NULL;
  GList                  *lp_provider;
  GList                  *lp_item;

  /* load the menu providers from the provider factory */
  provider_factory = thunarx_provider_factory_get_default ();
  providers = thunarx_provider_factory_list_providers (provider_factory, THUNARX_TYPE_MENU_PROVIDER);
  g_object_unref (provider_factory);

  if (G_UNLIKELY (providers == NULL))
    return;

  /* load the menu items offered by the menu providers */
  for (lp_provider = providers; lp_provider != NULL; lp_provider = lp_provider->next)
    {
      thunarx_menu_items = thunarx_menu_provider_get_folder_menu_items (lp_provider->data, GTK_WIDGET (window), THUNARX_FILE_INFO (window->current_directory));

      for (lp_item = thunarx_menu_items; lp_item != NULL; lp_item = lp_item->next)
        {
          gchar *name;

          g_object_get (G_OBJECT (lp_item->data), "name", &name, NULL);
          if (strncmp ("uca-action", name, 10) != 0)
            {
              g_free (name);
              break;
            }

          if (g_strcmp0 (action_name, name) == 0)
            thunarx_menu_item_activate (lp_item->data);
          g_free (name);
        }

      g_list_free (thunarx_menu_items);
    }
  g_list_free_full (providers, g_object_unref);
}



GtkWidget*
thunar_window_location_toolbar_add_uca (ThunarWindow *window,
                                        GObject      *thunarx_menu_item)
{
  gchar         *name, *label_text, *tooltip_text, *icon_name;
  GtkWidget     *image = NULL;
  GIcon         *icon = NULL;
  GtkToolItem   *tool_item;
  UCAActivation *data;

  g_return_val_if_fail (THUNARX_IS_MENU_ITEM (thunarx_menu_item), NULL);
  g_return_val_if_fail (THUNAR_IS_WINDOW (window), NULL);

  g_object_get (G_OBJECT (thunarx_menu_item),
                "name", &name,
                "label", &label_text,
                "tooltip", &tooltip_text,
                "icon", &icon_name,
                NULL);

  if (icon_name != NULL)
    icon = g_icon_new_for_string (icon_name, NULL);
  if (icon != NULL)
    image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
  if (image == NULL)
    image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_LARGE_TOOLBAR);

  tool_item = gtk_tool_button_new (image, label_text);
  gtk_widget_set_tooltip_text (GTK_WIDGET (tool_item), tooltip_text);

  data = g_slice_new0 (UCAActivation);
  data->action_name = g_strdup (name);
  data->window = window;
  g_signal_connect_data (G_OBJECT (tool_item), "clicked", G_CALLBACK (thunar_window_check_activate_toolbar_uca), data, uca_activation_callback_free, G_CONNECT_SWAPPED);

  gtk_toolbar_insert (GTK_TOOLBAR (window->location_toolbar), tool_item, -1);

  g_free (name);
  g_free (label_text);
  g_free (tooltip_text);
  g_free (icon_name);
  if (icon != NULL)
    g_object_unref (icon);

  return GTK_WIDGET (tool_item);
}



static GtkWidget*
thunar_window_create_toolbar_item_from_action (ThunarWindow       *window,
                                               ThunarWindowAction  action,
                                               guint               item_order)
{
  GtkWidget *toolbar_item;

  toolbar_item = xfce_gtk_tool_button_new_from_action_entry (get_action_entry (action), G_OBJECT (window), GTK_TOOLBAR (window->location_toolbar));
  g_object_set_data_full (G_OBJECT (toolbar_item), "label", g_strdup (get_action_entry (action)->menu_item_label_text), g_free);
  g_object_set_data_full (G_OBJECT (toolbar_item), "icon", g_strdup (get_action_entry (action)->menu_item_icon_name), g_free);
  
  if (action != THUNAR_WINDOW_ACTION_BACK && action != THUNAR_WINDOW_ACTION_FORWARD)
    g_signal_connect_after (G_OBJECT (toolbar_item), "button-press-event", G_CALLBACK (thunar_window_toolbar_button_clicked), G_OBJECT (window));

  thunar_g_object_set_guint_data (G_OBJECT (toolbar_item), "default-order", item_order);

  return toolbar_item;
}



static GtkWidget*
thunar_window_create_toolbar_toggle_item_from_action (ThunarWindow           *window,
                                                      ThunarWindowAction      action,
                                                      gboolean                active,
                                                      guint                   item_order)
{
  GtkWidget *toolbar_item;

  toolbar_item = xfce_gtk_toggle_tool_button_new_from_action_entry (get_action_entry (action), G_OBJECT (window), active, GTK_TOOLBAR (window->location_toolbar));
  g_object_set_data_full (G_OBJECT (toolbar_item), "label", g_strdup (get_action_entry (action)->menu_item_label_text), g_free);
  g_object_set_data_full (G_OBJECT (toolbar_item), "icon", g_strdup (get_action_entry (action)->menu_item_icon_name), g_free);
  g_signal_connect_after (G_OBJECT (toolbar_item), "button-press-event", G_CALLBACK (thunar_window_toolbar_button_clicked), G_OBJECT (window));

  thunar_g_object_set_guint_data (G_OBJECT (toolbar_item), "default-order", item_order);

  return toolbar_item;
}



static void
thunar_window_location_toolbar_create (ThunarWindow *window)
{
  GtkToolItem     *tool_item;
  guint            item_order = 0;
  gboolean         small_icons;

  g_object_get (G_OBJECT (window->preferences), "misc-small-toolbar-icons", &small_icons, NULL);

  /* allocate the new location bar widget */
  window->location_bar = thunar_location_bar_new ();
  g_object_bind_property (G_OBJECT (window), "current-directory", G_OBJECT (window->location_bar), "current-directory", G_BINDING_SYNC_CREATE);
  g_signal_connect_swapped (G_OBJECT (window->location_bar), "change-directory", G_CALLBACK (thunar_window_set_current_directory), window);
  g_signal_connect_swapped (G_OBJECT (window->location_bar), "open-new-tab", G_CALLBACK (thunar_window_notebook_open_new_tab), window);
  g_signal_connect_swapped (G_OBJECT (window->location_bar), "entry-done", G_CALLBACK (thunar_window_update_location_bar_visible), window);
  gtk_widget_show (window->location_bar);

  /* setup the toolbar for the location bar */
  window->location_toolbar = gtk_toolbar_new ();
  gtk_toolbar_set_style (GTK_TOOLBAR (window->location_toolbar), GTK_TOOLBAR_ICONS);
  gtk_toolbar_set_icon_size (GTK_TOOLBAR (window->location_toolbar),
                             small_icons ? GTK_ICON_SIZE_SMALL_TOOLBAR : GTK_ICON_SIZE_LARGE_TOOLBAR);
  gtk_widget_set_hexpand (window->location_toolbar, TRUE);

  /* The first toolbar item must always be THUNAR_WINDOW_ACTION_VIEW_MENUBAR which we hide by default */
  window->location_toolbar_item_view_menubar = thunar_window_create_toolbar_item_from_action (window, THUNAR_WINDOW_ACTION_VIEW_MENUBAR, item_order++);
  window->location_toolbar_item_back         = thunar_window_create_toolbar_item_from_action (window, THUNAR_WINDOW_ACTION_BACK, item_order++);
  window->location_toolbar_item_forward      = thunar_window_create_toolbar_item_from_action (window, THUNAR_WINDOW_ACTION_FORWARD, item_order++);
  window->location_toolbar_item_parent       = thunar_window_create_toolbar_item_from_action (window, THUNAR_WINDOW_ACTION_OPEN_PARENT, item_order++);
  window->location_toolbar_item_home         = thunar_window_create_toolbar_item_from_action (window, THUNAR_WINDOW_ACTION_OPEN_HOME, item_order++);
  window->location_toolbar_item_undo         = thunar_window_create_toolbar_item_from_action (window, THUNAR_WINDOW_ACTION_UNDO, item_order++);
  window->location_toolbar_item_redo         = thunar_window_create_toolbar_item_from_action (window, THUNAR_WINDOW_ACTION_REDO, item_order++);
  window->location_toolbar_item_zoom_out     = thunar_window_create_toolbar_item_from_action (window, THUNAR_WINDOW_ACTION_ZOOM_OUT, item_order++);
  window->location_toolbar_item_zoom_in      = thunar_window_create_toolbar_item_from_action (window, THUNAR_WINDOW_ACTION_ZOOM_IN, item_order++);
                                               thunar_window_create_toolbar_item_from_action (window, THUNAR_WINDOW_ACTION_ZOOM_RESET, item_order++);
                                               thunar_window_create_toolbar_item_from_action (window, THUNAR_WINDOW_ACTION_VIEW_AS_ICONS, item_order++);
                                               thunar_window_create_toolbar_item_from_action (window, THUNAR_WINDOW_ACTION_VIEW_AS_DETAILED_LIST, item_order++);
                                               thunar_window_create_toolbar_item_from_action (window, THUNAR_WINDOW_ACTION_VIEW_AS_COMPACT_LIST, item_order++);
                                               thunar_window_create_toolbar_toggle_item_from_action (window, THUNAR_WINDOW_ACTION_VIEW_SPLIT, thunar_window_split_view_is_active (window), item_order++);

  g_signal_connect (G_OBJECT (window->location_toolbar_item_back), "button-press-event", G_CALLBACK (thunar_window_history_clicked), G_OBJECT (window));
  g_signal_connect (G_OBJECT (window->location_toolbar_item_forward), "button-press-event", G_CALLBACK (thunar_window_history_clicked), G_OBJECT (window));
  g_signal_connect (G_OBJECT (window->location_toolbar_item_parent), "button-press-event", G_CALLBACK (thunar_window_open_parent_clicked), G_OBJECT (window));
  g_signal_connect (G_OBJECT (window->location_toolbar_item_home), "button-press-event", G_CALLBACK (thunar_window_open_home_clicked), G_OBJECT (window));
  g_signal_connect (G_OBJECT (window), "button-press-event", G_CALLBACK (thunar_window_button_press_event), G_OBJECT (window));
  window->signal_handler_id_history_changed = 0;

  g_object_bind_property (G_OBJECT (window->job_operation_history), "can-undo", G_OBJECT (window->location_toolbar_item_undo), "sensitive", G_BINDING_SYNC_CREATE);
  g_object_bind_property (G_OBJECT (window->job_operation_history), "can-redo", G_OBJECT (window->location_toolbar_item_redo), "sensitive", G_BINDING_SYNC_CREATE);

  /* The UCA shortcuts need to be checked 'by hand', since we dont want to permanently keep menu items for them */
  g_signal_connect (window, "key-press-event", G_CALLBACK (thunar_window_check_uca_key_activation), NULL);

  /* add the location bar to the toolbar */
  tool_item = gtk_tool_item_new ();
  gtk_tool_item_set_expand (tool_item, TRUE);
  gtk_toolbar_insert (GTK_TOOLBAR (window->location_toolbar), tool_item, -1);
  gtk_toolbar_set_show_arrow (GTK_TOOLBAR (window->location_toolbar), FALSE);
  g_object_set_data_full (G_OBJECT (tool_item), "label", g_strdup (_("Location Bar")), g_free);
  g_object_set_data_full (G_OBJECT (tool_item), "icon", g_strdup(""), g_free);
  thunar_g_object_set_guint_data (G_OBJECT (tool_item), "default-order", item_order++);

  thunar_window_create_toolbar_item_from_action (window, THUNAR_WINDOW_ACTION_RELOAD, item_order++);
  window->location_toolbar_item_search = thunar_window_create_toolbar_toggle_item_from_action (window, THUNAR_WINDOW_ACTION_SEARCH, window->is_searching, item_order++);

  /* add custom actions to the toolbar */
  thunar_window_location_toolbar_add_ucas (window);

  /* display the toolbar */
  gtk_widget_show_all (window->location_toolbar);

  /* view_menubar is only visible when the menu is hidden */
  gtk_widget_set_visible (window->location_toolbar_item_view_menubar, !window->menubar_visible);

  /* add the location bar itself after gtk_widget_show_all to not mess with the visibility of the location buttons */
  gtk_container_add (GTK_CONTAINER (tool_item), window->location_bar);

  /* load the correct order of items in the toolbar */
  thunar_window_location_toolbar_load_last_order (window);

  /* load the correct visibility of items in the toolbar */
  thunar_window_location_toolbar_load_visibility (window);

  /* attach the toolbar to the window */
  gtk_grid_attach (GTK_GRID (window->grid), window->location_toolbar, 0, 1, 1, 1);
}



static void
thunar_window_update_location_toolbar (GFileMonitor     *monitor,
                                       GFile            *file,
                                       GFile            *other_file,
                                       GFileMonitorEvent event_type,
                                       ThunarWindow     *window)
{
  gtk_widget_destroy (window->location_toolbar);
  thunar_window_location_toolbar_create (window);
}




static void
thunar_window_location_toolbar_add_ucas (ThunarWindow *window)
{
  ThunarxProviderFactory *provider_factory;
  ThunarFile             *home_folder;
  GFile                  *home_gfile;
  GList                  *providers;
  GList                  *thunarx_menu_items = NULL;
  GList                  *lp_provider;
  GList                  *lp_item;
  GList                  *toolbar_container_children;
  guint                   item_count;

  /* determine the file for the home directory */
  home_gfile = thunar_g_file_new_for_home ();
  home_folder = thunar_file_get (home_gfile, NULL);
  g_object_unref (home_gfile);
  if (home_folder == NULL)
    return;

  /* get number of items in the toolbar */
  toolbar_container_children = gtk_container_get_children (GTK_CONTAINER (window->location_toolbar));
  item_count = g_list_length (toolbar_container_children);
  g_list_free (toolbar_container_children);

  /* load the menu providers from the provider factory */
  provider_factory = thunarx_provider_factory_get_default ();
  providers = thunarx_provider_factory_list_providers (provider_factory, THUNARX_TYPE_MENU_PROVIDER);
  g_object_unref (provider_factory);

  if (G_UNLIKELY (providers != NULL))
    {
      /* load the menu items offered by the menu providers */
      for (lp_provider = providers; lp_provider != NULL; lp_provider = lp_provider->next)
        {
          thunarx_menu_items = thunarx_menu_provider_get_folder_menu_items (lp_provider->data, GTK_WIDGET (window),
                                                                            THUNARX_FILE_INFO (home_folder));

          for (lp_item = thunarx_menu_items; lp_item != NULL; lp_item = lp_item->next)
            {
              gchar     *name, *label_text, *icon_name;

              g_object_get (G_OBJECT (lp_item->data),
                            "name", &name,
                            "label", &label_text,
                            "icon", &icon_name,
                            NULL);

              if (g_str_has_prefix (name, "uca-action") == TRUE)
                {
                  GtkWidget *toolbar_item;
                  guint *item_order;

                  toolbar_item = thunar_window_location_toolbar_add_uca (window, lp_item->data);
                  g_object_set_data_full (G_OBJECT (toolbar_item), "label", label_text, g_free);
                  g_object_set_data_full (G_OBJECT (toolbar_item), "icon", icon_name, g_free);
                  item_order = g_malloc (sizeof (gint));
                  *item_order = item_count++;
                  g_object_set_data_full (G_OBJECT (toolbar_item), "default-order", item_order, g_free);
                  g_signal_connect (G_OBJECT (toolbar_item), "button-press-event", G_CALLBACK (thunar_window_toolbar_button_clicked), G_OBJECT (window));
                }

              g_free (name);
            }

          g_list_free_full (thunarx_menu_items, g_object_unref);
        }

      g_list_free_full (providers, g_object_unref);
    }

  g_object_unref (home_folder);
}



/**
 * thunar_window_location_toolbar_load_last_order:
 * @window            : a #ThunarWindow instance.
 *
 * Load the order of toolbar items stored in `last-toolbar-button-order`.
 **/
static void
thunar_window_location_toolbar_load_last_order (ThunarWindow *window)
{
  gchar      **item_order;
  guint        item_order_length;
  gchar       *tmp;
  guint        item_count = thunar_window_toolbar_item_count (window);
  guint        target_order[item_count];  /* item_order converted to guint and without invalid entries (i.e. entries >= item_count */
  guint        current_order[item_count];

  /* determine the column order from the preferences */
  g_object_get (G_OBJECT (window->preferences), "last-toolbar-item-order", &tmp, NULL);
  item_order = g_strsplit (tmp, ",", -1);
  item_order_length = g_strv_length (item_order);
  g_free (tmp);

  /* for this to work the toolbar must be in the default order */
  for (guint i = 0; i < item_count; i++)
    current_order[i] = i;

  for (guint i = 0; i < item_count; i++)
    target_order[i] = i;

  /* convert strings to guints for convenience */
  for (guint i = 0, j = 0; i < item_order_length; i++)
    {
      guint64 n;

      if (g_ascii_string_to_unsigned (item_order[i], 10, 0, UINT_MAX, &n, NULL) == FALSE)
        g_error ("Invalid entry in \"last-toolbar-button-order\"");

      /* the entry is invalid, it is likely that a custom action was removed and the preference hasn't been updated */
      if (n >= item_count)
        continue;

      target_order[j] = n;
      j++;
    }

  /* now rearrange the toolbar items, the goal is to make the current_order like the target_order */
  for (guint i = 0; i < item_count; i++)
    {
      guint x = target_order[i];
      for (guint j = i; j < item_count; j++)
        {
          guint y = current_order[j];
          if (x == y && i != j)
            {
              /* swap the positions of the toolbar items */
              thunar_window_toolbar_swap_items (window, i, j);
              y = current_order[i];
              current_order[i] = target_order[i];
              current_order[j] = y;
              break;
            }
        }
    }

  /* release the column order */
  g_strfreev (item_order);
}



/**
 * thunar_window_location_toolbar_load_visibility:
 * @window            : a #ThunarWindow instance.
 *
 * Load the visibility of toolbar items. This should run after thunar_window_location_toolbar_load_last_order because
 * it depends on the the saved order instead of the default.
 *
 * returns FALSE so it can be used as an idle function
 **/
static gboolean
thunar_window_location_toolbar_load_visibility (ThunarWindow *window)
{
  gchar      **item_visibility;
  guint        item_visibility_length;
  gchar       *item_visibility_string;
  guint        item_count = thunar_window_toolbar_item_count (window);
  guint        target_order[item_count];

  /* determine the column order from the preferences */
  g_object_get (G_OBJECT (window->preferences), "last-toolbar-visible-buttons", &item_visibility_string, NULL);
  item_visibility = g_strsplit (item_visibility_string, ",", -1);
  item_visibility_length = g_strv_length (item_visibility);
  g_free (item_visibility_string);

  for (guint i = 0; i < item_count; i++)
    target_order[i] = 0; /* default to invisible */

  /* convert strings to guints for convenience */
  for (guint i = 0, j = 0; i < item_visibility_length; i++)
    {
      guint64 n;

      if (g_ascii_string_to_unsigned (item_visibility[i], 10, 0, UINT_MAX, &n, NULL) == FALSE)
        g_error ("Invalid entry in \"last-toolbar-visible-buttons\"");

      target_order[j] = n;
      j++;
    }

  for (guint i = 0; i < item_count; i++)
    {
      guint visible = target_order[i];

      if (visible == 0)
        thunar_window_toolbar_toggle_item_visibility (window, i);
    }

  /* release the column order */
  g_strfreev (item_visibility);

  return FALSE;
}



static guint
thunar_window_toolbar_item_count (ThunarWindow *window)
{
  GList *toolbar_items;
  guint  count = 0;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), count);

  if (window->location_toolbar == NULL)
    return count;

  toolbar_items = gtk_container_get_children (GTK_CONTAINER (window->location_toolbar));
  count = g_list_length (toolbar_items);
  g_list_free (toolbar_items);

  return count;
}



void
thunar_window_toolbar_toggle_item_visibility (ThunarWindow *window,
                                              gint          index)
{
  GList *toolbar_items, *lp;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (index >= 0);

  if (window->location_toolbar == NULL)
    return;

  toolbar_items = gtk_container_get_children (GTK_CONTAINER (window->location_toolbar));
  lp = toolbar_items;

  for (gint i = 0; lp != NULL; lp = lp->next, i++)
    {
      GtkWidget *item = lp->data;

      /* visibility of this item is only controlled by 'window->menubar_visible' */
      if (item == window->location_toolbar_item_view_menubar)
        continue;

      if (index == i)
        {
          gtk_widget_set_visible (item, !gtk_widget_is_visible (item));
          break;
        }
    }

  g_list_free (toolbar_items);
}



void
thunar_window_toolbar_swap_items (ThunarWindow *window,
                                  gint          index_a,
                                  gint          index_b)
{
  GList     *toolbar_items, *lp;
  GtkWidget *item_a = NULL;
  GtkWidget *item_b = NULL;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (index_a >= 0);
  _thunar_return_if_fail (index_b >= 0);

  if (window->location_toolbar == NULL)
    return;

  toolbar_items = gtk_container_get_children (GTK_CONTAINER (window->location_toolbar));
  lp = toolbar_items;

  for (gint i = 0; lp != NULL; lp = lp->next, i++)
    {
      if (index_a == i)
        item_a = lp->data;
      if (index_b == i)
        item_b = lp->data;
    }

  if (item_a == NULL || item_b == NULL)
    {
      g_error("Unable to swap toolbar items.\n");
      return;
    }

  g_object_ref (item_a);
  gtk_container_remove (GTK_CONTAINER (window->location_toolbar), item_a);
  gtk_toolbar_insert (GTK_TOOLBAR (window->location_toolbar), GTK_TOOL_ITEM (item_a), index_b);
  g_object_unref (item_a);

  g_object_ref (item_b);
  gtk_container_remove (GTK_CONTAINER (window->location_toolbar), item_b);
  gtk_toolbar_insert (GTK_TOOLBAR (window->location_toolbar), GTK_TOOL_ITEM (item_b), index_a);
  g_object_unref (item_b);

  g_list_free (toolbar_items);
}
