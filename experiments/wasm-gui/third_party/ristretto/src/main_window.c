/*
 *  Copyright (c) Stephan Arts 2006-2012 <stephan@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#include "util.h"
#include "mime_db.h"
#include "icon_bar.h"
#include "thumbnailer.h"
#include "image_viewer.h"
#include "main_window.h"
#include "main_window_ui.h"
#include "xfce_wallpaper_manager.h"
#include "gnome_wallpaper_manager.h"
#include "privacy_dialog.h"
#include "properties_dialog.h"
#include "preferences_dialog.h"
#include "app_menu_item.h"
#include "print.h"

#include <gio/gdesktopappinfo.h>



#define RISTRETTO_APP_TITLE _("Image Viewer")
#define RISTRETTO_DESKTOP_ID RISTRETTO_APP_ID ".desktop"

#define ERROR_SAVE_FAILED _("Could not save file")
#define ERROR_OPEN_FAILED _("Some files could not be opened: see the text logs for details")
#define ERROR_DELETE_FAILED_DISK _("An error occurred when deleting image '%s' from disk")
#define ERROR_DELETE_FAILED_TRASH _("An error occurred when sending image '%s' to trash")

enum
{
    EDITOR_CHOOSER_MODEL_COLUMN_NAME = 0,
    EDITOR_CHOOSER_MODEL_COLUMN_GICON,
    EDITOR_CHOOSER_MODEL_COLUMN_APPLICATION,
    EDITOR_CHOOSER_MODEL_COLUMN_STYLE,
    EDITOR_CHOOSER_MODEL_COLUMN_STYLE_SET,
    EDITOR_CHOOSER_MODEL_COLUMN_WEIGHT,
    EDITOR_CHOOSER_MODEL_COLUMN_WEIGHT_SET
};

enum
{
    FIRST,
    LAST,
    PREVIOUS,
    NEXT
};

static RsttoMainWindow *app_window;
static RsttoImageList *app_image_list;
static RsttoIconBar *app_icon_bar;
static GtkFileFilter *app_file_filter;



static void
rstto_main_window_finalize (GObject *object);


static gboolean
key_press_event (GtkWidget *widget,
                 GdkEventKey *event);


static void
cb_icon_bar_selection_changed (RsttoIconBar *icon_bar,
                               gpointer user_data);
static gint
cb_compare_app_infos (gconstpointer a,
                      gconstpointer b);
static void
rstto_main_window_image_list_iter_changed (RsttoMainWindow *window);
static gboolean
rstto_main_window_recent_filter (const GtkRecentFilterInfo *filter_info,
                                 gpointer user_data);
static void
rstto_main_window_launch_editor_chooser (RsttoMainWindow *window);

static gboolean
cb_rstto_main_window_configure_event (GtkWidget *widget,
                                      GdkEventConfigure *event);
static gboolean
cb_rstto_main_window_state_event (GtkWidget *widget,
                                  GdkEventWindowState *event,
                                  gpointer user_data);
static gboolean
cb_rstto_main_window_show_fs_toolbar_timeout (gpointer user_data);
static void
cb_rstto_main_window_show_fs_toolbar_timeout_destroy (gpointer user_data);
static gboolean
cb_rstto_main_window_hide_fs_mouse_cursor_timeout (gpointer user_data);
static void
cb_rstto_main_window_hide_fs_mouse_cursor_timeout_destroy (gpointer user_data);
static void
cb_rstto_main_window_image_list_iter_changed (RsttoImageListIter *iter,
                                              RsttoMainWindow *window);
static void
rstto_main_window_update_statusbar (RsttoMainWindow *window);

static void
cb_rstto_main_window_zoom_100 (GtkWidget *widget,
                               RsttoMainWindow *window);
static void
cb_rstto_main_window_zoom_fit (GtkWidget *widget,
                               RsttoMainWindow *window);
static void
cb_rstto_main_window_zoom_in (GtkWidget *widget,
                              RsttoMainWindow *window);
static void
cb_rstto_main_window_zoom_out (GtkWidget *widget,
                               RsttoMainWindow *window);
static void
cb_rstto_main_window_default_zoom (GtkRadioAction *action,
                                   GtkRadioAction *current,
                                   RsttoMainWindow *window);

static void
cb_rstto_main_window_rotate_cw (GtkWidget *widget,
                                RsttoMainWindow *window);
static void
cb_rstto_main_window_rotate_ccw (GtkWidget *widget,
                                 RsttoMainWindow *window);

static void
cb_rstto_main_window_flip_hz (GtkWidget *widget,
                              RsttoMainWindow *window);
static void
cb_rstto_main_window_flip_vt (GtkWidget *widget,
                              RsttoMainWindow *window);

static gboolean
rstto_main_window_select_valid_image (RsttoMainWindow *window,
                                      gint position);
static void
cb_rstto_main_window_next_image (GtkWidget *widget,
                                 RsttoMainWindow *window);
static void
cb_rstto_main_window_previous_image (GtkWidget *widget,
                                     RsttoMainWindow *window);
static void
cb_rstto_main_window_first_image (GtkWidget *widget,
                                  RsttoMainWindow *window);
static void
cb_rstto_main_window_last_image (GtkWidget *widget,
                                 RsttoMainWindow *window);

static void
cb_rstto_main_window_open_with_other_app (GtkWidget *widget,
                                          RsttoMainWindow *window);
static void
cb_rstto_main_window_open_image (GtkWidget *widget,
                                 RsttoMainWindow *window);
static void
cb_rstto_main_window_open_recent (GtkRecentChooser *chooser,
                                  RsttoMainWindow *window);
static void
cb_rstto_main_window_properties (GtkWidget *widget,
                                 RsttoMainWindow *window);
static void
cb_rstto_main_window_print (GtkWidget *widget,
                            RsttoMainWindow *window);
static void
cb_rstto_main_window_close (GtkWidget *widget,
                            RsttoMainWindow *window);
static void
cb_rstto_main_window_edit (GtkWidget *widget,
                           RsttoMainWindow *window);
static void
cb_rstto_main_window_save_copy (GtkWidget *widget,
                                RsttoMainWindow *window);
static void
cb_rstto_main_window_delete (GtkWidget *widget,
                             RsttoMainWindow *window);
static void
cb_rstto_main_window_refresh (GtkWidget *widget,
                              RsttoMainWindow *window);
static void
cb_rstto_main_window_dnd_files (GtkWidget *widget,
                                gchar **uris,
                                RsttoMainWindow *window);
static void
cb_rstto_main_window_set_title (RsttoMainWindow *window);
static void
cb_rstto_main_window_set_icon (RsttoMainWindow *window,
                               RsttoFile *file);

static void
cb_rstto_main_window_set_as_wallpaper (GtkWidget *widget,
                                       RsttoMainWindow *window);
static void
cb_rstto_main_window_sorting_function_changed (GtkRadioAction *action,
                                               GtkRadioAction *current,
                                               RsttoMainWindow *window);
static void
cb_rstto_main_window_navigationtoolbar_position_changed (GtkRadioAction *action,
                                                         GtkRadioAction *current,
                                                         RsttoMainWindow *window);
static void
cb_rstto_main_window_thumbnail_size_changed (GtkRadioAction *action,
                                             GtkRadioAction *current,
                                             RsttoMainWindow *window);

static gboolean
cb_rstto_main_window_navigationtoolbar_button_press_event (GtkWidget *widget,
                                                           GdkEventButton *event,
                                                           gpointer user_data);
static void
cb_rstto_main_window_update_statusbar (GtkWidget *widget,
                                       RsttoMainWindow *window);

static void
cb_rstto_main_window_play (GtkWidget *widget,
                           RsttoMainWindow *window);
static void
cb_rstto_main_window_pause (GtkWidget *widget,
                            RsttoMainWindow *window);

static void
cb_rstto_main_window_toggle_show_toolbar (GtkWidget *widget,
                                          RsttoMainWindow *window);
static void
cb_rstto_main_window_toggle_show_thumbnailbar (GtkWidget *widget,
                                               RsttoMainWindow *window);
static void
cb_rstto_main_window_toggle_show_statusbar (GtkWidget *widget,
                                            RsttoMainWindow *window);

static void
cb_rstto_main_window_preferences (GtkWidget *widget,
                                  RsttoMainWindow *window);
static void
cb_rstto_main_window_copy_image (GtkWidget *widget,
                                 RsttoMainWindow *window);
static void
cb_rstto_main_window_clear_private_data (GtkWidget *widget,
                                         RsttoMainWindow *window);
static void
cb_rstto_main_window_about (GtkWidget *widget,
                            RsttoMainWindow *window);
static void
cb_rstto_main_window_contents (GtkWidget *widget,
                               RsttoMainWindow *window);
static void
cb_rstto_main_window_quit (GtkWidget *widget,
                           RsttoMainWindow *window);
static gboolean
cb_rstto_main_window_motion_notify_event (RsttoMainWindow *window,
                                          GdkEventMotion *event,
                                          gpointer user_data);
static gboolean
cb_rstto_main_window_image_viewer_enter_notify_event (GtkWidget *widget,
                                                      GdkEventCrossing *event,
                                                      gpointer user_data);
static gboolean
cb_rstto_main_window_image_viewer_scroll_event (GtkWidget *widget,
                                                GdkEventScroll *event,
                                                gpointer user_data);

static void
rstto_main_activate_file_menu_actions (RsttoMainWindow *window,
                                       gboolean activate);
static void
rstto_main_activate_go_menu_actions (RsttoMainWindow *window,
                                     gboolean activate);
static void
rstto_main_activate_view_menu_actions (RsttoMainWindow *window,
                                       gboolean activate);
static void
rstto_main_activate_popup_menu_actions (RsttoMainWindow *window,
                                        gboolean activate);
static void
rstto_main_activate_toolbar_actions (RsttoMainWindow *window,
                                     gboolean activate);
static void
rstto_main_window_update_buttons (RsttoMainWindow *window);
static void
rstto_main_window_set_navigationbar_position (RsttoMainWindow *window,
                                              guint orientation);
static void
rstto_main_window_set_thumbnail_size (RsttoMainWindow *window,
                                      RsttoThumbnailSize size);
static void
cb_rstto_wrap_images_changed (GObject *object,
                              GParamSpec *pspec,
                              gpointer user_data);
static void
cb_rstto_desktop_type_changed (GObject *object,
                               GParamSpec *pspec,
                               gpointer user_data);



static GtkActionEntry action_entries[] =
{
/* File Menu */
    { "file-menu",
            NULL,
            N_ ("_File"),
            NULL,
            NULL,
            NULL, },
    { "open",
            "document-open", /* Icon-name */
            N_ ("_Open..."), /* Label-text */
            "<control>O", /* Keyboard shortcut */
            N_ ("Open an image"), /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_open_image), },
    { "save-copy",
            "document-save-as", /* Icon-name */
            N_ ("_Save copy..."), /* Label-text */
            "<control>s", /* Keyboard shortcut */
            N_ ("Save a copy of the image"), /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_save_copy), },
    { "properties",
            "document-properties", /* Icon-name */
            N_ ("Proper_ties..."), /* Label-text */
            NULL, /* Keyboard shortcut */
            N_ ("Show file properties"), /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_properties), },
    { "print",
            "document-print", /* Icon-name */
            N_ ("_Print..."), /* Label-text */
            "<control>P", /* Keyboard shortcut */
            N_ ("Print the current image"), /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_print), },
    { "edit",
            "gtk-edit", /* Icon-name */
            N_ ("_Edit"), /* Label-text */
            NULL, /* Keyboard shortcut */
            N_ ("Edit this image"), /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_edit), },
    { "close",
            "window-close", /* Icon-name */
            N_ ("_Close"), /* Label-text */
            "<control>W", /* Keyboard shortcut */
            N_ ("Close this image"), /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_close), },
    { "quit",
            "application-exit", /* Icon-name */
            N_ ("_Quit"), /* Label-text */
            "<control>Q", /* Keyboard shortcut */
            N_ ("Quit Ristretto"), /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_quit), },
/* Edit Menu */
    { "edit-menu",
            NULL,
            N_ ("_Edit"),
            NULL,
            NULL,
            NULL, },
    { "copy-image",
            "edit-copy",
            N_ ("_Copy image to clipboard"),
            "<control>C",
            NULL,
            G_CALLBACK (cb_rstto_main_window_copy_image), },
    { "open-with-menu",
            NULL,
            N_ ("_Open with"),
            NULL,
            NULL,
            NULL, },
    { "sorting-menu",
            NULL,
            N_ ("_Sort by"),
            NULL,
            NULL,
            NULL, },
    { "delete",
            "edit-delete", /* Icon-name */
            N_ ("_Delete"), /* Label-text */
            "Delete", /* Keyboard shortcut */
            N_ ("Delete this image from disk"), /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_delete), },
    { "clear-private-data",
            "edit-clear", /* Icon-name */
            N_ ("_Clear private data..."), /* Label-text */
            "<control><shift>Delete", /* Keyboard shortcut */
            NULL, /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_clear_private_data), },
    { "preferences",
            "preferences-system", /* Icon-name */
            N_ ("_Preferences..."), /* Label-text */
            NULL, /* Keyboard shortcut */
            NULL, /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_preferences), },
/* View Menu */
    { "view-menu",
            NULL,
            N_ ("_View"),
            NULL,
            NULL,
            NULL, },
    { "fullscreen",
            "view-fullscreen", /* Icon-name */
            N_ ("_Fullscreen"), /* Label-text */
            "F11", /* Keyboard shortcut */
            N_ ("Switch to fullscreen"), /* Tooltip text */
            G_CALLBACK (rstto_main_window_fullscreen), },
    { "unfullscreen",
            "view-restore", /* Icon-name */
            N_ ("_Leave Fullscreen"), /* Label-text */
            NULL, /* Keyboard shortcut */
            N_ ("Leave Fullscreen"), /* Tooltip text */
            G_CALLBACK (rstto_main_window_fullscreen), },
    { "set-as-wallpaper",
            "preferences-desktop-wallpaper", /* Icon-name */
            N_ ("Set as _Wallpaper..."), /* Label-text */
            NULL, /* Keyboard shortcut */
            NULL, /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_set_as_wallpaper), },
/* Zoom submenu */
    { "zoom-menu",
            NULL,
            N_ ("_Zoom"),
            NULL,
            NULL,
            NULL, },
    { "zoom-in",
            "zoom-in", /* Icon-name */
            N_ ("Zoom _In"), /* Label-text */
            "<control>plus", /* Keyboard shortcut */
            N_ ("Zoom in"), /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_zoom_in), },
    { "zoom-out",
            "zoom-out", /* Icon-name */
            N_ ("Zoom _Out"), /* Label-text */
            "<control>minus", /* Keyboard shortcut */
            N_ ("Zoom out"), /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_zoom_out), },
    { "zoom-fit",
            "zoom-fit-best", /* Icon-name */
            N_ ("Zoom _Fit"), /* Label-text */
            "<control>equal", /* Keyboard shortcut */
            N_ ("Zoom to fit window"), /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_zoom_fit), },
    { "zoom-100",
            "zoom-original", /* Icon-name */
            N_ ("_Normal Size"), /* Label-text */
            "<control>0", /* Keyboard shortcut */
            N_ ("Zoom to 100%"), /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_zoom_100), },
/* Default zoom submenu */
    { "default-zoom-menu",
            NULL,
            N_ ("_Default Zoom"),
            NULL,
            NULL,
            NULL, },
/* Rotation submenu */
    { "rotation-menu",
            NULL,
            N_ ("_Rotation"),
            NULL,
            NULL,
            NULL, },
    { "rotate-cw",
            "object-rotate-right", /* Icon-name */
            N_ ("Rotate _Right"), /* Label-text */
            "<control>bracketright", /* Keyboard shortcut */
            NULL, /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_rotate_cw), },
    { "rotate-ccw",
            "object-rotate-left", /* Icon-name */
            N_ ("Rotate _Left"), /* Label-text */
            "<control>bracketleft", /* Keyboard shortcut */
            NULL, /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_rotate_ccw), },
/* Flip submenu */
    { "flip-menu",
            NULL,
            N_ ("_Flip"),
            NULL,
            NULL,
            NULL, },
    { "flip-horizontally",
            "object-flip-horizontal",
            N_ ("Flip _Horizontally"), /* Label-text */
            "<control>braceright", /* Keyboard shortcut */
            NULL, /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_flip_hz), },
    { "flip-vertically",
            "object-flip-vertical",
            N_ ("Flip _Vertically"), /* Label-text */
            "<control>braceleft", /* Keyboard shortcut */
            NULL, /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_flip_vt), },
/* Go Menu */
    { "go-menu",
            NULL,
            N_ ("_Go"),
            NULL,
            NULL,
            NULL, },
    { "forward",
            "go-next", /* Icon-name */
            N_ ("_Forward"), /* Label-text */
            "space", /* Keyboard shortcut */
            N_("Next image"), /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_next_image), },
    { "back",
            "go-previous", /* Icon-name */
            N_ ("_Back"), /* Label-text */
            "BackSpace", /* Keyboard shortcut */
            N_("Previous image"), /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_previous_image), },
    { "first",
            "go-first", /* Icon-name */
            N_ ("F_irst"), /* Label-text */
            "Home", /* Keyboard shortcut */
            N_("First image"), /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_first_image), },
    { "last",
            "go-last", /* Icon-name */
            N_ ("_Last"), /* Label-text */
            "End", /* Keyboard shortcut */
            N_("Last image"), /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_last_image), },
/* Help Menu */
    { "help-menu",
            NULL,
            N_ ("_Help"),
            NULL,
            NULL,
            NULL, },
    { "contents",
            "help-browser", /* Icon-name */
            N_ ("_Contents"), /* Label-text */
            "F1", /* Keyboard shortcut */
            N_ ("Display ristretto user manual"), /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_contents), },
    { "about",
            "help-about",  /* Icon-name */
            N_ ("_About"), /* Label-text */
            NULL, /* Keyboard shortcut */
            N_ ("Display information about ristretto"), /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_about), },
/* Position Menu */
    { "position-menu",
            NULL,
            N_ ("_Position"),
            NULL,
            NULL,
            NULL, },
    { "size-menu",
            NULL,
            N_ ("_Size"),
            NULL,
            NULL,
            NULL, },
    { "thumbnailbar-position-menu",
            NULL,
            N_ ("Thumbnail Bar _Position"),
            NULL,
            NULL,
            NULL, },
    { "thumbnailbar-size-menu",
            NULL,
            N_ ("Thumb_nail Size"),
            NULL,
            NULL,
            NULL, },
/* Misc */
    { "leave-fullscreen",
            "view-restore", /* Icon-name */
            N_ ("Leave _Fullscreen"), /* Label-text */
            NULL, /* Keyboard shortcut */
            NULL, /* Tooltip text */
            G_CALLBACK (rstto_main_window_fullscreen), },
    { "tb-menu",
            NULL,
            NULL,
            NULL,
            NULL,
            NULL, }
};

