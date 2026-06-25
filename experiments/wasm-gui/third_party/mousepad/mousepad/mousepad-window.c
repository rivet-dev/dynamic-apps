/*
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

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-settings.h>
#include <mousepad/mousepad-application.h>
#include <mousepad/mousepad-marshal.h>
#include <mousepad/mousepad-document.h>
#include <mousepad/mousepad-dialogs.h>
#include <mousepad/mousepad-replace-dialog.h>
#include <mousepad/mousepad-encoding-dialog.h>
#include <mousepad/mousepad-search-bar.h>
#include <mousepad/mousepad-statusbar.h>
#include <mousepad/mousepad-print.h>
#include <mousepad/mousepad-window.h>
#include <mousepad/mousepad-util.h>
#include <mousepad/mousepad-history.h>



#define PADDING                   2
#define PASTE_HISTORY_MENU_LENGTH 30
#define MIN_FONT_SIZE             6
#define MAX_FONT_SIZE             72

#define MENUBAR        MOUSEPAD_SETTING_MENUBAR_VISIBLE
#define STATUSBAR      MOUSEPAD_SETTING_STATUSBAR_VISIBLE
#define TOOLBAR        MOUSEPAD_SETTING_TOOLBAR_VISIBLE
#define NOTEBOOK_GROUP "Mousepad"



enum
{
  PROP_SEARCH_WIDGET_VISIBLE=1,
  N_PROPERTIES
};

enum
{
  NEW_WINDOW,
  NEW_WINDOW_WITH_DOCUMENT,
  SEARCH_COMPLETED,
  LAST_SIGNAL
};

enum
{
  AUTO = 0,
  NO,
  YES
};



/* overridden parent classes methods */
static void              mousepad_window_set_property                 (GObject                *object,
                                                                       guint                   prop_id,
                                                                       const GValue           *value,
                                                                       GParamSpec             *pspec);
static void              mousepad_window_get_property                 (GObject                *object,
                                                                       guint                   prop_id,
                                                                       GValue                 *value,
                                                                       GParamSpec             *pspec);
static void              mousepad_window_finalize                     (GObject                *object);

static gboolean          mousepad_window_configure_event              (GtkWidget              *widget,
                                                                       GdkEventConfigure      *event);
static gboolean          mousepad_window_delete_event                 (GtkWidget              *widget,
                                                                       GdkEventAny            *event);
static gboolean          mousepad_window_scroll_event                 (GtkWidget              *widget,
                                                                       GdkEventScroll         *event);
static gboolean          mousepad_window_window_state_event           (GtkWidget              *widget,
                                                                       GdkEventWindowState    *event);
static gboolean          mousepad_window_key_press_event              (GtkWidget              *widget,
                                                                       GdkEventKey            *event);

/* statusbar tooltips */
static void              mousepad_window_menu_set_tooltips            (MousepadWindow         *window,
                                                                       GtkWidget              *menu,
                                                                       GMenuModel             *model,
                                                                       gint                   *offset);
static void              mousepad_window_menu_item_selected           (GtkWidget              *menu_item,
                                                                       MousepadWindow         *window);
static void              mousepad_window_menu_item_deselected         (GtkWidget              *menu_item,
                                                                       MousepadWindow         *window);
static gboolean          mousepad_window_tool_item_enter_event        (GtkWidget              *tool_item,
                                                                       GdkEvent               *event,
                                                                       MousepadWindow         *window);
static gboolean          mousepad_window_tool_item_leave_event        (GtkWidget              *tool_item,
                                                                       GdkEvent               *event,
                                                                       MousepadWindow         *window);

/* window functions */
static gboolean          mousepad_window_open_file                    (MousepadWindow         *window,
                                                                       GFile                  *file,
                                                                       MousepadEncoding        encoding,
                                                                       gint                    line,
                                                                       gint                    column,
                                                                       gboolean                must_exist);
static gboolean          mousepad_window_close_document               (MousepadWindow         *window,
                                                                       MousepadDocument       *document);
static void              mousepad_window_button_close_tab             (MousepadDocument       *document,
                                                                       MousepadWindow         *window);
static void              mousepad_window_set_title                    (MousepadWindow         *window);
static gboolean          mousepad_window_get_in_fullscreen            (MousepadWindow         *window);
static void              mousepad_window_update_bar_visibility        (MousepadWindow         *window,
                                                                       const gchar            *key);
static void              mousepad_window_update_tabs_visibility       (MousepadWindow         *window,
                                                                       gchar                  *key,
                                                                       GSettings              *settings);

/* notebook signals */
static void              mousepad_window_notebook_switch_page         (GtkNotebook            *notebook,
                                                                       GtkWidget              *page,
                                                                       guint                   page_num,
                                                                       MousepadWindow         *window);
static void              mousepad_window_notebook_added               (GtkNotebook            *notebook,
                                                                       GtkWidget              *page,
                                                                       guint                   page_num,
                                                                       MousepadWindow         *window);
static void              mousepad_window_notebook_removed             (GtkNotebook            *notebook,
                                                                       GtkWidget              *page,
                                                                       guint                   page_num,
                                                                       MousepadWindow         *window);
static gboolean          mousepad_window_notebook_button_release_event (GtkNotebook           *notebook,
                                                                        GdkEventButton        *event,
                                                                        MousepadWindow        *window);
static gboolean          mousepad_window_notebook_button_press_event  (GtkNotebook            *notebook,
                                                                       GdkEventButton         *event,
                                                                       MousepadWindow         *window);
static GtkNotebook      *mousepad_window_notebook_create_window       (GtkNotebook            *notebook,
                                                                       GtkWidget              *page,
                                                                       gint                    x,
                                                                       gint                    y,
                                                                       MousepadWindow         *window);

/* document signals */
static void              mousepad_window_externally_modified          (MousepadFile           *file,
                                                                       MousepadWindow         *window);
static void              mousepad_window_location_changed             (MousepadFile           *file,
                                                                       GFile                  *location,
                                                                       MousepadWindow         *window);
static void              mousepad_window_readonly_changed             (MousepadFile           *file,
                                                                       gboolean                readonly,
                                                                       MousepadWindow         *window);
static void              mousepad_window_modified_changed             (GtkTextBuffer          *buffer,
                                                                       MousepadWindow         *window);
static void              mousepad_window_enable_edit_actions          (GObject                *object,
                                                                       GParamSpec             *pspec,
                                                                       MousepadWindow         *window);
static void              mousepad_window_cursor_changed               (MousepadDocument       *document,
                                                                       gint                    line,
                                                                       gint                    column,
                                                                       gint                    selection,
                                                                       MousepadWindow         *window);
static void              mousepad_window_encoding_changed             (MousepadDocument       *document,
                                                                       MousepadEncoding        encoding,
                                                                       MousepadWindow         *window);
static void              mousepad_window_language_changed             (MousepadDocument       *document,
                                                                       GtkSourceLanguage      *language,
                                                                       MousepadWindow         *window);
static void              mousepad_window_overwrite_changed            (MousepadDocument       *document,
                                                                       gboolean                overwrite,
                                                                       MousepadWindow         *window);
static void              mousepad_window_can_undo                     (GtkSourceBuffer        *buffer,
                                                                       GParamSpec             *unused,
                                                                       MousepadWindow         *window);
static void              mousepad_window_can_redo                     (GtkSourceBuffer        *buffer,
                                                                       GParamSpec             *unused,
                                                                       MousepadWindow         *window);

/* menu functions */
static void              mousepad_window_menu_templates               (GSimpleAction          *action,
                                                                       GVariant               *state,
                                                                       gpointer                data);
static void              mousepad_window_menu_tab_sizes_update        (MousepadWindow         *window);
static void              mousepad_window_menu_textview_popup          (GtkTextView            *textview,
                                                                       GtkMenu                *old_menu,
                                                                       MousepadWindow         *window);
static void              mousepad_window_update_menu_item             (MousepadWindow         *window,
                                                                       const gchar            *menu_id,
                                                                       gint                    index,
                                                                       gpointer                data);
static void              mousepad_window_update_gomenu                (GSimpleAction          *action,
                                                                       GVariant               *state,
                                                                       gpointer                data);
static void              mousepad_window_recent_menu                  (GSimpleAction          *action,
                                                                       GVariant               *state,
                                                                       gpointer                data);

/* dnd */
static void              mousepad_window_drag_data_received           (GtkWidget              *widget,
                                                                       GdkDragContext         *context,
                                                                       gint                    x,
                                                                       gint                    y,
                                                                       GtkSelectionData       *selection_data,
                                                                       guint                   info,
                                                                       guint                   drag_time,
                                                                       MousepadWindow         *window);

/* find and replace */
static void              mousepad_window_search                       (MousepadWindow         *window,
                                                                       MousepadSearchFlags     flags,
                                                                       const gchar            *string,
                                                                       const gchar            *replacement);
static void              mousepad_window_search_completed             (MousepadDocument       *document,
                                                                       gint                    cur_match_doc,
                                                                       gint                    n_matches_doc,
                                                                       const gchar            *string,
                                                                       MousepadSearchFlags     flags,
                                                                       MousepadWindow         *window);
static void              mousepad_window_hide_search_bar              (MousepadWindow         *window);

/* history clipboard functions */
static void              mousepad_window_paste_history_add            (MousepadWindow         *window);
static void              mousepad_window_paste_history_activate       (GtkMenuItem            *item,
                                                                       MousepadWindow         *window);
static GtkWidget        *mousepad_window_paste_history_menu_item      (const gchar            *text,
                                                                       const gchar            *mnemonic);
static GtkWidget        *mousepad_window_paste_history_menu           (MousepadWindow         *window);

