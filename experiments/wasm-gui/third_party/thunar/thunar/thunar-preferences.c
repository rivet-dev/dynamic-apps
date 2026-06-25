/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar/thunar-enum-types.h>
#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <xfconf/xfconf.h>


/* Property identifiers */
enum
{
  PROP_0,
  PROP_DEFAULT_VIEW,
  PROP_HIDDEN_DEVICES,
  PROP_HIDDEN_BOOKMARKS,
  PROP_LAST_RESTORE_TABS,
  PROP_LAST_TABS_LEFT,
  PROP_LAST_TABS_RIGHT,
  PROP_LAST_FOCUSED_TAB_LEFT,
  PROP_LAST_FOCUSED_TAB_RIGHT,
  PROP_LAST_COMPACT_VIEW_ZOOM_LEVEL,
  PROP_LAST_DETAILS_VIEW_COLUMN_ORDER,
  PROP_LAST_DETAILS_VIEW_COLUMN_WIDTHS,
  PROP_LAST_DETAILS_VIEW_FIXED_COLUMNS,
  PROP_LAST_DETAILS_VIEW_VISIBLE_COLUMNS,
  PROP_LAST_DETAILS_VIEW_ZOOM_LEVEL,
  PROP_LAST_ICON_VIEW_ZOOM_LEVEL,
  PROP_LAST_LOCATION_BAR,
  PROP_LAST_MENUBAR_VISIBLE,
  PROP_LAST_SEPARATOR_POSITION,
  PROP_LAST_SPLITVIEW_SEPARATOR_POSITION,
  PROP_LAST_SHOW_HIDDEN,
  PROP_LAST_SIDE_PANE,
  PROP_LAST_SORT_COLUMN,
  PROP_LAST_SORT_ORDER,
  PROP_LAST_STATUSBAR_VISIBLE,
  PROP_LAST_IMAGE_PREVIEW_VISIBLE,
  PROP_LAST_VIEW,
  PROP_LAST_WINDOW_HEIGHT,
  PROP_LAST_WINDOW_WIDTH,
  PROP_LAST_WINDOW_FULLSCREEN,
  PROP_LAST_TOOLBAR_VISIBLE_BUTTONS,
  PROP_LAST_TOOLBAR_BUTTON_ORDER,
  PROP_MISC_DIRECTORY_SPECIFIC_SETTINGS,
  PROP_MISC_ALWAYS_SHOW_TABS,
  PROP_MISC_VOLUME_MANAGEMENT,
  PROP_MISC_CASE_SENSITIVE,
  PROP_MISC_DATE_STYLE,
  PROP_MISC_DATE_CUSTOM_STYLE,
  PROP_EXEC_SHELL_SCRIPTS_BY_DEFAULT,
  PROP_MISC_FOLDERS_FIRST,
  PROP_MISC_FOLDER_ITEM_COUNT,
  PROP_MISC_FULL_PATH_IN_TAB_TITLE,
  PROP_MISC_FULL_PATH_IN_WINDOW_TITLE,
  PROP_MISC_HORIZONTAL_WHEEL_NAVIGATES,
  PROP_MISC_IMAGE_SIZE_IN_STATUSBAR,
  PROP_MISC_MIDDLE_CLICK_IN_TAB,
  PROP_MISC_OPEN_NEW_WINDOW_AS_TAB,
  PROP_MISC_RECURSIVE_PERMISSIONS,
  PROP_MISC_RECURSIVE_SEARCH,
  PROP_MISC_REMEMBER_GEOMETRY,
  PROP_MISC_SHOW_ABOUT_TEMPLATES,
  PROP_MISC_SHOW_DELETE_ACTION,
  PROP_MISC_SINGLE_CLICK,
  PROP_MISC_SINGLE_CLICK_TIMEOUT,
  PROP_MISC_SMALL_TOOLBAR_ICONS,
  PROP_MISC_TAB_CLOSE_MIDDLE_CLICK,
  PROP_MISC_TEXT_BESIDE_ICONS,
  PROP_MISC_THUMBNAIL_MODE,
  PROP_MISC_THUMBNAIL_DRAW_FRAMES,
  PROP_MISC_THUMBNAIL_MAX_FILE_SIZE,
  PROP_MISC_FILE_SIZE_BINARY,
  PROP_MISC_CONFIRM_CLOSE_MULTIPLE_TABS,
  PROP_MISC_STATUS_BAR_ACTIVE_INFO,
  PROP_MISC_PARALLEL_COPY_MODE,
  PROP_MISC_WINDOW_ICON,
  PROP_MISC_TRANSFER_USE_PARTIAL,
  PROP_MISC_TRANSFER_VERIFY_FILE,
  PROP_MISC_IMAGE_PREVIEW_FULL,
  PROP_SHORTCUTS_ICON_EMBLEMS,
  PROP_SHORTCUTS_ICON_SIZE,
  PROP_TREE_ICON_EMBLEMS,
  PROP_TREE_ICON_SIZE,
  PROP_MISC_SWITCH_TO_NEW_TAB,
  PROP_MISC_VERTICAL_SPLIT_PANE,
  PROP_MISC_OPEN_NEW_WINDOWS_IN_SPLIT_VIEW,
  PROP_MISC_COMPACT_VIEW_MAX_CHARS,
  PROP_MISC_HIGHLIGHTING_ENABLED,
  PROP_MISC_UNDO_REDO_HISTORY_SIZE,
  PROP_MISC_MAX_NUMBER_OF_TEMPLATES,
  N_PROPERTIES,
};



static void     thunar_preferences_finalize           (GObject                *object);
static void     thunar_preferences_get_property       (GObject                *object,
                                                       guint                   prop_id,
                                                       GValue                 *value,
                                                       GParamSpec             *pspec);
static void     thunar_preferences_set_property       (GObject                *object,
                                                       guint                   prop_id,
                                                       const GValue           *value,
                                                       GParamSpec             *pspec);