/** Toggle Action Entries */
static const GtkToggleActionEntry toggle_action_entries[] =
{
    /* Toggle visibility of the main file toolbar */
    { "show-toolbar",
            NULL, /* Icon-name */
            N_ ("_Show Toolbar"), /* Label-text */
            NULL, /* Keyboard shortcut */
            NULL, /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_toggle_show_toolbar),
            TRUE, },
    /* Toggle visibility of the main navigation toolbar */
    { "show-thumbnailbar",
            NULL, /* Icon-name */
            N_ ("Show _Thumbnail Bar"), /* Label-text */
            "<control>M", /* Keyboard shortcut */
            NULL, /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_toggle_show_thumbnailbar),
            TRUE, },
    /* Toggle visibility of the statusbar*/
    { "show-statusbar",
            NULL, /* Icon-name */
            N_ ("Show Status _Bar"), /* Label-text */
            NULL, /* Keyboard shortcut */
            NULL, /* Tooltip text */
            G_CALLBACK (cb_rstto_main_window_toggle_show_statusbar),
            TRUE, },
};

/** Image sorting options*/
static const GtkRadioActionEntry radio_action_sort_entries[] =
{
    { "sort-filename",
            NULL, /* Icon-name */
            N_("file name"), /* Label-text */
            NULL, /* Keyboard shortcut */
            NULL, /* Tooltip text */
            SORT_TYPE_NAME },
    { "sort-filetype",
            NULL, /* Icon-name */
            N_("file type"), /* Label-text */
            NULL, /* Keyboard shortcut */
            NULL, /* Tooltip text */
            SORT_TYPE_TYPE },
    { "sort-date",
            NULL, /* Icon-name */
            N_("date"), /* Label-text */
            NULL, /* Keyboard shortcut */
            NULL, /* Tooltip text */
            SORT_TYPE_DATE },
};

/** Navigationbar + Thumbnailbar positioning options*/
static const GtkRadioActionEntry radio_action_pos_entries[] =
{
    { "pos-left",
            NULL, /* Icon-name */
            N_("Left"), /* Label-text */
            NULL, /* Keyboard shortcut */
            NULL, /* Tooltip text */
            0 },
    { "pos-right",
            NULL,
            N_("Right"),
            NULL,
            NULL,
            1 },
    { "pos-top",
            NULL,
            N_("Top"),
            NULL,
            NULL,
            2 },
    { "pos-bottom",
            NULL,
            N_("Bottom"),
            NULL,
            NULL,
            3 },
};

/** Thumbnail-size options*/
static const GtkRadioActionEntry radio_action_size_entries[] =
{
    { "size-very-small",
            NULL,
            N_("Very Small"),
            NULL,
            NULL,
            0 },
    { "size-smaller",
            NULL,
            N_("Smaller"),
            NULL,
            NULL,
            1 },
    { "size-small",
            NULL,
            N_("Small"),
            NULL,
            NULL,
            2 },
    { "size-normal",
            NULL,
            N_("Normal"),
            NULL,
            NULL,
            3 },
    { "size-large",
            NULL,
            N_("Large"),
            NULL,
            NULL,
            4 },
    { "size-larger",
            NULL,
            N_("Larger"),
            NULL,
            NULL,
            5 },
    { "size-very-large",
            NULL,
            N_("Very Large"),
            NULL,
            NULL,
            6 },
};

/** Default zoom */
static const GtkRadioActionEntry radio_action_default_zoom[] =
{
    { "default-zoom-smart",
            NULL, /* Icon-name */
            N_ ("_Smart Zoom"), /* Label-text */
            "<alt>asterisk", /* Keyboard shortcut */
            NULL, /* Tooltip text */
            RSTTO_SCALE_NONE },
    { "default-zoom-fit",
            NULL, /* Icon-name */
            N_ ("Zoom _Fit"), /* Label-text */
            "<alt>equal", /* Keyboard shortcut */
            NULL, /* Tooltip text */
            RSTTO_SCALE_FIT_TO_VIEW },
    { "default-zoom-100",
            NULL, /* Icon-name */
            N_ ("_Normal Size"), /* Label-text */
            "<alt>0", /* Keyboard shortcut */
            NULL, /* Tooltip text */
            RSTTO_SCALE_REAL_SIZE },
};



struct _RsttoMainWindowPrivate
{
    RsttoImageList        *image_list;

    RsttoMimeDB           *db;

    GDBusProxy            *filemanager_proxy;

    guint                  show_fs_toolbar_timeout_id;
    guint                  hide_fs_mouse_cursor_timeout_id;
    guint                  window_save_geometry_timer_id;

    gboolean               fs_toolbar_sticky;

    RsttoImageListIter    *iter;

    GtkActionGroup        *action_group;
    GtkUIManager          *ui_manager;
    GtkRecentManager      *recent_manager;
    RsttoSettings         *settings_manager;
    RsttoWallpaperManager *wallpaper_manager;
    RsttoThumbnailer      *thumbnailer;

    GtkWidget             *menubar;
    GtkWidget             *toolbar;
    GtkWidget             *warning;
    GtkWidget             *warning_label;
    GtkWidget             *image_viewer_menu;
    GtkWidget             *position_menu;
    GtkWidget             *image_viewer;
    GtkWidget             *p_viewer_s_window;
    GtkWidget             *grid;
    GtkWidget             *t_bar_s_window;
    GtkWidget             *thumbnailbar;
    GtkWidget             *statusbar;
    guint                  statusbar_context_id;

    GtkWidget             *back;
    GtkWidget             *forward;

    guint                  t_open_merge_id;
    guint                  recent_merge_id;
    guint                  play_merge_id;
    guint                  pause_merge_id;
    guint                  toolbar_play_merge_id;
    guint                  toolbar_pause_merge_id;
    guint                  toolbar_fullscreen_merge_id;
    guint                  toolbar_unfullscreen_merge_id;

    GtkAction             *play_action;
    GtkAction             *pause_action;
    GtkAction             *recent_action;

    gboolean               playing;

    gchar                 *last_copy_folder_uri;
};



G_DEFINE_TYPE_WITH_PRIVATE (RsttoMainWindow, rstto_main_window, GTK_TYPE_WINDOW)



static void
filemanager_proxy_finish (GObject *source_object,
                          GAsyncResult *res,
                          gpointer data)
{
    RsttoMainWindow *window = data;
    window->priv->filemanager_proxy = g_dbus_proxy_new_finish (res, NULL);
}

static void
rstto_main_window_init (RsttoMainWindow *window)
{
    GtkAccelGroup   *accel_group;
    GtkWidget       *separator;
    GtkWidget       *main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    GtkRecentFilter *recent_filter;
    guint            window_width, window_height;
    gchar           *desktop_type = NULL;
    GtkWidget       *info_bar_content_area;

    GClosure        *toggle_fullscreen_closure = g_cclosure_new (G_CALLBACK (rstto_main_window_fullscreen), window, NULL);
    GClosure        *leave_fullscreen_closure = g_cclosure_new_swap (G_CALLBACK (gtk_window_unfullscreen), window, NULL);
    GClosure        *next_image_closure = g_cclosure_new (G_CALLBACK (cb_rstto_main_window_next_image), window, NULL);
    GClosure        *previous_image_closure = g_cclosure_new (G_CALLBACK (cb_rstto_main_window_previous_image), window, NULL);
    GClosure        *quit_closure = g_cclosure_new (G_CALLBACK (cb_rstto_main_window_quit), window, NULL);
    GClosure        *delete_closure = g_cclosure_new (G_CALLBACK (cb_rstto_main_window_delete), window, NULL);
    GClosure        *refresh_closure = g_cclosure_new (G_CALLBACK (cb_rstto_main_window_refresh), window, NULL);

    guint navigationbar_position = 3;
    guint thumbnail_size = 3;
    RsttoScale default_zoom = RSTTO_SCALE_NONE;
    gchar *db_path = NULL;

    /* an auto-reset pointer so that asynchronous jobs know the application state */
    app_window = window;
    g_signal_connect (window, "destroy", G_CALLBACK (gtk_widget_destroyed), &app_window);

    gtk_window_set_title (GTK_WINDOW (window), RISTRETTO_APP_TITLE);

    window->priv = rstto_main_window_get_instance_private (window);

    db_path = xfce_resource_save_location (XFCE_RESOURCE_DATA, "ristretto/mime.db", TRUE);
    if (db_path != NULL)
    {
        window->priv->db = rstto_mime_db_new (db_path, NULL);
        g_free (db_path);
    }

    window->priv->iter = NULL;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    window->priv->ui_manager = gtk_ui_manager_new ();
G_GNUC_END_IGNORE_DEPRECATIONS
    window->priv->recent_manager = gtk_recent_manager_get_default ();
    window->priv->settings_manager = rstto_settings_new ();
    window->priv->thumbnailer = rstto_thumbnailer_new ();

    window->priv->last_copy_folder_uri = NULL;

    /* setup the image filter list for the application */
    app_file_filter = g_object_ref_sink (gtk_file_filter_new ());
    gtk_file_filter_add_pixbuf_formats (app_file_filter);
    gtk_file_filter_set_name (app_file_filter, _("Images"));

    /* create a D-Bus proxy to a file manager but do not care about properties and signals,
     * so we have a reasonnable chance the call won't block in the background */
    g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                              G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                              G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                              NULL,
                              "org.freedesktop.FileManager1",
                              "/org/freedesktop/FileManager1",
                              "org.freedesktop.FileManager1",
                              NULL,
                              filemanager_proxy_finish,
                              window);

    desktop_type = rstto_settings_get_string_property (window->priv->settings_manager, "desktop-type");
    if (desktop_type)
    {
        if (!g_ascii_strcasecmp (desktop_type, "xfce"))
        {
            window->priv->wallpaper_manager = rstto_xfce_wallpaper_manager_new ();
        }

        if (!g_ascii_strcasecmp (desktop_type, "gnome"))
        {
            window->priv->wallpaper_manager = rstto_gnome_wallpaper_manager_new ();
        }

        if (!g_ascii_strcasecmp (desktop_type, "none"))
        {
            window->priv->wallpaper_manager = NULL;
        }

        g_free (desktop_type);
        desktop_type = NULL;
    }
    else
    {
        /* Default to xfce */
        window->priv->wallpaper_manager = rstto_xfce_wallpaper_manager_new ();
    }


    navigationbar_position = rstto_settings_get_navbar_position (window->priv->settings_manager);
    thumbnail_size = rstto_settings_get_uint_property (window->priv->settings_manager, "thumbnail-size");
    default_zoom = rstto_settings_get_int_property (window->priv->settings_manager, "default-zoom");

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    accel_group = gtk_ui_manager_get_accel_group (window->priv->ui_manager);
G_GNUC_END_IGNORE_DEPRECATIONS
    gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

    gtk_accel_group_connect_by_path (accel_group, "<Window>/fullscreen", toggle_fullscreen_closure);
    gtk_accel_group_connect_by_path (accel_group, "<Window>/unfullscreen", leave_fullscreen_closure);
    gtk_accel_group_connect_by_path (accel_group, "<Window>/next-image", next_image_closure);
    gtk_accel_group_connect_by_path (accel_group, "<Window>/previous-image", previous_image_closure);
    gtk_accel_group_connect_by_path (accel_group, "<Window>/quit", quit_closure);
    gtk_accel_group_connect_by_path (accel_group, "<Window>/delete", delete_closure);
    gtk_accel_group_connect_by_path (accel_group, "<Window>/refresh", refresh_closure);

    /* Set default accelerators */

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    /* Create mergeid's for adding ui-components */
    window->priv->recent_merge_id = gtk_ui_manager_new_merge_id (window->priv->ui_manager);
    window->priv->play_merge_id = gtk_ui_manager_new_merge_id (window->priv->ui_manager);
    window->priv->pause_merge_id = gtk_ui_manager_new_merge_id (window->priv->ui_manager);
    window->priv->toolbar_play_merge_id = gtk_ui_manager_new_merge_id (window->priv->ui_manager);
    window->priv->toolbar_pause_merge_id = gtk_ui_manager_new_merge_id (window->priv->ui_manager);
    window->priv->toolbar_fullscreen_merge_id = gtk_ui_manager_new_merge_id (window->priv->ui_manager);
    window->priv->toolbar_unfullscreen_merge_id = gtk_ui_manager_new_merge_id (window->priv->ui_manager);

    /* Create Play/Pause Slideshow actions */
    window->priv->play_action = gtk_action_new ("play", _("_Play"), _("Play slideshow"), "gtk-media-play");
    window->priv->pause_action = gtk_action_new ("pause", _("_Pause"), _("Pause slideshow"), "gtk-media-pause");

    /* Create Recently used items Action */
    window->priv->recent_action = gtk_recent_action_new_for_manager ("document-open-recent", _("_Recently used"), _("Recently used"),
                                                                     0, GTK_RECENT_MANAGER (window->priv->recent_manager));
G_GNUC_END_IGNORE_DEPRECATIONS

    gtk_recent_chooser_set_sort_type (GTK_RECENT_CHOOSER (window->priv->recent_action), GTK_RECENT_SORT_MRU);

    /*
     * Add a filter to the recent-chooser.
     * Specifying GTK_RECENT_FILTER_DISPLAY_NAME is needed because of a bug fixed in
     * GTK 3.24.31: see https://gitlab.gnome.org/GNOME/gtk/-/merge_requests/4080
     */
    recent_filter = gtk_recent_filter_new ();
    gtk_recent_filter_add_custom (recent_filter, GTK_RECENT_FILTER_URI
#if ! GTK_CHECK_VERSION (3, 24, 31)
                                    | GTK_RECENT_FILTER_DISPLAY_NAME
#endif
                                    | GTK_RECENT_FILTER_APPLICATION,
                                  rstto_main_window_recent_filter, window, NULL);
    gtk_recent_chooser_add_filter (GTK_RECENT_CHOOSER (window->priv->recent_action), recent_filter);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    /* Add the same accelerator path to play and pause, so the same kb-shortcut will be used for starting and stopping the slideshow */
    gtk_action_set_accel_path (window->priv->pause_action, "<Actions>/RsttoWindow/play");
    gtk_action_set_accel_path (window->priv->play_action, "<Actions>/RsttoWindow/play");

    /* Add the play and pause actions to the actiongroup */
    window->priv->action_group = gtk_action_group_new ("RsttoWindow");
    gtk_action_group_add_action (window->priv->action_group, window->priv->play_action);
    gtk_action_group_add_action (window->priv->action_group, window->priv->pause_action);
    gtk_action_group_add_action (window->priv->action_group, window->priv->recent_action);
G_GNUC_END_IGNORE_DEPRECATIONS

    /* Connect signal-handlers */
    g_signal_connect (window->priv->play_action, "activate",
                      G_CALLBACK (cb_rstto_main_window_play), window);
    g_signal_connect (window->priv->pause_action, "activate",
                      G_CALLBACK (cb_rstto_main_window_pause), window);
    g_signal_connect (window->priv->recent_action, "item-activated",
                      G_CALLBACK (cb_rstto_main_window_open_recent), window);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gtk_ui_manager_insert_action_group (window->priv->ui_manager, window->priv->action_group, 0);

    gtk_action_group_set_translation_domain (window->priv->action_group, GETTEXT_PACKAGE);
    gtk_action_group_add_actions (window->priv->action_group, action_entries,
                                  G_N_ELEMENTS (action_entries), window);
    gtk_action_group_add_toggle_actions (window->priv->action_group, toggle_action_entries,
                                         G_N_ELEMENTS (toggle_action_entries), window);
    gtk_action_group_add_radio_actions (window->priv->action_group, radio_action_sort_entries,
                                        G_N_ELEMENTS (radio_action_sort_entries), 0,
                                        G_CALLBACK (cb_rstto_main_window_sorting_function_changed), window);
    gtk_action_group_add_radio_actions (window->priv->action_group, radio_action_pos_entries,
                                        G_N_ELEMENTS (radio_action_pos_entries), navigationbar_position,
                                        G_CALLBACK (cb_rstto_main_window_navigationtoolbar_position_changed), window);
    gtk_action_group_add_radio_actions (window->priv->action_group, radio_action_size_entries,
                                        G_N_ELEMENTS (radio_action_size_entries), thumbnail_size,
                                        G_CALLBACK (cb_rstto_main_window_thumbnail_size_changed), window);
    gtk_action_group_add_radio_actions (window->priv->action_group, radio_action_default_zoom,
                                        G_N_ELEMENTS (radio_action_default_zoom), default_zoom,
                                        G_CALLBACK (cb_rstto_main_window_default_zoom), window);

    gtk_ui_manager_add_ui_from_string (window->priv->ui_manager, main_window_ui, main_window_ui_length, NULL);
    window->priv->menubar = gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-menu");
    window->priv->toolbar = gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-toolbar");
    window->priv->image_viewer_menu = gtk_ui_manager_get_widget (window->priv->ui_manager, "/image-viewer-menu");
    window->priv->position_menu = gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar-menu");
G_GNUC_END_IGNORE_DEPRECATIONS

    window->priv->warning = gtk_info_bar_new ();
    window->priv->warning_label = gtk_label_new (NULL);
    gtk_label_set_ellipsize (GTK_LABEL (window->priv->warning_label), PANGO_ELLIPSIZE_END);

    info_bar_content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (window->priv->warning));
    gtk_container_add (GTK_CONTAINER (info_bar_content_area), window->priv->warning_label);
    gtk_widget_show_all (info_bar_content_area);

    /**
     * Get the separator toolitem and tell it to expand
     */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    separator = gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-toolbar/separator-1");
G_GNUC_END_IGNORE_DEPRECATIONS
    gtk_tool_item_set_expand (GTK_TOOL_ITEM (separator), TRUE);
    gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (separator), FALSE);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    separator = gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-toolbar/separator-2");
G_GNUC_END_IGNORE_DEPRECATIONS
    gtk_tool_item_set_expand (GTK_TOOL_ITEM (separator), TRUE);
    gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (separator), FALSE);

    /**
     * Make the back and forward toolitems important,
     * when they are, the labels are shown when the toolbar style is 'both-horizontal'
     */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    window->priv->back = gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-toolbar/back");
    window->priv->forward = gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-toolbar/forward");