/* actions */
static void              mousepad_window_action_new                   (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_new_window            (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_new_from_template     (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_open                  (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_open_recent           (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_clear_recent          (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_save                  (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_save_as               (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_save_all              (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_reload                (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_print                 (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_detach                (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_close                 (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_close_window          (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_undo                  (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_redo                  (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_cut                   (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_copy                  (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_paste                 (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_paste_history         (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_paste_column          (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_delete_selection      (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_delete_line           (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_select_all            (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_lowercase             (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_uppercase             (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_titlecase             (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_opposite_case         (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_tabs_to_spaces        (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_spaces_to_tabs        (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_strip_trailing_spaces (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_transpose             (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_move_line_up          (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_move_line_down        (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_move_word_left        (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_move_word_right       (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_duplicate             (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_increase_indent       (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_decrease_indent       (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_find                  (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_find_next             (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_find_previous         (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_replace               (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_go_to_position        (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_select_font           (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_increase_font_size    (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_decrease_font_size    (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_reset_font_size       (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_bar_activate          (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_textview              (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_menubar_state         (GSimpleAction          *action,
                                                                       GVariant               *state,
                                                                       gpointer                data);
static void              mousepad_window_action_fullscreen            (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_line_ending           (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_tab_size              (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_language              (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_write_bom             (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_viewer_mode           (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_prev_tab              (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_next_tab              (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_go_to_tab             (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_contents              (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_about                 (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);



struct _MousepadWindow
{
  GtkApplicationWindow __parent__;

  /* the current and previous active documents */
  MousepadDocument    *active;
  MousepadDocument    *previous;

  /* main window widgets */
  GtkWidget           *box;
  GtkWidget           *menubar_box;
  GtkWidget           *toolbar_box;
  GtkWidget           *menubar;
  GtkWidget           *toolbar;
  GtkWidget           *notebook;
  GtkWidget           *search_bar;
  GtkWidget           *statusbar;
  GtkWidget           *replace_dialog;

  /* contextual gtkmenus created from the application resources */
  GtkWidget           *textview_menu;
  GtkWidget           *tab_menu;
  GtkWidget           *languages_menu;

  /* menubar related */
  const gchar         *gtkmenu_key, *offset_key;
  gboolean             old_style_menu;

  /* search widgets related */
  gboolean             search_widget_visible;
};



/* menubar actions */
static const GActionEntry action_entries[] =
{
  /* to make menu items insensitive, when needed */
  { "insensitive", NULL, NULL, NULL, NULL },

  /* additional action for the "Textview" menu to show the menubar when hidden */
  { "textview.menubar", mousepad_window_action_textview, NULL, "false", NULL },

  /* increase/decrease font size from keyboard/mouse */
  { "increase-font-size", mousepad_window_action_increase_font_size, NULL, NULL, NULL },
  { "decrease-font-size", mousepad_window_action_decrease_font_size, NULL, NULL, NULL },
  { "reset-font-size", mousepad_window_action_reset_font_size, NULL, NULL, NULL },

  /* "File" menu */
  { "file.new", mousepad_window_action_new, NULL, NULL, NULL },
  { "file.new-window", mousepad_window_action_new_window, NULL, NULL, NULL },
  { "file.new-from-template", NULL, NULL, "false", mousepad_window_menu_templates },
    { "file.new-from-template.new", mousepad_window_action_new_from_template, "s", NULL, NULL },

  { "file.open", mousepad_window_action_open, NULL, NULL, NULL },
  { "file.open-recent", NULL, NULL, "false", mousepad_window_recent_menu },
    { "file.open-recent.new", mousepad_window_action_open_recent, "s", NULL, NULL },
    { "file.open-recent.clear-history", mousepad_window_action_clear_recent, NULL, NULL, NULL },

  { "file.save", mousepad_window_action_save, NULL, "0", NULL },
  { "file.save-as", mousepad_window_action_save_as, NULL, "0", NULL },
  { "file.save-all", mousepad_window_action_save_all, NULL, NULL, NULL },
  { "file.reload", mousepad_window_action_reload, NULL, NULL, NULL },

  { "file.print", mousepad_window_action_print, NULL, NULL, NULL },

  { "file.detach-tab", mousepad_window_action_detach, NULL, NULL, NULL },

  { "file.close-tab", mousepad_window_action_close, NULL, NULL, NULL },
  { "file.close-window", mousepad_window_action_close_window, NULL, "0", NULL },

  /* "Edit" menu */
  { "edit.undo", mousepad_window_action_undo, NULL, NULL, NULL },
  { "edit.redo", mousepad_window_action_redo, NULL, NULL, NULL },

  { "edit.cut", mousepad_window_action_cut, NULL, NULL, NULL },
  { "edit.copy", mousepad_window_action_copy, NULL, NULL, NULL },
  { "edit.paste", mousepad_window_action_paste, NULL, NULL, NULL },
  /* "Paste Special" submenu */
    { "edit.paste-special.paste-from-history", mousepad_window_action_paste_history, NULL, NULL, NULL },
    { "edit.paste-special.paste-as-column", mousepad_window_action_paste_column, NULL, NULL, NULL },
  { "edit.delete-selection", mousepad_window_action_delete_selection, NULL, NULL, NULL },
  { "edit.delete-line", mousepad_window_action_delete_line, NULL, NULL, NULL },

  { "edit.select-all", mousepad_window_action_select_all, NULL, NULL, NULL },

  /* "Convert" submenu */
    { "edit.convert.to-lowercase", mousepad_window_action_lowercase, NULL, NULL, NULL },
    { "edit.convert.to-uppercase", mousepad_window_action_uppercase, NULL, NULL, NULL },
    { "edit.convert.to-title-case", mousepad_window_action_titlecase, NULL, NULL, NULL },
    { "edit.convert.to-opposite-case", mousepad_window_action_opposite_case, NULL, NULL, NULL },

    { "edit.convert.tabs-to-spaces", mousepad_window_action_tabs_to_spaces, NULL, NULL, NULL },
    { "edit.convert.spaces-to-tabs", mousepad_window_action_spaces_to_tabs, NULL, NULL, NULL },

    { "edit.convert.strip-trailing-spaces", mousepad_window_action_strip_trailing_spaces, NULL, NULL, NULL },

    { "edit.convert.transpose", mousepad_window_action_transpose, NULL, NULL, NULL },
  /* "Move" submenu */
    { "edit.move.line-up", mousepad_window_action_move_line_up, NULL, NULL, NULL },
    { "edit.move.line-down", mousepad_window_action_move_line_down, NULL, NULL, NULL },
    { "edit.move.word-left", mousepad_window_action_move_word_left, NULL, NULL, NULL },
    { "edit.move.word-right", mousepad_window_action_move_word_right, NULL, NULL, NULL },
  { "edit.duplicate-line-selection", mousepad_window_action_duplicate, NULL, NULL, NULL },
  { "edit.increase-indent", mousepad_window_action_increase_indent, NULL, NULL, NULL },
  { "edit.decrease-indent", mousepad_window_action_decrease_indent, NULL, NULL, NULL },

  /* "Search" menu */
  { "search.find", mousepad_window_action_find, NULL, NULL, NULL },
  { "search.find-next", mousepad_window_action_find_next, NULL, NULL, NULL },
  { "search.find-previous", mousepad_window_action_find_previous, NULL, NULL, NULL },
  { "search.find-and-replace", mousepad_window_action_replace, NULL, NULL, NULL },

  { "search.go-to", mousepad_window_action_go_to_position, NULL, NULL, NULL },

  /* "View" menu */
  { "view.select-font", mousepad_window_action_select_font, NULL, NULL, NULL },
  { MENUBAR, mousepad_window_action_bar_activate, NULL, "false", mousepad_window_action_menubar_state },
  { TOOLBAR, mousepad_window_action_bar_activate, NULL, "false", NULL },
  { STATUSBAR, mousepad_window_action_bar_activate, NULL, "false", NULL },

  { "view.fullscreen", mousepad_window_action_fullscreen, NULL, "false", NULL },

  /* "Document" menu */
  { "document", NULL, NULL, "false", mousepad_window_update_gomenu },
  /* "Tab Size" submenu */
    { "document.tab.tab-size", mousepad_window_action_tab_size, "i", "2", NULL },

  /* "Filetype" submenu */
    { "document.filetype", mousepad_window_action_language, "s", "'" MOUSEPAD_LANGUAGE_NONE "'", NULL },
  /* "Line Ending" submenu */
    { "document.line-ending", mousepad_window_action_line_ending, "i", "0", NULL },

  { "document.write-unicode-bom", mousepad_window_action_write_bom, NULL, "false", NULL },
  { "document.viewer-mode", mousepad_window_action_viewer_mode, NULL, "false", NULL },

  { "document.previous-tab", mousepad_window_action_prev_tab, NULL, NULL, NULL },
  { "document.next-tab", mousepad_window_action_next_tab, NULL, NULL, NULL },

  { "document.go-to-tab", mousepad_window_action_go_to_tab, "i", "0", NULL },

  /* "Help" menu */
  { "help.contents", mousepad_window_action_contents, NULL, NULL, NULL },
  { "help.about", mousepad_window_action_about, NULL, NULL, NULL }
};



/* global variables */
static guint   window_signals[LAST_SIGNAL];
static gint    lock_menu_updates = 0;
static GSList *clipboard_history = NULL;
static guint   clipboard_history_ref_count = 0;
static GFile  *last_save_location = NULL;
static guint   last_save_location_ref_count = 0;



G_DEFINE_TYPE (MousepadWindow, mousepad_window, GTK_TYPE_APPLICATION_WINDOW)



GtkWidget *
mousepad_window_new (MousepadApplication *application)
{
  return g_object_new (MOUSEPAD_TYPE_WINDOW, "application", GTK_APPLICATION (application), NULL);
}



static void
mousepad_window_class_init (MousepadWindowClass *klass)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *gtkwidget_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->set_property = mousepad_window_set_property;
  gobject_class->get_property = mousepad_window_get_property;
  gobject_class->finalize = mousepad_window_finalize;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->configure_event = mousepad_window_configure_event;
  gtkwidget_class->delete_event = mousepad_window_delete_event;
  gtkwidget_class->scroll_event = mousepad_window_scroll_event;
  gtkwidget_class->window_state_event = mousepad_window_window_state_event;
  gtkwidget_class->key_press_event = mousepad_window_key_press_event;

  window_signals[NEW_WINDOW] =
    g_signal_new (I_("new-window"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  window_signals[NEW_WINDOW_WITH_DOCUMENT] =
    g_signal_new (I_("new-window-with-document"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  _mousepad_marshal_VOID__OBJECT_INT_INT,
                  G_TYPE_NONE, 3,
                  G_TYPE_OBJECT,
                  G_TYPE_INT, G_TYPE_INT);

  window_signals[SEARCH_COMPLETED] =
    g_signal_new (I_("search-completed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  _mousepad_marshal_VOID__INT_INT_STRING_FLAGS,
                  G_TYPE_NONE, 4, G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING,
                  MOUSEPAD_TYPE_SEARCH_FLAGS);

  g_object_class_install_property (gobject_class, PROP_SEARCH_WIDGET_VISIBLE,
    g_param_spec_boolean ("search-widget-visible", "SearchWidgetVisible",
                          "At least one search widget is visible or not",
                          FALSE, G_PARAM_READWRITE));
}



static void
mousepad_window_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (object);

  switch (prop_id)
    {
    case PROP_SEARCH_WIDGET_VISIBLE:
      window->search_widget_visible = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
mousepad_window_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (object);

  switch (prop_id)
    {
    case PROP_SEARCH_WIDGET_VISIBLE:
      g_value_set_boolean (value, window->search_widget_visible);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
mousepad_window_finalize (GObject *object)
{
  /* decrease history clipboard ref count */
  clipboard_history_ref_count--;

  /* decrease last save location ref count */
  last_save_location_ref_count--;

  /* free clipboard history if needed */
  if (clipboard_history_ref_count == 0 && clipboard_history != NULL)
    g_slist_free_full (clipboard_history, g_free);

  /* free last save location if needed */
  if (last_save_location_ref_count == 0 && last_save_location != NULL)
    {
      g_object_unref (last_save_location);
      last_save_location = NULL;
    }

  (*G_OBJECT_CLASS (mousepad_window_parent_class)->finalize) (object);
}



static void
mousepad_window_update_toolbar_properties (MousepadWindow *window,
                                           gchar          *key,
                                           GSettings      *settings)
{
  GtkToolbarStyle style;
  GtkIconSize     size;

  style = MOUSEPAD_SETTING_GET_ENUM (TOOLBAR_STYLE);
  size = MOUSEPAD_SETTING_GET_ENUM (TOOLBAR_ICON_SIZE);

  gtk_toolbar_set_style (GTK_TOOLBAR (window->toolbar), style);
  gtk_toolbar_set_icon_size (GTK_TOOLBAR (window->toolbar), size);
}



static void
mousepad_window_restore_geometry (MousepadWindow *window)
{
  gboolean remember_size, remember_position, remember_state;

  remember_size = MOUSEPAD_SETTING_GET_BOOLEAN (REMEMBER_SIZE);
  remember_position = MOUSEPAD_SETTING_GET_BOOLEAN (REMEMBER_POSITION);
  remember_state = MOUSEPAD_SETTING_GET_BOOLEAN (REMEMBER_STATE);

  /* first restore size */
  if (remember_size)
    {
      gint width, height;

      width = MOUSEPAD_SETTING_GET_UINT (WINDOW_WIDTH);
      height = MOUSEPAD_SETTING_GET_UINT (WINDOW_HEIGHT);

      gtk_window_set_default_size (GTK_WINDOW (window), width, height);
    }

  /* then restore position */
  if (remember_position)
    {
      gint left, top;

      left = MOUSEPAD_SETTING_GET_UINT (WINDOW_LEFT);
      top = MOUSEPAD_SETTING_GET_UINT (WINDOW_TOP);

      gtk_window_move (GTK_WINDOW (window), left, top);
    }

  /* finally restore window state */
  if (remember_state)
    {
      gboolean maximized, fullscreen;

      maximized = MOUSEPAD_SETTING_GET_BOOLEAN (WINDOW_MAXIMIZED);
      fullscreen = MOUSEPAD_SETTING_GET_BOOLEAN (WINDOW_FULLSCREEN);

      /* first restore if it was maximized */
      if (maximized)
        gtk_window_maximize (GTK_WINDOW (window));

      /* then restore if it was fullscreen and update action state accordingly */
      if (fullscreen)
        g_action_group_activate_action (G_ACTION_GROUP (window), "view.fullscreen", NULL);
    }
}



static void
mousepad_window_update_toolbar_item (GMenuModel  *model,
                                     gint         position,
                                     gint         removed,
                                     gint         added,
                                     GtkToolItem *item)
{
  GtkApplication *application;
  GtkWidget      *window;
  GVariant       *value;

  /* don't update the toolbar item properties for the non active application windows */
  if ((window = gtk_widget_get_ancestor (GTK_WIDGET (item), MOUSEPAD_TYPE_WINDOW))
      && (application = gtk_window_get_application (GTK_WINDOW (window)))
      && (GTK_WINDOW (window) != gtk_application_get_active_window (application)))
    return;

  /* update the toolbar item properties only when the corresponding menu item is added,
   * that is at the end of its own update (see also the comment above the toolbar in
   * mousepad/resources/gtk/menus.ui) */
  if (added && position == GPOINTER_TO_INT (mousepad_object_get_data (item, "index")))
    {
      /* every menu item should have at least a label, so we can suppose it exists */
      value = g_menu_model_get_item_attribute_value (model, position, "label",
                                                     G_VARIANT_TYPE_STRING);
      gtk_tool_button_set_label (GTK_TOOL_BUTTON (item), g_variant_get_string (value, NULL));
      g_variant_unref (value);

      /* all the following item attributes should normally be filled to build a toolbar item,
       * but strictly speaking, it is not necessary */
      if ((value = g_menu_model_get_item_attribute_value (model, position, "icon",
                                                          G_VARIANT_TYPE_STRING)))
        {
          gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item),
                                         g_variant_get_string (value, NULL));
          g_variant_unref (value);
        }

      if ((value = g_menu_model_get_item_attribute_value (model, position, "tooltip",
                                                          G_VARIANT_TYPE_STRING)))
        {
          gtk_tool_item_set_tooltip_text (item, g_variant_get_string (value, NULL));
          g_variant_unref (value);
        }

      if ((value = g_menu_model_get_item_attribute_value (model, position, "action",
                                                          G_VARIANT_TYPE_STRING)))
        {
          gtk_actionable_set_action_name (GTK_ACTIONABLE (item),
                                          g_variant_get_string (value, NULL));
          g_variant_unref (value);
        }
    }
}



static void
mousepad_window_toolbar_insert (MousepadWindow *window,
                                GtkWidget      *toolbar,
                                GMenuModel     *model,
                                gint            index)
{
  GtkToolItem *item;
  GtkWidget   *child;

  /* create an empty toolbar item */
  item = gtk_tool_button_new (NULL, NULL);

  /* initialize the toolbar item properties from the menu item attributes */
  mousepad_object_set_data (item, "index", GINT_TO_POINTER (index));
  mousepad_window_update_toolbar_item (model, index, 0, 1, item);
  gtk_tool_button_set_use_underline (GTK_TOOL_BUTTON (item), TRUE);

  /* a kind of binding between the menu item attributes and the toolbar item properties */
  g_signal_connect_object (model, "items-changed",
                           G_CALLBACK (mousepad_window_update_toolbar_item), item, 0);

  /* tool items will have GtkButton or other widgets in them, we want the child */
  child = gtk_bin_get_child (GTK_BIN (item));

  /* get events for mouse enter/leave and focus in/out */
  gtk_widget_add_events (child,
                         GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK
                         | GDK_FOCUS_CHANGE_MASK);

  /* connect to signals for the events to show the tooltip in the status bar */
  g_signal_connect_object (child, "enter-notify-event",
                           G_CALLBACK (mousepad_window_tool_item_enter_event),
                           window, 0);
  g_signal_connect_object (child, "leave-notify-event",
                           G_CALLBACK (mousepad_window_tool_item_leave_event),
                           window, 0);
  g_signal_connect_object (child, "focus-in-event",
                           G_CALLBACK (mousepad_window_tool_item_enter_event),
                           window, 0);
  g_signal_connect_object (child, "focus-out-event",
                           G_CALLBACK (mousepad_window_tool_item_leave_event),
                           window, 0);

  /* append the item to the end of the toolbar */
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);
}



static GtkWidget *
mousepad_window_toolbar_new_from_model (MousepadWindow *window,
                                        GMenuModel     *model)
{
  GtkWidget   *toolbar;
  GtkToolItem *item = NULL;
  GMenuModel  *section;
  gint         m, n, n_items;

  /* create the toolbar and set the main properties */
  toolbar = gtk_toolbar_new ();
  gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_ICONS);
  gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_SMALL_TOOLBAR);

  /* insert items */
  for (m = 0; m < g_menu_model_get_n_items (model); m++)
    {
      /* section GMenuItem */
      if ((section = g_menu_model_get_item_link (model, m, G_MENU_LINK_SECTION))
          && (n_items = g_menu_model_get_n_items (section)))
        {
          /* append a toolbar separator when needed */
          if (m > 0)
            {
              item = gtk_separator_tool_item_new ();
              gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);
            }

          /* walk through the section */
          for (n = 0; n < n_items; n++)
            mousepad_window_toolbar_insert (window, toolbar, section, n);
        }
      /* real GMenuItem */
      else
        mousepad_window_toolbar_insert (window, toolbar, model, m);
    }

  /* make the last toolbar separator so it expands properly */
  if (item != NULL)
    {
      gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (item), FALSE);
      gtk_tool_item_set_expand (item, TRUE);
    }

  /* show all widgets */
  gtk_widget_show_all (toolbar);

  return toolbar;
}



static void
mousepad_window_post_init (MousepadWindow *window)
{
  GtkApplication *application;
  GMenuModel     *model;
  gchar          *gtkmenu_key, *offset_key;
  gint            window_id;

  /* disconnect this handler */
  mousepad_disconnect_by_func (window, mousepad_window_post_init, NULL);

  /* setup CSD titlebar */
  mousepad_util_set_titlebar (GTK_WINDOW (window));

  /* set the unique menu and offset keys for this window */
  window_id = gtk_application_window_get_id (GTK_APPLICATION_WINDOW (window));
  gtkmenu_key = g_strdup_printf ("gtkmenu-%d", window_id);
  offset_key = g_strdup_printf ("offset-%d", window_id);
  window->gtkmenu_key = g_intern_string (gtkmenu_key);
  window->offset_key = g_intern_string (offset_key);
  g_free (gtkmenu_key);
  g_free (offset_key);

  /* create text view menu and set tooltips (must be done before setting the menubar visibility) */
  application = gtk_window_get_application (GTK_WINDOW (window));
  model = G_MENU_MODEL (gtk_application_get_menu_by_id (application, "textview-menu"));
  window->textview_menu = gtk_menu_new_from_model (model);
  gtk_menu_attach_to_widget (GTK_MENU (window->textview_menu),
                             GTK_WIDGET (window), NULL);
  mousepad_window_menu_set_tooltips (window, window->textview_menu, model, NULL);

  /* create tab menu and set tooltips */
  model = G_MENU_MODEL (gtk_application_get_menu_by_id (application, "tab-menu"));
  window->tab_menu = gtk_menu_new_from_model (model);
  gtk_menu_attach_to_widget (GTK_MENU (window->tab_menu),
                             GTK_WIDGET (window), NULL);
  mousepad_window_menu_set_tooltips (window, window->tab_menu, model, NULL);

  /* create languages menu and set tooltips */
  model = G_MENU_MODEL (gtk_application_get_menu_by_id (application, "document.filetype"));
  window->languages_menu = gtk_menu_new_from_model (model);
  gtk_menu_attach_to_widget (GTK_MENU (window->languages_menu),
                             GTK_WIDGET (window), NULL);
  mousepad_window_menu_set_tooltips (window, window->languages_menu, model, NULL);

  /* hide the default menubar */
  gtk_application_window_set_show_menubar (GTK_APPLICATION_WINDOW (window), FALSE);

  /*
   * Outsource the creation of the menubar from
   * gtk/gtk/gtkapplicationwindow.c:gtk_application_window_update_menubar(), to make the menubar
   * a window attribute, and be able to access its items to show their tooltips in the statusbar.
   * With GTK+ 3, this leads to use gtk_menu_bar_new_from_model()
   * With GTK+ 4, this will lead to use gtk_popover_menu_bar_new_from_model()
   */
  model = gtk_application_get_menubar (application);
  window->menubar = gtk_menu_bar_new_from_model (model);

  /* insert the menubar in its previously reserved space */
  gtk_box_pack_start (GTK_BOX (window->menubar_box), window->menubar, TRUE, TRUE, 0);

  /* set tooltips and connect handlers to the menubar items signals */
  mousepad_window_menu_set_tooltips (window, window->menubar, model, NULL);

  /* update the menubar visibility and related actions state */
  mousepad_window_update_bar_visibility (window, MENUBAR);

  /* connect to some signals to keep the menubar visibility in sync */
  MOUSEPAD_SETTING_CONNECT_OBJECT (MENUBAR_VISIBLE, mousepad_window_update_bar_visibility,
                                   window, G_CONNECT_SWAPPED);

  MOUSEPAD_SETTING_CONNECT_OBJECT (MENUBAR_VISIBLE_FULLSCREEN,
                                   mousepad_window_update_bar_visibility,
                                   window, G_CONNECT_SWAPPED);

  /* create the toolbar */
  model = G_MENU_MODEL (gtk_application_get_menu_by_id (application, "toolbar"));
  window->toolbar = mousepad_window_toolbar_new_from_model (window, model);

  /* insert the toolbar in its previously reserved space */
  gtk_box_pack_start (GTK_BOX (window->toolbar_box), window->toolbar, TRUE, TRUE, 0);

  /* update the toolbar visibility and related actions state */
  mousepad_window_update_bar_visibility (window, TOOLBAR);

  /* update the toolbar with the settings */
  mousepad_window_update_toolbar_properties (window, NULL, NULL);

  /* connect to some signals to keep the toolbar properties in sync */
  MOUSEPAD_SETTING_CONNECT_OBJECT (TOOLBAR_VISIBLE, mousepad_window_update_bar_visibility,
                                   window, G_CONNECT_SWAPPED);

  MOUSEPAD_SETTING_CONNECT_OBJECT (TOOLBAR_VISIBLE_FULLSCREEN,
                                   mousepad_window_update_bar_visibility,
                                   window, G_CONNECT_SWAPPED);

  MOUSEPAD_SETTING_CONNECT_OBJECT (TOOLBAR_STYLE, mousepad_window_update_toolbar_properties,
                                   window, G_CONNECT_SWAPPED);

  MOUSEPAD_SETTING_CONNECT_OBJECT (TOOLBAR_ICON_SIZE, mousepad_window_update_toolbar_properties,
                                   window, G_CONNECT_SWAPPED);

  /* initialize the tab size menu and sync it with its setting */
  mousepad_window_menu_tab_sizes_update (window);
  MOUSEPAD_SETTING_CONNECT_OBJECT (TAB_WIDTH, mousepad_window_menu_tab_sizes_update,
                                   window, G_CONNECT_SWAPPED);

  /* restore window geometry settings */
  mousepad_window_restore_geometry (window);
}



static void
mousepad_window_create_root_warning (MousepadWindow *window)
{
  /* check if we need to add the root warning */
  if (G_UNLIKELY (geteuid () == 0))
    {
      GtkWidget       *ebox, *label, *separator;
      GtkCssProvider  *provider;
      GtkStyleContext *context;
      const gchar     *css_string;

      /* add the box for the root warning */
      ebox = gtk_event_box_new ();
      gtk_box_pack_start (GTK_BOX (window->box), ebox, FALSE, FALSE, 0);
      gtk_widget_show (ebox);

      /* add the label with the root warning */
      label = gtk_label_new (_("Warning: you are using the root account. You may harm your system."));
      gtk_widget_set_margin_start (label, 6);
      gtk_widget_set_margin_end (label, 6);
      gtk_widget_set_margin_top (label, 3);
      gtk_widget_set_margin_bottom (label, 3);
      gtk_container_add (GTK_CONTAINER (ebox), label);
      gtk_widget_show (label);

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_pack_start (GTK_BOX (window->box), separator, FALSE, FALSE, 0);
      gtk_widget_show (separator);

      /* apply a CSS style to capture the user's attention */
      provider = gtk_css_provider_new ();
      css_string = "label { background-color: #b4254b; color: #fefefe; }";
      context = gtk_widget_get_style_context (label);
      gtk_css_provider_load_from_data (provider, css_string, -1, NULL);
      gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (provider),
                                      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
      g_object_unref (provider);
    }
}



static void
mousepad_window_create_notebook (MousepadWindow *window)
{
  window->notebook = g_object_new (GTK_TYPE_NOTEBOOK,
                                   "scrollable", TRUE,
                                   "show-border", FALSE,
                                   "show-tabs", FALSE,
                                   NULL);

  /* set the group id */
  gtk_notebook_set_group_name (GTK_NOTEBOOK (window->notebook), NOTEBOOK_GROUP);

  /* connect signals to the notebooks */
  g_signal_connect (window->notebook, "switch-page",
                    G_CALLBACK (mousepad_window_notebook_switch_page), window);
  g_signal_connect (window->notebook, "page-added",
                    G_CALLBACK (mousepad_window_notebook_added), window);
  g_signal_connect (window->notebook, "page-removed",
                    G_CALLBACK (mousepad_window_notebook_removed), window);
  g_signal_connect (window->notebook, "button-press-event",
                    G_CALLBACK (mousepad_window_notebook_button_press_event), window);
  g_signal_connect (window->notebook, "button-release-event",
                    G_CALLBACK (mousepad_window_notebook_button_release_event), window);
  g_signal_connect (window->notebook, "create-window",
                    G_CALLBACK (mousepad_window_notebook_create_window), window);

  /* append and show the notebook */
  gtk_box_pack_start (GTK_BOX (window->box), window->notebook, TRUE, TRUE, PADDING);
  gtk_widget_show (window->notebook);
}



static void
mousepad_window_action_statusbar_overwrite (MousepadWindow *window,
                                            gboolean        overwrite)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* set the new overwrite mode */
  mousepad_document_set_overwrite (window->active, overwrite);
}



static void
mousepad_window_create_statusbar (MousepadWindow *window)
{
  /* setup a new statusbar */
  window->statusbar = mousepad_statusbar_new ();

  /* update the statusbar visibility and related actions state */
  mousepad_window_update_bar_visibility (window, STATUSBAR);

#if !GTK_CHECK_VERSION (4, 0, 0)
  /* make the statusbar smaller */
  /*
   * To fix hard-coded, oversized GTK+3 status bar padding, see:
   * https://gitlab.gnome.org/GNOME/gtk/-/commit/94ebe2106817f6bc7aaf868bd00d0fc381d33e7e
   * Fixed in GTK+4:
   * https://gitlab.gnome.org/GNOME/gtk/-/commit/1a7cbddbd4e98e4641e690035013abbfaec130b0
   */
  gtk_widget_set_margin_top (window->statusbar, 0);
  gtk_widget_set_margin_bottom (window->statusbar, 0);
#endif

  /* pack the statusbar into the window UI */
  gtk_box_pack_end (GTK_BOX (window->box), window->statusbar, FALSE, FALSE, 0);

  /* overwrite toggle signal */
  g_signal_connect_swapped (window->statusbar, "enable-overwrite",
                            G_CALLBACK (mousepad_window_action_statusbar_overwrite), window);

  /* connect to some signals to keep in sync */
  MOUSEPAD_SETTING_CONNECT_OBJECT (STATUSBAR_VISIBLE, mousepad_window_update_bar_visibility,
                                   window, G_CONNECT_SWAPPED);

  MOUSEPAD_SETTING_CONNECT_OBJECT (STATUSBAR_VISIBLE_FULLSCREEN,
                                   mousepad_window_update_bar_visibility,
                                   window, G_CONNECT_SWAPPED);
}



static void
mousepad_window_init (MousepadWindow *window)
{
  GAction *action;

  /* initialize stuff */
  window->active = NULL;
  window->previous = NULL;
  window->menubar = NULL;
  window->toolbar = NULL;
  window->notebook = NULL;
  window->search_bar = NULL;
  window->statusbar = NULL;
  window->replace_dialog = NULL;
  window->textview_menu = NULL;
  window->tab_menu = NULL;
  window->languages_menu = NULL;
  window->gtkmenu_key = NULL;
  window->offset_key = NULL;
  window->old_style_menu = MOUSEPAD_SETTING_GET_BOOLEAN (OLD_STYLE_MENU);

  /* increase clipboard history ref count */
  clipboard_history_ref_count++;

  /* increase last save location ref count */
  last_save_location_ref_count++;

  /* add mousepad style class for easier theming */
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (window)), "mousepad");

  /* add window actions */
  g_action_map_add_action_entries (G_ACTION_MAP (window), action_entries,
                                   G_N_ELEMENTS (action_entries), window);

  /* disable the "insensitive" action to make menu items that use it insensitive */
  action = g_action_map_lookup_action (G_ACTION_MAP (window), "insensitive");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

  /* create the main table */
  window->box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (window), window->box);
  gtk_widget_show (window->box);

  /* keep a place for the menubar and the toolbar, created later from the application resources */
  window->menubar_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (window->box), window->menubar_box, FALSE, FALSE, 0);
  gtk_widget_show (window->menubar_box);

  window->toolbar_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (window->box), window->toolbar_box, FALSE, FALSE, 0);
  gtk_widget_show (window->toolbar_box);

  /* create the root-warning bar (if needed) */
  mousepad_window_create_root_warning (window);

  /* create the notebook */
  mousepad_window_create_notebook (window);

  /* create the statusbar */
  mousepad_window_create_statusbar (window);

  /* defer actions that require the application to be set */
  g_signal_connect (window, "notify::application",
                    G_CALLBACK (mousepad_window_post_init), NULL);

  /* allow drops in the window */
  gtk_drag_dest_set (GTK_WIDGET (window),
                     GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
                     drop_targets,
                     G_N_ELEMENTS (drop_targets), GDK_ACTION_COPY | GDK_ACTION_MOVE);
  g_signal_connect (window, "drag-data-received",
                    G_CALLBACK (mousepad_window_drag_data_received), window);

  /* update the window title when 'path-in-title' setting changes */
  MOUSEPAD_SETTING_CONNECT_OBJECT (PATH_IN_TITLE, mousepad_window_set_title,
                                   window, G_CONNECT_SWAPPED);

  /* update the tabs when 'always-show-tabs' setting changes */
  MOUSEPAD_SETTING_CONNECT_OBJECT (ALWAYS_SHOW_TABS, mousepad_window_update_tabs_visibility,
                                   window, G_CONNECT_SWAPPED);
}



static gboolean
mousepad_window_save_geometry (gpointer data)
{
  GdkWindowState state;
  gboolean       remember_size, remember_position, remember_state;

  /* check if we should remember the window geometry */
  remember_size = MOUSEPAD_SETTING_GET_BOOLEAN (REMEMBER_SIZE);
  remember_position = MOUSEPAD_SETTING_GET_BOOLEAN (REMEMBER_POSITION);
  remember_state = MOUSEPAD_SETTING_GET_BOOLEAN (REMEMBER_STATE);

  if (remember_size || remember_position || remember_state)
    {
      /* check if the window is still visible */
      if (gtk_widget_get_visible (data))
        {
          /* determine the current state of the window */
          state = gdk_window_get_state (gtk_widget_get_window (data));

          /* don't save geometry for maximized or fullscreen windows */
          if ((state & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN)) == 0)
            {
              if (remember_size)
                {
                  gint width, height;

                  /* determine the current width/height of the window... */
                  gtk_window_get_size (data, &width, &height);

                  /* ...and remember them as default for new windows */
                  MOUSEPAD_SETTING_SET_UINT (WINDOW_WIDTH, width);
                  MOUSEPAD_SETTING_SET_UINT (WINDOW_HEIGHT, height);
                }

              if (remember_position)
                {
                  gint left, top;

                  /* determine the current left/top position of the window */
                  gtk_window_get_position (data, &left, &top);

                  /* and then remember it for next startup */
                  MOUSEPAD_SETTING_SET_UINT (WINDOW_LEFT, left);
                  MOUSEPAD_SETTING_SET_UINT (WINDOW_TOP, top);
                }
            }

          if (remember_state)
            {
              /* remember whether the window is maximized or full screen or not */
              MOUSEPAD_SETTING_SET_BOOLEAN (WINDOW_MAXIMIZED, (state & GDK_WINDOW_STATE_MAXIMIZED));
              MOUSEPAD_SETTING_SET_BOOLEAN (WINDOW_FULLSCREEN, (state & GDK_WINDOW_STATE_FULLSCREEN));
            }
        }
    }

  return FALSE;
}



static gboolean
mousepad_window_configure_event (GtkWidget         *widget,
                                 GdkEventConfigure *event)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (widget);
  static GSource *source = NULL;
  guint           source_id;

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);

  /*
   * As long as the window geometry changes, new configure events arrive and
   * the three actions below run as a loop.
   * The window geometry backup only takes place once, one second after the
   * last configure event.
   * "event == NULL" is a special case, corresponding to "save on close".
   */

  /* drop the previous timer source */
  if (source != NULL)
    {
      if (! g_source_is_destroyed (source))
        g_source_destroy (source);

      g_source_unref (source);
      source = NULL;
    }

  /* real event */
  if (event != NULL)
    {
      /* schedule a new backup of the window geometry */
      source_id = g_timeout_add_seconds (1, mousepad_window_save_geometry, window);

      /* retrieve the timer source and increase its ref count to test its destruction next time */
      source = g_main_context_find_source_by_id (NULL, source_id);
      g_source_ref (source);

      /* let gtk+ handle the configure event */
      return GTK_WIDGET_CLASS (mousepad_window_parent_class)->configure_event (widget, event);
    }
  /* save on close */
  else
    return mousepad_window_save_geometry (window);
}



static gboolean
mousepad_window_delete_event (GtkWidget   *widget,
                              GdkEventAny *event)
{
  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (widget), FALSE);

  /* try to close the window */
  g_action_group_activate_action (G_ACTION_GROUP (widget), "file.close-window", NULL);

  /* we will close the window when all the tabs are closed */
  return TRUE;
}



static gboolean
mousepad_window_scroll_event (GtkWidget      *widget,
                              GdkEventScroll *event)
{
  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (widget), FALSE);

  if (event->state & GDK_CONTROL_MASK && event->direction == GDK_SCROLL_UP)
    {
      g_action_group_activate_action (G_ACTION_GROUP (widget), "increase-font-size", NULL);
      return TRUE;
    }
  else if (event->state & GDK_CONTROL_MASK && event->direction == GDK_SCROLL_DOWN)
    {
      g_action_group_activate_action (G_ACTION_GROUP (widget), "decrease-font-size", NULL);
      return TRUE;
    }

  /* don't chain-up to parent here: it is not necessary and the parent class method
   * is defined only from GTK 3.24.13 */
  return FALSE;
}



static gboolean
mousepad_window_window_state_event (GtkWidget           *widget,
                                    GdkEventWindowState *event)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (widget);

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);

  /* update bars visibility when entering/leaving fullscreen mode */
  if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
    {
      mousepad_window_update_bar_visibility (window, MENUBAR);
      mousepad_window_update_bar_visibility (window, TOOLBAR);
      mousepad_window_update_bar_visibility (window, STATUSBAR);
    }

  /* let gtk+ handle the window state event */
  return GTK_WIDGET_CLASS (mousepad_window_parent_class)->window_state_event (widget, event);
}



static gboolean
mousepad_window_key_press_event (GtkWidget   *widget,
                                 GdkEventKey *event)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (widget);

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);

  /* hide the search bar if Esc key was pressed: this is G_SIGNAL_RUN_LAST, so hiding
   * the menubar has priority if necessary */
  if (event->keyval == GDK_KEY_Escape && window->search_bar != NULL
      && gtk_widget_get_visible (window->search_bar))
    {
      mousepad_window_hide_search_bar (window);
      return TRUE;
    }

  /* let gtk+ handle the key event */
  return GTK_WIDGET_CLASS (mousepad_window_parent_class)->key_press_event (widget, event);
}



/**
 * Statusbar Tooltip Functions
 **/
static void
mousepad_window_menu_item_selected (GtkWidget      *menu_item,
                                    MousepadWindow *window)
{
  gchar *tooltip;

  tooltip = gtk_widget_get_tooltip_text (menu_item);
  mousepad_statusbar_push_tooltip (MOUSEPAD_STATUSBAR (window->statusbar), tooltip);
  g_free (tooltip);
}



static void
mousepad_window_menu_item_deselected (GtkWidget      *menu_item,
                                      MousepadWindow *window)
{
  mousepad_statusbar_pop_tooltip (MOUSEPAD_STATUSBAR (window->statusbar));
}



static gboolean
mousepad_window_tool_item_enter_event (GtkWidget      *tool_item,
                                       GdkEvent       *event,
                                       MousepadWindow *window)
{
  gchar *tooltip;

  tooltip = gtk_widget_get_tooltip_text (tool_item);
  mousepad_statusbar_push_tooltip (MOUSEPAD_STATUSBAR (window->statusbar), tooltip);
  g_free (tooltip);

  return FALSE;
}



static gboolean
mousepad_window_tool_item_leave_event (GtkWidget      *tool_item,
                                       GdkEvent       *event,
                                       MousepadWindow *window)
{
  mousepad_statusbar_pop_tooltip (MOUSEPAD_STATUSBAR (window->statusbar));

  return FALSE;
}



static void
mousepad_window_menu_update_tooltips (GMenuModel     *model,
                                      gint            position,
                                      gint            removed,
                                      gint            added,
                                      MousepadWindow *window)
{
  GtkWidget *menu;
  gint       offset;

  /* disconnect this handler */
  mousepad_disconnect_by_func (model, mousepad_window_menu_update_tooltips, window);

  /* update tooltips */
  menu = mousepad_object_get_data (model, window->gtkmenu_key);
  offset = GPOINTER_TO_INT (mousepad_object_get_data (model, window->offset_key));
  mousepad_window_menu_set_tooltips (window, menu, model, &offset);
}



static void
mousepad_window_menu_item_activate (GtkMenuItem *new_item,
                                    gpointer     item)
{
  g_signal_emit_by_name (item, "activate");
}



static void
mousepad_window_menu_item_show_icon (GObject    *settings,
                                     GParamSpec *pspec,
                                     gpointer    icon)
{
  GIcon    *gicon, *replace_gicon;
  gboolean  show_icon;

  g_object_get (settings, "gtk-menu-images", &show_icon, NULL);
  replace_gicon = mousepad_object_get_data (icon, "replace-gicon");

  if (show_icon && replace_gicon != NULL)
    {
      g_object_set (icon, "gicon", replace_gicon, NULL);
      mousepad_object_set_data (icon, "replace-gicon", NULL);
    }
  else if (! show_icon && replace_gicon == NULL)
    {
      g_object_get (icon, "gicon", &gicon, NULL);
      g_object_set (icon, "icon-name", "", NULL);
      mousepad_object_set_data (icon, "replace-gicon", gicon);
    }
}



GtkWidget *
mousepad_window_menu_item_realign (MousepadWindow *window,
                                   GtkWidget      *item,
                                   const gchar    *action_name,
                                   GtkWidget      *menu,
                                   gint            index)
{
  GtkWidget          *new_item, *box, *button = NULL, *icon = NULL, *label;
  GtkCssProvider     *provider;
  GtkStyleContext    *context;
  GActionMap         *action_map = NULL;
  GAction            *action;
  GList              *widgets;
  const GVariantType *state_type = NULL, *param_type = NULL;
  const gchar        *label_text;
  gchar              *new_label_text;
  gboolean            toggle;

  /* do not treat the same item twice */
  if (mousepad_object_get_data (item, "done"))
    return item;

  /* manage action widget */
  if (action_name != NULL)
    {
      /* retrieve action map */
      if (g_str_has_prefix (action_name, "win."))
        action_map = G_ACTION_MAP (window);
      else if (g_str_has_prefix (action_name, "app."))
        action_map = G_ACTION_MAP (gtk_window_get_application (GTK_WINDOW (window)));
      /* in particular, the use of action namespaces in '.ui' files is not supported */
      else
        g_warn_if_reached ();

      if (action_map != NULL)
        {
          action = g_action_map_lookup_action (action_map, action_name + 4);
          state_type = g_action_get_state_type (action);
          param_type = g_action_get_parameter_type (action);
        }

      /* add a check/radio button only for a toggle/radio action */
      if (state_type != NULL && (
            (toggle = g_variant_type_equal (state_type, G_VARIANT_TYPE_BOOLEAN))
            || (param_type != NULL && g_variant_type_equal (state_type, param_type))
         ))
        {
          /* replace the menu item checkbox/radio with a button */
          if (toggle)
            {
              /* we can simply use a check button here, and this also seems to avoid a
               * slight display bug that occurs when using a check menu item like below */
              button = gtk_check_button_new ();
            }
          else
            {
              /* we can't use a radio button here, because its "active" property needs a
               * group to work properly, so let's use a check menu item instead (the
               * display bug mentioned above does not seem to occur in this case) */
              button = gtk_check_menu_item_new ();
              gtk_check_menu_item_set_draw_as_radio (GTK_CHECK_MENU_ITEM (button), TRUE);
              gtk_widget_set_margin_start (button, 4);

              /* remove extra margins */
              context = gtk_widget_get_style_context (button);
              provider = gtk_css_provider_new ();
              gtk_css_provider_load_from_data (provider,
                                               "menuitem { min-width: 0px; min-height: 0px; }",
                                               -1, NULL);
              gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
              g_object_unref (provider);
            }

          gtk_widget_show (button);

          /* bind the "active" property of the hidden and visible checkboxes/radios */
          g_object_bind_property (item, "active", button, "active", G_BINDING_SYNC_CREATE);
        }
    }

  /* manage icon and label: pick up existing widgets, in particular the GtkAccelLabel,
   * and hide the icon if there is a button (it's better not to destroy anything) */

  /* a directly accessible label means no icon */
  if ((label_text = gtk_menu_item_get_label (GTK_MENU_ITEM (item))) != NULL)
    {
      /* remove the label from the item: to be packed in a box */
      label = gtk_bin_get_child (GTK_BIN (item));
      g_object_ref (label);
      gtk_container_remove (GTK_CONTAINER (item), label);

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      g_object_ref (box);
      gtk_widget_show (box);

      /* either a button or an icon, not both, with an end margin when needed */
      if (button != NULL)
        {
          gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
          if (! toggle)
            gtk_widget_set_margin_end (button, 6);
        }
      else
        {
          icon = gtk_image_new_from_icon_name ("", GTK_ICON_SIZE_BUTTON);
          gtk_widget_set_margin_end (icon, 6);
          gtk_widget_show (icon);
          gtk_box_pack_start (GTK_BOX (box), icon, FALSE, FALSE, 0);
        }

      /* put the packed label back in place */
      gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);
      g_object_unref (label);
    }
  else
    {
      static GtkSettings *settings = NULL;
      if (settings == NULL)
        settings = gtk_settings_get_default ();

      /* remove the box from the item to operate on its child widgets */
      box = gtk_bin_get_child (GTK_BIN (item));
      g_object_ref (box);
      gtk_container_remove (GTK_CONTAINER (item), box);

      widgets = gtk_container_get_children (GTK_CONTAINER (box));
      icon = widgets->data;
      label = g_list_last (widgets)->data;
      label_text = gtk_label_get_label (GTK_LABEL (label));
      g_list_free (widgets);

      /* honor global setting for icon visibility */
      if (settings != NULL)
        {
          mousepad_window_menu_item_show_icon (G_OBJECT (settings), NULL, icon);
          g_signal_connect_object (settings, "notify::gtk-menu-images",
                                   G_CALLBACK (mousepad_window_menu_item_show_icon), icon, 0);
        }

      /* hide icon if there is a button, no extra margin here */
      if (button != NULL)
        {
          gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
          gtk_widget_hide (icon);
          if (toggle)
            gtk_box_set_spacing (GTK_BOX (box), 0);
        }
    }

  /* if there is no button, simply put the box back in place */
  if (button == NULL)
    {
      gtk_container_add (GTK_CONTAINER (item), box);
      new_item = item;
    }
  /* else, substitute a new menu item to the old one */
  else
    {
      /* create a new non-check menu item and add it to the menu */
      new_item = gtk_menu_item_new ();
      gtk_widget_show (new_item);
      gtk_container_add (GTK_CONTAINER (new_item), box);
      gtk_menu_shell_insert (GTK_MENU_SHELL (menu), new_item, index);

      /* remove the old check menu item from the menu, keeping it alive */
      gtk_widget_hide (item);
      g_object_ref (item);
      gtk_container_remove (GTK_CONTAINER (menu), item);

      /* forward "destroy" and "activate" signals from the new item to the old one */
      g_signal_connect_swapped (new_item, "destroy", G_CALLBACK (g_object_unref), item);
      g_signal_connect (new_item, "activate", G_CALLBACK (mousepad_window_menu_item_activate), item);
    }

  g_object_unref (box);

  /* we also need to put back some space between the text and the accel in the label */
  new_label_text = g_strconcat (label_text, "      ", NULL);
  gtk_label_set_label (GTK_LABEL (label), new_label_text);
  g_free (new_label_text);

  /* do not treat the same item twice */
  mousepad_object_set_data (new_item, "done", GINT_TO_POINTER (TRUE));

  return new_item;
}



static void
mousepad_window_menu_set_tooltips (MousepadWindow *window,
                                   GtkWidget      *menu,
                                   GMenuModel     *model,
                                   gint           *offset)
{
  GMenuModel   *section, *submodel;
  GtkWidget    *submenu;
  GVariant     *hidden_when, *action, *tooltip;
  GActionGroup *action_group;
  GList        *children, *child;
  const gchar  *action_name;
  gint          n_items, n, suboffset = 0;
  gboolean      realign;

  /* initialization */
  realign = window->old_style_menu && ! GTK_IS_MENU_BAR (menu);
  n_items = g_menu_model_get_n_items (model);
  children = gtk_container_get_children (GTK_CONTAINER (menu));
  child = children;
  if (offset == NULL)
    offset = &suboffset;

  /* attach the GtkMenu and the offset to the GMenuModel, and connect a wrapper of the
   * current function to its "items-changed" signal for future tooltip updates */
  mousepad_object_set_data (model, window->gtkmenu_key, menu);
  mousepad_object_set_data (model, window->offset_key, GINT_TO_POINTER (*offset));
  g_signal_connect_object (model, "items-changed",
                           G_CALLBACK (mousepad_window_menu_update_tooltips), window, 0);

  /* move to the right place in the GtkMenu if we are dealing with a section */
  for (n = 0; n < *offset; n++)
    child = child->next;

  /* realign menu items */
  if (realign)
    gtk_menu_set_reserve_toggle_size (GTK_MENU (menu), FALSE);

  /* exit if we have reached the end of the GtkMenu */
  if (child == NULL)
    {
      g_list_free (children);
      return;
    }

  for (n = 0; n < n_items; n++)
    {
      /* skip separators specific to GtkMenu */
      if (GTK_IS_SEPARATOR_MENU_ITEM (child->data))
        {
          child = child->next;
          (*offset)++;
        }

      /* section GMenuItem: one level down in the GMenuModel but same level in the GtkMenu,
       * so go ahead recursively only from GMenuModel point of view */
      if ((section = g_menu_model_get_item_link (model, n, G_MENU_LINK_SECTION)) != NULL)
        mousepad_window_menu_set_tooltips (window, menu, section, offset);
      /* real GMenuItem */
      else
        {
          action = g_menu_model_get_item_attribute_value (model, n, "action",
                                                          G_VARIANT_TYPE_STRING);
          if (action != NULL)
            {
              action_name = g_variant_get_string (action, NULL);
              g_variant_unref (action);
            }
          else
            action_name = NULL;

          /* an hidden GMenuItem doesn't correspond to an hidden GtkMenuItem,
           * but to nothing, so we have to skip it in this case */
          hidden_when = g_menu_model_get_item_attribute_value (model, n, "hidden-when",
                                                               G_VARIANT_TYPE_STRING);
          if (hidden_when != NULL)
            {
              /* skip the GMenuItem if hidden when action is missing */
              if (g_strcmp0 (g_variant_get_string (hidden_when, NULL), "action-missing") == 0
                  && action_name == NULL)
                continue;

              /* skip the GMenuItem if hidden when action is disabled (but not missing) */
              if (g_strcmp0 (g_variant_get_string (hidden_when, NULL), "action-disabled") == 0
                  && action_name != NULL)
                {
                  /* retrieve action group */
                  if (g_str_has_prefix (action_name, "win."))
                    action_group = G_ACTION_GROUP (window);
                  else if (g_str_has_prefix (action_name, "app."))
                    action_group = G_ACTION_GROUP (gtk_window_get_application (GTK_WINDOW (window)));
                  /* in particular, the use of action namespaces in '.ui' files is not supported */
                  else
                    {
                      g_warn_if_reached ();
                      action_group = NULL;
                    }

                  if (action_group != NULL
                      && ! g_action_group_get_action_enabled (action_group, action_name + 4))
                    continue;
                }

              /* cleanup */
              g_variant_unref (hidden_when);
            }

          /* realign menu item */
          if (realign)
            child->data = mousepad_window_menu_item_realign (window, child->data,
                                                             action_name, menu, *offset);

          /* set the tooltip on the corresponding GtkMenuItem */
          tooltip = g_menu_model_get_item_attribute_value (model, n, "tooltip",
                                                           G_VARIANT_TYPE_STRING);
          if (tooltip != NULL)
            {
              gtk_widget_set_tooltip_text (child->data, g_variant_get_string (tooltip, NULL));
              g_variant_unref (tooltip);
            }
          /* set a whitespace instead of NULL or an empty string, otherwise an absence of menu item
           * tooltip could reveal an underlying menu tooltip (e.g. in the case of a new unnamed
           * document in the go-to-tab menu) */
          else
            gtk_widget_set_tooltip_text (child->data, " ");

          /* don't show the tooltip as a tooltip */
          gtk_widget_set_has_tooltip (child->data, FALSE);

          /* connect signals to show the tooltip in the status bar */
          g_signal_connect_object (child->data, "select",
                                   G_CALLBACK (mousepad_window_menu_item_selected),
                                   window, 0);
          g_signal_connect_object (child->data, "deselect",
                                   G_CALLBACK (mousepad_window_menu_item_deselected),
                                   window, 0);

          /* submenu GMenuItem: go ahead recursively */
          if ((submodel = g_menu_model_get_item_link (model, n, G_MENU_LINK_SUBMENU)) != NULL)
            {
              submenu = gtk_menu_item_get_submenu (child->data);
              mousepad_window_menu_set_tooltips (window, submenu, submodel, NULL);
            }

          child = child->next;
          (*offset)++;
        }
    }

  /* cleanup */
  g_list_free (children);
}



/**
 * Mousepad Window Functions
 **/
static gboolean
mousepad_window_open_file (MousepadWindow   *window,
                           GFile            *file,
                           MousepadEncoding  encoding,
                           gint              line,
                           gint              column,
                           gboolean          must_exist)
{
  MousepadDocument *document;
  GtkNotebook      *notebook;
  GList            *list, *li;
  GError           *error = NULL;
  GFile            *open_file;
  gchar            *uri;
  const gchar      *autosave_uri;
  gint              npages, result, i;
  gboolean          user_set_encoding, user_set_cursor, succeed;

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (file != NULL, FALSE);

  /* check if the file is already open */
  list = gtk_application_get_windows (GTK_APPLICATION (
           gtk_window_get_application (GTK_WINDOW (window))));
  for (li = list; li != NULL; li = li->next)
    {
      notebook = GTK_NOTEBOOK (MOUSEPAD_WINDOW (li->data)->notebook);
      npages = gtk_notebook_get_n_pages (notebook);
      for (i = 0; i < npages; i++)
        {
          /* see if the file is already opened */
          document = MOUSEPAD_DOCUMENT (gtk_notebook_get_nth_page (notebook, i));
          open_file = mousepad_file_get_location (document->file);
          if (open_file != NULL && g_file_equal (file, open_file))
            {
              /* switch to the tab and present the window */
              gtk_notebook_set_current_page (notebook, i);
              gtk_window_present (li->data);

              /* and we're done */
              return TRUE;
            }
        }
    }

  /* new document */
  document = mousepad_document_new ();

  /* make sure it's not a floating object */
  g_object_ref_sink (document);

  /* get the data status attached to the GFile */
  user_set_encoding = GPOINTER_TO_INT (mousepad_object_get_data (file, "user-set-encoding"));
  user_set_cursor = GPOINTER_TO_INT (mousepad_object_get_data (file, "user-set-cursor"));
  autosave_uri = mousepad_object_get_data (file, "autosave-uri");

  /* set the file location */
  mousepad_file_set_location (document->file, file,
                              autosave_uri == NULL ? MOUSEPAD_LOCATION_REAL
                                                   : MOUSEPAD_LOCATION_VIRTUAL);

  /* the user chose to open the file in the encoding dialog */
  if (encoding == MOUSEPAD_ENCODING_NONE)
    {
      /* run the encoding dialog */
      if (mousepad_encoding_dialog (GTK_WINDOW (window), document->file, FALSE, &encoding)
          != MOUSEPAD_RESPONSE_OK)
        {
          /* release the document */
          g_object_unref (document);

          return FALSE;
        }

      user_set_encoding = TRUE;
    }
  /* try to lookup the encoding from the recent history if not set by the user */
  else if (! user_set_encoding)
    {
      MousepadEncoding history_encoding = MOUSEPAD_ENCODING_NONE;

      mousepad_history_recent_get_encoding (file, &history_encoding);
      if (history_encoding != MOUSEPAD_ENCODING_NONE)
        {
          encoding = history_encoding;
          user_set_encoding = TRUE;
        }
    }

  /* try to lookup the cursor position from the recent history if not set by the user */
  if (! user_set_cursor)
    mousepad_history_recent_get_cursor (file, &line, &column);

  retry:

  /* set the file encoding */
  mousepad_file_set_encoding (document->file, encoding);

  /* lock the undo manager */
  gtk_source_buffer_begin_not_undoable_action (GTK_SOURCE_BUFFER (document->buffer));

  /* read the content into the buffer */
  result = mousepad_file_open (document->file, line, column, must_exist,
                               FALSE, user_set_encoding, &error);

  /* release the lock */
  gtk_source_buffer_end_not_undoable_action (GTK_SOURCE_BUFFER (document->buffer));

  switch (result)
    {
      case 0:
        /* make sure the window wasn't destroyed during the file opening process
         * (e.g. by triggering "app.quit" when the dialog to confirm encoding is open) */
        if (G_LIKELY (mousepad_is_application_window (window)))
          {
            /* add the document to the window */
            mousepad_window_add (window, document);

            /* scroll to cursor if -l or -c is used */
            if (line != 0 || column != 0)
              g_idle_add (mousepad_view_scroll_to_cursor,
                          mousepad_util_source_autoremove (window->active->textview));

            /* insert in the recent history, don't pollute with autosave data */
            if (autosave_uri == NULL)
              mousepad_history_recent_add (document->file);
          }
        break;

      case ERROR_CONVERTING_FAILED:
      case ERROR_ENCODING_NOT_VALID:
        /* clear the error */
        g_clear_error (&error);

        /* run the encoding dialog */
        if (mousepad_encoding_dialog (GTK_WINDOW (window), document->file, FALSE, &encoding)
            == MOUSEPAD_RESPONSE_OK)
          {
            user_set_encoding = TRUE;
            goto retry;
          }

        break;

      default:
        /* something went wrong */
        if (G_LIKELY (error != NULL))
          {
            /* show the warning */
            mousepad_dialogs_show_error (GTK_WINDOW (window), error,
                                         MOUSEPAD_MESSAGE_IO_ERROR_OPEN);

            /* cleanup */
            g_error_free (error);
          }

        break;
    }

  /* decrease reference count if everything went well, else release the document */
  g_object_unref (document);

  /* autosave restore: some post-process actions if everything went well */
  succeed = (result == 0);
  if (succeed && autosave_uri != NULL)
    {
      /* set definitive location */
      uri = g_file_get_uri (file);
      if (g_strcmp0 (uri, autosave_uri) == 0)
        {
          mousepad_file_set_location (document->file, NULL, MOUSEPAD_LOCATION_REVERT);
          gtk_text_buffer_set_modified (document->buffer, TRUE);
        }
      else
        {
          mousepad_object_set_data (file, "autosave-uri", NULL);
          mousepad_file_set_location (document->file, file, MOUSEPAD_LOCATION_REAL);
          mousepad_file_invalidate_saved_state (document->file);
        }

      g_free (uri);
    }

  return succeed;
}



gint
mousepad_window_open_files (MousepadWindow    *window,
                            GFile            **files,
                            gint               n_files,
                            MousepadEncoding   encoding,
                            gint               line,
                            gint               column,
                            gboolean           must_exist)
{
  gint n_tabs_in, n_tabs_out, n, ret = -1;

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), 0);
  g_return_val_if_fail (files != NULL, 0);

  /* get the number of tabs before trying to open new files */
  n_tabs_in = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

  /* block menu updates */
  lock_menu_updates++;

  /* open new tabs with the files */
  for (n = 0; n < n_files; n++)
    mousepad_window_open_file (window, files[n], encoding, line, column, must_exist);

  /* allow menu updates again */
  lock_menu_updates--;

  /* return the number of open documents, unless the window was destroyd, e.g. by "app.quit" */
  if (G_LIKELY (mousepad_is_application_window (window)))
    {
      n_tabs_out = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));
      if (n_tabs_out > 0)
        ret = n_tabs_out - n_tabs_in;
    }

  return ret;
}



void
mousepad_window_add (MousepadWindow   *window,
                     MousepadDocument *document)
{
  MousepadDocument *prev_active = window->active;
  GtkNotebook      *notebook = GTK_NOTEBOOK (window->notebook);
  GtkWidget        *label, *widget = GTK_WIDGET (document);
  gint              page;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  /* create the tab label */
  label = mousepad_document_get_tab_label (document);

  /* get active page */
  page = gtk_notebook_get_current_page (notebook);

  /* insert the page right of the active tab */
  page = gtk_notebook_insert_page (notebook, widget, label, page + 1);

  /* set tab child properties */
  gtk_notebook_set_tab_reorderable (notebook, widget, TRUE);
  gtk_notebook_set_tab_detachable (notebook, widget, TRUE);

  /* show the document */
  gtk_widget_show (widget);

  /* don't bother about this when there was no previous active page (startup) */
  if (G_LIKELY (prev_active != NULL))
    {
      /* switch to the new tab */
      gtk_notebook_set_current_page (notebook, page);

      /* remove the previous tab if it was not modified, untitled and the new tab is not untitled */
      page = gtk_notebook_page_num (notebook, GTK_WIDGET (prev_active));
      if (! gtk_text_buffer_get_modified (prev_active->buffer)
          && ! mousepad_file_location_is_set (prev_active->file)
          && mousepad_file_location_is_set (document->file))
        gtk_notebook_remove_page (notebook, page);
    }

  /* make sure the textview is focused in the new document */
  mousepad_document_focus_textview (document);
}



static gboolean
mousepad_window_close_document (MousepadWindow   *window,
                                MousepadDocument *document)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (window->notebook);
  GAction     *action;
  gint         restore, quitting;
  gboolean     modified, autosave, succeed = FALSE;

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), FALSE);

  /* check if the document has been modified or the file deleted */
  modified = gtk_text_buffer_get_modified (document->buffer);
  if (modified || (
        mousepad_file_location_is_set (document->file)
        && ! mousepad_util_query_exists (mousepad_file_get_location (document->file), TRUE)
      ))
    {
      restore = MOUSEPAD_SETTING_GET_ENUM (SESSION_RESTORE);
      quitting = mousepad_history_session_get_quitting ();
      autosave = (quitting == MOUSEPAD_SESSION_QUITTING_NON_INTERACTIVE || (
                    quitting == MOUSEPAD_SESSION_QUITTING_INTERACTIVE && (
                      restore == MOUSEPAD_SESSION_RESTORE_UNSAVED
                      || restore == MOUSEPAD_SESSION_RESTORE_ALWAYS
                 )));

      /* non-interactive saving */
      if (modified && autosave)
        succeed = mousepad_file_autosave_save_sync (document->file);
      /* interactive procedure */
      else if (quitting != MOUSEPAD_SESSION_QUITTING_NON_INTERACTIVE)
        {
          /* mark the document as modified if it is not already so */
          if (! modified)
            mousepad_file_invalidate_saved_state (document->file);

          /* run save changes dialog */
          switch (mousepad_dialogs_save_changes (GTK_WINDOW (window), TRUE,
                                                 mousepad_file_get_read_only (document->file)))
            {
              case MOUSEPAD_RESPONSE_DONT_SAVE:
                /* don't save, only destroy the document, eventually triggering
                 * autosave file deletion */
                gtk_text_buffer_set_modified (document->buffer, FALSE);
                succeed = TRUE;
                break;

              case MOUSEPAD_RESPONSE_CANCEL:
                /* do nothing */
                break;

              case MOUSEPAD_RESPONSE_SAVE:
                action = g_action_map_lookup_action (G_ACTION_MAP (window), "file.save");
                g_action_activate (action, NULL);
                succeed = mousepad_action_get_state_int32_boolean (action);
                break;

              case MOUSEPAD_RESPONSE_SAVE_AS:
                action = g_action_map_lookup_action (G_ACTION_MAP (window), "file.save-as");
                g_action_activate (action, NULL);
                succeed = mousepad_action_get_state_int32_boolean (action);
                break;
            }
        }
      /* the file was deleted, file monitoring is disabled or didn't mark the document
       * as modified in time, and we are in non-interactive mode: it's too late to handle
       * this case */
      else /* ! modified && quitting == MOUSEPAD_SESSION_QUITTING_NON_INTERACTIVE */
        succeed = TRUE;
    }
  /* no changes in the document, safe to remove it */
  else
    succeed = TRUE;

  /* remove the document */
  if (succeed)
    {
      /* store some data in the recent history if the file exists on disk */
      if (mousepad_file_location_is_set (document->file)
          && mousepad_util_query_exists (mousepad_file_get_location (document->file), TRUE))
        mousepad_history_recent_add (document->file);

      gtk_notebook_remove_page (notebook, gtk_notebook_page_num (notebook, GTK_WIDGET (document)));
    }

  return succeed;
}



static void
mousepad_window_button_close_tab (MousepadDocument *document,
                                  MousepadWindow   *window)
{
  gint page_num;

  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* switch to the tab we're going to close */
  page_num = gtk_notebook_page_num (GTK_NOTEBOOK (window->notebook), GTK_WIDGET (document));
  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), page_num);

  /* close the document */
  mousepad_window_close_document (window, document);
}



static void
mousepad_window_set_title (MousepadWindow *window)
{
  gchar            *string;
  const gchar      *title;
  gboolean          show_full_path;
  MousepadDocument *document = window->active;

  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* whether to show the full path */
  show_full_path = MOUSEPAD_SETTING_GET_BOOLEAN (PATH_IN_TITLE);

  /* name we display in the title */
  if (G_UNLIKELY (show_full_path && mousepad_document_get_filename (document)))
    title = mousepad_document_get_filename (document);
  else
    title = mousepad_document_get_basename (document);

  /* build the title */
  if (G_UNLIKELY (mousepad_file_get_read_only (document->file)))
    string = g_strdup_printf ("%s%s [%s] - %s",
                              gtk_text_buffer_get_modified (document->buffer) ? "*" : "",
                              title, _("Read Only"), PACKAGE_NAME);
  else if (G_UNLIKELY (! gtk_text_view_get_editable (GTK_TEXT_VIEW (document->textview))))
    string = g_strdup_printf ("%s%s [%s] - %s",
                              gtk_text_buffer_get_modified (document->buffer) ? "*" : "",
                              title, _("Viewer Mode"), PACKAGE_NAME);
  else
    string = g_strdup_printf ("%s%s - %s",
                              gtk_text_buffer_get_modified (document->buffer) ? "*" : "",
                              title, PACKAGE_NAME);

  /* set the window title */
  gtk_window_set_title (GTK_WINDOW (window), string);

  /* cleanup */
  g_free (string);
}



/* give the statusbar a languages menu created from the application resources */
GtkWidget *
mousepad_window_get_languages_menu (MousepadWindow *window)
{
  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), NULL);

  return window->languages_menu;
}



static gboolean
mousepad_window_get_in_fullscreen (MousepadWindow *window)
{
  if (gtk_widget_get_visible (GTK_WIDGET (window)))
    {
      GdkWindow     *win = gtk_widget_get_window (GTK_WIDGET (window));
      GdkWindowState state = gdk_window_get_state (win);
      return (state & GDK_WINDOW_STATE_FULLSCREEN);
    }

  return FALSE;
}



static gboolean
mousepad_window_menubar_hide_event (MousepadWindow *window)
{
  mousepad_disconnect_by_func (window, mousepad_window_menubar_hide_event, NULL);
  mousepad_disconnect_by_func (window->menubar, mousepad_window_menubar_hide_event, window);
  mousepad_disconnect_by_func (window->notebook, mousepad_window_menubar_hide_event, window);
  gtk_widget_hide (window->menubar);

  return FALSE;
}



static gboolean
mousepad_window_menubar_focus_out_event (GtkWidget *widget,
                                         GdkEventFocus *event,
                                         gboolean *alt_pressed)
{
  *alt_pressed = FALSE;
  return FALSE;
}



static gboolean
mousepad_window_menubar_key_event (MousepadWindow *window,
                                   GdkEventKey    *event,
                                   GList          *mnemonics)
{
  GdkEvent *event_bis;
  static gboolean hidden_last_time = FALSE, alt_pressed = FALSE;

  /* only show the menubar on Alt key release if it matches an Alt key press without losing
   * focus in between (especially when grabbing the window via the Alt key, see issue #185) */
  mousepad_disconnect_by_func (window, mousepad_window_menubar_focus_out_event, &alt_pressed);
  if (event->type == GDK_KEY_PRESS)
    {
      alt_pressed = event->keyval == GDK_KEY_Alt_L;
      if (alt_pressed)
        g_signal_connect (window, "focus-out-event",
                          G_CALLBACK (mousepad_window_menubar_focus_out_event), &alt_pressed);
    }

  /* Alt key was pressed (alone or as a GdkModifierType) or released, or Esc key was pressed */
  if (event->state & GDK_MOD1_MASK || event->keyval == GDK_KEY_Alt_L
      || (event->keyval == GDK_KEY_Escape && event->type == GDK_KEY_PRESS))
    {
      /* hide the menubar if Alt/Esc key was pressed */
      if (event->type == GDK_KEY_PRESS
          && (event->keyval == GDK_KEY_Alt_L || event->keyval == GDK_KEY_Escape)
          && gtk_widget_get_visible (window->menubar))
        {
          /* disconnect signals and hide the menubar */
          mousepad_window_menubar_hide_event (window);

          /* don't show the menubar when the Alt key is released this time */
          hidden_last_time = TRUE;

          return TRUE;
        }
      /* show the menubar if Alt key was released or if one of its mnemonic keys matched */
      else if (! hidden_last_time && ! gtk_widget_get_visible (window->menubar)
               && ((alt_pressed && event->keyval == GDK_KEY_Alt_L && event->type == GDK_KEY_RELEASE)
                   || (event->type == GDK_KEY_PRESS && event->state & GDK_MOD1_MASK
                       && g_list_find (mnemonics, GUINT_TO_POINTER (event->keyval)))))
        {
          /* show the menubar and connect signals to hide it afterwards on user actions */
          gtk_widget_show (window->menubar);
          g_signal_connect (window, "button-press-event",
                            G_CALLBACK (mousepad_window_menubar_hide_event), NULL);
          g_signal_connect (window, "button-release-event",
                            G_CALLBACK (mousepad_window_menubar_hide_event), NULL);
          g_signal_connect (window, "focus-out-event",
                            G_CALLBACK (mousepad_window_menubar_hide_event), NULL);
          g_signal_connect (window, "scroll-event",
                            G_CALLBACK (mousepad_window_menubar_hide_event), NULL);
          g_signal_connect_swapped (window->menubar, "deactivate",
                                    G_CALLBACK (mousepad_window_menubar_hide_event), window);
          g_signal_connect_swapped (window->notebook, "button-press-event",
                                    G_CALLBACK (mousepad_window_menubar_hide_event), window);

          /* in case of a mnemonic key, repeat the same event to make its menu popup */
          if (event->keyval != GDK_KEY_Alt_L)
            {
              event_bis = gdk_event_copy ((GdkEvent *) event);
              gtk_main_do_event (event_bis);
              gdk_event_free (event_bis);
            }

          alt_pressed = FALSE;
          return TRUE;
        }
    }

  /* show the menubar the next time the Alt key is released */
  hidden_last_time = FALSE;

  if (event->type == GDK_KEY_RELEASE)
    alt_pressed = FALSE;

  return FALSE;
}



static void
mousepad_window_update_bar_visibility (MousepadWindow *window,
                                       const gchar    *hint)
{
  GtkWidget   *bar;
  GVariant    *state;
  const gchar *setting, *setting_fs;
  gboolean     visible, visible_fs;

  /* the hint may or may not contain the whole setting name and/or the fullscreen suffix,
   * but it will always be included in their concatenation */
  if (g_strstr_len (MOUSEPAD_SETTING_MENUBAR_VISIBLE_FULLSCREEN, -1, hint))
    {
      setting = MENUBAR;
      setting_fs = MOUSEPAD_SETTING_MENUBAR_VISIBLE_FULLSCREEN;
      bar = window->menubar;
    }
  else if (g_strstr_len (MOUSEPAD_SETTING_TOOLBAR_VISIBLE_FULLSCREEN, -1, hint))
    {
      setting = TOOLBAR;
      setting_fs = MOUSEPAD_SETTING_TOOLBAR_VISIBLE_FULLSCREEN;
      bar = window->toolbar;
    }
  else if (g_strstr_len (MOUSEPAD_SETTING_STATUSBAR_VISIBLE_FULLSCREEN, -1, hint))
    {
      setting = STATUSBAR;
      setting_fs = MOUSEPAD_SETTING_STATUSBAR_VISIBLE_FULLSCREEN;
      bar = window->statusbar;
    }
  /* should not be reached */
  else
    return;

  /* get visibility setting in window mode */
  visible = mousepad_setting_get_boolean (setting);

  /* deduce the visibility setting if we are in fullscreen mode */
  if (mousepad_window_get_in_fullscreen (window))
    {
      visible_fs = mousepad_setting_get_enum (setting_fs);
      visible = (visible_fs == AUTO) ? visible : (visible_fs == YES);
    }

  /* update the bar visibility */
  gtk_widget_set_visible (bar, visible);

  /* avoid menu actions */
  lock_menu_updates++;

  /* request for the action state to be changed according to the setting */
  state = mousepad_setting_get_variant (setting);
  g_action_group_change_action_state (G_ACTION_GROUP (window), setting, state);
  g_variant_unref (state);

  /* allow menu actions again */
  lock_menu_updates--;
}



static void
mousepad_window_update_tabs_visibility (MousepadWindow *window,
                                        gchar          *key,
                                        GSettings      *settings)
{
  gint     n_pages;
  gboolean always_show;

  always_show = MOUSEPAD_SETTING_GET_BOOLEAN (ALWAYS_SHOW_TABS);

  n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (window->notebook), n_pages > 1 || always_show);
}



/**
 * Notebook Signal Functions
 **/
GtkWidget *
mousepad_window_get_notebook (MousepadWindow *window)
{
  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), NULL);

  return window->notebook;
}



static void
mousepad_window_update_actions (MousepadWindow *window)
{
  GAction            *action;
  GtkNotebook        *notebook;
  MousepadDocument   *document;
  GtkSourceLanguage  *language;
  MousepadLineEnding  line_ending;
  gboolean            cycle_tabs, value;
  gint                n_pages, page_num;
  const gchar        *language_id;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  notebook = GTK_NOTEBOOK (window->notebook);
  document = window->active;

  /* update the actions for the active document */
  if (G_LIKELY (document))
    {
      /* avoid menu actions */
      lock_menu_updates++;

      /* determine the number of pages and the current page number */
      n_pages = gtk_notebook_get_n_pages (notebook);
      page_num = gtk_notebook_page_num (notebook, GTK_WIDGET (document));

      /* whether we cycle tabs */
      cycle_tabs = MOUSEPAD_SETTING_GET_BOOLEAN (CYCLE_TABS);

      /* set the sensitivity of the back and forward buttons in the go menu */
      action = g_action_map_lookup_action (G_ACTION_MAP (window), "document.previous-tab");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                   (cycle_tabs && n_pages > 1) || page_num > 0);

      action = g_action_map_lookup_action (G_ACTION_MAP (window), "document.next-tab");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                   (cycle_tabs && n_pages > 1 ) || page_num < n_pages - 1);

      /* set the reload, detach and save sensitivity */
      action = g_action_map_lookup_action (G_ACTION_MAP (window), "file.save");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                   mousepad_file_is_savable (document->file));

      action = g_action_map_lookup_action (G_ACTION_MAP (window), "file.detach-tab");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), n_pages > 1);

      action = g_action_map_lookup_action (G_ACTION_MAP (window), "file.reload");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                   mousepad_file_location_is_set (document->file));

      /* set the sensitivity of the undo and redo actions */
      mousepad_window_can_undo (GTK_SOURCE_BUFFER (document->buffer), NULL, window);
      mousepad_window_can_redo (GTK_SOURCE_BUFFER (document->buffer), NULL, window);

      /* set the current line ending type */
      line_ending = mousepad_file_get_line_ending (document->file);
      g_action_group_change_action_state (G_ACTION_GROUP (window), "document.line-ending",
                                          g_variant_new_int32 (line_ending));

      /* write bom */
      value = mousepad_file_get_write_bom (document->file);
      g_action_group_change_action_state (G_ACTION_GROUP (window), "document.write-unicode-bom",
                                          g_variant_new_boolean (value));

      /* viewer mode */
      value = ! gtk_text_view_get_editable (GTK_TEXT_VIEW (document->textview));
      g_action_group_change_action_state (G_ACTION_GROUP (window), "document.viewer-mode",
                                          g_variant_new_boolean (value));

      /* update the currently active language */
      language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (document->buffer));
      language_id = language != NULL ? gtk_source_language_get_id (language)
                                     : MOUSEPAD_LANGUAGE_NONE;
      g_action_group_change_action_state (G_ACTION_GROUP (window), "document.filetype",
                                          g_variant_new_string (language_id));

      /* update document dependent menu items */
      mousepad_window_update_document_menu_items (window);

      /* allow menu actions again */
      lock_menu_updates--;
    }
}



static void
mousepad_window_notebook_switch_page (GtkNotebook    *notebook,
                                      GtkWidget      *page,
                                      guint           page_num,
                                      MousepadWindow *window)
{
  MousepadDocument *document;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  /* get the new active document */
  document = MOUSEPAD_DOCUMENT (gtk_notebook_get_nth_page (notebook, page_num));

  /* only update when really changed */
  if (G_LIKELY (window->active != document))
    {
      /* set old and new active documents */
      window->previous = window->active;
      window->active = document;

      /* set the window title */
      mousepad_window_set_title (window);

      /* update the menu actions */
      mousepad_window_update_actions (window);

      /* update the statusbar */
      mousepad_document_send_signals (window->active);
    }
}



static void
mousepad_window_notebook_added (GtkNotebook     *notebook,
                                GtkWidget       *page,
                                guint            page_num,
                                MousepadWindow  *window)
{
  MousepadDocument *document = MOUSEPAD_DOCUMENT (page);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* connect signals to the document for this window */
  g_signal_connect (page, "close-tab",
                    G_CALLBACK (mousepad_window_button_close_tab), window);
  g_signal_connect (page, "cursor-changed",
                    G_CALLBACK (mousepad_window_cursor_changed), window);
  g_signal_connect (page, "encoding-changed",
                    G_CALLBACK (mousepad_window_encoding_changed), window);
  g_signal_connect (page, "language-changed",
                    G_CALLBACK (mousepad_window_language_changed), window);
  g_signal_connect (page, "overwrite-changed",
                    G_CALLBACK (mousepad_window_overwrite_changed), window);
  g_signal_connect (page, "search-completed",
                    G_CALLBACK (mousepad_window_search_completed), window);
  g_signal_connect (document->buffer, "notify::has-selection",
                    G_CALLBACK (mousepad_window_enable_edit_actions), window);
  g_signal_connect (document->buffer, "notify::can-undo",
                    G_CALLBACK (mousepad_window_can_undo), window);
  g_signal_connect (document->buffer, "notify::can-redo",
                    G_CALLBACK (mousepad_window_can_redo), window);
  g_signal_connect (document->buffer, "modified-changed",
                    G_CALLBACK (mousepad_window_modified_changed), window);
  g_signal_connect (document->file, "externally-modified",
                    G_CALLBACK (mousepad_window_externally_modified), window);
  g_signal_connect (document->file, "location-changed",
                    G_CALLBACK (mousepad_window_location_changed), window);
  g_signal_connect (document->file, "readonly-changed",
                    G_CALLBACK (mousepad_window_readonly_changed), window);
  g_signal_connect (document->textview, "drag-data-received",
                    G_CALLBACK (mousepad_window_drag_data_received), window);
  g_signal_connect (document->textview, "populate-popup",
                    G_CALLBACK (mousepad_window_menu_textview_popup), window);
  g_signal_connect (document->textview, "notify::has-focus",
                    G_CALLBACK (mousepad_window_enable_edit_actions), window);

  /* change the visibility of the tabs accordingly */
  mousepad_window_update_tabs_visibility (window, NULL, NULL);
}



static void
mousepad_window_notebook_removed (GtkNotebook     *notebook,
                                  GtkWidget       *page,
                                  guint            page_num,
                                  MousepadWindow  *window)
{
  MousepadDocument *document = MOUSEPAD_DOCUMENT (page);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  /* disconnect the old document signals */
  mousepad_disconnect_by_func (page, mousepad_window_button_close_tab, window);
  mousepad_disconnect_by_func (page, mousepad_window_cursor_changed, window);
  mousepad_disconnect_by_func (page, mousepad_window_encoding_changed, window);
  mousepad_disconnect_by_func (page, mousepad_window_language_changed, window);
  mousepad_disconnect_by_func (page, mousepad_window_overwrite_changed, window);
  mousepad_disconnect_by_func (page, mousepad_window_search_completed, window);
  mousepad_disconnect_by_func (document->buffer, mousepad_window_enable_edit_actions, window);
  mousepad_disconnect_by_func (document->buffer, mousepad_window_can_undo, window);
  mousepad_disconnect_by_func (document->buffer, mousepad_window_can_redo, window);
  mousepad_disconnect_by_func (document->buffer, mousepad_window_modified_changed, window);
  mousepad_disconnect_by_func (document->file, mousepad_window_externally_modified, window);
  mousepad_disconnect_by_func (document->file, mousepad_window_location_changed, window);
  mousepad_disconnect_by_func (document->file, mousepad_window_readonly_changed, window);
  mousepad_disconnect_by_func (document->textview, mousepad_window_drag_data_received, window);
  mousepad_disconnect_by_func (document->textview, mousepad_window_menu_textview_popup, window);
  mousepad_disconnect_by_func (document->textview, mousepad_window_enable_edit_actions, window);

  /* reset the reference to NULL to avoid illegal memory access */
  if (window->previous == document)
    window->previous = NULL;

  /* window contains no tabs: save geometry and destroy it */
  if (gtk_notebook_get_n_pages (notebook) == 0)
    {
      mousepad_window_configure_event (GTK_WIDGET (window), NULL);
      gtk_widget_destroy (GTK_WIDGET (window));
    }
  /* change the visibility of the tabs accordingly */
  else
    mousepad_window_update_tabs_visibility (window, NULL, NULL);
}



/* stolen from Geany notebook.c */
static gboolean
mousepad_window_is_position_on_tab_bar (GtkNotebook *notebook, GdkEventButton *event)
{
  GtkWidget      *page, *tab, *nb;
  GtkPositionType tab_pos;
  gint            scroll_arrow_hlength, scroll_arrow_vlength;
  gdouble         x, y;

  page = gtk_notebook_get_nth_page (notebook, 0);
  g_return_val_if_fail (page != NULL, FALSE);

  tab = gtk_notebook_get_tab_label (notebook, page);
  g_return_val_if_fail (tab != NULL, FALSE);

  tab_pos = gtk_notebook_get_tab_pos (notebook);
  nb = GTK_WIDGET (notebook);

  gtk_widget_style_get (GTK_WIDGET (notebook),
                        "scroll-arrow-hlength", &scroll_arrow_hlength,
                        "scroll-arrow-vlength", &scroll_arrow_vlength,
                        NULL);

  if (! gdk_event_get_coords ((GdkEvent *) event, &x, &y))
    {
      x = event->x;
      y = event->y;
    }

  switch (tab_pos)
    {
    case GTK_POS_TOP:
    case GTK_POS_BOTTOM:
      if (event->y >= 0 && event->y <= gtk_widget_get_allocated_height (tab))
        {
          if (! gtk_notebook_get_scrollable (notebook) || (
            x > scroll_arrow_hlength &&
            x < gtk_widget_get_allocated_width (nb) - scroll_arrow_hlength))
          {
            return TRUE;
          }
        }
      break;
    case GTK_POS_LEFT:
    case GTK_POS_RIGHT:
        if (event->x >= 0 && event->x <= gtk_widget_get_allocated_width (tab))
          {
            if (! gtk_notebook_get_scrollable (notebook) || (
                y > scroll_arrow_vlength &&
                y < gtk_widget_get_allocated_height (nb) - scroll_arrow_vlength))
              {
                return TRUE;
              }
          }
    }

  return FALSE;
}



static gboolean
mousepad_window_notebook_button_press_event (GtkNotebook    *notebook,
                                             GdkEventButton *event,
                                             MousepadWindow *window)
{
  GtkWidget *page, *label;
  guint      page_num = 0;
  gint       x_root, y_root;

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);

  if (event->type == GDK_BUTTON_PRESS && (event->button == 3 || event->button == 2))
    {
      /* walk through the tabs and look for the tab under the cursor */
      while ((page = gtk_notebook_get_nth_page (notebook, page_num)) != NULL)
        {
          GtkAllocation alloc = { 0, 0, 0, 0 };

          label = gtk_notebook_get_tab_label (notebook, page);

          /* get the origin of the label */
          gdk_window_get_origin (gtk_widget_get_window (label), &x_root, &y_root);
          gtk_widget_get_allocation (label, &alloc);
          x_root += alloc.x;
          y_root += alloc.y;

          /* check if the cursor is inside this label */
          if (event->x_root >= x_root && event->x_root <= (x_root + alloc.width)
              && event->y_root >= y_root && event->y_root <= (y_root + alloc.height))
            {
              /* switch to this tab */
              gtk_notebook_set_current_page (notebook, page_num);

              /* show the menu */
              if (event->button == 3)
                gtk_menu_popup_at_pointer (GTK_MENU (window->tab_menu), (GdkEvent *) event);
              /* close the document */
              else if (event->button == 2)
                g_action_group_activate_action (G_ACTION_GROUP (window), "file.close-tab", NULL);

              /* we succeed */
              return TRUE;
            }

          /* try the next tab */
          ++page_num;
        }
    }
  else if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
    {
      GtkWidget *ev_widget, *nb_child;

      ev_widget = gtk_get_event_widget ((GdkEvent *) event);
      nb_child = gtk_notebook_get_nth_page (notebook,
                                            gtk_notebook_get_current_page (notebook));
      if (ev_widget == NULL || ev_widget == nb_child || gtk_widget_is_ancestor (ev_widget, nb_child))
        return FALSE;

      /* check if the event window is the notebook event window (not a tab) */
      if (mousepad_window_is_position_on_tab_bar (notebook, event))
        {
          /* create new document */
          g_action_group_activate_action (G_ACTION_GROUP (window), "file.new", NULL);

          /* we succeed */
          return TRUE;
        }
    }

  return FALSE;
}



static gboolean
mousepad_window_notebook_button_release_event (GtkNotebook    *notebook,
                                               GdkEventButton *event,
                                               MousepadWindow *window)
{
  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (window->active), FALSE);

  /* focus the active textview */
  mousepad_document_focus_textview (window->active);

  return FALSE;
}



static GtkNotebook *
mousepad_window_notebook_create_window (GtkNotebook    *notebook,
                                        GtkWidget      *page,
                                        gint            x,
                                        gint            y,
                                        MousepadWindow *window)
{
  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), NULL);
  g_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (page), NULL);

  /* only create new window when there are more than 2 tabs */
  if (gtk_notebook_get_n_pages (notebook) >= 2)
    {
      /* take a reference */
      g_object_ref (page);

      /* remove the document from the active window */
      gtk_notebook_detach_tab (GTK_NOTEBOOK (window->notebook), page);

      /* emit the new window with document signal */
      g_signal_emit (window, window_signals[NEW_WINDOW_WITH_DOCUMENT], 0, page, x, y);

      /* release our reference */
      g_object_unref (page);
    }

  return NULL;
}



/**
 * Document Signals Functions
 **/
static gboolean
mousepad_window_pending_widget_idle (gpointer data)
{
  MousepadDocument *document = data;
  MousepadWindow   *window;

  window = MOUSEPAD_WINDOW (gtk_widget_get_ancestor (data, MOUSEPAD_TYPE_WINDOW));
  if (window != NULL)
    mousepad_window_externally_modified (document->file, window);

  return FALSE;
}



static void
mousepad_window_pending_window (MousepadWindow *window,
                                GParamSpec     *pspec,
                                gpointer       *document)
{
  /* disconnect this handler */
  mousepad_disconnect_by_func (window, mousepad_window_pending_window, document);

  /* try again to inform the user about this file whenever idle, to allow for a possible
   * closing process to take place before */
  g_idle_add (mousepad_window_pending_widget_idle,
              mousepad_util_source_autoremove (document));
}



static void
mousepad_window_pending_tab (GtkNotebook  *notebook,
                             GtkWidget    *page,
                             guint         page_num,
                             MousepadFile *file)
{
  if (MOUSEPAD_DOCUMENT (page)->file == file)
    {
      /* disconnect this handler */
      mousepad_disconnect_by_func (notebook, mousepad_window_pending_tab, file);

      /* try again to inform the user about this file whenever idle, to allow for a possible
       * closing process to take place before */
      g_idle_add (mousepad_window_pending_widget_idle, mousepad_util_source_autoremove (page));
    }
}



static void
mousepad_window_externally_modified (MousepadFile   *file,
                                     MousepadWindow *window)
{
  MousepadDocument *document = window->active;
  gboolean modified;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_FILE (file));

  /* disconnect this handler, the time we ask the user what to do or the file is loadable */
  mousepad_disconnect_by_func (file, mousepad_window_externally_modified, window);

  /* auto-reload the file if it's unmodified and in the active tab (active window or not) */
  modified = gtk_text_buffer_get_modified (document->buffer);
  if (! modified && document->file == file && MOUSEPAD_SETTING_GET_BOOLEAN (AUTO_RELOAD))
    {
      g_signal_connect (file, "externally-modified",
                        G_CALLBACK (mousepad_window_externally_modified), window);
      g_action_group_activate_action (G_ACTION_GROUP (window), "file.reload", NULL);
      return;
    }

  /* the file is active */
  if (document->file == file && gtk_window_is_active (GTK_WINDOW (window)))
    {
      /* keep a reference on the document in case it is removed during the process
       * below, e.g. by an "app.quit" */
      g_object_ref (document);

      /* ask the user what to do */
      if (mousepad_dialogs_externally_modified (GTK_WINDOW (window), FALSE, modified)
          == MOUSEPAD_RESPONSE_RELOAD)
        {
          gtk_text_buffer_set_modified (document->buffer, FALSE);
          g_action_group_activate_action (G_ACTION_GROUP (window), "file.reload", NULL);
        }

      /* reconnect this handler if the document wasn't removed */
      if (gtk_widget_get_parent (GTK_WIDGET (document)) != NULL)
        g_signal_connect (file, "externally-modified",
                          G_CALLBACK (mousepad_window_externally_modified), window);

      /* drop extra reference, maybe releasing the document */
      g_object_unref (document);
    }
  /* the file is inactive in an inactive tab */
  else if (document->file != file)
    g_signal_connect_object (window->notebook, "switch-page",
                             G_CALLBACK (mousepad_window_pending_tab), file, 0);
  /* the file is inactive in the active tab of an inactive window */
  else
    g_signal_connect_object (window, "notify::is-active",
                             G_CALLBACK (mousepad_window_pending_window), document, 0);
}



static void
mousepad_window_location_changed (MousepadFile   *file,
                                  GFile          *location,
                                  MousepadWindow *window)
{
  GAction *action;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  if (window->active->file == file)
    {
      /* update window title */
      mousepad_window_set_title (window);

      /* set the reload action sensitivity */
      action = g_action_map_lookup_action (G_ACTION_MAP (window), "file.reload");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), location != NULL);
    }
}



static void
mousepad_window_readonly_changed (MousepadFile   *file,
                                  gboolean        readonly,
                                  MousepadWindow *window)
{
  GAction *action;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  if (window->active->file == file)
    {
      /* update window title */
      mousepad_window_set_title (window);

      /* set the save action sensitivity */
      action = g_action_map_lookup_action (G_ACTION_MAP (window), "file.save");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), mousepad_file_is_savable (file));
    }
}



static void
mousepad_window_modified_changed (GtkTextBuffer  *buffer,
                                  MousepadWindow *window)
{
  GAction *action;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  if (window->active->buffer == buffer)
    {
      /* update window title */
      mousepad_window_set_title (window);

      /* set the save action sensitivity */
      action = g_action_map_lookup_action (G_ACTION_MAP (window), "file.save");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                   mousepad_file_is_savable (window->active->file));

      /* update document dependent menu items */
      mousepad_window_update_document_menu_items (window);
    }
}



static void
mousepad_window_enable_edit_actions (GObject        *object,
                                     GParamSpec     *pspec,
                                     MousepadWindow *window)
{
  MousepadDocument *document = window->active;
  GList            *items;
  GAction          *action;
  guint             n;
  gboolean          enabled;
  const gchar      *focus_actions[] = { "edit.paste", "edit.delete-selection", "edit.select-all" };
  const gchar      *select_actions[] = { "edit.cut", "edit.copy" };

  if (GTK_IS_TEXT_VIEW (object) || document->buffer == GTK_TEXT_BUFFER (object))
    {
      /* actions enabled only in a focused text view or in the text view menu,
       * to prevent conflicts with GtkEntry keybindings */
      items = gtk_container_get_children (GTK_CONTAINER (window->textview_menu));
      enabled = gtk_widget_has_focus (GTK_WIDGET (document->textview)) || items == NULL;
      g_list_free (items);
      for (n = 0; n < G_N_ELEMENTS (focus_actions); n++)
        {
          action = g_action_map_lookup_action (G_ACTION_MAP (window), focus_actions[n]);
          g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);
        }

      /* actions enabled only for selections, in addition to the above conditions */
      enabled = enabled && gtk_text_buffer_get_has_selection (document->buffer);
      for (n = 0; n < G_N_ELEMENTS (select_actions); n++)
        {
          action = g_action_map_lookup_action (G_ACTION_MAP (window), select_actions[n]);
          g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);
        }
    }
}



static void
mousepad_window_cursor_changed (MousepadDocument *document,
                                gint              line,
                                gint              column,
                                gint              selection,
                                MousepadWindow   *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* set the new statusbar cursor position and selection length */
  if (window->statusbar && window->active == document)
    mousepad_statusbar_set_cursor_position (MOUSEPAD_STATUSBAR (window->statusbar),
                                            line, column, selection);
}



static void
mousepad_window_encoding_changed (MousepadDocument  *document,
                                  MousepadEncoding   encoding,
                                  MousepadWindow    *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* update the encoding shown in the statusbar */
  if (window->statusbar && window->active == document)
    mousepad_statusbar_set_encoding (MOUSEPAD_STATUSBAR (window->statusbar), encoding);
}



static void
mousepad_window_language_changed (MousepadDocument  *document,
                                  GtkSourceLanguage *language,
                                  MousepadWindow    *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* update the filetype shown in the statusbar */
  if (window->statusbar && window->active == document)
    mousepad_statusbar_set_language (MOUSEPAD_STATUSBAR (window->statusbar), language);
}



static void
mousepad_window_overwrite_changed (MousepadDocument *document,
                                   gboolean          overwrite,
                                   MousepadWindow   *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* set the new overwrite mode in the statusbar */
  if (window->statusbar && window->active == document)
    mousepad_statusbar_set_overwrite (MOUSEPAD_STATUSBAR (window->statusbar), overwrite);
}



static void
mousepad_window_can_undo (GtkSourceBuffer *buffer,
                          GParamSpec      *unused,
                          MousepadWindow  *window)
{
  GAction  *action;
  gboolean  can_undo;

  if (window->active->buffer == GTK_TEXT_BUFFER (buffer))
    {
      can_undo = gtk_source_buffer_can_undo (buffer);

      action = g_action_map_lookup_action (G_ACTION_MAP (window), "edit.undo");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_undo);
    }
}



static void
mousepad_window_can_redo (GtkSourceBuffer *buffer,
                          GParamSpec      *unused,
                          MousepadWindow  *window)
{
  GAction  *action;
  gboolean  can_redo;

  if (window->active->buffer == GTK_TEXT_BUFFER (buffer))
    {
      can_redo = gtk_source_buffer_can_redo (buffer);

      action = g_action_map_lookup_action (G_ACTION_MAP (window), "edit.redo");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_redo);
    }
}



/**
 * Menu Functions
 **/
static void
mousepad_window_menu_templates_fill (MousepadWindow *window,
                                     GMenu          *menu,
                                     const gchar    *path)
{
  GDir         *dir;
  GSList       *files_list = NULL, *dirs_list = NULL, *li;
  gchar        *absolute_path, *label, *dot, *message, *filename_utf8, *tooltip;
  const gchar  *name;
  gboolean      files_added = FALSE;
  GMenu        *submenu;
  GMenuItem    *item;

  /* open the directory */
  dir = g_dir_open (path, 0, NULL);

  /* read the directory */
  if (G_LIKELY (dir))
    {
      /* walk the directory */
      for (;;)
        {
          /* read the filename of the next file */
          name = g_dir_read_name (dir);

          /* break when we reached the last file */
          if (G_UNLIKELY (name == NULL))
            break;

          /* skip hidden files */
          if (name[0] == '.')
            continue;

          /* build absolute path */
          absolute_path = g_build_path (G_DIR_SEPARATOR_S, path, name, NULL);

          /* check if the file is a regular file or directory */
          if (g_file_test (absolute_path, G_FILE_TEST_IS_DIR))
            dirs_list = g_slist_insert_sorted (dirs_list, absolute_path, (GCompareFunc) strcmp);
          else if (g_file_test (absolute_path, G_FILE_TEST_IS_REGULAR))
            files_list = g_slist_insert_sorted (files_list, absolute_path, (GCompareFunc) strcmp);
          else
            g_free (absolute_path);
        }

      /* close the directory */
      g_dir_close (dir);
    }

  /* append the directories */
  for (li = dirs_list; li != NULL; li = li->next)
    {
      /* create a new submenu for the directory */
      submenu = g_menu_new ();

      /* fill the menu */
      mousepad_window_menu_templates_fill (window, submenu, li->data);

      /* check if the submenu contains items */
      if (g_menu_model_get_n_items (G_MENU_MODEL (submenu)))
        {
          /* append the menu */
          label = g_filename_display_basename (li->data);
          item = g_menu_item_new (label, NULL);
          g_free (label);

          /* set submenu icon */
          /* TODO: is there a way to apply an icon to a submenu?
           * (the documentation says yes, the "folder" icon name is valid, but…) */
          g_menu_item_set_attribute_value (item, "icon", g_variant_new_string ("folder"));

          g_menu_item_set_submenu (item, G_MENU_MODEL (submenu));
          g_menu_append_item (menu, item);
          g_object_unref (item);
        }

      /* cleanup */
      g_free (li->data);
    }

  /* append the files */
  for (li = files_list; li != NULL; li = li->next)
    {
      /* create directory label */
      label = g_filename_display_basename (li->data);

      /* strip the extension from the label */
      dot = g_utf8_strrchr (label, -1, '.');
      if (dot != NULL)
        *dot = '\0';

      /* create menu item, adding the action target separately to avoid any
       * character escape issue */
      item = g_menu_item_new (label, NULL);
      g_menu_item_set_action_and_target_value (item, "win.file.new-from-template.new",
                                               g_variant_new_string (li->data));

      /* create an utf-8 valid version of the filename for the tooltip */
      filename_utf8 = g_filename_to_utf8 (li->data, -1, NULL, NULL, NULL);
      tooltip = g_strdup_printf (_("Use '%s' as template"), filename_utf8);
      g_menu_item_set_attribute_value (item, "tooltip", g_variant_new_string (tooltip));
      g_free (filename_utf8);
      g_free (tooltip);

      /* set item icon */
      g_menu_item_set_attribute_value (item, "icon", g_variant_new_string ("text-x-generic"));

      /* append item to the menu */
      g_menu_append_item (menu, item);
      g_object_unref (item);

      /* disable the menu item telling the user there's no templates */
      files_added = TRUE;

      /* cleanup */
      g_free (label);
      g_free (li->data);
    }

  /* cleanup */
  g_slist_free (dirs_list);
  g_slist_free (files_list);

  if (! files_added)
    {
      message = g_strdup_printf (_("No template files found in\n'%s'"), path);
      item = g_menu_item_new (message, "win.insensitive");
      g_free (message);
      g_menu_append_item (menu, item);
      g_object_unref (item);
    }
}



static void
mousepad_window_menu_templates (GSimpleAction *action,
                                GVariant      *state,
                                gpointer       data)
{
  GtkApplication *application;
  GMenu          *menu;
  GMenuItem      *item;
  const gchar    *homedir;
  gchar          *templates_path, *message;
  gboolean        bstate;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (data));

  /* exit if there is actually no change */
  bstate = g_variant_get_boolean (state);
  if (bstate == mousepad_action_get_state_boolean (G_ACTION (action)))
    return;

  /* set the action state */
  g_simple_action_set_state (action, state);

  /* open the menu, ensuring the window has not been removed from the application list,
   * e.g. following an "app.quit" */
  if (bstate && (application = gtk_window_get_application (data)) != NULL)
    {
      /* lock menu updates */
      lock_menu_updates++;

      /* get the templates path */
      templates_path = (gchar *) g_get_user_special_dir (G_USER_DIRECTORY_TEMPLATES);

      /* check if the templates directory is valid and is not home: if not, fall back
       * to "~/Templates" */
      homedir = g_get_home_dir ();
      if (G_UNLIKELY (templates_path == NULL) || g_strcmp0 (templates_path, homedir) == 0)
        templates_path = g_build_filename (homedir, "Templates", NULL);
      else
        templates_path = g_strdup (templates_path);

      /* get and empty the "Templates" submenu */
      menu = gtk_application_get_menu_by_id (application, "file.new-from-template");
      g_menu_remove_all (menu);

      /* check if the directory exists */
      if (g_file_test (templates_path, G_FILE_TEST_IS_DIR))
        {
          /* fill the menu, blocking tooltip update meanwhile */
          g_signal_handlers_block_by_func (menu, mousepad_window_menu_update_tooltips, data);
          mousepad_window_menu_templates_fill (data, menu, templates_path);
          g_signal_handlers_unblock_by_func (menu, mousepad_window_menu_update_tooltips, data);
          mousepad_window_menu_update_tooltips (G_MENU_MODEL (menu), 0, 0, 0, data);
        }
      else
        {
          message = g_strdup_printf (_("Missing Templates directory\n'%s'"), templates_path);
          item = g_menu_item_new (message, "win.insensitive");
          g_free (message);
          g_menu_append_item (menu, item);
          g_object_unref (item);
        }

      /* cleanup */
      g_free (templates_path);

      /* unlock */
      lock_menu_updates--;
    }
}



static void
mousepad_window_menu_tab_sizes_update (MousepadWindow *window)
{
  GtkApplication *application;
  GMenuModel     *model;
  GMenuItem      *item;
  gint32          tab_size;
  gint            tab_size_n, nitem, nitems;
  gchar          *text = NULL;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* avoid menu actions */
  lock_menu_updates++;

  /* get tab size of active document */
  tab_size = MOUSEPAD_SETTING_GET_UINT (TAB_WIDTH);

  /* get the number of items in the tab-size submenu */
  application = gtk_window_get_application (GTK_WINDOW (window));
  model = G_MENU_MODEL (gtk_application_get_menu_by_id (application, "document.tab.tab-size"));
  nitems = g_menu_model_get_n_items (model);

  /* check if there is a default item with this tab size */
  for (nitem = 0; nitem < nitems; nitem++)
    {
      tab_size_n = atoi (g_variant_get_string (
                     g_menu_model_get_item_attribute_value (model, nitem, "label", NULL), NULL));
      if (tab_size_n == tab_size)
        break;
    }

  if (nitem == nitems)
    {
      /* create suitable text label for the "Other" menu */
      text = g_strdup_printf (_("Ot_her (%d)..."), tab_size);
      tab_size = 0;
    }

  /* toggle the action */
  g_action_group_change_action_state (G_ACTION_GROUP (window), "document.tab.tab-size",
                                      g_variant_new_int32 (tab_size));

  /* set the "Other" menu label */
  item = g_menu_item_new_from_model (model, nitems - 1);
  g_menu_item_set_label (item, text ? text : _("Ot_her..."));
  g_menu_remove (G_MENU (model), nitems - 1);
  g_menu_append_item (G_MENU (model), item);
  g_object_unref (item);

  /* cleanup */
  g_free (text);

  /* allow menu actions again */
  lock_menu_updates--;
}



static void
mousepad_window_menu_textview_shown (GtkWidget      *menu,
                                     MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* disconnect this handler */
  mousepad_disconnect_by_func (menu, mousepad_window_menu_textview_shown, window);

  /* empty the original menu */
  mousepad_util_container_clear (GTK_CONTAINER (menu));

  /* realign menu items */
  if (window->old_style_menu)
    gtk_menu_set_reserve_toggle_size (GTK_MENU (menu), FALSE);

  /* move the textview menu children into the other menu */
  mousepad_util_container_move_children (GTK_CONTAINER (window->textview_menu),
                                         GTK_CONTAINER (menu));
}



static void
mousepad_window_menu_textview_deactivate (GtkMenuShell   *menu,
                                          MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* disconnect this handler */
  mousepad_disconnect_by_func (menu, mousepad_window_menu_textview_deactivate, window);

  /* copy the menu children back into the textview menu */
  mousepad_util_container_move_children (GTK_CONTAINER (menu),
                                         GTK_CONTAINER (window->textview_menu));
}



static void
mousepad_window_menu_textview_popup (GtkTextView    *textview,
                                     GtkMenu        *menu,
                                     MousepadWindow *window)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (textview));
  g_return_if_fail (GTK_IS_MENU (menu));
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* connect signal */
  g_signal_connect (menu, "show",
                    G_CALLBACK (mousepad_window_menu_textview_shown), window);
  g_signal_connect (menu, "deactivate",
                    G_CALLBACK (mousepad_window_menu_textview_deactivate), window);
}



static void
mousepad_window_update_menu_item (MousepadWindow *window,
                                  const gchar    *menu_id,
                                  gint            index,
                                  gpointer        data)
{
  GtkApplication *application;
  GMenu          *menu;
  GMenuItem      *item;
  const gchar    *label = NULL, *icon = NULL, *tooltip = NULL;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* prevent menu updates */
  lock_menu_updates++;

  /* get the menu item */
  application = gtk_window_get_application (GTK_WINDOW (window));
  menu = gtk_application_get_menu_by_id (application, menu_id);
  item = g_menu_item_new_from_model (G_MENU_MODEL (menu), index);

  /* set the menu item attributes */
  if (g_strcmp0 (menu_id, "item.file.reload") == 0)
    if (GPOINTER_TO_INT (data))
      {
        label = MOUSEPAD_LABEL_REVERT;
        icon = "document-revert";
        tooltip = _("Revert to the saved version of the file");
      }
    else
      {
        label = MOUSEPAD_LABEL_RELOAD;
        icon = "view-refresh";
        tooltip = _("Reload file from disk");
      }
  else if (g_strcmp0 (menu_id, "item.view.fullscreen") == 0)
    if (GPOINTER_TO_INT (data))
      {
        icon = "view-restore";
        tooltip = _("Leave fullscreen mode");
      }
    else
      {
        icon = "view-fullscreen";
        tooltip = _("Make the window fullscreen");
      }
  else
    g_warn_if_reached ();

  /* update the menu item */
  if (label != NULL)
    g_menu_item_set_label (item, label);
  if (icon != NULL)
    g_menu_item_set_attribute_value (item, "icon", g_variant_new_string (icon));
  if (tooltip != NULL)
    g_menu_item_set_attribute_value (item, "tooltip", g_variant_new_string (tooltip));

  g_menu_remove (menu, index);
  g_menu_insert_item (menu, index, item);
  g_object_unref (item);

  /* allow menu actions again */
  lock_menu_updates--;
}



void
mousepad_window_update_document_menu_items (MousepadWindow *window)
{
  gpointer data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* update the "Reload/Revert" menu item */
  data = GINT_TO_POINTER (gtk_text_buffer_get_modified (window->active->buffer));
  mousepad_window_update_menu_item (window, "item.file.reload", 0, data);
}



void
mousepad_window_update_window_menu_items (MousepadWindow *window)
{
  gpointer data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* update the "Fullscreen" menu item */
  data = GINT_TO_POINTER (mousepad_window_get_in_fullscreen (window));
  mousepad_window_update_menu_item (window, "item.view.fullscreen", 0, data);
}



static void
mousepad_window_update_gomenu (GSimpleAction *action,
                               GVariant      *state,
                               gpointer       data)
{
  MousepadWindow   *window = data;
  MousepadDocument *document;
  GtkApplication   *application;
  GMenu            *menu;
  GMenuItem        *item;
  const gchar      *label, *tooltip;
  gchar            *action_name, *accelerator;
  gint              n_pages, n;
  gboolean          bstate;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* exit if there is actually no change */
  bstate = g_variant_get_boolean (state);
  if (bstate == mousepad_action_get_state_boolean (G_ACTION (action)))
    return;

  /* set the action state */
  g_simple_action_set_state (action, state);

  /* open the menu, ensuring the window has not been removed from the application list,
   * e.g. following an "app.quit" */
  if (bstate && (application = gtk_window_get_application (GTK_WINDOW (window))) != NULL)
    {
      /* prevent menu updates */
      lock_menu_updates++;

      /* get the "Go to tab" submenu */
      menu = gtk_application_get_menu_by_id (application, "document.go-to-tab");

      /* block tooltip updates */
      g_signal_handlers_block_by_func (menu, mousepad_window_menu_update_tooltips, window);

      /* empty the menu */
      g_menu_remove_all (menu);

      /* walk through the notebook pages */
      n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

      for (n = 0; n < n_pages; ++n)
        {
          document = MOUSEPAD_DOCUMENT (gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), n));

          /* add the item to the "Go to Tab" submenu */
          label = mousepad_document_get_basename (document);
          action_name = g_strdup_printf ("win.document.go-to-tab(%d)", n);
          item = g_menu_item_new (label, action_name);
          tooltip = mousepad_document_get_filename (document);
          if (tooltip)
            g_menu_item_set_attribute_value (item, "tooltip", g_variant_new_string (tooltip));
          g_free (action_name);

          if (G_LIKELY (n < 9))
            {
              /* create an accelerator and add it to the menu item */
              accelerator = g_strdup_printf ("<Alt>%d", n + 1);
              g_menu_item_set_attribute_value (item, "accel", g_variant_new_string (accelerator));
              g_free (accelerator);
            }

          /* add the menu item */
          g_menu_append_item (menu, item);
          g_object_unref (item);

          /* select the active entry */
          if (gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook)) == n)
            g_action_group_change_action_state (G_ACTION_GROUP (window), "document.go-to-tab",
                                                g_variant_new_int32 (n));
        }

      /* release our lock */
      g_signal_handlers_unblock_by_func (menu, mousepad_window_menu_update_tooltips, window);
      mousepad_window_menu_update_tooltips (G_MENU_MODEL (menu), 0, 0, 0, window);
      lock_menu_updates--;
    }
}