static void     thunar_preferences_prop_changed       (XfconfChannel          *channel,
                                                       const gchar            *prop_name,
                                                       const GValue           *value,
                                                       ThunarPreferences      *preferences);
static void     thunar_preferences_load_rc_file       (ThunarPreferences      *preferences);



struct _ThunarPreferencesClass
{
  GObjectClass __parent__;
};

struct _ThunarPreferences
{
  GObject __parent__;

  XfconfChannel *channel;

  gulong         property_changed_id;
};



/* don't do anything in case xfconf_init() failed */
static gboolean no_xfconf = FALSE;



G_DEFINE_TYPE (ThunarPreferences, thunar_preferences, G_TYPE_OBJECT)



static GParamSpec *preferences_props[N_PROPERTIES] = { NULL, };



static void
thunar_preferences_class_init (ThunarPreferencesClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_preferences_finalize;
  gobject_class->get_property = thunar_preferences_get_property;
  gobject_class->set_property = thunar_preferences_set_property;

  /**
   * ThunarPreferences:default-view:
   *
   * The name of the widget class, which should be used for the
   * view pane in new #ThunarWindow<!---->s or "void" to use the
   * last selected view from the "last-view" preference.
   **/
  preferences_props[PROP_DEFAULT_VIEW] =
      g_param_spec_string ("default-view",
                           "DefaultView",
                           NULL,
                           "void",
                           EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:hidden-bookmarks:
   *
   * List of URI's that are hidden in the bookmarks (obtained from ~/.gtk-bookmarks).
   * If an URI is not in the bookmarks file it will be removed from this list.
   **/
  preferences_props[PROP_HIDDEN_BOOKMARKS] =
      g_param_spec_boxed ("hidden-bookmarks",
                          NULL,
                          NULL,
                          G_TYPE_STRV,
                          EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:hidden-devices:
   *
   * List of hidden devices. The value could be an UUID or name.
   * Visibility of the device can be obtained with
   * thunar_device_get_hidden().
   **/
  preferences_props[PROP_HIDDEN_DEVICES] =
      g_param_spec_boxed ("hidden-devices",
                          NULL,
                          NULL,
                          G_TYPE_STRV,
                          EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:last-restore-tabs:
   *
   * %TRUE to restore the tabs as they were before closing Thunar.
   **/
  preferences_props[PROP_LAST_RESTORE_TABS] =
      g_param_spec_boolean ("last-restore-tabs",
                            "LastRestoreTabs",
                            NULL,
                            FALSE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:last-tabs-left:
   *
   * List of URI's that are used to reopen tabs on restart. There is one URI for each tab/folder that was open at the time
   * of the last program exit. This preference holds the tabs of the default view (or the left split-view).
   **/
  preferences_props[PROP_LAST_TABS_LEFT] =
      g_param_spec_boxed ("last-tabs-left",
                          NULL,
                          NULL,
                          G_TYPE_STRV,
                          EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:last-tabs-right:
   *
   * List of URI's that are used to reopen tabs on restart. There is one URI for each tab/folder that was open at the time
   * of the last program exit. This preference holds the tabs of the right split-view.
   **/
  preferences_props[PROP_LAST_TABS_RIGHT] =
        g_param_spec_boxed ("last-tabs-right",
                            NULL,
                            NULL,
                            G_TYPE_STRV,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:last-focused-tab-left:
   *
   * The index (starting from 0) of the last focused tab in left split-view
   **/
  preferences_props[PROP_LAST_FOCUSED_TAB_LEFT] =
      g_param_spec_int ("last-focused-tab-left",
                         "LastFocusedTabLeft",
                         NULL,
                         0, G_MAXINT, 0,
                         EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:last-focused-tab-right:
   *
   * The index (starting from 0) of the last focused tab in right split-view
   **/
  preferences_props[PROP_LAST_FOCUSED_TAB_RIGHT] =
      g_param_spec_int ("last-focused-tab-right",
                         "LastFocusedTabRight",
                         NULL,
                         0, G_MAXINT, 0,
                         EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:last-compact-view-zoom-level:
   *
   * The last selected #ThunarZoomLevel for the #ThunarCompactView.
   **/
  preferences_props[PROP_LAST_COMPACT_VIEW_ZOOM_LEVEL] =
      g_param_spec_enum ("last-compact-view-zoom-level",
                         "LastCompactViewZoomLevel",
                         NULL,
                         THUNAR_TYPE_ZOOM_LEVEL,
                         THUNAR_ZOOM_LEVEL_25_PERCENT,
                         EXO_PARAM_READWRITE);


  /**
   * ThunarPreferences:last-details-view-column-order:
   *
   * The comma separated list of columns that specifies the order of the
   * columns in the #ThunarDetailsView.
   **/
  preferences_props[PROP_LAST_DETAILS_VIEW_COLUMN_ORDER] =
      g_param_spec_string ("last-details-view-column-order",
                           "LastDetailsViewColumnOrder",
                           NULL,
                           "THUNAR_COLUMN_NAME,THUNAR_COLUMN_SIZE,THUNAR_COLUMN_SIZE_IN_BYTES,THUNAR_COLUMN_TYPE,THUNAR_COLUMN_DATE_MODIFIED",
                           EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:last-details-view-column-widths:
   *
   * The comma separated list of column widths used for fixed width
   * #ThunarDetailsView<!---->s.
   **/
  preferences_props[PROP_LAST_DETAILS_VIEW_COLUMN_WIDTHS] =
      g_param_spec_string ("last-details-view-column-widths",
                           "LastDetailsViewColumnWidths",
                           NULL,
                           "",
                           EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:last-details-view-fixed-columns:
   *
   * %TRUE to use fixed column widths in the #ThunarDetailsView. Else the
   * column widths will be automatically determined from the model contents.
   **/
  preferences_props[PROP_LAST_DETAILS_VIEW_FIXED_COLUMNS] =
      g_param_spec_boolean ("last-details-view-fixed-columns",
                            "LastDetailsViewFixedColumns",
                            NULL,
                            FALSE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:last-details-view-visible-columns:
   *
   * The comma separated list of visible columns in the #ThunarDetailsView.
   **/
  preferences_props[PROP_LAST_DETAILS_VIEW_VISIBLE_COLUMNS] =
      g_param_spec_string ("last-details-view-visible-columns",
                           "LastDetailsViewVisibleColumns",
                           NULL,
                           "THUNAR_COLUMN_DATE_MODIFIED,THUNAR_COLUMN_NAME,THUNAR_COLUMN_SIZE,THUNAR_COLUMN_TYPE",
                           EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:last-details-view-zoom-level:
   *
   * The last selected #ThunarZoomLevel for the #ThunarDetailsView.
   **/
  preferences_props[PROP_LAST_DETAILS_VIEW_ZOOM_LEVEL] =
      g_param_spec_enum ("last-details-view-zoom-level",
                         "LastDetailsViewZoomLevel",
                         NULL,
                         THUNAR_TYPE_ZOOM_LEVEL,
                         THUNAR_ZOOM_LEVEL_38_PERCENT,
                         EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:last-icon-view-zoom-level:
   *
   * The last selected #ThunarZoomLevel for the #ThunarIconView.
   **/
  preferences_props[PROP_LAST_ICON_VIEW_ZOOM_LEVEL] =
      g_param_spec_enum ("last-icon-view-zoom-level",
                         "LastIconViewZoomLevel",
                         NULL,
                         THUNAR_TYPE_ZOOM_LEVEL,
                         THUNAR_ZOOM_LEVEL_100_PERCENT,
                         EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:last-location-bar:
   *
   * The name of the widget class, which should be used for the
   * location bar in #ThunarWindow<!---->s or "void" to hide the
   * location bar.
   **/
  preferences_props[PROP_LAST_LOCATION_BAR] =
      g_param_spec_string ("last-location-bar",
                           "LastLocationBar",
                           NULL,
                           "ThunarLocationEntry",
                           EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:last-menubar-visible:
   *
   * Whether to display a menubar in new windows by default.
   **/
  preferences_props[PROP_LAST_MENUBAR_VISIBLE] =
      g_param_spec_boolean ("last-menubar-visible",
                            "LastMenubarVisible",
                            NULL,
                            TRUE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:last-separator-position:
   *
   * The last position of the gutter in the main window,
   * which separates the side pane from the main view.
   **/
  preferences_props[PROP_LAST_SEPARATOR_POSITION] =
      g_param_spec_int ("last-separator-position",
                        "LastSeparatorPosition",
                        NULL,
                        0, G_MAXINT, 170,
                        EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:last-splitview-separator-position:
   *
   * The last position of the gutter in the main window,
   * which separates the notebooks in split-view.
   **/
  preferences_props[PROP_LAST_SPLITVIEW_SEPARATOR_POSITION] =
      g_param_spec_int ("last-splitview-separator-position",
                        "LastSplitviewSeparatorPosition",
                        NULL,
                        0, G_MAXINT, 0,
                        EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:last-show-hidden:
   *
   * Whether to show hidden files by default in new windows.
   **/
  preferences_props[PROP_LAST_SHOW_HIDDEN] =
      g_param_spec_boolean ("last-show-hidden",
                            "LastShowHidden",
                            NULL,
                            FALSE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:last-toolbar-visible-buttons:
   *
   * The comma separated list that specifies the visibility of toolbar items.
   * The order of the default value corresponds to the order in which the buttons are added inside 'thunar_window_location_toolbar_create'
   **/
    preferences_props[PROP_LAST_TOOLBAR_VISIBLE_BUTTONS] =
        g_param_spec_string ("last-toolbar-visible-buttons",
                             "LastToolbarVisibleButtons",
                             NULL,
                             "0,1,1,1,1,0,0,0,0,0,0,0,0,0,1,0,1",
                             EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:last-toolbar-button-order:
   *
   * The comma separated list that specifies the order of toolbar items.
   **/
    preferences_props[PROP_LAST_TOOLBAR_BUTTON_ORDER] =
        g_param_spec_string ("last-toolbar-item-order",
                             "LastToolbarButtonOrder",
                             NULL,
                             "0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16",
                             EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-directory-specific-settings:
   *
   * Whether to use directory specific settings.
   **/
  preferences_props[PROP_MISC_DIRECTORY_SPECIFIC_SETTINGS] =
      g_param_spec_boolean ("misc-directory-specific-settings",
                            "MiscDirectorySpecificSettings",
                            NULL,
                            FALSE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:last-side-pane:
   *
   * The name of the widget class, which should be used for the
   * side pane in #ThunarWindow<!---->s or "void" to hide the
   * side pane completely.
   **/
  preferences_props[PROP_LAST_SIDE_PANE] =
      g_param_spec_string ("last-side-pane",
                           "LastSidePane",
                           NULL,
                           "ThunarShortcutsPane",
                           EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:last-sort-column:
   *
   * The default sort column for new views.
   **/
  preferences_props[PROP_LAST_SORT_COLUMN] =
      g_param_spec_enum ("last-sort-column",
                         "LastSortColumn",
                         NULL,
                         THUNAR_TYPE_COLUMN,
                         THUNAR_COLUMN_NAME,
                         EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:last-sort-order:
   *
   * The default sort order for new views.
   **/
  preferences_props[PROP_LAST_SORT_ORDER] =
      g_param_spec_enum ("last-sort-order",
                         "LastSortOrder",
                         NULL,
                         GTK_TYPE_SORT_TYPE,
                         GTK_SORT_ASCENDING,
                         EXO_PARAM_READWRITE);
  /**
   * ThunarPreferences:last-statusbar-visible:
   *
   * Whether to display a statusbar in new windows by default.
   **/
  preferences_props[PROP_LAST_STATUSBAR_VISIBLE] =
      g_param_spec_boolean ("last-statusbar-visible",
                            "LastStatusbarVisible",
                            NULL,
                            TRUE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:last-image-preview-visible:
   *
   * Whether to display an image preview in new windows by default.
   **/
  preferences_props[PROP_LAST_IMAGE_PREVIEW_VISIBLE] =
      g_param_spec_boolean ("last-image-preview-visible",
                            "LastImagePreviewVisible",
                            NULL,
                            FALSE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:last-view:
   *
   * The name of the widget class, which should be used for the
   * main view component in #ThunarWindow<!---->s.
   **/
  preferences_props[PROP_LAST_VIEW] =
      g_param_spec_string ("last-view",
                           "LastView",
                           NULL,
                           "ThunarIconView",
                           EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:last-window-height:
   *
   * The last known height of a #ThunarWindow, which will be used as
   * default height for newly created windows.
   **/
  preferences_props[PROP_LAST_WINDOW_HEIGHT] =
      g_param_spec_int ("last-window-height",
                        "LastWindowHeight",
                        NULL,
                        1, G_MAXINT, 480,
                        EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:last-window-width:
   *
   * The last known width of a #ThunarWindow, which will be used as
   * default width for newly created windows.
   **/
  preferences_props[PROP_LAST_WINDOW_WIDTH] =
      g_param_spec_int ("last-window-width",
                        "LastWindowWidth",
                        NULL,
                        1, G_MAXINT, 640,
                        EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:last-window-maximized:
   *
   * The last known maximized state of a #ThunarWindow, which will be used as
   * default width for newly created windows.
   **/
  preferences_props[PROP_LAST_WINDOW_FULLSCREEN] =
      g_param_spec_boolean ("last-window-maximized",
                            "LastWindowMaximized",
                            NULL,
                            FALSE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-always-show-tabs:
   *
   * If the view tabs should always be visible.
   **/
  preferences_props[PROP_MISC_ALWAYS_SHOW_TABS] =
      g_param_spec_boolean ("misc-always-show-tabs",
                            NULL,
                            NULL,
                            FALSE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-volume-management:
   *
   * Whether to enable volume management capabilities (requires HAL and the
   * thunar-volman package).
   **/
  preferences_props[PROP_MISC_VOLUME_MANAGEMENT] =
      g_param_spec_boolean ("misc-volume-management",
                            "MiscVolumeManagement",
                            NULL,
                            TRUE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-case-sensitive:
   *
   * Whether to use case-sensitive sort.
   **/
  preferences_props[PROP_MISC_CASE_SENSITIVE] =
      g_param_spec_boolean ("misc-case-sensitive",
                            "MiscCaseSensitive",
                            NULL,
                            FALSE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-date-style:
   *
   * The style used to display dates in the user interface.
   **/
  preferences_props[PROP_MISC_DATE_STYLE] =
      g_param_spec_enum ("misc-date-style",
                         "MiscDateStyle",
                         NULL,
                         THUNAR_TYPE_DATE_STYLE,
                         THUNAR_DATE_STYLE_SIMPLE,
                         EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-date-custom-style:
   *
   * If 'custom' is selected as date format, this date-style will be used
   **/
  preferences_props[PROP_MISC_DATE_CUSTOM_STYLE] =
      g_param_spec_string ("misc-date-custom-style",
                           "MiscDateCustomStyle",
                           NULL,
                           "%Y-%m-%d %H:%M:%S",
                           EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-execute-shell-scripts-by-default:
   *
   * Shell scripts are often unsafe to execute, require additional
   * parameters and most users will only want to edit them in their
   * favorite editor, so the default is to open them in the associated
   * application. Setting this to TRUE allows executing them, like
   * binaries, by default. See bug #7596.
   **/
  preferences_props[PROP_EXEC_SHELL_SCRIPTS_BY_DEFAULT] =
      g_param_spec_boolean ("misc-exec-shell-scripts-by-default",
                            "MiscExecShellScriptsByDefault",
                            NULL,
                            FALSE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-folders-first:
   *
   * Whether to sort folders before files.
   **/
  preferences_props[PROP_MISC_FOLDERS_FIRST] =
      g_param_spec_boolean ("misc-folders-first",
                            "MiscFoldersFirst",
                            NULL,
                            TRUE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-folder-item-count
   *
   * Tells when the size column of folders should show the number of containing files
   **/
  preferences_props[PROP_MISC_FOLDER_ITEM_COUNT] =
      g_param_spec_enum ("misc-folder-item-count",
                         "MiscFolderItemCount",
                         NULL,
                         THUNAR_TYPE_FOLDER_ITEM_COUNT,
                         THUNAR_FOLDER_ITEM_COUNT_NEVER,
                         EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-full-path-in-tab-title:
   *
   * Show the full directory path in the tab title, instead of
   * only the directory name.
   **/
  preferences_props[PROP_MISC_FULL_PATH_IN_TAB_TITLE] =
      g_param_spec_boolean ("misc-full-path-in-tab-title",
                            "MiscFullPathInTabTitle",
                            NULL,
                            FALSE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-full-path-in-window-title:
   *
   * Show the full directory path in the window title, instead of
   * only the directory name.
   **/
  preferences_props[PROP_MISC_FULL_PATH_IN_WINDOW_TITLE] =
      g_param_spec_boolean ("misc-full-path-in-window-title",
                            "MiscFullPathInWindowTitle",
                            NULL,
                            FALSE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-horizontal-wheel-navigates:
   *
   * Whether the horizontal mouse wheel is used to navigate
   * forth and back within a Thunar view.
   **/
  preferences_props[PROP_MISC_HORIZONTAL_WHEEL_NAVIGATES] =
      g_param_spec_boolean ("misc-horizontal-wheel-navigates",
                            "MiscHorizontalWheelNavigates",
                            NULL,
                            FALSE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-image-size-in-statusbar:
   *
   * When a single image file is selected, show its size
   * in the statusbar. This heavily increases I/O in image
   * folders when moving the selection across files.
   **/
  preferences_props[PROP_MISC_IMAGE_SIZE_IN_STATUSBAR] =
      g_param_spec_boolean ("misc-image-size-in-statusbar",
                            "MiscImageSizeInStatusbar",
                            NULL,
                            FALSE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-middle-click-in-tab:
   *
   * If middle click opens a folder in a new window (FALSE) or in a new window (TRUE);
   **/
  preferences_props[PROP_MISC_MIDDLE_CLICK_IN_TAB] =
      g_param_spec_boolean ("misc-middle-click-in-tab",
                            NULL,
                            NULL,
                            FALSE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-open-new-window-as_tab:
   *
   * %TRUE to open a new tab in an existing thunar instance. instead of a new window
   * if a thunar windows already is present
   **/
  preferences_props[PROP_MISC_OPEN_NEW_WINDOW_AS_TAB] =
      g_param_spec_boolean ("misc-open-new-window-as-tab",
                            "MiscOpenNewWindowAsTab",
                            NULL,
                            FALSE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-recursive-permissions:
   *
   * Whether to apply permissions recursively every time the
   * permissions are altered by the user.
   **/
  preferences_props[PROP_MISC_RECURSIVE_PERMISSIONS] =
      g_param_spec_enum ("misc-recursive-permissions",
                         "MiscRecursivePermissions",
                         NULL,
                         THUNAR_TYPE_RECURSIVE_PERMISSIONS,
                         THUNAR_RECURSIVE_PERMISSIONS_ASK,
                         EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-recursive-search:
   *
   * Whether to search only in the current folder or recursively
  **/
  preferences_props[PROP_MISC_RECURSIVE_SEARCH] =
       g_param_spec_enum ("misc-recursive-search",
                          "MiscRecursiveSearch",
                          NULL,
                          THUNAR_TYPE_RECURSIVE_SEARCH,
                          THUNAR_RECURSIVE_SEARCH_ALWAYS,
                          EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-remember-geometry:
   *
   * Whether Thunar should remember the size of windows and apply
   * that size to new windows. If %TRUE the width and height are
   * saved to "last-window-width" and "last-window-height". If
   * %FALSE the user may specify the start size in "last-window-with"
   * and "last-window-height".
   **/
  preferences_props[PROP_MISC_REMEMBER_GEOMETRY] =
      g_param_spec_boolean ("misc-remember-geometry",
                            "MiscRememberGeometry",
                            NULL,
                            TRUE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-show-about-templates:
   *
   * Whether to display the "About Templates" dialog, when opening the
   * Templates folder from the Go menu.
   **/
  preferences_props[PROP_MISC_SHOW_ABOUT_TEMPLATES] =
      g_param_spec_boolean ("misc-show-about-templates",
                            "MiscShowAboutTemplates",
                            NULL,
                            TRUE,
                            EXO_PARAM_READWRITE);

 /**
   * ThunarPreferences:misc-show-delete-action:
   *
   * Whether to display a "delete" action to permanently delete files and folders
   * If trash is not supported, "delete" is displayed by default.
   **/
  preferences_props[PROP_MISC_SHOW_DELETE_ACTION] =
      g_param_spec_boolean ("misc-show-delete-action",
                            "MiscShowDeleteAction",
                            NULL,
                            !thunar_g_vfs_is_uri_scheme_supported ("trash"),
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-single-click:
   *
   * Whether to use single click navigation.
   **/
  preferences_props[PROP_MISC_SINGLE_CLICK] =
      g_param_spec_boolean ("misc-single-click",
                            "MiscSingleClick",
                            NULL,
                            FALSE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-single-click-timeout:
   *
   * If single-click mode is enabled this is the amount of time
   * in milliseconds after which the item under the mouse cursor
   * will be selected automatically. A value of %0 disables the
   * automatic selection.
   **/
  preferences_props[PROP_MISC_SINGLE_CLICK_TIMEOUT] =
      g_param_spec_uint ("misc-single-click-timeout",
                         "MiscSingleClickTimeout",
                         NULL,
                         0u, G_MAXUINT, 500u,
                         EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-small-toolbar-icons:
   *
   * Use small icons on the toolbar instead of the default toolbar size.
   **/
  preferences_props[PROP_MISC_SMALL_TOOLBAR_ICONS] =
      g_param_spec_boolean ("misc-small-toolbar-icons",
                            NULL,
                            NULL,
                            FALSE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-tab-close-middle-click:
   *
   * Whether to close tabs when the tab label is clicked with the 2nd
   * mouse button.
   **/
  preferences_props[PROP_MISC_TAB_CLOSE_MIDDLE_CLICK] =
      g_param_spec_boolean ("misc-tab-close-middle-click",
                            NULL,
                            NULL,
                            TRUE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-text-beside-icons:
   *
   * Whether the icon view should display the file names beside the
   * file icons instead of below the file icons.
   **/
  preferences_props[PROP_MISC_TEXT_BESIDE_ICONS] =
      g_param_spec_boolean ("misc-text-beside-icons",
                            "MiscTextBesideIcons",
                            NULL,
                            FALSE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-thumbnail-mode:
   *
   * Whether to generate and display thumbnails for previewable files.
   **/
  preferences_props[PROP_MISC_THUMBNAIL_MODE] =
      g_param_spec_enum ("misc-thumbnail-mode",
                         NULL,
                         NULL,
                         THUNAR_TYPE_THUMBNAIL_MODE,
                         THUNAR_THUMBNAIL_MODE_ONLY_LOCAL,
                         EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-thumbnail-draw-frames:
   *
   * Whether to draw black frames around thumbnails.
   * This looks neat, but will delay the first draw a bit.
   * May have an impact on older systems, on folders with many pictures.
   **/
  preferences_props[PROP_MISC_THUMBNAIL_DRAW_FRAMES] =
      g_param_spec_boolean ("misc-thumbnail-draw-frames",
                            NULL,
                            NULL,
                            FALSE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-thumbnail-max-file-size:
   *
   * Maximum file size (in bytes) allowed to be thumbnailed.
   * 0 means no limit is in place.
   **/
  preferences_props[PROP_MISC_THUMBNAIL_MAX_FILE_SIZE] =
      g_param_spec_uint64 ("misc-thumbnail-max-file-size",
                           NULL,
                           NULL,
                           0, G_MAXUINT64, 0,
                           EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-file-size-binary:
   *
   * Show file size in binary format instead of decimal.
   **/
  preferences_props[PROP_MISC_FILE_SIZE_BINARY] =
      g_param_spec_boolean ("misc-file-size-binary",
                            "MiscFileSizeBinary",
                            NULL,
                            TRUE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-parallel-copy-mode:
   *
   * Do parallel copy (or not) on files copy.
   **/
  preferences_props[PROP_MISC_PARALLEL_COPY_MODE] =
      g_param_spec_enum ("misc-parallel-copy-mode",
                         "MiscParallelCopyMode",
                         NULL,
                         THUNAR_TYPE_PARALLEL_COPY_MODE,
                         THUNAR_PARALLEL_COPY_MODE_ONLY_LOCAL,
                         EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-change-window-icon:
   *
   * Whether to change the window icon to the folder's icon.
   **/
  preferences_props[PROP_MISC_WINDOW_ICON] =
      g_param_spec_boolean ("misc-change-window-icon",
                            "MiscChangeWindowIcon",
                            NULL,
                            TRUE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-transfer-use-partial:
   *
   * Whether to use intermediate file(*.partial~) to copy.
   **/
  preferences_props[PROP_MISC_TRANSFER_USE_PARTIAL] =
    g_param_spec_enum ("misc-transfer-use-partial",
                       "MiscTransferUsePartial",
                       NULL,
                       THUNAR_TYPE_USE_PARTIAL_MODE,
                       THUNAR_USE_PARTIAL_MODE_DISABLED,
                       EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-transfer-verify-file:
   *
   * Whether to verify copied file with checksum.
   **/
  preferences_props[PROP_MISC_TRANSFER_VERIFY_FILE] =
    g_param_spec_enum ("misc-transfer-verify-file",
                       "MiscTransferVerifyFile",
                       NULL,
                       THUNAR_TYPE_VERIFY_FILE_MODE,
                       THUNAR_VERIFY_FILE_MODE_DISABLED,
                       EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-image-preview-mode:
   *
   * Whether the image preview functionality uses its own sidepane or is embedded
   * in the left sidepane.
   **/
  preferences_props[PROP_MISC_IMAGE_PREVIEW_FULL] =
    g_param_spec_enum ("misc-image-preview-mode",
                       "MiscImagePreviewMode",
                       NULL,
                       THUNAR_TYPE_IMAGE_PREVIEW_MODE,
                       THUNAR_IMAGE_PREVIEW_MODE_EMBEDDED,
                       EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-confirm-close-multiple-tabs:
   *
   * Ask the user for confirmation before closing a window with
   * multiple tabs.
   **/
  preferences_props[PROP_MISC_CONFIRM_CLOSE_MULTIPLE_TABS] =
      g_param_spec_boolean ("misc-confirm-close-multiple-tabs",
                            "ConfirmCloseMultipleTabs",
                            NULL,
                            TRUE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-status-bar-active-info:
   *
   * What kind of info is presented in the statusbar..
   **/
  preferences_props[PROP_MISC_STATUS_BAR_ACTIVE_INFO] =
      g_param_spec_uint ("misc-status-bar-active-info",
                         "MiscStatusBarActiveInfo",
                         NULL,
                         0, G_MAXUINT,
                         THUNAR_STATUS_BAR_INFO_DISPLAY_NAME | THUNAR_STATUS_BAR_INFO_FILETYPE | THUNAR_STATUS_BAR_INFO_SIZE
                         | THUNAR_STATUS_BAR_INFO_SIZE_IN_BYTES,
                         EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:shortcuts-icon-emblems:
   *
   * Whether to display emblems for file icons (if defined) in the
   * shortcuts side pane.
   **/
  preferences_props[PROP_SHORTCUTS_ICON_EMBLEMS] =
      g_param_spec_boolean ("shortcuts-icon-emblems",
                            "ShortcutsIconEmblems",
                            NULL,
                            TRUE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:shortcuts-icon-size:
   *
   * The icon size to use for the icons displayed in the
   * shortcuts side pane.
   **/
  preferences_props[PROP_SHORTCUTS_ICON_SIZE] =
      g_param_spec_enum ("shortcuts-icon-size",
                         "ShortcutsIconSize",
                         NULL,
                         THUNAR_TYPE_ICON_SIZE,
                         THUNAR_ICON_SIZE_24,
                         EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:tree-icon-emblems:
   *
   * Whether to display emblems for file icons (if defined) in the
   * tree side pane.
   **/
  preferences_props[PROP_TREE_ICON_EMBLEMS] =
      g_param_spec_boolean ("tree-icon-emblems",
                            "TreeIconEmblems",
                            NULL,
                            TRUE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:tree-icon-size:
   *
   * The icon size to use for the icons displayed in the
   * tree side pane.
   **/
  preferences_props[PROP_TREE_ICON_SIZE] =
      g_param_spec_enum ("tree-icon-size",
                         "TreeIconSize",
                         NULL,
                         THUNAR_TYPE_ICON_SIZE,
                         THUNAR_ICON_SIZE_16,
                         EXO_PARAM_READWRITE);

  /**
 * ThunarPreferences:misc-switch-to-new-tab:
 *
 * Whether to switch to the new tab after opening a folder in a new tab.
 **/
  preferences_props[PROP_MISC_SWITCH_TO_NEW_TAB] =
      g_param_spec_boolean ("misc-switch-to-new-tab",
                            "SwitchToNewTab",
                            NULL,
                            TRUE,
                            EXO_PARAM_READWRITE);



  /**
   * ThunarPreferences:misc-vertical-split-pane
   *
   * If true, split the thunar window vertically instead of horizontally
   * when splitting the thunar window into two panes.
   **/
  preferences_props[PROP_MISC_VERTICAL_SPLIT_PANE] =
    g_param_spec_boolean ("misc-vertical-split-pane",
                          "MiscVerticalSplitPane",
                          NULL,
                          FALSE,
                          EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-open-new-windows-in-split-view
   *
   * If true, all thunar windows will have split view enabled.
   **/
  preferences_props[PROP_MISC_OPEN_NEW_WINDOWS_IN_SPLIT_VIEW] =
    g_param_spec_boolean ("misc-open-new-windows-in-split-view",
                          "MiscOpenNewWindowsInSplitView",
                          NULL,
                          FALSE,
                          EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:compact-view-max-chars
   *
   * The ellipsization threshold for compact view. Negative values disable ellipsization.
   **/
  preferences_props[PROP_MISC_COMPACT_VIEW_MAX_CHARS] =
      g_param_spec_int ("misc-compact-view-max-chars",
                        "MiscCompactViewMaxChars",
                        NULL,
                        G_MININT, G_MAXINT,
                        100,
                        EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-highlighting-enabled
   *
   * If true file highlighting feature across the various views is enabled.
   * Can be toggled using the View > Show File Highlight.
   **/
  preferences_props[PROP_MISC_HIGHLIGHTING_ENABLED] =
      g_param_spec_boolean ("misc-highlighting-enabled",
                            "MiscHighlightingEnabled",
                            NULL,
                            thunar_g_vfs_metadata_is_supported (),
                            EXO_PARAM_READWRITE);

  /**
   * ThunarPreferences:misc-undo-redo-history-size
   *
   * Maximum number of ThunarJobOperations which can be undone/redone
   * -1 for unlimited
   **/
  preferences_props[PROP_MISC_UNDO_REDO_HISTORY_SIZE] =
      g_param_spec_int ("misc-undo-redo-history-size",
                        "MiscUndoRedoHistorySize",
                        NULL,
                        -1, G_MAXINT,
                        10,
                        EXO_PARAM_READWRITE);

   /**
   * ThunarPreferences:misc-max-number-of-templates
   *
   * Maximum number of templates for which will be scanned in the 'templates' directory
   * Required to prevent possible lag when thecontext menu is opened 
   **/
  preferences_props[PROP_MISC_MAX_NUMBER_OF_TEMPLATES] =
      g_param_spec_uint ("misc-max-number-of-templates",
                         "MiscMaxNumberOfTemplates",
                         NULL,
                         0, G_MAXUINT,
                         100,
                         EXO_PARAM_READWRITE);

  /* install all properties */
  g_object_class_install_properties (gobject_class, N_PROPERTIES, preferences_props);
}



static void
thunar_preferences_init (ThunarPreferences *preferences)
{
  const gchar check_prop[] = "/last-view";

  /* don't set a channel if xfconf init failed */
  if (no_xfconf)
    return;

  /* load the channel */
  preferences->channel = xfconf_channel_get ("thunar");

  /* check one of the property to see if there are values */
  if (!xfconf_channel_has_property (preferences->channel, check_prop))
    {
      /* try to load the old config file */
      thunar_preferences_load_rc_file (preferences);

      /* set the string we check */
      if (!xfconf_channel_has_property (preferences->channel, check_prop))
        xfconf_channel_set_string (preferences->channel, check_prop, "ThunarIconView");
    }

  preferences->property_changed_id =
    g_signal_connect (G_OBJECT (preferences->channel), "property-changed",
                      G_CALLBACK (thunar_preferences_prop_changed), preferences);
}



static void
thunar_preferences_finalize (GObject *object)
{
  ThunarPreferences *preferences = THUNAR_PREFERENCES (object);

  /* disconnect from the updates */
  g_signal_handler_disconnect (preferences->channel, preferences->property_changed_id);

  (*G_OBJECT_CLASS (thunar_preferences_parent_class)->finalize) (object);
}



static void
thunar_preferences_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ThunarPreferences  *preferences = THUNAR_PREFERENCES (object);
  GValue              src = { 0, };
  gchar               prop_name[64];
  gchar             **array;

  /* only set defaults if channel is not set */
  if (G_UNLIKELY (preferences->channel == NULL))
    {
      g_param_value_set_default (pspec, value);
      return;
    }

  /* build property name */
  g_snprintf (prop_name, sizeof (prop_name), "/%s", g_param_spec_get_name (pspec));

  if (G_VALUE_TYPE (value) == G_TYPE_STRV)
    {
      /* handle arrays directly since we cannot transform those */
      array = xfconf_channel_get_string_list (preferences->channel, prop_name);
      g_value_take_boxed (value, array);
    }
  else if (xfconf_channel_get_property (preferences->channel, prop_name, &src))
    {
      if (G_VALUE_TYPE (value) == G_VALUE_TYPE (&src))
        g_value_copy (&src, value);
      else if (!g_value_transform (&src, value))
        g_printerr ("Thunar: Failed to transform property %s\n", prop_name);
      g_value_unset (&src);
    }
  else
    {
      /* value is not found, return default */
      g_param_value_set_default (pspec, value);
    }
}



static void
thunar_preferences_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ThunarPreferences  *preferences = THUNAR_PREFERENCES (object);
  GValue              dst = { 0, };
  gchar               prop_name[64];
  gchar             **array;

  /* leave if the channel is not set */
  if (G_UNLIKELY (preferences->channel == NULL))
    return;

  /* build property name */
  g_snprintf (prop_name, sizeof (prop_name), "/%s", g_param_spec_get_name (pspec));

  /* freeze */
  g_signal_handler_block (preferences->channel, preferences->property_changed_id);

  if (G_VALUE_HOLDS_ENUM (value))
    {
      /* convert into a string */
      g_value_init (&dst, G_TYPE_STRING);
      if (g_value_transform (value, &dst))
        xfconf_channel_set_property (preferences->channel, prop_name, &dst);
      g_value_unset (&dst);
    }
  else if (G_VALUE_HOLDS (value, G_TYPE_STRV))
    {
      /* convert to a GValue GPtrArray in xfconf */
      array = g_value_get_boxed (value);
      if (array != NULL && *array != NULL)
        xfconf_channel_set_string_list (preferences->channel, prop_name, (const gchar * const *) array);
      else
        xfconf_channel_reset_property (preferences->channel, prop_name, FALSE);
    }
  else
    {
      /* other types we support directly */
      xfconf_channel_set_property (preferences->channel, prop_name, value);
    }

  /* thaw */
  g_signal_handler_unblock (preferences->channel, preferences->property_changed_id);
}



static void
thunar_preferences_prop_changed (XfconfChannel     *channel,
                                 const gchar       *prop_name,
                                 const GValue      *value,
                                 ThunarPreferences *preferences)
{
  GParamSpec *pspec;

  /* check if the property exists and emit change */
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (preferences), prop_name + 1);
  if (G_LIKELY (pspec != NULL))
    g_object_notify_by_pspec (G_OBJECT (preferences), pspec);
}



static void
thunar_preferences_load_rc_file (ThunarPreferences *preferences)
{
  GParamSpec  **pspecs;
  GParamSpec   *pspec;
  XfceRc       *rc;
  guint         nspecs, n;
  const gchar  *string;
  GValue        dst = { 0, };
  GValue        src = { 0, };
  gchar         prop_name[64];
  const gchar  *nick;
  gchar        *filename;

  /* find file */
  filename = xfce_resource_lookup (XFCE_RESOURCE_CONFIG, "Thunar/thunarrc");
  if (G_UNLIKELY (filename == NULL))
    return;

  /* look for preferences */
  rc = xfce_rc_simple_open (filename, TRUE);
  if (G_UNLIKELY (rc == NULL))
    return;

  xfce_rc_set_group (rc, "Configuration");

  pspecs = g_object_class_list_properties (G_OBJECT_GET_CLASS (preferences), &nspecs);
  for (n = 0; n < nspecs; ++n)
    {
      pspec = pspecs[n];

      /* continue if the nick is null */
      nick = g_param_spec_get_nick (pspec);
      if (G_UNLIKELY (nick == NULL))
        continue;

      /* read the value from the rc file */
      string = xfce_rc_read_entry (rc, nick, NULL);
      if (G_UNLIKELY (string == NULL))
        continue;

      /* xfconf property name, continue if exists */
      g_snprintf (prop_name, sizeof (prop_name), "/%s", g_param_spec_get_name (pspec));
      if (xfconf_channel_has_property (preferences->channel, prop_name))
        continue;

      /* source property */
      g_value_init (&src, G_TYPE_STRING);
      g_value_set_static_string (&src, string);

      /* store string and enums directly */
      if (G_IS_PARAM_SPEC_STRING (pspec) || G_IS_PARAM_SPEC_ENUM (pspec))
        {
          xfconf_channel_set_property (preferences->channel, prop_name, &src);
        }
      else if (g_value_type_transformable (G_TYPE_STRING, G_PARAM_SPEC_VALUE_TYPE (pspec)))
        {
          g_value_init (&dst, G_PARAM_SPEC_VALUE_TYPE (pspec));
          if (g_value_transform (&src, &dst))
            xfconf_channel_set_property (preferences->channel, prop_name, &dst);
          g_value_unset (&dst);
        }
      else
        {
          g_warning ("Failed to migrate property \"%s\"", g_param_spec_get_name (pspec));
        }

      g_value_unset (&src);
    }

  /* manually migrate the thumbnails property */
  if (!xfce_rc_read_bool_entry (rc, "MiscShowThumbnails", TRUE))
    {
      xfconf_channel_set_string (preferences->channel,
                                 "/misc-thumbnail-mode",
                                 "THUNAR_THUMBNAIL_MODE_NEVER");
    }

  g_free (pspecs);
  xfce_rc_close (rc);

  g_print ("\n\n"
           "Your Thunar settings have been migrated to Xfconf.\n"
           "The config file \"%s\"\n"
           "is not used anymore.\n\n", filename);

  g_free (filename);
}



/**
 * thunar_preferences_get:
 *
 * Queries the global #ThunarPreferences instance, which is shared
 * by all modules. The function automatically takes a reference
 * for the caller, so you'll need to call g_object_unref() when
 * you're done with it.
 *
 * Return value: the global #ThunarPreferences instance.
 **/
ThunarPreferences*
thunar_preferences_get (void)
{
  static ThunarPreferences *preferences = NULL;

  if (G_UNLIKELY (preferences == NULL))
    {
      preferences = g_object_new (THUNAR_TYPE_PREFERENCES, NULL);
      g_object_add_weak_pointer (G_OBJECT (preferences),
                                 (gpointer) &preferences);
    }
  else
    {
      g_object_ref (G_OBJECT (preferences));
    }

  return preferences;
}



void
thunar_preferences_xfconf_init_failed (void)
{
  no_xfconf = TRUE;
}