G_GNUC_END_IGNORE_DEPRECATIONS

    window->priv->image_viewer = rstto_image_viewer_new ();
    window->priv->p_viewer_s_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_overlay_scrolling (GTK_SCROLLED_WINDOW (window->priv->p_viewer_s_window), FALSE);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (window->priv->p_viewer_s_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (window->priv->p_viewer_s_window), window->priv->image_viewer);

    rstto_image_viewer_set_menu (RSTTO_IMAGE_VIEWER (window->priv->image_viewer), GTK_MENU (window->priv->image_viewer_menu));

    window->priv->t_bar_s_window = gtk_scrolled_window_new (NULL, NULL);
    window->priv->thumbnailbar = rstto_icon_bar_new (window->priv->t_bar_s_window);
    app_icon_bar = RSTTO_ICON_BAR (window->priv->thumbnailbar);
    gtk_container_add (GTK_CONTAINER (window->priv->t_bar_s_window), window->priv->thumbnailbar);

    rstto_icon_bar_set_show_text (RSTTO_ICON_BAR (window->priv->thumbnailbar), FALSE);

    g_signal_connect (window->priv->thumbnailbar, "selection-changed",
                      G_CALLBACK (cb_icon_bar_selection_changed), window);

    window->priv->grid = gtk_grid_new ();

    window->priv->statusbar = gtk_statusbar_new ();
    window->priv->statusbar_context_id = gtk_statusbar_get_context_id (GTK_STATUSBAR (window->priv->statusbar), "image-data");
    gtk_statusbar_push (GTK_STATUSBAR (window->priv->statusbar),
                        gtk_statusbar_get_context_id (GTK_STATUSBAR (window->priv->statusbar), "fallback-data"),
                        _("Press open to select an image"));

    gtk_container_add (GTK_CONTAINER (window), main_vbox);
    gtk_box_pack_start (GTK_BOX (main_vbox), window->priv->menubar, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (main_vbox), window->priv->toolbar, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (main_vbox), window->priv->warning, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (main_vbox), window->priv->grid, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (main_vbox), window->priv->statusbar, FALSE, FALSE, 0);

    gtk_grid_attach (GTK_GRID (window->priv->grid), window->priv->t_bar_s_window, 1, 0, 1, 5);
    gtk_grid_attach (GTK_GRID (window->priv->grid), window->priv->p_viewer_s_window, 2, 2, 1, 1);
    gtk_widget_set_hexpand (window->priv->p_viewer_s_window, TRUE);
    gtk_widget_set_vexpand (window->priv->p_viewer_s_window, TRUE);

    gtk_widget_set_no_show_all (window->priv->toolbar, TRUE);
    gtk_widget_set_no_show_all (window->priv->warning, TRUE);
    gtk_widget_set_no_show_all (window->priv->t_bar_s_window, TRUE);
    gtk_widget_set_no_show_all (window->priv->statusbar, TRUE);
    gtk_widget_show_all (window->priv->thumbnailbar);

    /* Make the statusbar smaller - by default, the margins are way too big */
    gtk_widget_set_margin_top (window->priv->statusbar, 1);
    gtk_widget_set_margin_bottom (window->priv->statusbar, 1);
    gtk_widget_set_margin_start (window->priv->statusbar, 2);
    gtk_widget_set_margin_end (window->priv->statusbar, 2);
    gtk_widget_set_margin_top (gtk_statusbar_get_message_area (GTK_STATUSBAR (window->priv->statusbar)), 0);
    gtk_widget_set_margin_bottom (gtk_statusbar_get_message_area (GTK_STATUSBAR (window->priv->statusbar)), 0);
    gtk_widget_set_margin_start (gtk_statusbar_get_message_area (GTK_STATUSBAR (window->priv->statusbar)), 0);
    gtk_widget_set_margin_end (gtk_statusbar_get_message_area (GTK_STATUSBAR (window->priv->statusbar)), 0);

    rstto_main_window_set_navigationbar_position (window, navigationbar_position);

    /**
     * Add missing pieces to the UI
     */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gtk_ui_manager_add_ui (window->priv->ui_manager,
                           window->priv->play_merge_id,
                           "/main-menu/go-menu/placeholder-slideshow",
                           "play",
                           "play",
                           GTK_UI_MANAGER_MENUITEM,
                           FALSE);
    gtk_ui_manager_add_ui (window->priv->ui_manager,
                           window->priv->recent_merge_id,
                           "/main-menu/file-menu/placeholder-open-recent",
                           "document-open-recent",
                           "document-open-recent",
                           GTK_UI_MANAGER_MENUITEM,
                           FALSE);
    gtk_ui_manager_add_ui (window->priv->ui_manager,
                           window->priv->toolbar_play_merge_id,
                           "/main-toolbar/placeholder-slideshow",
                           "play",
                           "play",
                           GTK_UI_MANAGER_TOOLITEM,
                           FALSE);
    gtk_ui_manager_add_ui (window->priv->ui_manager,
                           window->priv->toolbar_fullscreen_merge_id,
                           "/main-toolbar/placeholder-fullscreen",
                           "fullscreen",
                           "fullscreen",
                           GTK_UI_MANAGER_TOOLITEM,
                           FALSE);
G_GNUC_END_IGNORE_DEPRECATIONS

    /**
     * Retrieve the last window-size from the settings-manager
     * and make it the default for this window
     */
    window_width = rstto_settings_get_uint_property (RSTTO_SETTINGS (window->priv->settings_manager), "window-width");
    window_height = rstto_settings_get_uint_property (RSTTO_SETTINGS (window->priv->settings_manager), "window-height");
    gtk_window_set_default_size (GTK_WINDOW (window), window_width, window_height);

    /**
     * Retrieve the toolbar state from the settings-manager
     */
    if (rstto_settings_get_boolean_property (RSTTO_SETTINGS (window->priv->settings_manager), "show-toolbar"))
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        gtk_check_menu_item_set_active (
                GTK_CHECK_MENU_ITEM (
                        gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-menu/view-menu/show-toolbar")),
                TRUE);
G_GNUC_END_IGNORE_DEPRECATIONS
        gtk_widget_show (window->priv->toolbar);
    }
    else
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        gtk_check_menu_item_set_active (
                GTK_CHECK_MENU_ITEM (
                        gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-menu/view-menu/show-toolbar")),
                FALSE);
G_GNUC_END_IGNORE_DEPRECATIONS
        gtk_widget_hide (window->priv->toolbar);
    }

    if (rstto_settings_get_boolean_property (RSTTO_SETTINGS (window->priv->settings_manager), "show-thumbnailbar"))
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        gtk_check_menu_item_set_active (
                GTK_CHECK_MENU_ITEM (
                        gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-menu/view-menu/show-thumbnailbar")),
                TRUE);
G_GNUC_END_IGNORE_DEPRECATIONS
        gtk_widget_show (window->priv->t_bar_s_window);
    }
    else
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        gtk_check_menu_item_set_active (
                GTK_CHECK_MENU_ITEM (
                        gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-menu/view-menu/show-thumbnailbar")),
                FALSE);
G_GNUC_END_IGNORE_DEPRECATIONS
        gtk_widget_hide (window->priv->t_bar_s_window);
    }
    if (rstto_settings_get_boolean_property (RSTTO_SETTINGS (window->priv->settings_manager), "show-statusbar"))
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        gtk_check_menu_item_set_active (
                GTK_CHECK_MENU_ITEM (
                        gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-menu/view-menu/show-statusbar")),
                TRUE);
G_GNUC_END_IGNORE_DEPRECATIONS
        gtk_widget_show (window->priv->statusbar);
    }
    else
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        gtk_check_menu_item_set_active (
                GTK_CHECK_MENU_ITEM (
                        gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-menu/view-menu/show-statusbar")),
                FALSE);
G_GNUC_END_IGNORE_DEPRECATIONS
        gtk_widget_hide (window->priv->statusbar);
    }

    /**
     * Set sort-type
     */
    switch (rstto_settings_get_uint_property (window->priv->settings_manager, "sort-type"))
    {
        case SORT_TYPE_NAME:
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
            gtk_check_menu_item_set_active (
                    GTK_CHECK_MENU_ITEM (
                            gtk_ui_manager_get_widget (
                                    window->priv->ui_manager,
                                    "/main-menu/edit-menu/sorting-menu/sort-filename")),
                    TRUE);
G_GNUC_END_IGNORE_DEPRECATIONS
            break;
        case SORT_TYPE_TYPE:
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
            gtk_check_menu_item_set_active (
                    GTK_CHECK_MENU_ITEM (
                            gtk_ui_manager_get_widget (
                                    window->priv->ui_manager,
                                    "/main-menu/edit-menu/sorting-menu/sort-filetype")),
                    TRUE);
G_GNUC_END_IGNORE_DEPRECATIONS
            break;
        case SORT_TYPE_DATE:
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
            gtk_check_menu_item_set_active (
                    GTK_CHECK_MENU_ITEM (
                            gtk_ui_manager_get_widget (
                                    window->priv->ui_manager,
                                    "/main-menu/edit-menu/sorting-menu/sort-date")),
                    TRUE);
G_GNUC_END_IGNORE_DEPRECATIONS
            break;
        default:
            g_warning ("Sort type unsupported");
            break;
    }

    g_signal_connect (window, "motion-notify-event",
                      G_CALLBACK (cb_rstto_main_window_motion_notify_event), window);
    g_signal_connect (window->priv->image_viewer, "enter-notify-event",
                      G_CALLBACK (cb_rstto_main_window_image_viewer_enter_notify_event), window);
    g_signal_connect (window->priv->image_viewer, "scroll-event",
                      G_CALLBACK (cb_rstto_main_window_image_viewer_scroll_event), window);

    g_signal_connect (window, "configure-event",
                      G_CALLBACK (cb_rstto_main_window_configure_event), NULL);
    g_signal_connect (window, "window-state-event",
                      G_CALLBACK (cb_rstto_main_window_state_event), NULL);
    g_signal_connect (window->priv->thumbnailbar, "button-press-event",
                      G_CALLBACK (cb_rstto_main_window_navigationtoolbar_button_press_event), window);
    g_signal_connect (window->priv->image_viewer, "size-ready",
                      G_CALLBACK (cb_rstto_main_window_update_statusbar), window);
    g_signal_connect (window->priv->image_viewer, "scale-changed",
                      G_CALLBACK (cb_rstto_main_window_update_statusbar), window);
    g_signal_connect (window->priv->image_viewer, "status-changed",
                      G_CALLBACK (cb_rstto_main_window_update_statusbar), window);
    g_signal_connect (window->priv->image_viewer, "files-dnd",
                      G_CALLBACK (cb_rstto_main_window_dnd_files), window);

    g_signal_connect (window->priv->settings_manager, "notify::wrap-images",
                      G_CALLBACK (cb_rstto_wrap_images_changed), window);
    g_signal_connect (window->priv->settings_manager, "notify::desktop-type",
                      G_CALLBACK (cb_rstto_desktop_type_changed), window);

    g_signal_connect_swapped (window->priv->thumbnailer, "ready",
                              G_CALLBACK (cb_rstto_main_window_set_icon), window);
}

static void
rstto_main_window_class_init (RsttoMainWindowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->finalize = rstto_main_window_finalize;

    widget_class->key_press_event = key_press_event;
}

static void
rstto_main_window_finalize (GObject *object)
{
    RsttoMainWindow *window = RSTTO_MAIN_WINDOW (object);

    if (window->priv->ui_manager)
    {
        g_object_unref (window->priv->ui_manager);
        window->priv->ui_manager = NULL;
    }

    if (window->priv->settings_manager)
    {
        g_object_unref (window->priv->settings_manager);
        window->priv->settings_manager = NULL;
    }

    if (window->priv->image_list)
    {
        g_object_unref (window->priv->image_list);
        window->priv->image_list = NULL;
    }

    if (window->priv->iter)
    {
        g_object_unref (window->priv->iter);
        window->priv->iter = NULL;
    }

    if (window->priv->db)
    {
        g_object_unref (window->priv->db);
        window->priv->db = NULL;
    }

    if (window->priv->thumbnailer)
    {
        g_object_unref (window->priv->thumbnailer);
        window->priv->thumbnailer = NULL;
    }

    if (window->priv->last_copy_folder_uri)
    {
        g_free (window->priv->last_copy_folder_uri);
        window->priv->last_copy_folder_uri = NULL;
    }

    if (window->priv->action_group)
    {
        g_object_unref (window->priv->action_group);
        window->priv->action_group = NULL;
    }

    g_clear_object (&window->priv->filemanager_proxy);

    if (app_file_filter)
    {
        g_object_unref (app_file_filter);
        app_file_filter = NULL;
    }

    G_OBJECT_CLASS (rstto_main_window_parent_class)->finalize (object);
}

static gboolean
key_press_event (
        GtkWidget *widget,
        GdkEventKey *event)
{
    GtkWindow *window = GTK_WINDOW (widget);
    RsttoMainWindow *rstto_window = RSTTO_MAIN_WINDOW (widget);

    if (! gtk_window_activate_key (window, event))
    {
        switch (event->keyval)
        {
            case GDK_KEY_Up:
            case GDK_KEY_Left:
                rstto_main_window_select_valid_image (rstto_window, PREVIOUS);
                break;
            case GDK_KEY_Right:
            case GDK_KEY_Down:
                rstto_main_window_select_valid_image (rstto_window, NEXT);
                break;
            case GDK_KEY_Escape:
                if (rstto_window->priv->playing)
                {
                    cb_rstto_main_window_pause (GTK_WIDGET (window), rstto_window);
                }
                else
                {
                    if (! (gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (window))) & GDK_WINDOW_STATE_FULLSCREEN))
                    {
                        gtk_widget_destroy (GTK_WIDGET (window));
                    }
                }
                break;
        }
    }
    return TRUE;
}


RsttoMainWindow *
rstto_main_window_get_app_window (void)
{
    return app_window;
}

gboolean
rstto_main_window_get_app_exited (void)
{
    return app_window == NULL;
}

RsttoImageList *
rstto_main_window_get_app_image_list (void)
{
    return app_image_list;
}

RsttoIconBar *
rstto_main_window_get_app_icon_bar (void)
{
    return app_icon_bar;
}

GtkFileFilter *
rstto_main_window_get_app_file_filter (void)
{
    return app_file_filter;
}

/**
 * rstto_main_window_new:
 * @image_list:
 *
 * Return value:
 */
GtkWidget *
rstto_main_window_new (RsttoImageList *image_list, gboolean fullscreen)
{
    RsttoMainWindow *window;

    g_return_val_if_fail (RSTTO_IS_IMAGE_LIST (image_list), NULL);

    window = g_object_new (RSTTO_TYPE_MAIN_WINDOW, NULL);

    app_image_list = window->priv->image_list = g_object_ref (image_list);
    g_signal_connect_swapped (image_list, "row-deleted",
                              G_CALLBACK (cb_rstto_main_window_set_title), window);
    g_signal_connect_swapped (image_list, "row-inserted",
                              G_CALLBACK (cb_rstto_main_window_set_title), window);

    switch (rstto_settings_get_uint_property (window->priv->settings_manager, "sort-type"))
    {
        case SORT_TYPE_NAME:
            rstto_image_list_set_sort_by_name (window->priv->image_list);
            break;
        case SORT_TYPE_TYPE:
            rstto_image_list_set_sort_by_type (window->priv->image_list);
            break;
        case SORT_TYPE_DATE:
            rstto_image_list_set_sort_by_date (window->priv->image_list);
            break;
        default:
            g_warning ("Sort type unsupported");
            break;
    }

    window->priv->iter = rstto_image_list_get_iter (window->priv->image_list);
    g_signal_connect (window->priv->iter, "changed",
                      G_CALLBACK (cb_rstto_main_window_image_list_iter_changed), window);

    rstto_icon_bar_set_model (RSTTO_ICON_BAR (window->priv->thumbnailbar),
                              GTK_TREE_MODEL (window->priv->image_list));

    rstto_main_window_update_buttons (window);

    if (fullscreen)
    {
        gtk_window_fullscreen (GTK_WINDOW (window));
    }

    return GTK_WIDGET (window);
}

/**
 * rstto_main_window_image_list_iter_changed:
 * @window:
 *
 */
static void
rstto_main_window_image_list_iter_changed (RsttoMainWindow *window)
{
    RsttoFile       *cur_file = NULL;
    GList           *app_list, *iter;
    const gchar     *content_type;
    const gchar     *editor;
    const gchar     *id;
    GtkWidget       *menu_item = NULL;
    GDesktopAppInfo *app_info = NULL;

    GtkWidget *open_with_menu = gtk_menu_new ();
    GtkWidget *open_with_window_menu = gtk_menu_new ();
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (gtk_ui_manager_get_widget (window->priv->ui_manager, "/image-viewer-menu/open-with-menu")), open_with_menu);
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-menu/edit-menu/open-with-menu")), open_with_window_menu);
G_GNUC_END_IGNORE_DEPRECATIONS

    if (window->priv->image_list)
    {
        cb_rstto_main_window_set_title (window);

        cur_file = rstto_image_list_iter_get_file (window->priv->iter);
        cb_rstto_main_window_set_icon (window, cur_file);
        if (NULL != cur_file)
        {
            rstto_icon_bar_set_active (RSTTO_ICON_BAR (window->priv->thumbnailbar),
                                       rstto_image_list_iter_get_position (window->priv->iter));
            rstto_icon_bar_show_active (RSTTO_ICON_BAR (window->priv->thumbnailbar));
            content_type = rstto_file_get_content_type (cur_file);

            rstto_image_viewer_set_file (RSTTO_IMAGE_VIEWER (window->priv->image_viewer),
                                         cur_file, rstto_file_get_scale (cur_file),
                                         rstto_file_get_auto_scale (cur_file),
                                         rstto_file_get_orientation (cur_file));


            app_list = g_app_info_get_all_for_type (content_type);
            editor = rstto_mime_db_lookup (window->priv->db, content_type);

            if (editor)
            {
                app_info = g_desktop_app_info_new (editor);
                if (app_info != NULL)
                {
                    menu_item = rstto_app_menu_item_new (G_APP_INFO (app_info), rstto_file_get_file (cur_file));
                    gtk_menu_shell_append (GTK_MENU_SHELL (open_with_menu), menu_item);
                    menu_item = rstto_app_menu_item_new (G_APP_INFO (app_info), rstto_file_get_file (cur_file));
                    gtk_menu_shell_append (GTK_MENU_SHELL (open_with_window_menu), menu_item);

                    menu_item = gtk_separator_menu_item_new ();
                    gtk_menu_shell_append (GTK_MENU_SHELL (open_with_window_menu), menu_item);
                    menu_item = gtk_separator_menu_item_new ();
                    gtk_menu_shell_append (GTK_MENU_SHELL (open_with_menu), menu_item);
                }
            }

            if (NULL != app_list)
            {
                for (iter = app_list; iter; iter = g_list_next (iter))
                {
                    id = g_app_info_get_id (iter->data);
                    if (strcmp (id, RISTRETTO_DESKTOP_ID))
                    {
                        if ((!editor) || (editor && strcmp (id, editor)))
                        {
                            menu_item = rstto_app_menu_item_new (iter->data, rstto_file_get_file (cur_file));
                            gtk_menu_shell_append (GTK_MENU_SHELL (open_with_menu), menu_item);
                            menu_item = rstto_app_menu_item_new (iter->data, rstto_file_get_file (cur_file));
                            gtk_menu_shell_append (GTK_MENU_SHELL (open_with_window_menu), menu_item);
                        }
                    }
                }

                menu_item = gtk_separator_menu_item_new ();
                gtk_menu_shell_append (GTK_MENU_SHELL (open_with_window_menu), menu_item);
                menu_item = gtk_separator_menu_item_new ();
                gtk_menu_shell_append (GTK_MENU_SHELL (open_with_menu), menu_item);

                g_list_free_full (app_list, g_object_unref);
            }
            else
            {
            }

            menu_item = gtk_menu_item_new_with_mnemonic (_("Open With Other _Application..."));
            gtk_menu_shell_append (GTK_MENU_SHELL (open_with_menu), menu_item);
            g_signal_connect (menu_item, "activate",
                              G_CALLBACK (cb_rstto_main_window_open_with_other_app), window);

            menu_item = gtk_menu_item_new_with_mnemonic (_("Open With Other _Application..."));
            gtk_menu_shell_append (GTK_MENU_SHELL (open_with_window_menu), menu_item);
            g_signal_connect (menu_item, "activate",
                              G_CALLBACK (cb_rstto_main_window_open_with_other_app), window);

            gtk_widget_show_all (open_with_menu);
            gtk_widget_show_all (open_with_window_menu);
        }
        else
        {
            menu_item = gtk_menu_item_new_with_label (_("Empty"));
            gtk_menu_shell_append (GTK_MENU_SHELL (open_with_menu), menu_item);
            gtk_widget_set_sensitive (menu_item, FALSE);

            rstto_image_viewer_set_file (RSTTO_IMAGE_VIEWER (window->priv->image_viewer),
                                         NULL, RSTTO_SCALE_NONE, RSTTO_SCALE_NONE,
                                         RSTTO_IMAGE_ORIENT_NONE);

            menu_item = gtk_menu_item_new_with_label (_("Empty"));
            gtk_menu_shell_append (GTK_MENU_SHELL (open_with_window_menu), menu_item);
            gtk_widget_set_sensitive (menu_item, FALSE);

            gtk_widget_show_all (open_with_menu);
            gtk_widget_show_all (open_with_window_menu);
        }

        rstto_main_window_update_buttons (window);
        rstto_main_window_update_statusbar (window);
    }
}