/* sort list in descending order */
static gint
mousepad_window_recent_sort (gconstpointer ga,
                             gconstpointer gb)
{
  time_t ta, tb;

  ta = gtk_recent_info_get_modified ((GtkRecentInfo *) ga);
  tb = gtk_recent_info_get_modified ((GtkRecentInfo *) gb);

  if (ta < tb)
    return 1;
  else if (ta > tb)
    return -1;
  else
    return 0;
}



static void
mousepad_window_recent_menu (GSimpleAction *action,
                             GVariant      *state,
                             gpointer       data)
{
  GtkApplication   *application;
  GtkRecentManager *manager;
  GtkRecentInfo    *info;
  GMenu            *menu;
  GMenuItem        *menu_item;
  GAction          *subaction;
  GFile            *file;
  GList            *items, *li, *next, *filtered = NULL;
  const gchar      *uri, *display_name;
  gchar            *label, *filename_utf8, *tooltip;
  guint             n;
  gboolean          bstate;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (data));

  /* exit if there is actually no change */
  bstate = g_variant_get_boolean (state);
  if (bstate == mousepad_action_get_state_boolean (G_ACTION (action)))
    return;

  /* set the action state */
  g_simple_action_set_state (action, state);

  /* open the menu, ensuring the window has not been removed from the application list,
   * e.g. following an "app.quit" */
  if (bstate && (application = gtk_window_get_application (data)) != NULL)
    {
      /* avoid updating the menu */
      lock_menu_updates++;

      /* get the "Recent" submenu */
      menu = gtk_application_get_menu_by_id (application, "file.open-recent.list");

      /* block tooltip updates */
      g_signal_handlers_block_by_func (menu, mousepad_window_menu_update_tooltips, data);

      /* empty the menu */
      g_menu_remove_all (menu);

      /* get all the items in the manager */
      manager = gtk_recent_manager_get_default ();
      items = gtk_recent_manager_get_items (manager);

      /* walk through the items in the manager and pick the ones that are in the mousepad group */
      for (li = items; li != NULL; li = li->next)
        {
          /* check if the item is in the Mousepad group */
          if (! gtk_recent_info_has_group (li->data, PACKAGE_NAME))
            continue;

          /* insert the list, sorted by date */
          filtered = g_list_insert_sorted (filtered, li->data,
                                           mousepad_window_recent_sort);
        }

      /* get the recent menu limit number */
      n = MOUSEPAD_SETTING_GET_UINT (RECENT_MENU_ITEMS);

      /* append the items to the menu */
      li = filtered;
      while (n > 0 && li != NULL)
        {
          next = li->next;
          info = li->data;

          /* get the file */
          uri = gtk_recent_info_get_uri (info);
          file = g_file_new_for_uri (uri);

          /* append to the menu if the file exists, else remove it from the history */
          if (mousepad_util_query_exists (file, TRUE))
            {
              /* get label, escaping underscores for mnemonics */
              display_name = gtk_recent_info_get_display_name (info);
              label = mousepad_util_escape_underscores (display_name);

              /* create an utf-8 valid version of the filename for the tooltip */
              filename_utf8 = mousepad_util_get_display_path (file);
              tooltip = g_strdup_printf (_("Open '%s'"), filename_utf8);
              g_free (filename_utf8);

              /* append menu item, adding the action target separately to avoid any
               * character escape issue */
              menu_item = g_menu_item_new (label, NULL);
              g_menu_item_set_action_and_target_value (menu_item, "win.file.open-recent.new",
                                                       g_variant_new_string (uri));
              g_menu_item_set_attribute_value (menu_item, "tooltip", g_variant_new_string (tooltip));
              g_menu_append_item (menu, menu_item);
              g_object_unref (menu_item);
              g_free (label);
              g_free (tooltip);

              /* update counter */
              n--;
            }
          /* remove the item, don't both the user if this fails */
          else if (gtk_recent_manager_remove_item (manager, uri, NULL))
            filtered = g_list_delete_link (filtered, li);

          /* update pointer */
          li = next;

          /* cleanup */
          g_object_unref (file);
        }

      /* add the "No items found"/"History disabled" insensitive menu item */
      if (filtered == NULL)
        {
          label = n > 0 ? _("No items found") : _("History disabled");
          menu_item = g_menu_item_new (label, "win.insensitive");

          g_menu_append_item (menu, menu_item);
          g_object_unref (menu_item);
        }

      /* set the sensitivity of the clear button */
      subaction = g_action_map_lookup_action (data, "file.open-recent.clear-history");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (subaction), filtered != NULL);

      /* cleanup */
      g_list_free_full (items, (GDestroyNotify) gtk_recent_info_unref);
      g_list_free (filtered);

      /* allow menu updates again */
      g_signal_handlers_unblock_by_func (menu, mousepad_window_menu_update_tooltips, data);
      mousepad_window_menu_update_tooltips (G_MENU_MODEL (menu), 0, 0, 0, data);
      lock_menu_updates--;
    }
}



/**
 * Drag and drop functions
 **/
static void
mousepad_window_drag_data_received (GtkWidget        *widget,
                                    GdkDragContext   *context,
                                    gint              x,
                                    gint              y,
                                    GtkSelectionData *selection_data,
                                    guint             info,
                                    guint             drag_time,
                                    MousepadWindow   *window)
{
  GtkWidget  *notebook, **document;
  GtkWidget  *child, *label;
  GFile     **files;
  gchar     **uris;
  gint        i, n_pages;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  /* we only accept text/uri-list drops with format 8 and atleast one byte of data */
  if (info == TARGET_TEXT_URI_LIST
      && gtk_selection_data_get_format (selection_data) == 8
      && gtk_selection_data_get_length (selection_data) > 0
      && (uris = gtk_selection_data_get_uris (selection_data)) != NULL)
    {
      /* prepare the GFile array */
      n_pages = g_strv_length (uris);
      files = g_new (GFile *, n_pages);
      for (i = 0; i < n_pages; i++)
        files[i] = g_file_new_for_uri (uris[i]);

      /* open the files */
      mousepad_window_open_files (window, files, n_pages,
                                  mousepad_encoding_get_default (),
                                  0, 0, FALSE);

      /* cleanup */
      g_strfreev (uris);
      for (i = 0; i < n_pages; i++)
        g_object_unref (files[i]);

      g_free (files);

      /* finish the drag (copy) */
      gtk_drag_finish (context, TRUE, FALSE, drag_time);
    }
  else if (info == TARGET_GTK_NOTEBOOK_TAB)
    {
      /* get the source notebook */
      notebook = gtk_drag_get_source_widget (context);

      /* get the document that has been dragged */
      document = (GtkWidget **) (gconstpointer) gtk_selection_data_get_data (selection_data);

      /* check */
      g_return_if_fail (MOUSEPAD_IS_DOCUMENT (*document));

      /* take a reference on the document before we remove it */
      g_object_ref (*document);

      /* remove the document from the source window */
      gtk_notebook_detach_tab (GTK_NOTEBOOK (notebook), *document);

      /* get the number of pages in the notebook */
      n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

      /* figure out where to insert the tab in the notebook */
      for (i = 0; i < n_pages; i++)
        {
          GtkAllocation alloc = { 0, 0, 0, 0 };

          /* get the child label */
          child = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), i);
          label = gtk_notebook_get_tab_label (GTK_NOTEBOOK (window->notebook), child);

          gtk_widget_get_allocation (label, &alloc);

          /* break if we have a matching drop position */
          if (x < (alloc.x + alloc.width / 2))
            break;
        }

      /* add the document to the new window */
      mousepad_window_add (window, MOUSEPAD_DOCUMENT (*document));

      /* move the tab to the correct position */
      gtk_notebook_reorder_child (GTK_NOTEBOOK (window->notebook), *document, i);

      /* release our reference on the document */
      g_object_unref (*document);

      /* finish the drag (move) */
      gtk_drag_finish (context, TRUE, TRUE, drag_time);
    }
}