/**
 * rstto_main_window_update_statusbar:
 * @window:
 *
 */
static void
rstto_main_window_update_statusbar (RsttoMainWindow *window)
{
    ExifEntry *exif_entry;
    RsttoImageViewer *viewer = RSTTO_IMAGE_VIEWER (window->priv->image_viewer);
    RsttoFile *cur_file;
    GError *error = NULL;
    gchar *status, *tmp_status;
    gchar exif_data[20];
    gint width, height;

    if (window->priv->image_list == NULL)
        return;

    cur_file = rstto_image_list_iter_get_file (window->priv->iter);
    if (cur_file == NULL)
    {
        status = g_strdup (_("Press open to select an image"));
    }
    else if (rstto_image_viewer_is_busy (viewer) ||
             rstto_image_list_is_busy (window->priv->image_list))
    {
        status = g_strdup (_("Loading..."));
    }
    else
    {
        status = g_strdup (rstto_file_get_display_name (cur_file));

        if (rstto_file_has_exif (cur_file))
        {
            /* Extend the status-message with exif-info */
            /********************************************/
            exif_entry = rstto_file_get_exif (cur_file, EXIF_TAG_FNUMBER);
            if (exif_entry)
            {
                exif_entry_get_value (exif_entry, exif_data, sizeof (exif_data));
                tmp_status = g_strdup_printf ("%s\t%s", status, exif_data);
                g_free (status);
                status = tmp_status;
            }

            exif_entry = rstto_file_get_exif (cur_file, EXIF_TAG_EXPOSURE_TIME);
            if (exif_entry)
            {
                exif_entry_get_value (exif_entry, exif_data, sizeof (exif_data));
                tmp_status = g_strdup_printf ("%s\t%s", status, exif_data);
                g_free (status);
                status = tmp_status;
            }

            exif_entry = rstto_file_get_exif (cur_file, EXIF_TAG_FOCAL_LENGTH);
            if (exif_entry)
            {
                exif_entry_get_value (exif_entry, exif_data, sizeof (exif_data));
                tmp_status = g_strdup_printf ("%s\t%s", status, exif_data);
                g_free (status);
                status = tmp_status;
            }

            exif_entry = rstto_file_get_exif (cur_file, EXIF_TAG_ISO_SPEED_RATINGS);
            if (exif_entry)
            {
                exif_entry_get_value (exif_entry, exif_data, sizeof (exif_data));
                tmp_status = g_strdup_printf ("%s\tISO %s", status, exif_data);
                g_free (status);
                status = tmp_status;
            }
        }

        width = rstto_image_viewer_get_width (viewer);
        height = rstto_image_viewer_get_height (viewer);
        if (width > 0)
        {
            gchar *size_string = g_format_size (rstto_file_get_size (cur_file));
            tmp_status = g_strdup_printf ("%s\t%d x %d\t%s\t%.1f%%",
                                          status, width, height, size_string,
                                          100 * rstto_image_viewer_get_scale (viewer));
            g_free (size_string);
            g_free (status);
            status = tmp_status;
        }

        error = rstto_image_viewer_get_error (viewer);
        if (error != NULL)
        {
            if (width == 0)
            {
                gtk_label_set_text (GTK_LABEL (window->priv->warning_label), error->message);
                gtk_widget_set_tooltip_text (
                        window->priv->warning_label,
                        error->message);
                gtk_widget_show (window->priv->warning);
            }
            else
                gtk_widget_hide (window->priv->warning);

            tmp_status = g_strdup_printf ("%s  -  %s", status, error->message);
            g_free (status);
            status = tmp_status;
            g_error_free (error);
        }
        else
            gtk_widget_hide (window->priv->warning);
    }

    gtk_statusbar_pop (GTK_STATUSBAR (window->priv->statusbar),
                       window->priv->statusbar_context_id);
    gtk_statusbar_push (GTK_STATUSBAR (window->priv->statusbar),
                        window->priv->statusbar_context_id, status);
    g_free (status);
}

static void
rstto_main_activate_file_menu_actions (RsttoMainWindow *window, gboolean activate)
{
    GtkWidget   *widget;
    guint        i;
    const gchar *actions[] = {
        "/main-menu/file-menu/save-copy",
        //"/main-menu/file-menu/print",
        "/main-menu/file-menu/properties",
        "/main-menu/file-menu/close",
        "/main-menu/edit-menu/copy-image",
        "/main-menu/edit-menu/delete"
    };

    for (i = 0; i < G_N_ELEMENTS (actions); ++i)
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        widget = gtk_ui_manager_get_widget (window->priv->ui_manager, actions[i]);
G_GNUC_END_IGNORE_DEPRECATIONS
        gtk_widget_set_sensitive (widget, activate);
    }
}

static void
rstto_main_activate_go_menu_actions (RsttoMainWindow *window, gboolean activate)
{
    GtkWidget   *widget;
    guint        i;
    const gchar *actions[] = {
        "/main-menu/go-menu/forward",
        "/main-menu/go-menu/back",
        "/main-menu/go-menu/first",
        "/main-menu/go-menu/last"
    };

    for (i = 0; i < G_N_ELEMENTS (actions); ++i)
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        widget = gtk_ui_manager_get_widget (window->priv->ui_manager, actions[i]);
G_GNUC_END_IGNORE_DEPRECATIONS
        gtk_widget_set_sensitive (widget, activate);
    }
}

static void
rstto_main_activate_view_menu_actions (RsttoMainWindow *window, gboolean activate)
{
    GtkWidget   *widget;
    guint        i;
    const gchar *actions[] = {
        "/main-menu/view-menu/set-as-wallpaper",
        "/main-menu/view-menu/zoom-menu",
        "/main-menu/view-menu/rotation-menu",
        "/main-menu/view-menu/flip-menu"
    };

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    widget = gtk_ui_manager_get_widget (window->priv->ui_manager, actions[0]);
G_GNUC_END_IGNORE_DEPRECATIONS
    gtk_widget_set_sensitive (widget, (activate && window->priv->wallpaper_manager) ? TRUE : FALSE);
    for (i = 1; i < G_N_ELEMENTS (actions); ++i)
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        widget = gtk_ui_manager_get_widget (window->priv->ui_manager, actions[i]);
G_GNUC_END_IGNORE_DEPRECATIONS
        gtk_widget_set_sensitive (widget, activate);
    }
}

static void
rstto_main_activate_popup_menu_actions (RsttoMainWindow *window, gboolean activate)
{
    GtkWidget   *widget;
    guint        i;
    const gchar *actions[] = {
        "/image-viewer-menu/close",
        "/image-viewer-menu/open-with-menu",
        "/image-viewer-menu/copy-image",
        "/image-viewer-menu/zoom-in",
        "/image-viewer-menu/zoom-out",
        "/image-viewer-menu/zoom-100",
        "/image-viewer-menu/zoom-fit"
    };

    for (i = 0; i < G_N_ELEMENTS (actions); ++i)
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        widget = gtk_ui_manager_get_widget (window->priv->ui_manager, actions[i]);
G_GNUC_END_IGNORE_DEPRECATIONS
        gtk_widget_set_sensitive (widget, activate);
    }
}

static void
rstto_main_activate_toolbar_actions (RsttoMainWindow *window, gboolean activate)
{
    GtkWidget   *widget;
    guint        i;
    const gchar *actions[] = {
        "/main-toolbar/save-copy",
        "/main-toolbar/edit",
        "/main-toolbar/delete",
        "/main-toolbar/zoom-in",
        "/main-toolbar/zoom-out",
        "/main-toolbar/zoom-fit",
        "/main-toolbar/zoom-100"
    };

    for (i = 0; i < G_N_ELEMENTS (actions); ++i)
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        widget = gtk_ui_manager_get_widget (window->priv->ui_manager, actions[i]);
G_GNUC_END_IGNORE_DEPRECATIONS
        gtk_widget_set_sensitive (widget, activate);
    }
}

static void
rstto_main_window_toolbar_remove_shadow (GtkWidget *widget,
                                         gpointer data)
{
    GtkStyleContext *context;
    GtkCssProvider *provider;

    if (GTK_IS_BUTTON (widget))
    {
        provider = gtk_css_provider_new ();
        context = gtk_widget_get_style_context (widget);
        gtk_css_provider_load_from_data (provider, "button { box-shadow: none; }", -1, NULL);
        gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (provider),
                                        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref (provider);
    }
    else if (GTK_IS_CONTAINER (widget))
        gtk_container_foreach (GTK_CONTAINER (widget),
                               rstto_main_window_toolbar_remove_shadow, NULL);
}

/**
 * rstto_main_window_update_buttons:
 * @window:
 * @sensitive:
 *
 */
static void
rstto_main_window_update_buttons (RsttoMainWindow *window)
{
    g_return_if_fail (window->priv->image_list != NULL);
    switch (rstto_image_list_get_n_images (window->priv->image_list))
    {
        case 0:
            if (gtk_widget_get_visible (GTK_WIDGET (window)))
            {
                if (0 != (gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (window))) & GDK_WINDOW_STATE_FULLSCREEN))
                {
                    gtk_widget_show (window->priv->toolbar);
                }
            }

            gtk_widget_hide (window->priv->t_bar_s_window);
            rstto_main_activate_file_menu_actions (window, FALSE);
            rstto_main_activate_go_menu_actions (window, FALSE);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
            gtk_action_set_sensitive (window->priv->play_action, FALSE);
            gtk_action_set_sensitive (window->priv->pause_action, FALSE);
G_GNUC_END_IGNORE_DEPRECATIONS
            gtk_widget_set_sensitive (window->priv->forward, FALSE);
            gtk_widget_set_sensitive (window->priv->back, FALSE);

            /* Stop the slideshow if no image is opened */
            if (window->priv->playing)
            {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                gtk_ui_manager_add_ui (
                        window->priv->ui_manager,
                        window->priv->play_merge_id,
                        "/main-menu/go-menu/placeholder-slideshow",
                        "play",
                        "play",
                        GTK_UI_MANAGER_MENUITEM,
                        FALSE);
                gtk_ui_manager_remove_ui (window->priv->ui_manager, window->priv->pause_merge_id);

                gtk_ui_manager_add_ui (
                        window->priv->ui_manager,
                        window->priv->toolbar_play_merge_id,
                        "/main-toolbar/placeholder-slideshow",
                        "play",
                        "play",
                        GTK_UI_MANAGER_TOOLITEM,
                        FALSE);
                gtk_ui_manager_remove_ui (window->priv->ui_manager, window->priv->toolbar_pause_merge_id);
G_GNUC_END_IGNORE_DEPRECATIONS

                window->priv->playing = FALSE;
            }

            rstto_main_activate_view_menu_actions (window, FALSE);
            rstto_main_activate_popup_menu_actions (window, FALSE);
            rstto_main_activate_toolbar_actions (window, FALSE);
            break;
        case 1:
            if (rstto_settings_get_boolean_property (window->priv->settings_manager, "show-thumbnailbar"))
            {
                if (0 == (gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (window))) & GDK_WINDOW_STATE_FULLSCREEN))
                {
                    gtk_widget_show (window->priv->t_bar_s_window);
                }
                else
                {
                    if (rstto_settings_get_boolean_property (
                            window->priv->settings_manager,
                            "hide-thumbnails-fullscreen"))
                    {
                        gtk_widget_hide (window->priv->t_bar_s_window);
                    }
                    else
                    {
                        gtk_widget_show (window->priv->t_bar_s_window);
                    }
                }
            }
            if (0 != (gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (window))) & GDK_WINDOW_STATE_FULLSCREEN))
            {
                gtk_widget_hide (window->priv->toolbar);
            }

            rstto_main_activate_file_menu_actions (window, TRUE);
            rstto_main_activate_go_menu_actions (window, FALSE);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
            gtk_action_set_sensitive (window->priv->play_action, FALSE);
            gtk_action_set_sensitive (window->priv->pause_action, FALSE);
G_GNUC_END_IGNORE_DEPRECATIONS
            gtk_widget_set_sensitive (window->priv->forward, FALSE);
            gtk_widget_set_sensitive (window->priv->back, FALSE);

            /* Stop the slideshow if only one image is opened */
            if (window->priv->playing)
            {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                gtk_ui_manager_add_ui (
                        window->priv->ui_manager,
                        window->priv->play_merge_id,
                        "/main-menu/go-menu/placeholder-slideshow",
                        "play",
                        "play",
                        GTK_UI_MANAGER_MENUITEM,
                        FALSE);
                gtk_ui_manager_remove_ui (
                        window->priv->ui_manager,
                        window->priv->pause_merge_id);

                gtk_ui_manager_add_ui (
                        window->priv->ui_manager,
                        window->priv->toolbar_play_merge_id,
                        "/main-toolbar/placeholder-slideshow",
                        "play",
                        "play",
                        GTK_UI_MANAGER_TOOLITEM,
                        FALSE);

                gtk_ui_manager_remove_ui (
                        window->priv->ui_manager,
                        window->priv->toolbar_pause_merge_id);
G_GNUC_END_IGNORE_DEPRECATIONS

                window->priv->playing = FALSE;
            }

            rstto_main_activate_view_menu_actions (window, TRUE);
            rstto_main_activate_popup_menu_actions (window, TRUE);
            rstto_main_activate_toolbar_actions (window, TRUE);
            break;
        default:
            if (rstto_settings_get_boolean_property (window->priv->settings_manager, "show-thumbnailbar"))
            {
                if (0 == (gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (window))) & GDK_WINDOW_STATE_FULLSCREEN))
                {
                    gtk_widget_show (window->priv->t_bar_s_window);
                }
                else
                {
                    if (rstto_settings_get_boolean_property (
                            window->priv->settings_manager,
                            "hide-thumbnails-fullscreen"))
                    {
                        gtk_widget_hide (window->priv->t_bar_s_window);
                    }
                    else
                    {
                        gtk_widget_show (window->priv->t_bar_s_window);
                    }
                }
            }
            if (0 != (gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (window))) & GDK_WINDOW_STATE_FULLSCREEN))
            {
                gtk_widget_hide (window->priv->toolbar);
            }

            rstto_main_activate_file_menu_actions (window, TRUE);
            rstto_main_activate_go_menu_actions (window, TRUE);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
            gtk_action_set_sensitive (window->priv->play_action, TRUE);
            gtk_action_set_sensitive (window->priv->pause_action, TRUE);
G_GNUC_END_IGNORE_DEPRECATIONS
            gtk_widget_set_sensitive (window->priv->forward, rstto_image_list_iter_has_next (window->priv->iter) ? TRUE : FALSE);
            gtk_widget_set_sensitive (window->priv->back, rstto_image_list_iter_has_previous (window->priv->iter) ? TRUE : FALSE);

            rstto_main_activate_view_menu_actions (window, TRUE);
            rstto_main_activate_popup_menu_actions (window, TRUE);
            rstto_main_activate_toolbar_actions (window, TRUE);
            break;
    }

    if (window->priv->playing)
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        gtk_ui_manager_remove_ui (
                window->priv->ui_manager,
                window->priv->toolbar_play_merge_id);
        gtk_ui_manager_add_ui (
                window->priv->ui_manager,
                window->priv->toolbar_pause_merge_id,
                "/main-toolbar/placeholder-slideshow",
                "pause",
                "pause",
                GTK_UI_MANAGER_TOOLITEM,
                FALSE);
G_GNUC_END_IGNORE_DEPRECATIONS
    }
    else
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        gtk_ui_manager_remove_ui (
                window->priv->ui_manager,
                window->priv->toolbar_pause_merge_id);
        gtk_ui_manager_add_ui (
                window->priv->ui_manager,
                window->priv->toolbar_play_merge_id,
                "/main-toolbar/placeholder-slideshow",
                "play",
                "play",
                GTK_UI_MANAGER_TOOLITEM,
                FALSE);
G_GNUC_END_IGNORE_DEPRECATIONS
    }

    if (gtk_widget_get_visible (GTK_WIDGET (window)))
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        gtk_ui_manager_remove_ui (
            window->priv->ui_manager,
            window->priv->toolbar_unfullscreen_merge_id);
        gtk_ui_manager_remove_ui (
            window->priv->ui_manager,
            window->priv->toolbar_fullscreen_merge_id);
        /* Do not make the widget visible when in
         * fullscreen mode.
         */
        if (0 == (gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (window))) & GDK_WINDOW_STATE_FULLSCREEN))
        {
            gtk_ui_manager_add_ui (window->priv->ui_manager,
                                   window->priv->toolbar_fullscreen_merge_id,
                                   "/main-toolbar/placeholder-fullscreen",
                                   "fullscreen",
                                   "fullscreen",
                                   GTK_UI_MANAGER_TOOLITEM,
                                   FALSE);
        }
        else
        {
            gtk_ui_manager_add_ui (window->priv->ui_manager,
                                   window->priv->toolbar_unfullscreen_merge_id,
                                   "/main-toolbar/placeholder-fullscreen",
                                   "unfullscreen",
                                   "unfullscreen",
                                   GTK_UI_MANAGER_TOOLITEM,
                                   FALSE);
        }
G_GNUC_END_IGNORE_DEPRECATIONS
    }

    /* remove the shadow of the toolbar buttons so that it does not drool on
     * the image viewer, triggering its redraw (gtk_container_forall() is used
     * to reach also the overflow menu arrow) */
    gtk_container_forall (GTK_CONTAINER (window->priv->toolbar),
                          rstto_main_window_toolbar_remove_shadow, NULL);
}