/**
 * Find and replace
 **/
static void
mousepad_window_search (MousepadWindow      *window,
                        MousepadSearchFlags  flags,
                        const gchar         *string,
                        const gchar         *replacement)
{
  GtkWidget *document;
  gint       n_docs, n;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* multi-document mode */
  if (flags & MOUSEPAD_SEARCH_FLAGS_AREA_ALL_DOCUMENTS)
    {
      n_docs = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));
      for (n = 0; n < n_docs; n++)
        {
          /* search in the nth document */
          document = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), n);
          mousepad_document_search (MOUSEPAD_DOCUMENT (document), string, replacement, flags);
        }
    }
  /* search in the active document */
  else
    mousepad_document_search (window->active, string, replacement, flags);
}



static void
mousepad_window_search_completed (MousepadDocument    *document,
                                  gint                 cur_match_doc,
                                  gint                 n_matches_doc,
                                  const gchar         *string,
                                  MousepadSearchFlags  flags,
                                  MousepadWindow      *window)
{
  static GList *documents = NULL, *n_matches_docs = NULL;
  static gchar *multi_string = NULL;
  static gint   n_matches = 0, n_documents = 0;
  GList        *up_doc, *up_match;
  gint          index;

  /* always send the active document result, although it will only be relevant for the
   * search bar if the multi-document mode is active */
  if (document == window->active)
    g_signal_emit (window, window_signals[SEARCH_COMPLETED], 0, cur_match_doc, n_matches_doc,
                   string, flags & (~ MOUSEPAD_SEARCH_FLAGS_AREA_ALL_DOCUMENTS));

  /* multi-document mode is active: collect the search results regardless of their origin:
   * replace dialog, search bar or buffer change */
  if (window->replace_dialog != NULL
      && MOUSEPAD_SETTING_GET_BOOLEAN (SEARCH_REPLACE_ALL)
      && MOUSEPAD_SETTING_GET_UINT (SEARCH_REPLACE_ALL_LOCATION) == 2)
    {
      /* update the current search string and reset other static variables, or exit if
       * the search is irrelevant */
      if (g_strcmp0 (multi_string, string) != 0)
        {
          if (flags & MOUSEPAD_SEARCH_FLAGS_AREA_ALL_DOCUMENTS)
            {
              g_free (multi_string);
              multi_string = g_strdup (string);
              g_list_free (documents);
              g_list_free (n_matches_docs);
              documents = NULL;
              n_matches_docs = NULL;
              n_documents = 0;
              n_matches = 0;
            }
          else
            return;
        }

      /* remove obsolete document results */
      if (documents != NULL)
        {
          for (up_doc = documents, up_match = n_matches_docs; up_doc != NULL;)
            if (gtk_notebook_page_num (GTK_NOTEBOOK (window->notebook), up_doc->data) == -1)
              {
                /* update data */
                n_documents--;
                n_matches -= GPOINTER_TO_INT (up_match->data);

                /* remove the result from the list */
                up_match->data = GINT_TO_POINTER (-1);
                n_matches_docs = g_list_remove (n_matches_docs, up_match->data);
                up_match = n_matches_docs;

                /* remove the document from the list */
                documents = g_list_remove (documents, up_doc->data);
                up_doc = documents;

                /* complementary exit condition */
                if (documents == NULL)
                  break;
              }
            /* walk the list */
            else
              {
                up_doc = up_doc->next;
                up_match = up_match->next;
              }
        }

      /* update an old document result */
      if (documents != NULL && (index = g_list_index (documents, document)) != -1)
        {
          up_match = g_list_nth (n_matches_docs, index);
          n_matches += n_matches_doc - GPOINTER_TO_INT (up_match->data);
          up_match->data = GINT_TO_POINTER (n_matches_doc);
        }
      /* add a new document result, and wait until all documents have completed their search
       * to send the final result to the replace dialog */
      else
        {
          /* update lists */
          documents = g_list_prepend (documents, document);
          n_matches_docs = g_list_prepend (n_matches_docs, GINT_TO_POINTER (n_matches_doc));
          n_matches += n_matches_doc;
          n_documents++;
        }

      /* exit if all documents have not yet send their result */
      if (n_documents < gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook)))
        return;

      /* send the final result, only relevant for the replace dialog */
      g_signal_emit (window, window_signals[SEARCH_COMPLETED], 0, 0, n_matches, string,
                     flags | MOUSEPAD_SEARCH_FLAGS_AREA_ALL_DOCUMENTS);
    }

  /* make sure the selection is visible whenever idle */
  if (! (flags & MOUSEPAD_SEARCH_FLAGS_ACTION_NONE) && n_matches_doc > 0)
    g_idle_add (mousepad_view_scroll_to_cursor,
                mousepad_util_source_autoremove (window->active->textview));
}



/**
 * Paste from History
 **/
static void
mousepad_window_paste_history_add (MousepadWindow *window)
{
  GtkClipboard *clipboard;
  gchar        *text;
  GSList       *li;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* get the current clipboard text */
  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (window), GDK_SELECTION_CLIPBOARD);
  text = gtk_clipboard_wait_for_text (clipboard);

  /* leave when there is no text */
  if (G_UNLIKELY (text == NULL))
    return;

  /* check if the item is already in the history */
  for (li = clipboard_history; li != NULL; li = li->next)
    if (strcmp (li->data, text) == 0)
      break;

  /* append the item or remove it */
  if (G_LIKELY (li == NULL))
    {
      /* add to the list */
      clipboard_history = g_slist_prepend (clipboard_history, text);

      /* get the 10th item from the list and remove it if it exists */
      li = g_slist_nth (clipboard_history, 10);
      if (li != NULL)
        {
          /* cleanup */
          g_free (li->data);
          clipboard_history = g_slist_delete_link (clipboard_history, li);
        }
    }
  else
    {
      /* already in the history, remove it */
      g_free (text);
    }
}



static void
mousepad_window_paste_history_activate (GtkMenuItem    *item,
                                        MousepadWindow *window)
{
  const gchar *text;

  g_return_if_fail (GTK_IS_MENU_ITEM (item));
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));
  g_return_if_fail (MOUSEPAD_IS_VIEW (window->active->textview));

  /* get the menu item text */
  text = mousepad_object_get_data (item, "history-pointer");

  /* paste the text */
  if (G_LIKELY (text != NULL))
    mousepad_view_custom_paste (window->active->textview, text);
}