static void
rstto_main_window_set_thumbnail_size (
        RsttoMainWindow *window,
        RsttoThumbnailSize size)
{
    rstto_settings_set_uint_property (
            window->priv->settings_manager,
            "thumbnail-size",
            size);
}

static void
rstto_main_window_set_navigationbar_position (RsttoMainWindow *window, guint orientation)
{
    rstto_settings_set_navbar_position (window->priv->settings_manager, orientation);

    switch (orientation)
    {
        case 0: /* Left */
            g_object_ref (window->priv->t_bar_s_window);

            gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (window->priv->t_bar_s_window)), window->priv->t_bar_s_window);
            gtk_grid_attach (GTK_GRID (window->priv->grid), window->priv->t_bar_s_window, 1, 0, 1, 5);

            gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (window->priv->t_bar_s_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
            gtk_scrolled_window_set_placement (GTK_SCROLLED_WINDOW (window->priv->t_bar_s_window), GTK_CORNER_BOTTOM_RIGHT);
            rstto_icon_bar_set_orientation (RSTTO_ICON_BAR (window->priv->thumbnailbar), GTK_ORIENTATION_VERTICAL);
            break;
        case 1: /* Right */
            g_object_ref (window->priv->t_bar_s_window);

            gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (window->priv->t_bar_s_window)), window->priv->t_bar_s_window);
            gtk_grid_attach (GTK_GRID (window->priv->grid), window->priv->t_bar_s_window, 3, 0, 1, 5);

            gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (window->priv->t_bar_s_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
            gtk_scrolled_window_set_placement (GTK_SCROLLED_WINDOW (window->priv->t_bar_s_window), GTK_CORNER_BOTTOM_LEFT);
            rstto_icon_bar_set_orientation (RSTTO_ICON_BAR (window->priv->thumbnailbar), GTK_ORIENTATION_VERTICAL);
            break;
        case 2: /* Top */
            g_object_ref (window->priv->t_bar_s_window);

            gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (window->priv->t_bar_s_window)), window->priv->t_bar_s_window);
            gtk_grid_attach (GTK_GRID (window->priv->grid), window->priv->t_bar_s_window, 0, 1, 5, 1);

            gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (window->priv->t_bar_s_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
            gtk_scrolled_window_set_placement (GTK_SCROLLED_WINDOW (window->priv->t_bar_s_window), GTK_CORNER_BOTTOM_RIGHT);
            rstto_icon_bar_set_orientation (RSTTO_ICON_BAR (window->priv->thumbnailbar), GTK_ORIENTATION_HORIZONTAL);
            break;
        case 3: /* Bottom */
            g_object_ref (window->priv->t_bar_s_window);

            gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (window->priv->t_bar_s_window)), window->priv->t_bar_s_window);
            gtk_grid_attach (GTK_GRID (window->priv->grid), window->priv->t_bar_s_window, 0, 3, 5, 1);

            gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (window->priv->t_bar_s_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
            gtk_scrolled_window_set_placement (GTK_SCROLLED_WINDOW (window->priv->t_bar_s_window), GTK_CORNER_TOP_RIGHT);
            rstto_icon_bar_set_orientation (RSTTO_ICON_BAR (window->priv->thumbnailbar), GTK_ORIENTATION_HORIZONTAL);
            break;
        default:
            break;
    }
}


/************************/
/**                    **/
/** Callback functions **/
/**                    **/
/************************/

static gboolean
cb_rstto_main_window_navigationtoolbar_button_press_event (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    RsttoMainWindow *window = user_data;

    if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
    {
        gtk_menu_popup_at_pointer (GTK_MENU (window->priv->position_menu), NULL);
    }

    return FALSE;
}

static void
cb_rstto_main_window_image_list_iter_changed (RsttoImageListIter *iter, RsttoMainWindow *window)
{
    rstto_main_window_image_list_iter_changed (window);
}

static void
cb_rstto_main_window_sorting_function_changed (GtkRadioAction *action, GtkRadioAction *current, RsttoMainWindow *window)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gint value = gtk_radio_action_get_current_value (current);
G_GNUC_END_IGNORE_DEPRECATIONS
    switch (value)
    {
        case SORT_TYPE_NAME:
        default:
            if (window->priv->image_list != NULL)
            {
                rstto_image_list_set_sort_by_name (window->priv->image_list);
                rstto_settings_set_uint_property (window->priv->settings_manager, "sort-type", SORT_TYPE_NAME);
            }
            break;
        case SORT_TYPE_TYPE:
            if (window->priv->image_list != NULL)
            {
                rstto_image_list_set_sort_by_type (window->priv->image_list);
                rstto_settings_set_uint_property (window->priv->settings_manager, "sort-type", SORT_TYPE_TYPE);
            }
            break;
        case SORT_TYPE_DATE:
            if (window->priv->image_list != NULL)
            {
                rstto_image_list_set_sort_by_date (window->priv->image_list);
                rstto_settings_set_uint_property (window->priv->settings_manager, "sort-type", SORT_TYPE_DATE);
            }
            break;
    }
}

static void
cb_rstto_main_window_navigationtoolbar_position_changed (GtkRadioAction *action, GtkRadioAction *current, RsttoMainWindow *window)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    rstto_main_window_set_navigationbar_position (window, gtk_radio_action_get_current_value (current));
G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
cb_rstto_main_window_thumbnail_size_changed (
        GtkRadioAction *action,
        GtkRadioAction *current,
        RsttoMainWindow *window)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    rstto_main_window_set_thumbnail_size (window, gtk_radio_action_get_current_value (current));
G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
cb_rstto_main_window_set_as_wallpaper (GtkWidget *widget, RsttoMainWindow *window)
{
    gint response = GTK_RESPONSE_APPLY;
    RsttoFile *file = NULL;
    gchar *desktop_type = NULL;
    GtkWidget *dialog = NULL;
    GtkWidget *content_area = NULL;
    GtkWidget *behaviour_desktop_lbl;
    GtkWidget *choose_desktop_combo_box;
    GtkWidget *button;

    if (window->priv->iter)
    {
        file = rstto_image_list_iter_get_file (window->priv->iter);
    }

    g_return_if_fail (NULL != file);

    desktop_type = rstto_settings_get_string_property (window->priv->settings_manager, "desktop-type");
    if (G_UNLIKELY (NULL == desktop_type))
    {
        /* No desktop has been selected, first time this feature is
         * used. -- Ask the user which method he wants ristretto to
         * apply to set the desktop wallpaper.
         */
        dialog = gtk_dialog_new ();
        gtk_window_set_title (GTK_WINDOW (dialog), _("Choose 'set wallpaper' method"));
        gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
        gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

        button = xfce_gtk_button_new_mixed ("gtk-cancel", _("_Cancel"));
        gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_CANCEL);
        gtk_widget_show (button);
        button = xfce_gtk_button_new_mixed ("gtk-ok", _("_OK"));
        gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);
        gtk_widget_show (button);

        /* Populate the dialog */
        content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

        behaviour_desktop_lbl = gtk_label_new (NULL);
        gtk_label_set_markup (
                GTK_LABEL (behaviour_desktop_lbl),
                _("Configure which system is currently managing your desktop.\n"
                  "This setting determines the method <i>Ristretto</i> will use\n"
                  "to configure the desktop wallpaper."));
        gtk_label_set_xalign (
                GTK_LABEL (behaviour_desktop_lbl),
                0.0);
        gtk_label_set_yalign (
                GTK_LABEL (behaviour_desktop_lbl),
                0.5);
        gtk_box_pack_start (
                GTK_BOX (content_area),
                behaviour_desktop_lbl,
                FALSE,
                FALSE,
                0);

        choose_desktop_combo_box = gtk_combo_box_text_new ();
        gtk_box_pack_start (
                GTK_BOX (content_area),
                choose_desktop_combo_box,
                FALSE,
                FALSE,
                0);
        gtk_combo_box_text_insert_text (
                GTK_COMBO_BOX_TEXT (choose_desktop_combo_box),
                DESKTOP_TYPE_NONE,
                _("None"));
        gtk_combo_box_text_insert_text (
                GTK_COMBO_BOX_TEXT (choose_desktop_combo_box),
                DESKTOP_TYPE_XFCE,
                _("Xfce"));
        gtk_combo_box_text_insert_text (
                GTK_COMBO_BOX_TEXT (choose_desktop_combo_box),
                DESKTOP_TYPE_GNOME,
                _("GNOME"));

        gtk_combo_box_set_active (
            GTK_COMBO_BOX (choose_desktop_combo_box),
            DESKTOP_TYPE_XFCE);

        gtk_widget_show_all (content_area);

        /* Show the dialog */
        response = gtk_dialog_run (GTK_DIALOG (dialog));

        /* If the response was 'OK', the user has made a choice */
        if (GTK_RESPONSE_OK == response)
        {
            switch (gtk_combo_box_get_active (
                    GTK_COMBO_BOX (choose_desktop_combo_box)))
            {
                case DESKTOP_TYPE_NONE:
                    desktop_type = g_strdup ("none");
                    if (NULL != window->priv->wallpaper_manager)
                    {
                        g_object_unref (window->priv->wallpaper_manager);
                        window->priv->wallpaper_manager = NULL;
                    }
                    break;
                case DESKTOP_TYPE_XFCE:
                    desktop_type = g_strdup ("xfce");
                    if (NULL != window->priv->wallpaper_manager)
                    {
                        g_object_unref (window->priv->wallpaper_manager);
                    }
                    window->priv->wallpaper_manager = rstto_xfce_wallpaper_manager_new ();
                    break;
                case DESKTOP_TYPE_GNOME:
                    desktop_type = g_strdup ("gnome");
                    if (NULL != window->priv->wallpaper_manager)
                    {
                        g_object_unref (window->priv->wallpaper_manager);
                    }
                    window->priv->wallpaper_manager = rstto_gnome_wallpaper_manager_new ();
                    break;
            }
        rstto_settings_set_string_property (
            window->priv->settings_manager,
            "desktop-type",
            desktop_type);
        }

        /* Clean-up the dialog */
        gtk_widget_destroy (dialog);
        dialog = NULL;
    }

    if (NULL != desktop_type && NULL != window->priv->wallpaper_manager)
    {
        /* Set the response to GTK_RESPONSE_APPLY,
         * so we at least do one run.
         */
        response = GTK_RESPONSE_APPLY;
        while (GTK_RESPONSE_APPLY == response)
        {
            response = rstto_wallpaper_manager_configure_dialog_run (window->priv->wallpaper_manager, file, GTK_WINDOW (window));
            switch (response)
            {
                case GTK_RESPONSE_OK:
                case GTK_RESPONSE_APPLY:
                    rstto_wallpaper_manager_set (window->priv->wallpaper_manager, file);
                    break;
            }
        }
    }

    if (G_LIKELY (NULL != desktop_type))
    {
        g_free (desktop_type);
        desktop_type = NULL;
    }
}

static gboolean
cb_rstto_main_window_state_event (GtkWidget *widget, GdkEventWindowState *event, gpointer user_data)
{
    RsttoMainWindow *window = RSTTO_MAIN_WINDOW (widget);

    if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
    {
        if (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN)
        {
            guint timeout = rstto_settings_get_uint_property (RSTTO_SETTINGS (window->priv->settings_manager),
                                                              "hide-mouse-cursor-fullscreen-timeout");

            gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (window->priv->p_viewer_s_window),
                                            GTK_POLICY_NEVER, GTK_POLICY_NEVER);
            rstto_image_viewer_set_show_clock (
                    RSTTO_IMAGE_VIEWER (window->priv->image_viewer),
                    rstto_settings_get_boolean_property (window->priv->settings_manager, "show-clock"));

            gtk_widget_hide (window->priv->menubar);
            if (! rstto_image_list_is_empty (window->priv->image_list))
            {
                gtk_widget_hide (window->priv->toolbar);
            }
            else
            {
                gtk_widget_show (window->priv->toolbar);
            }
            gtk_widget_hide (window->priv->statusbar);

            if (window->priv->fs_toolbar_sticky)
            {
                if (window->priv->show_fs_toolbar_timeout_id > 0)
                {
                    REMOVE_SOURCE (window->priv->show_fs_toolbar_timeout_id);
                }
                if (! rstto_image_list_is_empty (window->priv->image_list))
                {
                    window->priv->show_fs_toolbar_timeout_id =
                            g_timeout_add_full (G_PRIORITY_DEFAULT, 500,
                                                cb_rstto_main_window_show_fs_toolbar_timeout, window,
                                                cb_rstto_main_window_show_fs_toolbar_timeout_destroy);
                }
            }

            if (timeout > 0)
            {
                window->priv->hide_fs_mouse_cursor_timeout_id =
                        g_timeout_add_full (G_PRIORITY_DEFAULT, 1000 * timeout,
                                            cb_rstto_main_window_hide_fs_mouse_cursor_timeout, window,
                                            cb_rstto_main_window_hide_fs_mouse_cursor_timeout_destroy);
            }

            if (rstto_settings_get_boolean_property (window->priv->settings_manager, "hide-thumbnails-fullscreen"))
            {
                gtk_widget_hide (window->priv->t_bar_s_window);
            }

            if (rstto_image_list_is_empty (window->priv->image_list))
            {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                gtk_ui_manager_add_ui (
                        window->priv->ui_manager,
                        window->priv->toolbar_unfullscreen_merge_id,
                        "/main-toolbar/placeholder-fullscreen",
                        "unfullscreen",
                        "unfullscreen",
                        GTK_UI_MANAGER_TOOLITEM,
                        FALSE);
                gtk_ui_manager_remove_ui (
                        window->priv->ui_manager,
                        window->priv->toolbar_fullscreen_merge_id);
G_GNUC_END_IGNORE_DEPRECATIONS
            }
            else
            {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                gtk_ui_manager_add_ui (
                        window->priv->ui_manager,
                        window->priv->toolbar_unfullscreen_merge_id,
                        "/main-toolbar/placeholder-fullscreen",
                        "unfullscreen",
                        "unfullscreen",
                        GTK_UI_MANAGER_TOOLITEM,
                        FALSE);
                gtk_ui_manager_remove_ui (
                        window->priv->ui_manager,
                        window->priv->toolbar_fullscreen_merge_id);
G_GNUC_END_IGNORE_DEPRECATIONS
            }
        }
        else
        {
            gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (window->priv->p_viewer_s_window),
                                            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
            rstto_image_viewer_set_show_clock (RSTTO_IMAGE_VIEWER (window->priv->image_viewer), FALSE);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
            gtk_ui_manager_add_ui (
                    window->priv->ui_manager,
                    window->priv->toolbar_fullscreen_merge_id,
                    "/main-toolbar/placeholder-fullscreen",
                    "fullscreen",
                    "fullscreen",
                    GTK_UI_MANAGER_TOOLITEM,
                    FALSE);
            gtk_ui_manager_remove_ui (
                    window->priv->ui_manager,
                    window->priv->toolbar_unfullscreen_merge_id);
G_GNUC_END_IGNORE_DEPRECATIONS

            if (rstto_settings_get_boolean_property (RSTTO_SETTINGS (window->priv->settings_manager), "show-toolbar"))
            {
                gtk_widget_show (window->priv->toolbar);
            }
            else
            {
                gtk_widget_hide (window->priv->toolbar);
            }

            if (window->priv->show_fs_toolbar_timeout_id > 0)
            {
                REMOVE_SOURCE (window->priv->show_fs_toolbar_timeout_id);
            }

            if (window->priv->hide_fs_mouse_cursor_timeout_id > 0)
            {
                REMOVE_SOURCE (window->priv->hide_fs_mouse_cursor_timeout_id);
            }
            else
            {
                gdk_window_set_cursor (gtk_widget_get_window (widget), NULL);
            }

            gtk_widget_show (window->priv->menubar);
            if (rstto_settings_get_boolean_property (RSTTO_SETTINGS (window->priv->settings_manager), "show-statusbar"))
            {
                gtk_widget_show (window->priv->statusbar);
            }

            if (rstto_settings_get_boolean_property (window->priv->settings_manager, "show-thumbnailbar"))
            {
                if (! rstto_image_list_is_empty (window->priv->image_list))
                {
                    gtk_widget_show (window->priv->t_bar_s_window);
                }
            }
        }
    }

    /* to prevent costly redrawing of the image viewer (drawback: the other child widgets are
     * not redrawn either, so they are not grayed out when the main window loses focus) */
    if (event->changed_mask & GDK_WINDOW_STATE_FOCUSED)
        return TRUE;

    return FALSE;
}

static gboolean
cb_rstto_main_window_motion_notify_event (RsttoMainWindow *window, GdkEventMotion *event, gpointer user_data)
{
    if (gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (window))) & GDK_WINDOW_STATE_FULLSCREEN)
    {
        guint timeout = rstto_settings_get_uint_property (RSTTO_SETTINGS (window->priv->settings_manager),
                                                          "hide-mouse-cursor-fullscreen-timeout");

        /* Show toolbar when the mouse pointer is moved to the top of the screen */
        if (event->y < 1 && event->window == gtk_widget_get_window (window->priv->image_viewer)
            && ! rstto_image_list_is_empty (window->priv->image_list))
        {
            gtk_widget_show (window->priv->toolbar);
            window->priv->fs_toolbar_sticky = TRUE;

            if (window->priv->show_fs_toolbar_timeout_id > 0)
            {
                REMOVE_SOURCE (window->priv->show_fs_toolbar_timeout_id);
            }
        }

        /* Show the mouse cursor, but set a timer to hide it if not moved again */
        if (window->priv->hide_fs_mouse_cursor_timeout_id > 0)
        {
            REMOVE_SOURCE (window->priv->hide_fs_mouse_cursor_timeout_id);
        }
        else
        {
            gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (window)), NULL);
        }

        if (timeout > 0)
        {
            window->priv->hide_fs_mouse_cursor_timeout_id =
                    g_timeout_add_full (G_PRIORITY_DEFAULT, 1000 * timeout,
                                        cb_rstto_main_window_hide_fs_mouse_cursor_timeout, window,
                                        cb_rstto_main_window_hide_fs_mouse_cursor_timeout_destroy);
        }

        return TRUE;
    }

    return FALSE;
}

static gboolean
cb_rstto_main_window_image_viewer_scroll_event (GtkWidget *widget, GdkEventScroll *event, gpointer user_data)
{
    RsttoMainWindow *window = user_data;
    gboolean ret = FALSE;

    /*
     * - scroll through images with scroll
     * - pan across current image with shift + scroll
     * - zoom on current image with ctrl + scroll
     */
    if (! (event->state & (GDK_CONTROL_MASK)) && ! (event->state & (GDK_SHIFT_MASK)))
    {
        switch (event->direction)
        {
            case GDK_SCROLL_UP:
            case GDK_SCROLL_LEFT:
                rstto_main_window_select_valid_image (window, PREVIOUS);
                break;
            case GDK_SCROLL_DOWN:
            case GDK_SCROLL_RIGHT:
                rstto_main_window_select_valid_image (window, NEXT);
                break;
            case GDK_SCROLL_SMOOTH:
                /* TODO */
                break;
        }
        ret = TRUE; /* don't call other callbacks */
    }
    return ret;
}