static GtkWidget *
mousepad_window_paste_history_menu_item (const gchar *text,
                                         const gchar *mnemonic)
{
  GtkWidget   *item;
  GtkWidget   *label;
  GtkWidget   *hbox;
  const gchar *s;
  gchar       *label_str;
  GString     *string;

  /* create new label string */
  string = g_string_sized_new (PASTE_HISTORY_MENU_LENGTH);

  /* get the first 30 chars of the clipboard text */
  if (g_utf8_strlen (text, -1) > PASTE_HISTORY_MENU_LENGTH)
    {
      /* append the first 30 chars */
      s = g_utf8_offset_to_pointer (text, PASTE_HISTORY_MENU_LENGTH);
      string = g_string_append_len (string, text, s - text);

      /* make it look like a ellipsized string */
      string = g_string_append (string, "...");
    }
  else
    {
      /* append the entire string */
      string = g_string_append (string, text);
    }

  /* get the string */
  label_str = g_string_free (string, FALSE);

  /* replace tab and new lines with spaces */
  label_str = g_strdelimit (label_str, "\n\t\r", ' ');

  /* create a new item */
  item = gtk_menu_item_new ();

  /* create a hbox */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 14);
  gtk_container_add (GTK_CONTAINER (item), hbox);
  gtk_widget_show (hbox);

  /* create the clipboard label */
  label = gtk_label_new (label_str);
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_yalign (GTK_LABEL (label), 0.5);
  gtk_widget_show (label);

  /* create the mnemonic label */
  label = gtk_label_new_with_mnemonic (mnemonic);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_label_set_xalign (GTK_LABEL (label), 1.0);
  gtk_label_set_yalign (GTK_LABEL (label), 0.5);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), item);
  gtk_widget_show (label);

  /* cleanup */
  g_free (label_str);

  return item;
}



static GtkWidget *
mousepad_window_paste_history_menu (MousepadWindow *window)
{
  GSList       *li;
  gchar        *text;
  gpointer      list_data = NULL;
  GtkWidget    *item;
  GtkWidget    *menu;
  GtkClipboard *clipboard;
  gchar         mnemonic[4];
  gint          n;

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), NULL);

  /* create new menu and set the screen */
  menu = gtk_menu_new ();
  g_object_ref_sink (menu);
  g_signal_connect (menu, "deactivate", G_CALLBACK (g_object_unref), NULL);
  gtk_menu_set_screen (GTK_MENU (menu), gtk_widget_get_screen (GTK_WIDGET (window)));

  /* get the current clipboard text */
  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (window), GDK_SELECTION_CLIPBOARD);
  text = gtk_clipboard_wait_for_text (clipboard);

  /* append the history items */
  for (li = clipboard_history, n = 1; li != NULL; li = li->next)
    {
      /* skip the active clipboard item */
      if (G_UNLIKELY (list_data == NULL && text && strcmp (li->data, text) == 0))
        {
          /* store the pointer so we can attach it at the end of the menu */
          list_data = li->data;
        }
      else
        {
          /* create mnemonic string */
          g_snprintf (mnemonic, sizeof (mnemonic), "_%d", n++);

          /* create menu item */
          item = mousepad_window_paste_history_menu_item (li->data, mnemonic);
          mousepad_object_set_data (item, "history-pointer", li->data);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          g_signal_connect (item, "activate", G_CALLBACK (mousepad_window_paste_history_activate), window);
          gtk_widget_show (item);
        }
    }

  /* cleanup */
  g_free (text);

  if (list_data != NULL)
    {
      /* add separator between history and active menu items */
      if (mousepad_util_container_has_children (GTK_CONTAINER (menu)))
        {
          item = gtk_separator_menu_item_new ();
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          gtk_widget_show (item);
        }

      /* create menu item for current clipboard text */
      item = mousepad_window_paste_history_menu_item (list_data, "_0");
      mousepad_object_set_data (item, "history-pointer", list_data);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      g_signal_connect (item, "activate",
                        G_CALLBACK (mousepad_window_paste_history_activate), window);
      gtk_widget_show (item);
    }
  else if (! mousepad_util_container_has_children (GTK_CONTAINER (menu)))
    {
      /* create an item to inform the user */
      item = gtk_menu_item_new_with_label (_("No clipboard data"));
      gtk_widget_set_sensitive (item, FALSE);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);
    }

  return menu;
}



/**
 * Menu Actions
 *
 * All those function should be sorted by the menu structure so it's
 * easy to find a function. The function can always use window->active, since
 * we can assume there is always an active document inside a window.
 **/
static void
mousepad_window_action_new (GSimpleAction *action,
                            GVariant      *value,
                            gpointer       data)
{
  MousepadDocument *document;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (data));

  /* create new document */
  document = mousepad_document_new ();

  /* add the document to the window */
  mousepad_window_add (data, document);
}



static void
mousepad_window_action_new_window (GSimpleAction *action,
                                   GVariant      *value,
                                   gpointer       data)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (data));

  /* emit the new window signal */
  g_signal_emit (data, window_signals[NEW_WINDOW], 0);
}



static void
mousepad_window_action_new_from_template (GSimpleAction *action,
                                          GVariant      *value,
                                          gpointer       data)
{
  MousepadDocument *document;
  MousepadEncoding  encoding;
  GFile            *file;
  GError           *error = NULL;
  const gchar      *filename;
  gchar            *message;
  gint              result;


  g_return_if_fail (MOUSEPAD_IS_WINDOW (data));

  /* retrieve the filename */
  filename = g_variant_get_string (value, NULL);

  /* test if the file exists */
  if (G_LIKELY (filename != NULL))
    {
      /* create new document */
      document = mousepad_document_new ();

      /* sink floating object */
      g_object_ref_sink (document);

      /* lock the undo manager */
      gtk_source_buffer_begin_not_undoable_action (GTK_SOURCE_BUFFER (document->buffer));

      /* virtually set the file location */
      file = g_file_new_for_path (filename);
      mousepad_file_set_location (document->file, file, MOUSEPAD_LOCATION_VIRTUAL);
      g_object_unref (file);

      /* set encoding to default */
      encoding = mousepad_encoding_get_default ();
      mousepad_file_set_encoding (document->file, encoding);

      /* try to load the template into the buffer */
      result = mousepad_file_open (document->file, 0, 0, TRUE, FALSE, FALSE, &error);

      /* reset the file location */
      mousepad_file_set_location (document->file, NULL, MOUSEPAD_LOCATION_REVERT);

      /* release the lock */
      gtk_source_buffer_end_not_undoable_action (GTK_SOURCE_BUFFER (document->buffer));

      /* no errors, insert the document */
      if (G_LIKELY (result == 0))
        mousepad_window_add (data, document);
      else
        {
          /* handle the error */
          switch (result)
            {
              case ERROR_ENCODING_NOT_VALID:
              case ERROR_CONVERTING_FAILED:
                /* set error message */
                message = g_strdup_printf (_("Templates should be %s valid"),
                                           mousepad_encoding_get_charset (encoding));
                break;

              case ERROR_READING_FAILED:
                /* set error message */
                message = g_strdup (_("Reading the template failed"));
                break;

              default:
                /* set error message */
                message = g_strdup (_("Loading the template failed"));
                break;
            }

          /* show the error */
          mousepad_dialogs_show_error (data, error, message);

          /* cleanup */
          g_free (message);
          g_error_free (error);
        }

      /* decrease reference count if everything went well, else release the document */
      g_object_unref (document);
    }
}



static void
mousepad_window_action_open (GSimpleAction *action,
                             GVariant      *value,
                             gpointer       data)
{
  MousepadWindow   *window = data;
  MousepadEncoding  encoding;
  GSList           *files, *file;
  GFile           **files_array;
  gint              n, n_files;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* run the dialog */
  if (G_LIKELY (mousepad_dialogs_open (GTK_WINDOW (window),
                                       mousepad_file_get_location (window->active->file),
                                       &files, &encoding)
                == GTK_RESPONSE_ACCEPT))
    {
      /* lock menu updates */
      lock_menu_updates++;

      /* prepare GFile array */
      n_files = g_slist_length (files);
      files_array = g_new (GFile *, n_files);
      for (n = 0, file = files; file != NULL; n++, file = file->next)
        files_array[n] = file->data;

      /* open selected locations according to the application's opening mode */
      g_signal_emit_by_name (g_application_get_default (), "open", files_array, n_files, NULL);

      /* cleanup */
      g_free (files_array);
      g_slist_free_full (files, g_object_unref);

      /* allow menu updates again */
      lock_menu_updates--;
    }
}



static void
mousepad_window_action_open_recent (GSimpleAction *action,
                                    GVariant      *value,
                                    gpointer       data)
{
  GFile       *file;
  const gchar *uri;
  gboolean     succeed = FALSE;

  /* try to open the file */
  uri = g_variant_get_string (value, NULL);
  file = g_file_new_for_uri (uri);
  succeed = mousepad_window_open_file (data, file, mousepad_encoding_get_default (), 0, 0, TRUE);
  g_object_unref (file);

  /* update the recent history, don't both the user if this fails */
  if (G_LIKELY (succeed))
    gtk_recent_manager_add_item (gtk_recent_manager_get_default (), uri);
  else
    gtk_recent_manager_remove_item (gtk_recent_manager_get_default (), uri, NULL);
}



static void
mousepad_window_action_clear_recent (GSimpleAction *action,
                                     GVariant      *value,
                                     gpointer       data)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (data));

  /* ask the user if he really want to clear the history */
  if (mousepad_dialogs_clear_recent (data))
    {
      /* avoid updating the menu */
      lock_menu_updates++;

      /* clear the document history */
      mousepad_history_recent_clear ();

      /* allow menu updates again */
      lock_menu_updates--;
    }
}



static void
mousepad_window_action_save (GSimpleAction *action,
                             GVariant      *value,
                             gpointer       data)
{
  MousepadWindow   *window = data;
  MousepadDocument *document = window->active;
  GAction          *save_as;
  GError           *error = NULL;
  gboolean          succeed = FALSE;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  if (! mousepad_file_location_is_set (document->file))
    {
      /* file has no filename yet, open the save as dialog */
      save_as = g_action_map_lookup_action (G_ACTION_MAP (window), "file.save-as");
      g_action_activate (save_as, NULL);
      succeed = mousepad_action_get_state_int32_boolean (save_as);
    }
  else
    {
      /* try to save the file */
      succeed = mousepad_file_save (document->file, FALSE, &error);

      /* file has been externally modified */
      if (G_UNLIKELY (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_WRONG_ETAG)))
        {
          /* cleanup */
          g_clear_error (&error);

          /* ask the user what to do */
          switch (mousepad_dialogs_externally_modified (GTK_WINDOW (window), TRUE, TRUE))
            {
              case MOUSEPAD_RESPONSE_SAVE_AS:
                /* run save as dialog */
                save_as = g_action_map_lookup_action (G_ACTION_MAP (window), "file.save-as");
                g_action_activate (save_as, NULL);
                succeed = mousepad_action_get_state_int32_boolean (save_as);
                break;

              case MOUSEPAD_RESPONSE_SAVE:
                /* force to save the document */
                succeed = mousepad_file_save (document->file, TRUE, &error);
                break;

              default:
                /* do nothing */
                succeed = FALSE;
                break;
            }
        }
      /* file is readonly */
      else if (G_UNLIKELY (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED)
                           || g_error_matches (error, G_IO_ERROR, G_IO_ERROR_READ_ONLY)
                           || mousepad_file_get_path (document->file) == NULL))
        {
          /* cleanup */
          g_clear_error (&error);

          /* ask the user what to do */
          switch (mousepad_dialogs_save_changes (GTK_WINDOW (window), FALSE, TRUE))
            {
              case MOUSEPAD_RESPONSE_SAVE_AS:
                /* run save as dialog */
                save_as = g_action_map_lookup_action (G_ACTION_MAP (window), "file.save-as");
                g_action_activate (save_as, NULL);
                succeed = mousepad_action_get_state_int32_boolean (save_as);
                break;

              default:
                /* do nothing */
                succeed = FALSE;
                break;
            }
        }

      /* other kind of error, which may result from the previous exceptions */
      if (G_UNLIKELY (error != NULL))
        {
          mousepad_dialogs_show_error (GTK_WINDOW (window), error, MOUSEPAD_MESSAGE_IO_ERROR_SAVE);
          g_error_free (error);
        }
    }

  /* store the save result as the action state */
  g_action_change_state (G_ACTION (action), g_variant_new_int32 (succeed));
}



static void
mousepad_window_action_save_as (GSimpleAction *action,
                                GVariant      *value,
                                gpointer       data)
{
  MousepadWindow   *window = data;
  MousepadDocument *document = window->active;
  MousepadEncoding  encoding, current_encoding = MOUSEPAD_ENCODING_NONE;
  GAction          *save;
  GFile            *file, *current_file = NULL;
  gboolean          succeed = FALSE;

  /* "Save As" calls "Save" which can call "Save As" in turn,
   * and we have to act differently if there is recursion */
  static guint depth = 0, max_depth = 0;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* increase recursion count */
  max_depth = ++depth;

  /* run the dialog */
  if (mousepad_dialogs_save_as (GTK_WINDOW (window), document->file,
                                last_save_location, &file, &encoding)
      == GTK_RESPONSE_ACCEPT && G_LIKELY (file != NULL))
    {
      /* keep a ref of the current file location to restore it in case of failure */
      if (mousepad_file_location_is_set (document->file))
        {
          current_file = g_object_ref (mousepad_file_get_location (document->file));
          current_encoding = mousepad_file_get_encoding (document->file);
        }

      /* virtually set the new file location */
      mousepad_file_set_location (document->file, file, MOUSEPAD_LOCATION_VIRTUAL);
      mousepad_file_set_encoding (document->file, encoding);

      /* save the file by an internal call (the save action may be disabled, depending
       * on the file status) */
      save = g_action_map_lookup_action (G_ACTION_MAP (window), "file.save");
      mousepad_window_action_save (G_SIMPLE_ACTION (save), NULL, window);
      succeed = mousepad_action_get_state_int32_boolean (save);

      if (G_LIKELY (succeed && depth == max_depth))
        {
          /* validate file location change */
          mousepad_file_set_location (document->file, file, MOUSEPAD_LOCATION_REAL);

          /* add to the recent history */
          mousepad_history_recent_add (document->file);

          /* update last save location */
          if (last_save_location != NULL)
            g_object_unref (last_save_location);

          last_save_location = g_file_get_parent (file);
        }
      /* revert file location change */
      else if (depth == 1)
        {
          mousepad_file_set_location (document->file, current_file, MOUSEPAD_LOCATION_REVERT);
          mousepad_file_set_encoding (document->file, current_encoding);
        }

      /* cleanup */
      g_object_unref (file);
      if (current_file != NULL)
        g_object_unref (current_file);
    }

  /* store the save result as the action state */
  g_action_change_state (G_ACTION (action), g_variant_new_int32 (succeed));

  /* decrease recursion count */
  depth--;
}



static void
mousepad_window_action_save_all (GSimpleAction *action,
                                 GVariant      *value,
                                 gpointer       data)
{
  MousepadWindow   *window = data;
  MousepadDocument *document;
  GAction          *save;
  GSList           *li, *documents = NULL;
  GError           *error = NULL;
  const gchar      *action_name;
  gboolean          succeed = TRUE;
  gint              current, i, n_docs, page_num;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* get the current active tab */
  current = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook));

  /* walk though all the document in the window */
  n_docs = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));
  for (i = 0; i < n_docs; i++)
    {
      /* get the document */
      document = MOUSEPAD_DOCUMENT (gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), i));

      /* continue if the document is not modified */
      if (! gtk_text_buffer_get_modified (document->buffer))
        continue;

      /* we try to quickly save files, without bothering the user */
      if (mousepad_file_location_is_set (document->file)
          && ! mousepad_file_get_read_only (document->file))
        {
          /* try to save the file */
          succeed = mousepad_file_save (document->file, FALSE, &error);

          /* file has been externally modified: add it to a queue */
          if (G_UNLIKELY (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_WRONG_ETAG)))
            {
              g_clear_error (&error);
              documents = g_slist_prepend (documents, document);
            }
          /* break on other problems */
          else if (G_UNLIKELY (! succeed))
            break;
        }
      /* add the document to a queue to bother the user later */
      else
        documents = g_slist_prepend (documents, document);
    }

  if (G_UNLIKELY (error != NULL))
    {
      /* focus the tab that triggered the problem */
      gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), i);

      /* show the error */
      mousepad_dialogs_show_error (GTK_WINDOW (window), error, MOUSEPAD_MESSAGE_IO_ERROR_SAVE);

      /* free error */
      g_error_free (error);
    }
  else
    {
      /* open a dialog for each pending file */
      for (li = documents; li != NULL; li = li->next)
        {
          /* make sure the document is still there */
          page_num = gtk_notebook_page_num (GTK_NOTEBOOK (window->notebook), li->data);
          if (G_LIKELY (page_num > -1))
            {
              /* focus the tab we're going to save */
              gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), page_num);

              /* trigger the save as function */
              document = li->data;
              if (! mousepad_file_location_is_set (document->file)
                  || mousepad_file_get_read_only (document->file))
                action_name = "file.save-as";
              /* trigger the save function (externally modified document) */
              else
                action_name = "file.save";

              save = g_action_map_lookup_action (G_ACTION_MAP (window), action_name);
              g_action_activate (save, NULL);
              succeed = mousepad_action_get_state_int32_boolean (save);
            }

          /* break on problems */
          if (G_UNLIKELY (! succeed))
            break;
        }

      /* focus the origional doc if everything went fine */
      if (G_LIKELY (li == NULL))
        gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), current);
    }

  /* cleanup */
  g_slist_free (documents);
}



static void
mousepad_window_action_reload (GSimpleAction *action,
                               GVariant      *value,
                               gpointer       data)
{
  MousepadWindow   *window = data;
  MousepadDocument *document = window->active;
  GtkTextIter       cursor;
  GError           *error = NULL;
  gint              retval, line, column;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* ask the user if he really wants to do this when the file is modified */
  if (gtk_text_buffer_get_modified (document->buffer))
    {
      /* ask the user if he really wants to revert */
      switch (mousepad_dialogs_revert (GTK_WINDOW (window)))
        {
          case MOUSEPAD_RESPONSE_SAVE_AS:
            /* open the save as dialog */
            g_action_group_activate_action (G_ACTION_GROUP (window), "file.save-as", NULL);

            /* exit regardless of save status */
            return;
            break;

          case MOUSEPAD_RESPONSE_RELOAD:
            /* reload below */
            break;

          /* revert cancelled */
          default:
            return;
            break;
        }
    }

  /* get iter at cursor position */
  gtk_text_buffer_get_iter_at_mark (document->buffer, &cursor,
                                    gtk_text_buffer_get_insert (document->buffer));

  line = gtk_text_iter_get_line (&cursor);
  column = mousepad_util_get_real_line_offset (&cursor);

  /* lock the undo manager */
  gtk_source_buffer_begin_not_undoable_action (GTK_SOURCE_BUFFER (document->buffer));

  /* reload the file */
  retval = mousepad_file_open (document->file, line, column, TRUE, FALSE, TRUE, &error);

  /* release the lock */
  gtk_source_buffer_end_not_undoable_action (GTK_SOURCE_BUFFER (document->buffer));

  if (G_UNLIKELY (retval != 0))
    {
      /* show the error */
      mousepad_dialogs_show_error (GTK_WINDOW (window), error, _("Failed to reload the document"));
      g_error_free (error);
    }
  else
    {
      mousepad_window_update_actions (window);
      g_idle_add (mousepad_view_scroll_to_cursor,
                  mousepad_util_source_autoremove (window->active->textview));
    }
}



static void
mousepad_window_action_print (GSimpleAction *action,
                              GVariant      *value,
                              gpointer       data)
{
  MousepadWindow *window = data;
  MousepadPrint  *print;
  GError         *error = NULL;
  gboolean        succeed;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* create new print operation */
  print = mousepad_print_new ();

  /* print the current document */
  succeed = mousepad_print_document_interactive (print, window->active, GTK_WINDOW (window), &error);

  if (G_UNLIKELY (! succeed))
    {
      /* show the error */
      mousepad_dialogs_show_error (GTK_WINDOW (window), error, _("Failed to print the document"));
      g_error_free (error);
    }

  /* release the object */
  g_object_unref (print);
}



static void
mousepad_window_action_detach (GSimpleAction *action,
                               GVariant      *value,
                               gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* invoke function without cooridinates */
  mousepad_window_notebook_create_window (GTK_NOTEBOOK (window->notebook),
                                          GTK_WIDGET (window->active),
                                          -1, -1, window);
}



static void
mousepad_window_action_close (GSimpleAction *action,
                              GVariant      *value,
                              gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* close active document */
  mousepad_window_close_document (window, window->active);
}



static void
mousepad_window_action_close_window (GSimpleAction *action,
                                     GVariant      *value,
                                     gpointer       data)
{
  MousepadWindow *window = data;
  GtkWidget      *document;
  gint            npages, i;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* reset action state */
  g_action_change_state (G_ACTION (action), g_variant_new_int32 (TRUE));

  /* the window may be hidden without any document, e.g. when running the encoding
   * dialog at startup */
  if ((npages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook))) == 0)
    {
      gtk_widget_destroy (GTK_WIDGET (window));
      return;
    }

  /* block session handler at last window */
  if (g_list_length (gtk_application_get_windows (gtk_window_get_application (data))) == 1)
    mousepad_history_session_set_quitting (TRUE);

  /* prevent menu updates */
  lock_menu_updates++;

  /* ask what to do with the modified document in this window */
  for (i = npages - 1; i >= 0; --i)
    {
      /* get the document */
      document = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), i);

      /* check for debug builds */
      g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

      /* focus the tab we're going to close */
      gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), i);

      /* close each document */
      if (! mousepad_window_close_document (window, MOUSEPAD_DOCUMENT (document)))
        {
          /* closing cancelled, release menu lock */
          lock_menu_updates--;

          /* store the close result as the action state */
          g_action_change_state (G_ACTION (action), g_variant_new_int32 (FALSE));

          /* unblock session handler and save session */
          mousepad_history_session_set_quitting (FALSE);
          mousepad_history_session_save ();

          /* leave function */
          return;
        }
    }

  /* release lock */
  lock_menu_updates--;
}



static void
mousepad_window_action_undo (GSimpleAction *action,
                             GVariant      *value,
                             gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* undo */
  g_signal_emit_by_name (window->active->textview, "undo");
}



static void
mousepad_window_action_redo (GSimpleAction *action,
                             GVariant      *value,
                             gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* redo */
  g_signal_emit_by_name (window->active->textview, "redo");
}



static void
mousepad_window_action_cut (GSimpleAction *action,
                            GVariant      *value,
                            gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* cut from textview */
  g_signal_emit_by_name (window->active->textview, "cut-clipboard");

  /* update the history */
  mousepad_window_paste_history_add (window);
}



static void
mousepad_window_action_copy (GSimpleAction *action,
                             GVariant      *value,
                             gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* copy from textview */
  g_signal_emit_by_name (window->active->textview, "copy-clipboard");

  /* update the history */
  mousepad_window_paste_history_add (window);
}



static void
mousepad_window_action_paste (GSimpleAction *action,
                              GVariant      *value,
                              gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* paste in textview */
  g_signal_emit_by_name (window->active->textview, "paste-clipboard");
}