static gboolean
cb_rstto_main_window_image_viewer_enter_notify_event (GtkWidget *widget, GdkEventCrossing *event, gpointer user_data)
{
    RsttoMainWindow *window = user_data;
    if (gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (window))) & GDK_WINDOW_STATE_FULLSCREEN)
    {
        if (! rstto_image_list_is_empty (window->priv->image_list))
        {
            window->priv->fs_toolbar_sticky = FALSE;
            if (window->priv->show_fs_toolbar_timeout_id > 0)
            {
                REMOVE_SOURCE (window->priv->show_fs_toolbar_timeout_id);
            }
            window->priv->show_fs_toolbar_timeout_id =
                    g_timeout_add_full (G_PRIORITY_DEFAULT, 500,
                                        cb_rstto_main_window_show_fs_toolbar_timeout, window,
                                        cb_rstto_main_window_show_fs_toolbar_timeout_destroy);
        }
    }

    return TRUE;
}

static gboolean
cb_rstto_main_window_show_fs_toolbar_timeout (gpointer user_data)
{
    RsttoMainWindow *window = user_data;
    gtk_widget_hide (window->priv->toolbar);
    return FALSE;
}

static void
cb_rstto_main_window_show_fs_toolbar_timeout_destroy (gpointer user_data)
{
    RSTTO_MAIN_WINDOW (user_data)->priv->show_fs_toolbar_timeout_id = 0;
}

static gboolean
cb_rstto_main_window_hide_fs_mouse_cursor_timeout (gpointer user_data)
{
    RsttoMainWindow *window = user_data;
    GdkCursor *cursor = gdk_cursor_new_from_name (gtk_widget_get_display (GTK_WIDGET (window)), "none");
    gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (window)), cursor);
    return FALSE;
}

static void
cb_rstto_main_window_hide_fs_mouse_cursor_timeout_destroy (gpointer user_data)
{
    RSTTO_MAIN_WINDOW (user_data)->priv->hide_fs_mouse_cursor_timeout_id = 0;
}

/**
 * cb_rstto_main_window_play:
 * @widget:
 * @window:
 *
 * Remove the play button from the menu, and add the pause button.
 *
 */
static void
cb_rstto_main_window_play (GtkWidget *widget, RsttoMainWindow *window)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gtk_ui_manager_add_ui (window->priv->ui_manager,
                           window->priv->pause_merge_id,
                           "/main-menu/go-menu/placeholder-slideshow",
                           "pause",
                           "pause",
                           GTK_UI_MANAGER_MENUITEM,
                           FALSE);
    gtk_ui_manager_remove_ui (window->priv->ui_manager,
                              window->priv->play_merge_id);

    gtk_ui_manager_add_ui (
            window->priv->ui_manager,
            window->priv->toolbar_pause_merge_id,
            "/main-toolbar/placeholder-slideshow",
            "pause",
            "pause",
            GTK_UI_MANAGER_TOOLITEM,
            FALSE);

    gtk_ui_manager_remove_ui (
            window->priv->ui_manager,
            window->priv->toolbar_play_merge_id);
G_GNUC_END_IGNORE_DEPRECATIONS

    rstto_main_window_play_slideshow (window);
}

/**
 * cb_rstto_main_window_pause:
 * @widget:
 * @window:
 *
 * Remove the pause button from the menu, and add the play button.
 *
 */
static void
cb_rstto_main_window_pause (GtkWidget *widget, RsttoMainWindow *window)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gtk_ui_manager_add_ui (window->priv->ui_manager,
                           window->priv->play_merge_id,
                           "/main-menu/go-menu/placeholder-slideshow",
                           "play",
                           "play",
                           GTK_UI_MANAGER_MENUITEM,
                           FALSE);
    gtk_ui_manager_remove_ui (window->priv->ui_manager,
                              window->priv->pause_merge_id);

    gtk_ui_manager_add_ui (
            window->priv->ui_manager,
            window->priv->toolbar_play_merge_id,
            "/main-toolbar/placeholder-slideshow",
            "play",
            "play",
            GTK_UI_MANAGER_TOOLITEM,
            FALSE);
    gtk_ui_manager_remove_ui (
            window->priv->ui_manager,
            window->priv->toolbar_pause_merge_id);
G_GNUC_END_IGNORE_DEPRECATIONS

    window->priv->playing = FALSE;
}

/**
 * cb_rstto_main_window_play_slideshow:
 * @window:
 *
 */
static gboolean
cb_rstto_main_window_play_slideshow (gpointer user_data)
{
    RsttoMainWindow *window = user_data;

    if (window->priv->playing)
    {
        /* Check if we could navigate forward, if not, wrapping is
         * disabled and we should force the iter to position 0
         */
        if (! rstto_main_window_select_valid_image (window, NEXT))
        {
            rstto_main_window_select_valid_image (window, FIRST);
        }
    }

    return window->priv->playing;
}

/**
 * rstto_main_window_fullscreen:
 * @widget:
 * @window:
 *
 * Toggle the fullscreen mode of this window.
 *
 */
void
rstto_main_window_fullscreen (GtkWidget *widget, RsttoMainWindow *window)
{
    if (gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (window))) & GDK_WINDOW_STATE_FULLSCREEN)
    {
        gtk_window_unfullscreen (GTK_WINDOW (window));
    }
    else
    {
        gtk_window_fullscreen (GTK_WINDOW (window));
    }
}

/**
 * cb_rstto_main_window_preferences:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_preferences (GtkWidget *widget, RsttoMainWindow *window)
{
    GtkWidget *dialog = rstto_preferences_dialog_new (GTK_WINDOW (window));

    gtk_dialog_run (GTK_DIALOG (dialog));

    gtk_widget_destroy (dialog);
}

/**
 * cb_rstto_main_window_about:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_about (GtkWidget *widget, RsttoMainWindow *window)
{
    const gchar *authors[] =
    {
      _("Developers:"),
        "Stephan Arts <stephan@xfce.org>",
        "Igor Zakharov <f2404@yandex.ru>",
        "Gaël Bonithon <gael@xfce.org>",
        NULL
    };

    GtkWidget *about_dialog = gtk_about_dialog_new ();

    gtk_about_dialog_set_version (GTK_ABOUT_DIALOG (about_dialog), PACKAGE_VERSION);

    gtk_about_dialog_set_comments (GTK_ABOUT_DIALOG (about_dialog),
        _("Ristretto is an image viewer for the Xfce desktop environment."));
    gtk_about_dialog_set_website (GTK_ABOUT_DIALOG (about_dialog),
        "https://docs.xfce.org/apps/ristretto/start");
    gtk_about_dialog_set_logo_icon_name (GTK_ABOUT_DIALOG (about_dialog),
        RISTRETTO_APP_ID);
    gtk_about_dialog_set_authors (GTK_ABOUT_DIALOG (about_dialog),
        authors);
    gtk_about_dialog_set_translator_credits (GTK_ABOUT_DIALOG (about_dialog),
        _("translator-credits"));
    gtk_about_dialog_set_license (GTK_ABOUT_DIALOG (about_dialog),
        xfce_get_license_text (XFCE_LICENSE_TEXT_GPL));
    gtk_about_dialog_set_copyright (GTK_ABOUT_DIALOG (about_dialog),
        "Copyright \302\251 2006-2012 Stephan Arts\n"
        "Copyright \302\251 2013-2024 Xfce Developers");

    gtk_window_set_transient_for (GTK_WINDOW (about_dialog), GTK_WINDOW (window));
    gtk_dialog_run (GTK_DIALOG (about_dialog));

    gtk_widget_destroy (about_dialog);
}

/**
 * cb_rstto_main_window_contents:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_contents (GtkWidget *widget, RsttoMainWindow *window)
{
    xfce_dialog_show_help (
            GTK_WINDOW (window),
            "ristretto",
            "start",
            "");
}


/**
 * cb_rstto_main_window_quit:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_quit (GtkWidget *widget, RsttoMainWindow *window)
{
    gtk_widget_destroy (GTK_WIDGET (window));
}

static gboolean
rstto_main_window_save_geometry_timer (gpointer user_data)
{
    GtkWindow *window = user_data;
    gint width = 0;
    gint height = 0;
    /* check if the window is still visible */
    if (gtk_widget_get_visible (GTK_WIDGET (window)))
    {
        /* determine the current state of the window */
        gint state = gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (window)));

        /* don't save geometry for maximized or fullscreen windows */
        if ((state & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN)) == 0)
        {
            /* determine the current width / height of the window... */
            gtk_window_get_size (GTK_WINDOW (window), &width, &height);

            /* ...and remember them as default for new windows */
            g_object_set (RSTTO_MAIN_WINDOW (window)->priv->settings_manager,
                          "window-width", width,
                          "window-height", height,
                          NULL);
        }
    }
    return FALSE;
}

static void
rstto_main_window_save_geometry_timer_destroy (gpointer user_data)
{
    RSTTO_MAIN_WINDOW (user_data)->priv->window_save_geometry_timer_id = 0;
}

static gboolean
cb_rstto_main_window_configure_event (GtkWidget *widget, GdkEventConfigure *event)
{
    RsttoMainWindow *window = RSTTO_MAIN_WINDOW (widget);
    GtkAllocation allocation;

    gtk_widget_get_allocation (widget, &allocation);

    /* shamelessly copied from thunar, written by benny */
    /* check if we have a new dimension here */
    if (allocation.width != event->width || allocation.height != event->height)
    {
        /* drop any previous timer source */
        if (window->priv->window_save_geometry_timer_id > 0)
        {
            REMOVE_SOURCE (window->priv->window_save_geometry_timer_id);
        }

        /* check if we should schedule another save timer */
        if (gtk_widget_get_visible (GTK_WIDGET (window)))
        {
            /* save the geometry one second after the last configure event */
            window->priv->window_save_geometry_timer_id = g_timeout_add_seconds_full (
                    G_PRIORITY_DEFAULT, 1, rstto_main_window_save_geometry_timer,
                    widget, rstto_main_window_save_geometry_timer_destroy);
        }
    }

    /* let Gtk+ handle the configure event */
    return FALSE;
}

static void
cb_rstto_main_window_update_statusbar (GtkWidget *widget, RsttoMainWindow *window)
{
    rstto_main_window_update_statusbar (window);
}

/******************/
/* ZOOM CALLBACKS */
/******************/

/**
 * cb_rstto_main_window_zoom_fit:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_zoom_fit (GtkWidget *widget, RsttoMainWindow *window)
{
    rstto_image_viewer_set_scale (RSTTO_IMAGE_VIEWER (window->priv->image_viewer),
                                  RSTTO_SCALE_FIT_TO_VIEW);
}

/**
 * cb_rstto_main_window_zoom_100:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_zoom_100 (GtkWidget *widget, RsttoMainWindow *window)
{
    rstto_image_viewer_set_scale (RSTTO_IMAGE_VIEWER (window->priv->image_viewer),
                                  RSTTO_SCALE_REAL_SIZE);
}

/**
 * cb_rstto_main_window_zoom_in:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_zoom_in (GtkWidget *widget, RsttoMainWindow *window)
{
    gdouble scale = rstto_image_viewer_get_scale (RSTTO_IMAGE_VIEWER (window->priv->image_viewer));
    rstto_image_viewer_set_scale (RSTTO_IMAGE_VIEWER (window->priv->image_viewer),
                                  scale * RSTTO_SCALE_FACTOR);
}

/**
 * cb_rstto_main_window_zoom_out:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_zoom_out (GtkWidget *widget, RsttoMainWindow *window)
{
    gdouble scale = rstto_image_viewer_get_scale (RSTTO_IMAGE_VIEWER (window->priv->image_viewer));
    rstto_image_viewer_set_scale (RSTTO_IMAGE_VIEWER (window->priv->image_viewer),
                                  scale / RSTTO_SCALE_FACTOR);
}

static void
cb_rstto_main_window_default_zoom (GtkRadioAction *action,
                                   GtkRadioAction *current,
                                   RsttoMainWindow *window)
{
    GtkTreeModel *model = GTK_TREE_MODEL (window->priv->image_list);
    GtkTreeIter iter;
    RsttoFile *file;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    rstto_settings_set_int_property (RSTTO_SETTINGS (window->priv->settings_manager),
                                     "default-zoom",
                                     gtk_radio_action_get_current_value (current));
G_GNUC_END_IGNORE_DEPRECATIONS

    if (gtk_tree_model_get_iter_first (model, &iter))
    {
        /* reset stored scale for all images */
        do
        {
            gtk_tree_model_get (model, &iter, 0, &file, -1);
            rstto_file_set_scale (file, RSTTO_SCALE_NONE);
            rstto_file_set_auto_scale (file, RSTTO_SCALE_NONE);
        }
        while (gtk_tree_model_iter_next (model, &iter));

        /* reset current image scale */
        rstto_image_viewer_set_scale (RSTTO_IMAGE_VIEWER (window->priv->image_viewer),
                                      RSTTO_SCALE_NONE);
    }
}

/**********************/
/* ROTATION CALLBACKS */
/**********************/

/**
 * cb_rstto_main_window_rotate_cw:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_rotate_cw (GtkWidget *widget, RsttoMainWindow *window)
{
    RsttoImageViewer *viewer = RSTTO_IMAGE_VIEWER (window->priv->image_viewer);
    switch (rstto_image_viewer_get_orientation (viewer))
    {
        default:
        case RSTTO_IMAGE_ORIENT_NONE:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_90);
            break;
        case RSTTO_IMAGE_ORIENT_90:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_180);
            break;
        case RSTTO_IMAGE_ORIENT_180:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_270);
            break;
        case RSTTO_IMAGE_ORIENT_270:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_NONE);
            break;
        case RSTTO_IMAGE_ORIENT_FLIP_HORIZONTAL:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_FLIP_TRANSVERSE);
            break;
        case RSTTO_IMAGE_ORIENT_FLIP_VERTICAL:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_FLIP_TRANSPOSE);
            break;
        case RSTTO_IMAGE_ORIENT_FLIP_TRANSPOSE:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_FLIP_HORIZONTAL);
            break;
        case RSTTO_IMAGE_ORIENT_FLIP_TRANSVERSE:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_FLIP_VERTICAL);
            break;
    }
    rstto_main_window_update_statusbar (window);
}

/**
 * cb_rstto_main_window_rotate_ccw:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_rotate_ccw (GtkWidget *widget, RsttoMainWindow *window)
{
    RsttoImageViewer *viewer = RSTTO_IMAGE_VIEWER (window->priv->image_viewer);
    switch (rstto_image_viewer_get_orientation (viewer))
    {
        default:
        case RSTTO_IMAGE_ORIENT_NONE:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_270);
            break;
        case RSTTO_IMAGE_ORIENT_90:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_NONE);
            break;
        case RSTTO_IMAGE_ORIENT_180:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_90);
            break;
        case RSTTO_IMAGE_ORIENT_270:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_180);
            break;
        case RSTTO_IMAGE_ORIENT_FLIP_HORIZONTAL:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_FLIP_TRANSPOSE);
            break;
        case RSTTO_IMAGE_ORIENT_FLIP_VERTICAL:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_FLIP_TRANSVERSE);
            break;
        case RSTTO_IMAGE_ORIENT_FLIP_TRANSPOSE:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_FLIP_VERTICAL);
            break;
        case RSTTO_IMAGE_ORIENT_FLIP_TRANSVERSE:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_FLIP_HORIZONTAL);
            break;
    }
    rstto_main_window_update_statusbar (window);
}

/**********************/
/* FLIP CALLBACKS */
/**********************/

/**
 * cb_rstto_main_window_flip_hz:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_flip_hz (GtkWidget *widget, RsttoMainWindow *window)
{
    RsttoImageViewer *viewer = RSTTO_IMAGE_VIEWER (window->priv->image_viewer);
    switch (rstto_image_viewer_get_orientation (viewer))
    {
        default:
        case RSTTO_IMAGE_ORIENT_NONE:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_FLIP_HORIZONTAL);
            break;
        case RSTTO_IMAGE_ORIENT_90:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_FLIP_TRANSPOSE);
            break;
        case RSTTO_IMAGE_ORIENT_180:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_FLIP_VERTICAL);
            break;
        case RSTTO_IMAGE_ORIENT_270:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_FLIP_TRANSVERSE);
            break;
        case RSTTO_IMAGE_ORIENT_FLIP_HORIZONTAL:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_NONE);
            break;
        case RSTTO_IMAGE_ORIENT_FLIP_VERTICAL:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_180);
            break;
        case RSTTO_IMAGE_ORIENT_FLIP_TRANSPOSE:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_90);
            break;
        case RSTTO_IMAGE_ORIENT_FLIP_TRANSVERSE:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_270);
            break;
    }
    rstto_main_window_update_statusbar (window);
}

/**
 * cb_rstto_main_window_flip_vt:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_flip_vt (GtkWidget *widget, RsttoMainWindow *window)
{
    RsttoImageViewer *viewer = RSTTO_IMAGE_VIEWER (window->priv->image_viewer);
    switch (rstto_image_viewer_get_orientation (viewer))
    {
        default:
        case RSTTO_IMAGE_ORIENT_NONE:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_FLIP_VERTICAL);
            break;
        case RSTTO_IMAGE_ORIENT_90:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_FLIP_TRANSVERSE);
            break;
        case RSTTO_IMAGE_ORIENT_180:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_FLIP_HORIZONTAL);
            break;
        case RSTTO_IMAGE_ORIENT_270:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_FLIP_TRANSPOSE);
            break;
        case RSTTO_IMAGE_ORIENT_FLIP_HORIZONTAL:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_180);
            break;
        case RSTTO_IMAGE_ORIENT_FLIP_VERTICAL:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_NONE);
            break;
        case RSTTO_IMAGE_ORIENT_FLIP_TRANSPOSE:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_270);
            break;
        case RSTTO_IMAGE_ORIENT_FLIP_TRANSVERSE:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_90);
            break;
    }
    rstto_main_window_update_statusbar (window);
}


/************************/
/* NAVIGATION CALLBACKS */
/************************/

static gboolean
rstto_main_window_select_valid_image (RsttoMainWindow *window,
                                      gint position)
{
    gboolean (*move_iter) (RsttoImageListIter *);
    RsttoImageListIter *iter;
    RsttoFile *file, *ref_file;
    gboolean moved = FALSE;

    iter = rstto_image_list_iter_clone (window->priv->iter);
    ref_file = rstto_image_list_iter_get_file (iter);
    switch (position)
    {
        case FIRST:
            rstto_image_list_iter_set_position (iter, 0);
            move_iter = rstto_image_list_iter_next;
            break;
        case LAST:
            rstto_image_list_iter_set_position (iter,
                rstto_image_list_get_n_images (window->priv->image_list) - 1);
            move_iter = rstto_image_list_iter_previous;
            break;
        case PREVIOUS:
            rstto_image_list_iter_previous (iter);
            move_iter = rstto_image_list_iter_previous;
            break;
        case NEXT:
            rstto_image_list_iter_next (iter);
            move_iter = rstto_image_list_iter_next;
            break;
        default:
            g_warn_if_reached ();
            return FALSE;
    }

    while ((file = rstto_image_list_iter_get_file (iter)) != ref_file)
    {
        /* directories are loaded without this costly filtering, so it is done
         * here only when required */
        if (! rstto_file_is_valid (file))
        {
            moved = move_iter (iter);
            rstto_image_list_remove_file (window->priv->image_list, file);
            if (! moved)
                break;
            else
                rstto_image_list_iter_find_file (iter, rstto_image_list_iter_get_file (iter));
        }
        else
        {
            moved = TRUE;
            break;
        }
    }

    if (moved)
        rstto_image_list_iter_set_position (window->priv->iter,
                                            rstto_image_list_iter_get_position (iter));

    g_object_unref (iter);

    return moved;
}

/**
 * cb_rstto_main_window_first_image:
 * @widget:
 * @window:
 *
 * Move the iter to the first image;
 *
 */
static void
cb_rstto_main_window_first_image (GtkWidget *widget, RsttoMainWindow *window)
{
    rstto_main_window_select_valid_image (window, FIRST);
}


/**
 * cb_rstto_main_window_last_image:
 * @widget:
 * @window:
 *
 * Move the iter to the last image;
 *
 */
static void
cb_rstto_main_window_last_image (GtkWidget *widget, RsttoMainWindow *window)
{
    rstto_main_window_select_valid_image (window, LAST);
}

/**
 * cb_rstto_main_window_next_image:
 * @widget:
 * @window:
 *
 * Move the iter to the next image;
 *
 */
static void
cb_rstto_main_window_next_image (GtkWidget *widget, RsttoMainWindow *window)
{
    rstto_main_window_select_valid_image (window, NEXT);
}

/**
 * cb_rstto_main_window_previous_image:
 * @widget:
 * @window:
 *
 * Move the iter to the previous image;
 *
 */
static void
cb_rstto_main_window_previous_image (GtkWidget *widget, RsttoMainWindow *window)
{
    rstto_main_window_select_valid_image (window, PREVIOUS);
}

/**
 * cb_rstto_main_window_open_with_other_app:
 * @widget:
 * @window:
 *
 */
static void
cb_rstto_main_window_open_with_other_app (GtkWidget *widget, RsttoMainWindow *window)
{
    rstto_main_window_launch_editor_chooser (window);
}

/**********************/
/* FILE I/O CALLBACKS */
/**********************/

/**
 * cb_rstto_main_window_open_image:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_open_image (GtkWidget *widget, RsttoMainWindow *window)
{
    GtkFileChooserNative *dialog;
    GtkFileFilter *filter;
    GVariant *variant;
    GSList *files;
    gchar *str;
    gint response;

    dialog = gtk_file_chooser_native_new (_("Open image"), GTK_WINDOW (window),
                                          GTK_FILE_CHOOSER_ACTION_OPEN,
                                          _("_Open"), _("_Cancel"));

    gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (dialog), TRUE);
    gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), FALSE);

    g_object_get (window->priv->settings_manager, "current-uri", &str, NULL);
    if (str != NULL && *str != '\0')
        gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (dialog), str);

    g_free (str);

    variant = g_variant_ref_sink (gtk_file_filter_to_gvariant (app_file_filter));
    filter = gtk_file_filter_new_from_gvariant (variant);
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
    g_variant_unref (variant);

    filter = gtk_file_filter_new ();
    gtk_file_filter_add_mime_type (filter, "image/jpeg");
    gtk_file_filter_set_name (filter, _(".jp(e)g"));
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

    filter = gtk_file_filter_new ();
    gtk_file_filter_add_pattern (filter, "*");
    gtk_file_filter_set_name (filter, _("All Files"));
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

    response = gtk_native_dialog_run (GTK_NATIVE_DIALOG (dialog));
    gtk_native_dialog_hide (GTK_NATIVE_DIALOG (dialog));
    if (response == GTK_RESPONSE_ACCEPT)
    {
        files = gtk_file_chooser_get_files (GTK_FILE_CHOOSER (dialog));
        rstto_main_window_open (window, files);
        g_slist_free_full (files, g_object_unref);

        str = gtk_file_chooser_get_current_folder_uri (GTK_FILE_CHOOSER (dialog));
        g_object_set (window->priv->settings_manager, "current-uri", str, NULL);
        g_free (str);
    }

    gtk_native_dialog_destroy (GTK_NATIVE_DIALOG (dialog));
}

/**
 * cb_rstto_main_window_open_recent:
 * @chooser:
 * @window:
 *
 */
static void
cb_rstto_main_window_open_recent (GtkRecentChooser *chooser, RsttoMainWindow *window)
{
    GSList *files = NULL;
    GFile *file;
    gchar *uri;

    uri = gtk_recent_chooser_get_current_uri (chooser);
    file = g_file_new_for_uri (uri);
    files = g_slist_prepend (files, file);

    rstto_main_window_open (window, files);

    g_free (uri);
    g_slist_free_full (files, g_object_unref);
}

/**
 * cb_rstto_main_window_save_copy:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_save_copy (GtkWidget *widget, RsttoMainWindow *window)
{
    GtkWidget *dialog;
    gint response;
    GFile *file, *s_file;
    RsttoFile *r_file;

    r_file = rstto_image_list_iter_get_file (window->priv->iter);

    if (r_file == NULL)
        return;

    dialog = gtk_file_chooser_dialog_new (_("Save copy"),
                                         GTK_WINDOW (window),
                                         GTK_FILE_CHOOSER_ACTION_SAVE,
                                         _("_Cancel"), GTK_RESPONSE_CANCEL,
                                         _("_Save"), GTK_RESPONSE_OK,
                                         NULL);
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);

    if (window->priv->last_copy_folder_uri)
        gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (dialog),
            window->priv->last_copy_folder_uri);

    gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog),
        rstto_file_get_display_name (r_file));

    response = gtk_dialog_run (GTK_DIALOG (dialog));
    if (response == GTK_RESPONSE_OK)
    {
        file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
        s_file = rstto_file_get_file (rstto_image_list_iter_get_file (window->priv->iter));
        if (! g_file_copy (s_file, file, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, NULL))
            rstto_util_dialog_error (ERROR_SAVE_FAILED, NULL);

        g_free (window->priv->last_copy_folder_uri);
        window->priv->last_copy_folder_uri = gtk_file_chooser_get_current_folder_uri (
            GTK_FILE_CHOOSER (dialog));
    }

    gtk_widget_destroy (dialog);

}

static void
cb_rstto_main_window_properties (GtkWidget *widget, RsttoMainWindow *window)
{
    GError *error = NULL;
    RsttoFile *file = rstto_image_list_iter_get_file (window->priv->iter);
    const gchar *uri[] = { NULL, NULL };
    GtkWidget *dialog = NULL;
    gboolean use_thunar_properties = rstto_settings_get_boolean_property (
            window->priv->settings_manager,
            "use-thunar-properties");

    if (NULL != file)
    {
        /* Check if we should first ask the file manager
         * to show the file properties dialog.
         */
        if (use_thunar_properties && window->priv->filemanager_proxy != NULL)
        {
            GVariant *unused = NULL;

            /* Get the file-uri */
            uri[0] = rstto_file_get_uri (file);

            /* Call the ShowItemProperties D-Bus
             * interface. If it fails, fall back to the
             * internal properties-dialog.
             */
            unused = g_dbus_proxy_call_sync (window->priv->filemanager_proxy,
                                             "ShowItemProperties",
                                              g_variant_new ("(^ass)",
                                                             uri,
                                                             ""),
                                             G_DBUS_CALL_FLAGS_NONE,
                                             -1,
                                             NULL,
                                             &error);
            if (error != NULL)
            {
                g_warning ("DBUS CALL FAILED: '%s'", error->message);

                /* Create the internal file-properties dialog */
                dialog = rstto_properties_dialog_new (
                        GTK_WINDOW (window),
                        file);

                gtk_dialog_run (GTK_DIALOG (dialog));

                /* Cleanup the file-properties dialog */
                gtk_widget_destroy (dialog);
            }
            else
            {
                g_variant_unref (unused);
            }
        }
        else
        {
            /* Create the internal file-properties dialog */
            dialog = rstto_properties_dialog_new (
                    GTK_WINDOW (window),
                    file);

            gtk_dialog_run (GTK_DIALOG (dialog));

            /* Cleanup the file-properties dialog */
            gtk_widget_destroy (dialog);
        }
    }
}

static void
cb_rstto_main_window_print (GtkWidget *widget, RsttoMainWindow *window)
{
    RsttoPrint *print;
    GError *error = NULL;

    g_return_if_fail (RSTTO_IS_MAIN_WINDOW (window));

    print = rstto_print_new (RSTTO_IMAGE_VIEWER (window->priv->image_viewer));
    if (! rstto_print_image_interactive (print, GTK_WINDOW (window), &error))
    {
        rstto_util_dialog_error (_("Failed to print the image"), error);
        g_error_free (error);
    }

    g_object_unref (print);
}

/**
 * cb_rstto_main_window_close:
 * @widget:
 * @window:
 *
 * Close all images.
 *
 * Set the directory to NULL, the image-list-iter will emit an
 * 'iter-changed' signal. The ui will be updated in response to
 * that just like it is when an image is opened.
 */
static void
cb_rstto_main_window_close (
        GtkWidget *widget,
        RsttoMainWindow *window)
{
    rstto_image_list_set_directory (window->priv->image_list, NULL, NULL, NULL);
    gtk_widget_hide (window->priv->warning);
}

/**
 * cb_rstto_main_window_edit:
 * @widget:
 * @window:
 *
 * Edit images.
 *
 */
static void
cb_rstto_main_window_edit (
        GtkWidget *widget,
        RsttoMainWindow *window)
{
    RsttoFile       *r_file = rstto_image_list_iter_get_file (window->priv->iter);
    const gchar     *content_type = rstto_file_get_content_type (r_file);
    const gchar     *editor = rstto_mime_db_lookup (window->priv->db, content_type);
    GList           *files = g_list_prepend (NULL, rstto_file_get_file (r_file));
    GDesktopAppInfo *app_info = NULL;

    if (editor != NULL)
    {
        app_info = g_desktop_app_info_new (editor);
        if (app_info != NULL)
        {
            g_app_info_launch (G_APP_INFO (app_info), files, NULL, NULL);
            g_list_free (files);
            return;
        }
    }

    rstto_main_window_launch_editor_chooser (window);

    g_list_free (files);
}

/**
 * rstto_confirm_deletion:
 * @window: Ristretto main window
 * @file: Ristretto file to delete
 * @to_trash: File is to be trashed (TRUE) or deleted (FALSE)
 *
 * Show dialog to confirm deletion/trashing.
 * Use can choose not be be asked again if the file is to be trashed.
 */
static int
rstto_confirm_deletion (
    RsttoMainWindow *window,
    RsttoFile *file,
    gboolean to_trash)
{
    static gboolean dont_ask_trash = FALSE;
    gchar *prompt = NULL;
    const gchar *file_basename = rstto_file_get_display_name (file);
    GtkWidget *dialog;
    GtkWidget* dont_ask_checkbox = NULL;
    gint response;

    if (to_trash)
    {
        if (dont_ask_trash)
        {
            return GTK_RESPONSE_OK;
        }
        prompt = g_strdup_printf (_("Are you sure you want to send image '%s' to trash?"), file_basename);
    }
    else
    {
        prompt = g_strdup_printf (_("Are you sure you want to delete image '%s' from disk?"), file_basename);
    }
    dialog = gtk_message_dialog_new (
            GTK_WINDOW (window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_WARNING,
            GTK_BUTTONS_OK_CANCEL,
            "%s",
            prompt);

    if (to_trash)
    {
        dont_ask_checkbox = gtk_check_button_new_with_mnemonic (_("_Do not ask again for this session"));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dont_ask_checkbox), FALSE);
        gtk_box_pack_end (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), dont_ask_checkbox, TRUE, TRUE, 0);
        gtk_widget_show (dont_ask_checkbox);
        gtk_widget_grab_focus (gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL));
    }

    response = gtk_dialog_run (GTK_DIALOG (dialog));
    if (to_trash && (response == GTK_RESPONSE_OK))
    {
      dont_ask_trash = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dont_ask_checkbox));
    }
    gtk_widget_destroy (dialog);

    g_free (prompt);

    return response;
}

/**
 * cb_rstto_main_window_delete:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_delete (
        GtkWidget *widget,
        RsttoMainWindow *window)
{
    RsttoFile *file = rstto_image_list_iter_get_file (window->priv->iter);
    const gchar *file_basename;
    GdkModifierType state;
    gboolean delete_file = FALSE;
    gboolean success = FALSE;
    gchar *prompt = NULL;
    GError *error = NULL;
    gint response;

    if (file == NULL)
    {
        return;
    }

    g_return_if_fail (! rstto_image_list_is_empty (window->priv->image_list));

    if (gtk_get_current_event_state (&state))
    {
        if (state & GDK_SHIFT_MASK)
        {
            delete_file = TRUE;
        }
    }

    response = rstto_confirm_deletion (window, file, !delete_file);
    if (response == GTK_RESPONSE_OK)
    {
        g_object_ref (file);
        if (delete_file)
        {
            success = g_file_delete (rstto_file_get_file (file), NULL, &error);
        }
        else
        {
            success = g_file_trash (rstto_file_get_file (file), NULL, &error);
        }

        if (success)
        {
            rstto_image_list_remove_file (window->priv->image_list, file);
        }
        else
        {
            file_basename = rstto_file_get_display_name (file);
            if (delete_file)
            {
                prompt = g_strdup_printf (ERROR_DELETE_FAILED_DISK, file_basename);
            }
            else
            {
                prompt = g_strdup_printf (ERROR_DELETE_FAILED_TRASH, file_basename);
            }
            rstto_util_dialog_error (prompt, error);
            g_free (prompt);
            g_error_free (error);
        }
        g_object_unref (file);
    }
}

/**
 * cb_rstto_main_window_refresh:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_refresh (
        GtkWidget *widget,
        RsttoMainWindow *window)
{
    RsttoFile *r_file = rstto_image_list_iter_get_file (window->priv->iter);

    /* Trigger a reload of all things using this file */
    rstto_file_changed (r_file);
}

/**
 * cb_rstto_main_window_dnd_files:
 * @widget:
 * @uris:
 * @window:
 *
 */
static void
cb_rstto_main_window_dnd_files (GtkWidget *widget,
                                gchar **uris,
                                RsttoMainWindow *window)
{
    GSList *files = NULL;
    GFile *file;
    guint n, n_uris;

    g_return_if_fail (RSTTO_IS_MAIN_WINDOW (window));

    n_uris = g_strv_length (uris);
    for (n = 0; n < n_uris; n++)
    {
        file = g_file_new_for_uri (uris[n]);
        files = g_slist_prepend (files, file);
    }

    files = g_slist_reverse (files);
    rstto_main_window_open (window, files);
    g_slist_free_full (files, g_object_unref);
}

static void
cb_rstto_main_window_set_title (RsttoMainWindow *window)
{
    RsttoFile *file;
    gchar *title;
    const gchar *basename;
    gint n_images, position;

    file = rstto_image_list_iter_get_file (window->priv->iter);
    if (file == NULL)
    {
        gtk_window_set_title (GTK_WINDOW (window), RISTRETTO_APP_TITLE);
        return;
    }

    n_images = rstto_image_list_get_n_images (window->priv->image_list);
    basename = rstto_file_get_display_name (file);
    position = rstto_image_list_iter_get_position (window->priv->iter);
    if (n_images > 1)
        title = g_strdup_printf ("%s - %s [%d/%d]", basename, RISTRETTO_APP_TITLE,
                                 position + 1, n_images);
    else
        title = g_strdup_printf ("%s - %s", basename, RISTRETTO_APP_TITLE);

    gtk_window_set_title (GTK_WINDOW (window), title);
    g_free (title);
}

static void
cb_rstto_main_window_set_icon (RsttoMainWindow *window,
                               RsttoFile *file)
{
    const GdkPixbuf *pixbuf;

    if (file != rstto_image_list_iter_get_file (window->priv->iter))
        return;

    if (file == NULL || (pixbuf = rstto_file_get_thumbnail (file, RSTTO_THUMBNAIL_SIZE_LARGE)) == NULL)
    {
        gtk_window_set_icon (GTK_WINDOW (window), NULL);
        gtk_window_set_icon_name (GTK_WINDOW (window), RISTRETTO_APP_ID);
    }
    else
        gtk_window_set_icon (GTK_WINDOW (window), (GdkPixbuf *) pixbuf);
}

/**********************/
/* PRINTING CALLBACKS */
/**********************/

/*************************/
/* GUI-RELATED CALLBACKS */
/*************************/

/**
 * cb_rstto_main_window_toggle_show_file_toolbar:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_toggle_show_toolbar (GtkWidget *widget, RsttoMainWindow *window)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gboolean active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (widget));
G_GNUC_END_IGNORE_DEPRECATIONS
    if (active)
    {
        gtk_widget_show (window->priv->toolbar);
    }
    else
    {
        gtk_widget_hide (window->priv->toolbar);
    }
    rstto_settings_set_boolean_property (RSTTO_SETTINGS (window->priv->settings_manager), "show-toolbar", active);
}

/**
 * cb_rstto_main_window_toggle_show_thumbnailbar:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_toggle_show_thumbnailbar (GtkWidget *widget, RsttoMainWindow *window)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gboolean active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (widget));
G_GNUC_END_IGNORE_DEPRECATIONS
    if (active)
    {
        if (! rstto_image_list_is_empty (window->priv->image_list))
            gtk_widget_show (window->priv->t_bar_s_window);
    }
    else
    {
        gtk_widget_hide (window->priv->t_bar_s_window);
    }
    rstto_settings_set_boolean_property (RSTTO_SETTINGS (window->priv->settings_manager), "show-thumbnailbar", active);
}

/**
 * cb_rstto_main_window_toggle_show_statusbar:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_toggle_show_statusbar (GtkWidget *widget, RsttoMainWindow *window)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gboolean active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (widget));
G_GNUC_END_IGNORE_DEPRECATIONS
    if (active)
    {
        gtk_widget_show (window->priv->statusbar);
    }
    else
    {
        gtk_widget_hide (window->priv->statusbar);
    }
    rstto_settings_set_boolean_property (RSTTO_SETTINGS (window->priv->settings_manager), "show-statusbar", active);
}

RsttoImageListIter *
rstto_main_window_get_iter (
        RsttoMainWindow *window)
{
    return window->priv->iter;
}

static gboolean
rstto_main_window_recent_filter (const GtkRecentFilterInfo *filter_info,
                                 gpointer user_data)
{
    RsttoMainWindow *window = user_data;
    GtkRecentInfo *info;
    gboolean kept = FALSE;

    info = gtk_recent_manager_lookup_item (window->priv->recent_manager, filter_info->uri, NULL);
    if (info == NULL)
        return FALSE;

    if (gtk_recent_info_has_application (info, PACKAGE_NAME))
    {
        /* silently remove the file from the recent list if it no longer exists */
        if (! gtk_recent_info_exists (info))
            gtk_recent_manager_remove_item (window->priv->recent_manager, filter_info->uri, NULL);
        else
            kept = TRUE;
    }

    gtk_recent_info_unref (info);

    return kept;
}