static void
mousepad_window_action_paste_history (GSimpleAction *action,
                                      GVariant      *value,
                                      gpointer       data)
{
  MousepadWindow *window = data;
  GtkWidget      *menu;
  GdkRectangle    location;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* get the history menu */
  menu = mousepad_window_paste_history_menu (window);

  /* select the first item in the menu */
  gtk_menu_shell_select_first (GTK_MENU_SHELL (menu), TRUE);

  /* get cursor location in textview coordinates */
  gtk_text_view_get_cursor_locations (GTK_TEXT_VIEW (window->active->textview), NULL, &location, NULL);
  gtk_text_view_buffer_to_window_coords (GTK_TEXT_VIEW (window->active->textview),
                                         GTK_TEXT_WINDOW_WIDGET,
                                         location.x, location.y,
                                         &(location.x), &(location.y));

  /* popup the menu */
  gtk_menu_popup_at_rect (GTK_MENU (menu),
                          gtk_widget_get_parent_window (GTK_WIDGET (window->active->textview)),
                          &location, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
}



static void
mousepad_window_action_paste_column (GSimpleAction *action,
                                     GVariant      *value,
                                     gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* paste the clipboard into a column */
  mousepad_view_custom_paste (window->active->textview, NULL);
}



static void
mousepad_window_action_delete_selection (GSimpleAction *action,
                                         GVariant      *value,
                                         gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* delete selection in textview */
  g_signal_emit_by_name (window->active->textview, "delete-from-cursor", GTK_DELETE_CHARS, 1);
}



static void
mousepad_window_action_delete_line (GSimpleAction *action,
                                    GVariant      *value,
                                    gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* delete line in textview */
  g_signal_emit_by_name (window->active->textview, "delete-from-cursor", GTK_DELETE_PARAGRAPHS, 1);
}



static void
mousepad_window_action_select_all (GSimpleAction *action,
                                   GVariant      *value,
                                   gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* select everything in the document */
  g_signal_emit_by_name (window->active->textview, "select-all", TRUE);
}



static void
mousepad_window_action_lowercase (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert selection to lowercase */
  g_signal_emit_by_name (window->active->textview, "change-case", GTK_SOURCE_CHANGE_CASE_LOWER);
}



static void
mousepad_window_action_uppercase (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert selection to uppercase */
  g_signal_emit_by_name (window->active->textview, "change-case", GTK_SOURCE_CHANGE_CASE_UPPER);
}



static void
mousepad_window_action_titlecase (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert selection to titlecase */
  g_signal_emit_by_name (window->active->textview, "change-case", GTK_SOURCE_CHANGE_CASE_TITLE);
}



static void
mousepad_window_action_opposite_case (GSimpleAction *action,
                                      GVariant      *value,
                                      gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert selection to opposite case */
  g_signal_emit_by_name (window->active->textview, "change-case", GTK_SOURCE_CHANGE_CASE_TOGGLE);
}



static void
mousepad_window_action_tabs_to_spaces (GSimpleAction *action,
                                       GVariant      *value,
                                       gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert tabs to spaces */
  mousepad_view_convert_spaces_and_tabs (window->active->textview, TABS_TO_SPACES);
}



static void
mousepad_window_action_spaces_to_tabs (GSimpleAction *action,
                                       GVariant      *value,
                                       gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert spaces to tabs */
  mousepad_view_convert_spaces_and_tabs (window->active->textview, SPACES_TO_TABS);
}



static void
mousepad_window_action_strip_trailing_spaces (GSimpleAction *action,
                                              GVariant      *value,
                                              gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert spaces to tabs */
  mousepad_view_strip_trailing_spaces (window->active->textview);
}



static void
mousepad_window_action_transpose (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* transpose */
  mousepad_view_transpose (window->active->textview);
}



static void
mousepad_window_action_move_line_up (GSimpleAction *action,
                                     GVariant      *value,
                                     gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* move lines one line up */
  g_signal_emit_by_name (window->active->textview, "move-lines", FALSE);
}



static void
mousepad_window_action_move_line_down (GSimpleAction *action,
                                       GVariant      *value,
                                       gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* move lines one line down */
  g_signal_emit_by_name (window->active->textview, "move-lines", TRUE);
}



static void
mousepad_window_action_move_word_left (GSimpleAction *action,
                                       GVariant      *value,
                                       gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* move words one word left */
  g_signal_emit_by_name (window->active->textview, "move-words", -1);
}



static void
mousepad_window_action_move_word_right (GSimpleAction *action,
                                        GVariant      *value,
                                        gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* move words one word right */
  g_signal_emit_by_name (window->active->textview, "move-words", 1);
}



static void
mousepad_window_action_duplicate (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* dupplicate */
  mousepad_view_duplicate (window->active->textview);
}



static void
mousepad_window_action_increase_indent (GSimpleAction *action,
                                        GVariant      *value,
                                        gpointer       data)
{
  MousepadWindow *window = data;
  GtkTextIter     start, end;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* increase the indent */
  gtk_text_buffer_get_selection_bounds (window->active->buffer, &start, &end);
  gtk_source_view_indent_lines (GTK_SOURCE_VIEW (window->active->textview), &start, &end);
}



static void
mousepad_window_action_decrease_indent (GSimpleAction *action,
                                        GVariant      *value,
                                        gpointer       data)
{
  MousepadWindow *window = data;
  GtkTextIter     start, end;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* decrease the indent */
  gtk_text_buffer_get_selection_bounds (window->active->buffer, &start, &end);
  gtk_source_view_unindent_lines (GTK_SOURCE_VIEW (window->active->textview), &start, &end);
}



static void
mousepad_window_search_bar_switch_page (MousepadWindow *window)
{
  GtkTextBuffer *old_buffer, *new_buffer;
  gboolean       search;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (window->search_bar));

  old_buffer = window->previous != NULL ? window->previous->buffer : NULL;
  new_buffer = window->active->buffer;

  /* run a search only if the replace dialog is not shown */
  search = window->replace_dialog == NULL || ! gtk_widget_get_visible (window->replace_dialog);
  mousepad_search_bar_page_switched (MOUSEPAD_SEARCH_BAR (window->search_bar),
                                     old_buffer, new_buffer, search);
}



static void
mousepad_window_hide_search_bar (MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));
  g_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (window->search_bar));

  /* disconnect tab switch signal */
  mousepad_disconnect_by_func (window->notebook,
                               mousepad_window_search_bar_switch_page, window);

  /* hide the search bar */
  gtk_widget_hide (window->search_bar);

  /* set the window property if no search widget is visible */
  if (window->replace_dialog == NULL || ! gtk_widget_get_visible (window->replace_dialog))
    g_object_set (window, "search-widget-visible", FALSE, NULL);

  /* focus the active document's text view */
  mousepad_document_focus_textview (window->active);
}



static void
mousepad_window_action_find (GSimpleAction *action,
                             GVariant      *value,
                             gpointer       data)
{
  MousepadWindow *window = data;
  gchar          *selection;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* create a new search bar is needed */
  if (window->search_bar == NULL)
    {
      /* create a new toolbar and pack it into the box */
      window->search_bar = mousepad_search_bar_new ();
      gtk_box_pack_start (GTK_BOX (window->box), window->search_bar, FALSE, FALSE, PADDING);

      /* connect signals */
      g_signal_connect_swapped (window->search_bar, "hide-bar",
                                G_CALLBACK (mousepad_window_hide_search_bar), window);
      g_signal_connect_swapped (window->search_bar, "search",
                                G_CALLBACK (mousepad_window_search), window);
    }

  /* set the search entry text */
  selection = mousepad_util_get_selection (window->active->buffer);
  if (selection != NULL)
    {
      mousepad_search_bar_set_text (MOUSEPAD_SEARCH_BAR (window->search_bar), selection);
      g_free (selection);
    }

  if (! gtk_widget_get_visible (window->search_bar))
    {
      /* connect to "switch-page" signal */
      g_signal_connect_swapped (window->notebook, "switch-page",
                                G_CALLBACK (mousepad_window_search_bar_switch_page), window);

      /* connect to the current buffer signals */
      mousepad_window_search_bar_switch_page (window);

      /* show the search bar */
      gtk_widget_show (window->search_bar);

      /* set the window property if no search widget was visible */
      if (window->replace_dialog == NULL || ! gtk_widget_get_visible (window->replace_dialog))
        g_object_set (window, "search-widget-visible", TRUE, NULL);
    }

  /* focus the search entry */
  mousepad_search_bar_focus (MOUSEPAD_SEARCH_BAR (window->search_bar));
}



static void
mousepad_window_action_find_next (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* find the next occurrence */
  if (G_LIKELY (window->search_bar != NULL))
    mousepad_search_bar_find_next (MOUSEPAD_SEARCH_BAR (window->search_bar));
}



static void
mousepad_window_action_find_previous (GSimpleAction *action,
                                      GVariant      *value,
                                      gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* find the previous occurrence */
  if (G_LIKELY (window->search_bar != NULL))
    mousepad_search_bar_find_previous (MOUSEPAD_SEARCH_BAR (window->search_bar));
}



static void
mousepad_window_replace_dialog_switch_page (MousepadWindow *window)
{
  GtkTextBuffer *old_buffer, *new_buffer;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_REPLACE_DIALOG (window->replace_dialog));

  old_buffer = window->previous != NULL ? window->previous->buffer : NULL;
  new_buffer = window->active->buffer;

  mousepad_replace_dialog_page_switched (MOUSEPAD_REPLACE_DIALOG (window->replace_dialog),
                                         old_buffer, new_buffer);
}



static void
mousepad_window_replace_dialog_destroy (MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* disconnect tab switch signal */
  mousepad_disconnect_by_func (window->notebook,
                               mousepad_window_replace_dialog_switch_page, window);

  /* reset the dialog variable */
  window->replace_dialog = NULL;

  /* set the window property if no search widget is visible */
  if (window->search_bar == NULL || ! gtk_widget_get_visible (window->search_bar))
    g_object_set (window, "search-widget-visible", FALSE, NULL);
}



static void
mousepad_window_action_replace (GSimpleAction *action,
                                GVariant      *value,
                                gpointer       data)
{
  MousepadWindow *window = data;
  gchar          *selection;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  if (window->replace_dialog == NULL)
    {
      /* create a new dialog */
      window->replace_dialog = mousepad_replace_dialog_new (window);

      /* connect signals */
      g_signal_connect_swapped (window->replace_dialog, "destroy",
                                G_CALLBACK (mousepad_window_replace_dialog_destroy), window);
      g_signal_connect_swapped (window->replace_dialog, "search",
                                G_CALLBACK (mousepad_window_search), window);
      g_signal_connect_swapped (window->notebook, "switch-page",
                                G_CALLBACK (mousepad_window_replace_dialog_switch_page), window);

      /* connect to the current buffer signals */
      mousepad_window_replace_dialog_switch_page (window);

      /* set the window property if no search widget was visible */
      if (window->search_bar == NULL || ! gtk_widget_get_visible (window->search_bar))
        g_object_set (window, "search-widget-visible", TRUE, NULL);
    }
  else
    {
      /* focus the existing dialog */
      gtk_window_present (GTK_WINDOW (window->replace_dialog));
    }

  /* set the search entry text */
  selection = mousepad_util_get_selection (window->active->buffer);
  if (selection != NULL)
    {
      mousepad_replace_dialog_set_text (MOUSEPAD_REPLACE_DIALOG (window->replace_dialog),
                                        selection);
      g_free (selection);
    }
}



static void
mousepad_window_action_go_to_position (GSimpleAction *action,
                                       GVariant      *value,
                                       gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (window->active->buffer));

  /* run jump dialog */
  if (mousepad_dialogs_go_to (GTK_WINDOW (window), window->active->buffer))
    {
      /* put the cursor on screen */
      mousepad_view_scroll_to_cursor (window->active->textview);
    }
}



static void
mousepad_window_action_select_font (GSimpleAction *action,
                                    GVariant      *value,
                                    gpointer       data)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (data));

  /* run the dialog */
  mousepad_dialogs_select_font (GTK_WINDOW (data));
}



static void
mousepad_window_change_font_size (MousepadWindow *window,
                                  gint            change)
{
  PangoFontDescription *font_desc;
  GtkStyleContext      *context;
  GValue                font = G_VALUE_INIT;
  gchar                *font_string;
  gint                  font_size;

  /* reset case */
  if (change == 0)
    {
      if (MOUSEPAD_SETTING_GET_BOOLEAN (USE_DEFAULT_FONT))
        g_object_get (g_application_get_default (), "default-font", &font_string, NULL);
      else
        font_string = MOUSEPAD_SETTING_GET_STRING (FONT);
    }
  else
    {
      /* retrieve current font size and add it the change */
      context = gtk_widget_get_style_context (GTK_WIDGET (window->active->textview));
      gtk_style_context_get_property (context, "font", gtk_style_context_get_state (context), &font);
      font_desc = g_value_get_boxed (&font);
      font_size = pango_font_description_get_size (font_desc) / PANGO_SCALE + change;

      /* exit silently if the new font size is outside [MIN_FONT_SIZE, MAX_FONT_SIZE] */
      if (font_size < MIN_FONT_SIZE || font_size > MAX_FONT_SIZE)
        {
          g_value_unset (&font);
          return;
        }

      /* generate new font string */
      pango_font_description_set_size (font_desc, font_size * PANGO_SCALE);
      font_string = pango_font_description_to_string (font_desc);
      g_value_unset (&font);
    }

  /* change font size */
  g_object_set (window->active->textview, "font", font_string, NULL);

  /* cleanup */
  g_free (font_string);
}



static void
mousepad_window_action_increase_font_size (GSimpleAction *action,
                                           GVariant      *value,
                                           gpointer       data)
{
  mousepad_window_change_font_size (data, 1);
}



static void
mousepad_window_action_decrease_font_size (GSimpleAction *action,
                                           GVariant      *value,
                                           gpointer       data)
{
  mousepad_window_change_font_size (data, -1);
}



static void
mousepad_window_action_reset_font_size (GSimpleAction *action,
                                        GVariant      *value,
                                        gpointer       data)
{
  mousepad_window_change_font_size (data, 0);
}



static void
mousepad_window_action_bar_activate (GSimpleAction *action,
                                     GVariant      *value,
                                     gpointer       data)
{
  gboolean state;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (data));

  state = ! mousepad_action_get_state_boolean (G_ACTION (action));
  mousepad_setting_set_boolean (g_action_get_name (G_ACTION (action)), state);
}



static void
mousepad_window_action_textview (GSimpleAction *action,
                                 GVariant      *value,
                                 gpointer       data)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (data));

  g_action_group_activate_action (G_ACTION_GROUP (data), MENUBAR, NULL);
}



static void
mousepad_window_action_menubar_state (GSimpleAction *action,
                                      GVariant      *state,
                                      gpointer       data)
{
  MousepadWindow *window = data;
  GtkApplication *application;
  GtkWidget      *label;
  GAction        *textview_action;
  GMenuModel     *model;
  GList          *children, *child;
  static GList   *mnemonics = NULL;
  gpointer        mnemonic;
  gboolean        visible;
  gint            offset;

  /* we have to pass below at least once at initialization, both in the visible and in the
   * hidden case, and not pass again afterwards if the action state doesn't change, so we
   * can't escape an additional test: the action should have a "non-initialized" state */
  static gboolean init = TRUE;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* exit if there is actually no change, except at initialization */
  visible = g_variant_get_boolean (state);
  if (! init && visible == mousepad_action_get_state_boolean (G_ACTION (action)))
    return;
  else if (init)
    init = FALSE;

  /* set the action state */
  g_simple_action_set_state (action, state);

  /* show/hide the "Menubar" item in the text view menu */
  textview_action = g_action_map_lookup_action (G_ACTION_MAP (window), "textview.menubar");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (textview_action), ! visible);

  if (! visible)
    {
      /* set the textview menu last tooltip */
      application = gtk_window_get_application (GTK_WINDOW (window));
      model = G_MENU_MODEL (gtk_application_get_menu_by_id (application, "textview.menubar"));
      offset = GPOINTER_TO_INT (mousepad_object_get_data (model, window->offset_key));
      mousepad_window_menu_set_tooltips (window, window->textview_menu, model, &offset);

      /* get the main menubar mnemonic keys */
      if (mnemonics == NULL)
        {
          children = gtk_container_get_children (GTK_CONTAINER (window->menubar));
          for (child = children; child != NULL; child = child->next)
            {
              label = gtk_bin_get_child (GTK_BIN (child->data));
              mnemonic = GUINT_TO_POINTER (gtk_label_get_mnemonic_keyval (GTK_LABEL (label)));
              mnemonics = g_list_prepend (mnemonics, mnemonic);
            }
          g_list_free (children);
        }

      /* handle key events to show the menubar temporarily when hidden */
      g_signal_connect (window, "key-press-event",
                        G_CALLBACK (mousepad_window_menubar_key_event), mnemonics);
      g_signal_connect (window, "key-release-event",
                        G_CALLBACK (mousepad_window_menubar_key_event), mnemonics);
    }
  /* disconnect handlers that show the menubar temporarily when hidden */
  else
    {
      mousepad_disconnect_by_func (window, mousepad_window_menubar_key_event, mnemonics);
      mousepad_disconnect_by_func (window, mousepad_window_menubar_hide_event, NULL);
      mousepad_disconnect_by_func (window->menubar, mousepad_window_menubar_hide_event, window);
      mousepad_disconnect_by_func (window->notebook, mousepad_window_menubar_hide_event, window);
    }
}



static void
mousepad_window_action_fullscreen (GSimpleAction *action,
                                   GVariant      *value,
                                   gpointer       data)
{
  gboolean fullscreen;

  fullscreen = ! mousepad_action_get_state_boolean (G_ACTION (action));
  g_action_change_state (G_ACTION (action), g_variant_new_boolean (fullscreen));

  /* entering/leaving fullscreen mode */
  if (fullscreen)
    gtk_window_fullscreen (data);
  else
    gtk_window_unfullscreen (data);

  /* update the "Fullscreen" menu item */
  mousepad_window_update_menu_item (data, "item.view.fullscreen", 0, GINT_TO_POINTER (fullscreen));
}



static void
mousepad_window_action_line_ending (GSimpleAction *action,
                                    GVariant      *value,
                                    gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* leave when menu updates are locked */
  if (lock_menu_updates == 0)
    {
      /* avoid menu actions */
      lock_menu_updates++;

      /* set the current state */
      g_action_change_state (G_ACTION (action), value);
      mousepad_file_set_line_ending (window->active->file, g_variant_get_int32 (value));

      /* allow menu actions again */
      lock_menu_updates--;
    }
}



static void
mousepad_window_action_tab_size (GSimpleAction *action,
                                 GVariant      *value,
                                 gpointer       data)
{
  gint tab_size;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (data));

  /* leave when menu updates are locked */
  if (lock_menu_updates == 0)
    {
      /* get the tab size */
      tab_size = g_variant_get_int32 (value);

      /* whether the other item was clicked */
      if (tab_size == 0)
        {
          /* get tab size from document */
          tab_size = MOUSEPAD_SETTING_GET_UINT (TAB_WIDTH);

          /* select other size in dialog */
          tab_size = mousepad_dialogs_other_tab_size (data, tab_size);
        }

      /* store as last used value */
      MOUSEPAD_SETTING_SET_UINT (TAB_WIDTH, tab_size);
    }
}



static void
mousepad_window_action_language (GSimpleAction *action,
                                 GVariant      *value,
                                 gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* leave when menu updates are locked */
  if (lock_menu_updates == 0)
    {
      /* avoid menu actions */
      lock_menu_updates++;

      g_action_change_state (G_ACTION (action), value);
      mousepad_file_set_language (window->active->file, g_variant_get_string (value, NULL));

      /* allow menu actions again */
      lock_menu_updates--;
    }
}



static void
mousepad_window_action_write_bom (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  MousepadWindow *window = data;
  gboolean        state;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* leave when menu updates are locked */
  if (lock_menu_updates == 0)
    {
      /* avoid menu actions */
      lock_menu_updates++;

      /* set the current state */
      state = ! mousepad_action_get_state_boolean (G_ACTION (action));
      g_action_change_state (G_ACTION (action), g_variant_new_boolean (state));
      mousepad_file_set_write_bom (window->active->file, state);

      /* allow menu actions again */
      lock_menu_updates--;
    }
}



static void
mousepad_window_action_viewer_mode (GSimpleAction *action,
                                    GVariant      *value,
                                    gpointer       data)
{
  MousepadWindow *window = data;
  gboolean        state;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* leave when menu updates are locked */
  if (lock_menu_updates == 0)
    {
      /* avoid menu actions */
      lock_menu_updates++;

      /* set the current state */
      state = ! mousepad_action_get_state_boolean (G_ACTION (action));
      g_action_change_state (G_ACTION (action), g_variant_new_boolean (state));

      /* set new value */
      gtk_text_view_set_editable (GTK_TEXT_VIEW (window->active->textview), ! state);

      /* update window title */
      mousepad_window_set_title (window);

      /* allow menu actions again */
      lock_menu_updates--;
    }
}



static void
mousepad_window_action_prev_tab (GSimpleAction *action,
                                 GVariant      *value,
                                 gpointer       data)
{
  MousepadWindow *window = data;
  gint            page_num, n_pages;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* get notebook info */
  page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook));
  n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

  /* switch to the previous tab or cycle to the last tab */
  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), (page_num - 1) % n_pages);
}



static void
mousepad_window_action_next_tab (GSimpleAction *action,
                                 GVariant      *value,
                                 gpointer       data)
{
  MousepadWindow *window = data;
  gint            page_num, n_pages;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* get notebook info */
  page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook));
  n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

  /* switch to the next tab or cycle to the first tab */
  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), (page_num + 1) % n_pages);
}



static void
mousepad_window_action_go_to_tab (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  MousepadWindow *window = data;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* leave when the menu is locked */
  if (lock_menu_updates == 0)
    {
      /* avoid menu actions */
      lock_menu_updates++;

      gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), g_variant_get_int32 (value));
      g_action_change_state (G_ACTION (action), value);

      /* allow menu actions again */
      lock_menu_updates--;
    }
}



static void
mousepad_window_action_contents (GSimpleAction *action,
                                 GVariant      *value,
                                 gpointer       data)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (data));

  /* show help */
  mousepad_dialogs_show_help (GTK_WINDOW (data), NULL, NULL);
}



static void
mousepad_window_action_about (GSimpleAction *action,
                              GVariant      *value,
                              gpointer       data)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (data));

  /* show about dialog */
  mousepad_dialogs_show_about (GTK_WINDOW (data));
}