static void
rstto_main_window_add_file_to_recent_files (const gchar *uri,
                                            const gchar *content_type)
{
    GtkRecentData recent_data;
    static gchar *groups[2] = { "Graphics", NULL };

    if (uri == NULL || content_type == NULL)
        return;

    recent_data.display_name = NULL;
    recent_data.description = NULL;
    recent_data.mime_type = (gchar *) content_type;
    recent_data.app_name = PACKAGE_NAME;
    recent_data.app_exec = PACKAGE " %u";
    recent_data.groups = groups;
    recent_data.is_private = FALSE;

    gtk_recent_manager_add_full (gtk_recent_manager_get_default (), uri, &recent_data);
}

void
rstto_main_window_open (RsttoMainWindow *window,
                        GSList *files)
{
    RsttoFile *r_file;
    GError *error = NULL, *tmp_error = NULL;
    GSList *file, *deleted = NULL, *invalid = NULL;
    GFileInfo *info;
    GFile *dir;
    gchar *uri;

    /* in case of several files, open only those, adding them to the list one by one: this
     * is expensive and supposed to be done only for a limited number of files */
    if (g_slist_length (files) > 1)
    {
        for (file = files; file != NULL; file = file->next)
        {
            /* we will show only one dialog for all deleted files later */
            if (! g_file_query_exists (file->data, NULL))
            {
                deleted = g_slist_prepend (deleted, file->data);
                continue;
            }

            r_file = rstto_file_new (file->data);
            if (! rstto_image_list_add_file (window->priv->image_list, r_file, &tmp_error))
            {
                if (error == NULL)
                    error = g_error_copy (tmp_error);

                invalid = g_slist_prepend (invalid, file->data);
                g_object_unref (r_file);
                g_clear_error (&tmp_error);

                continue;
            }
            else
            {
                rstto_main_window_add_file_to_recent_files (rstto_file_get_uri (r_file),
                                                            rstto_file_get_content_type (r_file));
                g_object_unref (r_file);
            }
        }
    }
    /* in case of a single file, replace the list with the contents of the parent directory,
     * or the directory itself if the file is a directory */
    else if (! g_file_query_exists (files->data, NULL))
        deleted = g_slist_prepend (deleted, files->data);
    else if ((info = g_file_query_info (files->data, "standard::type,standard::content-type",
                                        G_FILE_QUERY_INFO_NONE, NULL, &error)) == NULL)
        invalid = g_slist_prepend (invalid, files->data);
    else
    {
        /* add the directory contents asynchronously */
        if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
        {
            rstto_image_list_set_directory (window->priv->image_list, files->data, NULL, NULL);
            uri = g_file_get_uri (files->data);
            rstto_main_window_add_file_to_recent_files (uri,
                g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE));
            g_free (uri);
        }
        else
        {
            dir = g_file_get_parent (files->data);
            r_file = rstto_file_new (files->data);
            if (rstto_image_list_set_directory (window->priv->image_list, dir, r_file, &error))
                rstto_main_window_add_file_to_recent_files (rstto_file_get_uri (r_file),
                                                            rstto_file_get_content_type (r_file));
            else
                invalid = g_slist_prepend (invalid, files->data);

            g_object_unref (dir);
            g_object_unref (r_file);
        }

        g_object_unref (info);
    }

    if (deleted != NULL || invalid != NULL)
    {
        for (file = deleted; file != NULL; file = file->next)
        {
            uri = g_file_get_uri (file->data);
            g_message ("Could not open file '%s': File does not exist", uri);
            g_free (uri);
        }

        for (file = invalid; file != NULL; file = file->next)
        {
            uri = g_file_get_uri (file->data);
            g_message ("Could not open file '%s': %s", uri, error->message);
            g_free (uri);
        }

        rstto_util_dialog_error (ERROR_OPEN_FAILED, NULL);

        g_slist_free (deleted);
        g_slist_free (invalid);
        if (error != NULL)
            g_error_free (error);
    }
}

static void
rstto_main_window_launch_editor_chooser (
        RsttoMainWindow *window)
{
    RsttoFile *r_file = rstto_image_list_iter_get_file (window->priv->iter);
    const gchar *content_type = rstto_file_get_content_type (r_file);
    GList *files = g_list_prepend (NULL, rstto_file_get_file (r_file));
    GList *app_infos_all = NULL;
    GList *app_infos_recommended = NULL;
    GList *app_infos_iter = NULL;
    GDesktopAppInfo *app_info = NULL;
    GtkCellRenderer    *renderer;
    GtkWidget *dialog = NULL;
    GtkWidget *content_area;
    GtkWidget *hbox;
    GtkWidget *vbox;
    GtkWidget *image;
    GtkWidget *label;
    GtkWidget *button;
    GtkWidget *check_button;
    GtkWidget *treeview;
    GtkWidget *scrolled_window;
    GtkTreeStore *tree_store;
    GtkTreeIter   iter;
    GtkTreeIter   parent_iter;
    GtkTreeSelection *selection;
    GtkTreeViewColumn *column;
    gchar *label_text = NULL;
    const gchar *id;
    const gchar *name;
    GIcon *icon;

    dialog = gtk_dialog_new ();
    gtk_window_set_title (GTK_WINDOW (dialog), _("Edit with"));
    gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
    gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

    button = xfce_gtk_button_new_mixed ("gtk-cancel", _("_Cancel"));
    gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_CANCEL);
    gtk_widget_show (button);
    button = xfce_gtk_button_new_mixed ("gtk-ok", _("_OK"));
    gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);
    gtk_widget_show (button);

    content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

    icon = g_content_type_get_icon (content_type);
    image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_DIALOG);
    g_object_unref (icon);

    label_text = g_strdup_printf (
            _("Open %s and other files of type %s with:"),
            rstto_file_get_display_name (r_file),
            content_type);
    label = gtk_label_new (label_text);
    g_free (label_text);

    check_button = gtk_check_button_new_with_mnemonic (_("Use as _default for this kind of file"));

    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (
            GTK_SCROLLED_WINDOW (scrolled_window),
            GTK_POLICY_AUTOMATIC,
            GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type (
            GTK_SCROLLED_WINDOW (scrolled_window),
            GTK_SHADOW_IN);

    gtk_widget_set_size_request (
            scrolled_window,
            300,
            200);

    treeview = gtk_tree_view_new ();
    tree_store = gtk_tree_store_new (
            7,
            G_TYPE_STRING,
            G_TYPE_ICON,
            G_TYPE_APP_INFO,
            PANGO_TYPE_STYLE,
            G_TYPE_BOOLEAN,
            PANGO_TYPE_WEIGHT,
            G_TYPE_BOOLEAN);
    gtk_tree_view_set_show_expanders (GTK_TREE_VIEW (treeview), FALSE);
    gtk_tree_view_set_level_indentation (GTK_TREE_VIEW (treeview), 24);

    gtk_tree_view_set_headers_visible (
            GTK_TREE_VIEW (treeview),
            FALSE);
    gtk_tree_view_set_model (
            GTK_TREE_VIEW (treeview),
            GTK_TREE_MODEL (tree_store));

    column = g_object_new (
            GTK_TYPE_TREE_VIEW_COLUMN,
            "expand",
            TRUE,
            NULL);
    renderer = gtk_cell_renderer_pixbuf_new ();
    g_object_set (renderer, "stock-size", GTK_ICON_SIZE_LARGE_TOOLBAR, NULL);

    gtk_tree_view_column_pack_start (column, renderer, FALSE);
    gtk_tree_view_column_set_attributes (
            column,
            renderer,
            "gicon",
            EDITOR_CHOOSER_MODEL_COLUMN_GICON,
            NULL);

    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer, TRUE);
    gtk_tree_view_column_set_attributes (
            column,
            renderer,
            "style",
            EDITOR_CHOOSER_MODEL_COLUMN_STYLE,
            "style-set",
            EDITOR_CHOOSER_MODEL_COLUMN_STYLE_SET,
            "text",
            EDITOR_CHOOSER_MODEL_COLUMN_NAME,
            "weight",
            EDITOR_CHOOSER_MODEL_COLUMN_WEIGHT,
            "weight-set",
            EDITOR_CHOOSER_MODEL_COLUMN_WEIGHT_SET,
            NULL);

    gtk_tree_view_append_column (
            GTK_TREE_VIEW (treeview),
            column);

    app_infos_all = g_app_info_get_all ();
    app_infos_recommended = g_app_info_get_all_for_type (content_type);
    app_infos_iter = app_infos_recommended;

    icon = g_themed_icon_new ("org.xfce.settings.default-applications");
    gtk_tree_store_append (tree_store, &parent_iter, NULL);
    gtk_tree_store_set (
            tree_store,
            &parent_iter,
            EDITOR_CHOOSER_MODEL_COLUMN_GICON,
            icon,
            EDITOR_CHOOSER_MODEL_COLUMN_NAME,
            _("Recommended Applications"),
            EDITOR_CHOOSER_MODEL_COLUMN_APPLICATION,
            NULL,
            EDITOR_CHOOSER_MODEL_COLUMN_WEIGHT,
            PANGO_WEIGHT_SEMIBOLD,
            EDITOR_CHOOSER_MODEL_COLUMN_WEIGHT_SET,
            TRUE,
            EDITOR_CHOOSER_MODEL_COLUMN_STYLE_SET,
            FALSE,
            -1);
    if (NULL != icon)
        g_object_unref (icon);

    while (app_infos_iter)
    {
        id = g_app_info_get_id (app_infos_iter->data);

        /* Do not add ristretto to the list */
        if (strcmp (id, RISTRETTO_DESKTOP_ID))
        {
            icon = g_app_info_get_icon (app_infos_iter->data);
            name = g_app_info_get_display_name (app_infos_iter->data),
            gtk_tree_store_append (tree_store, &iter, &parent_iter);
            gtk_tree_store_set (
                    tree_store,
                    &iter,
                    EDITOR_CHOOSER_MODEL_COLUMN_GICON,
                    icon,
                    EDITOR_CHOOSER_MODEL_COLUMN_NAME,
                    name,
                    EDITOR_CHOOSER_MODEL_COLUMN_APPLICATION,
                    app_infos_iter->data,
                    EDITOR_CHOOSER_MODEL_COLUMN_WEIGHT_SET, FALSE,
                    EDITOR_CHOOSER_MODEL_COLUMN_STYLE_SET, FALSE,
                    -1);
            if (NULL != icon)
                g_object_unref (icon);
        }
        app_infos_iter = g_list_next (app_infos_iter);
    }

    icon = g_themed_icon_new ("application-x-executable");
    gtk_tree_store_append (tree_store, &parent_iter, NULL);
    gtk_tree_store_set (
            tree_store,
            &parent_iter,
            EDITOR_CHOOSER_MODEL_COLUMN_GICON,
            icon,
            EDITOR_CHOOSER_MODEL_COLUMN_NAME,
            _("Other Applications"),
            EDITOR_CHOOSER_MODEL_COLUMN_APPLICATION,
            NULL,
            EDITOR_CHOOSER_MODEL_COLUMN_WEIGHT,
            PANGO_WEIGHT_SEMIBOLD,
            EDITOR_CHOOSER_MODEL_COLUMN_WEIGHT_SET,
            TRUE,
            EDITOR_CHOOSER_MODEL_COLUMN_STYLE_SET,
            FALSE,
            -1);
    if (NULL != icon)
        g_object_unref (icon);

    app_infos_iter = app_infos_all;
    while (app_infos_iter)
    {
        if (g_list_find_custom (app_infos_recommended,
                                app_infos_iter->data,
                                cb_compare_app_infos) == NULL)
        {
            id = g_app_info_get_id (app_infos_iter->data);
            /* Do not add ristretto to the list */
            if (strcmp (id, RISTRETTO_DESKTOP_ID))
            {
                icon = g_app_info_get_icon (app_infos_iter->data);
                name = g_app_info_get_display_name (app_infos_iter->data),
                gtk_tree_store_append (tree_store, &iter, &parent_iter);
                gtk_tree_store_set (
                        tree_store,
                        &iter,
                        EDITOR_CHOOSER_MODEL_COLUMN_GICON,
                        icon,
                        EDITOR_CHOOSER_MODEL_COLUMN_NAME,
                        name,
                        EDITOR_CHOOSER_MODEL_COLUMN_APPLICATION,
                        app_infos_iter->data,
                        EDITOR_CHOOSER_MODEL_COLUMN_WEIGHT_SET, FALSE,
                        EDITOR_CHOOSER_MODEL_COLUMN_STYLE_SET, FALSE,
                        -1);
                if (NULL != icon)
                    g_object_unref (icon);
            }
        }
        app_infos_iter = g_list_next (app_infos_iter);
    }

    gtk_tree_view_expand_all (GTK_TREE_VIEW (treeview));
    gtk_widget_set_vexpand (GTK_WIDGET (treeview), TRUE);
    gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
    gtk_container_add (GTK_CONTAINER (scrolled_window), treeview);

    gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), check_button, FALSE, FALSE, 0);
    gtk_container_add (GTK_CONTAINER (content_area), vbox);

    gtk_widget_show_all (content_area);

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
        if (gtk_tree_selection_get_selected (selection, NULL, &iter))
        {
            gtk_tree_model_get (GTK_TREE_MODEL (tree_store), &iter, 2, &app_info, -1);
            if (app_info != NULL)
            {
                g_app_info_launch (G_APP_INFO (app_info), files, NULL, NULL);
                if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check_button)))
                {
                    rstto_mime_db_store (window->priv->db, content_type, g_app_info_get_id (G_APP_INFO (app_info)));
                }
            }
        }
    }

    gtk_widget_destroy (dialog);

    g_list_free_full (app_infos_recommended, g_object_unref);
    g_list_free_full (app_infos_all, g_object_unref);
    g_list_free (files);
}


static void
cb_rstto_main_window_copy_image (
        GtkWidget *widget,
        RsttoMainWindow *window)
{
    GdkPixbuf *pixbuf;
    RsttoImageViewer *viewer;

    viewer = RSTTO_IMAGE_VIEWER (window->priv->image_viewer);
    pixbuf = rstto_image_viewer_get_pixbuf (viewer);

    if (pixbuf)
    {
        gtk_clipboard_set_image (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), pixbuf);
        g_object_unref (pixbuf);
    }
}

static void
cb_rstto_main_window_clear_private_data (
        GtkWidget *widget,
        RsttoMainWindow *window)
{
    GtkRecentFilter *recent_filter;
    gsize n_uris = 0;
    gchar **uris = NULL;
    guint i = 0;

    GtkWidget *dialog = rstto_privacy_dialog_new (GTK_WINDOW (window), window->priv->recent_manager);

    recent_filter = gtk_recent_filter_new ();
    gtk_recent_filter_add_application (recent_filter, PACKAGE_NAME);
    gtk_recent_chooser_add_filter (GTK_RECENT_CHOOSER (dialog), recent_filter);

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
        uris = gtk_recent_chooser_get_uris (GTK_RECENT_CHOOSER (dialog), &n_uris);
        for (i = 0; i < n_uris; ++i)
        {
            gtk_recent_manager_remove_item (window->priv->recent_manager, uris[i], NULL);
        }
        g_strfreev (uris);
    }

    gtk_widget_destroy (dialog);
}

static void
cb_rstto_wrap_images_changed (
        GObject *object,
        GParamSpec *pspec,
        gpointer user_data)
{
    rstto_main_window_update_buttons (user_data);
}

static void
cb_rstto_desktop_type_changed (
        GObject *object,
        GParamSpec *pspec,
        gpointer user_data)
{
    RsttoMainWindow *window = user_data;
    gchar *desktop_type = NULL;

    if (window->priv->wallpaper_manager)
    {
        g_object_unref (window->priv->wallpaper_manager);
        window->priv->wallpaper_manager = NULL;
    }

    desktop_type = rstto_settings_get_string_property (window->priv->settings_manager, "desktop-type");

    if (desktop_type)
    {
        if (!g_ascii_strcasecmp (desktop_type, "xfce"))
        {
            window->priv->wallpaper_manager = rstto_xfce_wallpaper_manager_new ();
        }

        if (!g_ascii_strcasecmp (desktop_type, "gnome"))
        {
            window->priv->wallpaper_manager = rstto_gnome_wallpaper_manager_new ();
        }

        if (!g_ascii_strcasecmp (desktop_type, "none"))
        {
            window->priv->wallpaper_manager = NULL;
        }

        g_free (desktop_type);
        desktop_type = NULL;
    }
    else
    {
        /* Default to xfce */
        window->priv->wallpaper_manager = rstto_xfce_wallpaper_manager_new ();
    }

    rstto_main_window_update_buttons (window);
}


gboolean
rstto_main_window_play_slideshow (RsttoMainWindow *window)
{
    guint timeout;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gtk_ui_manager_add_ui (window->priv->ui_manager,
                           window->priv->pause_merge_id,
                           "/main-menu/go-menu/placeholder-slideshow",
                           "pause",
                           "pause",
                           GTK_UI_MANAGER_MENUITEM,
                           FALSE);
    gtk_ui_manager_remove_ui (window->priv->ui_manager,
                              window->priv->play_merge_id);

    gtk_ui_manager_add_ui (
            window->priv->ui_manager,
            window->priv->toolbar_pause_merge_id,
            "/file-toolbar/placeholder-slideshow",
            "pause",
            "pause",
            GTK_UI_MANAGER_TOOLITEM,
            FALSE);
    gtk_ui_manager_remove_ui (
            window->priv->ui_manager,
            window->priv->toolbar_play_merge_id);
G_GNUC_END_IGNORE_DEPRECATIONS

    g_object_get (window->priv->settings_manager, "slideshow-timeout", &timeout, NULL);

    window->priv->playing = TRUE;
    g_timeout_add (timeout * 1000, cb_rstto_main_window_play_slideshow,
                   rstto_util_source_autoremove (window));

    return TRUE;
}

static void
cb_icon_bar_selection_changed (
        RsttoIconBar *icon_bar,
        gpointer user_data)
{
    RsttoMainWindow *window = user_data;

    gint position = rstto_image_list_iter_get_position (window->priv->iter);
    gint selection = rstto_icon_bar_get_active (RSTTO_ICON_BAR (window->priv->thumbnailbar));

    if (position != selection)
    {
        rstto_image_list_iter_set_position (window->priv->iter, selection);
    }
}

static gint
cb_compare_app_infos (
        gconstpointer a,
        gconstpointer b)
{
    return g_app_info_equal (G_APP_INFO (a), G_APP_INFO (b)) ? 0 : 1;
}
