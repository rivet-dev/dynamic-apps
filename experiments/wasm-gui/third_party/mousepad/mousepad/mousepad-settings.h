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

#ifndef __MOUSEPAD_SETTINGS_H__
#define __MOUSEPAD_SETTINGS_H__ 1

#include <gio/gio.h>

G_BEGIN_DECLS

/* Setting names */
#define MOUSEPAD_SETTING_DEFAULT_ENCODING             "preferences.file.default-encoding"
#define MOUSEPAD_SETTING_ADD_LAST_EOL                 "preferences.file.add-last-end-of-line"
#define MOUSEPAD_SETTING_MAKE_BACKUP                  "preferences.file.make-backup"
#define MOUSEPAD_SETTING_MONITOR_CHANGES              "preferences.file.monitor-changes"
#define MOUSEPAD_SETTING_MONITOR_DISABLING_TIMER      "preferences.file.monitor-disabling-timer"
#define MOUSEPAD_SETTING_AUTO_RELOAD                  "preferences.file.auto-reload"
#define MOUSEPAD_SETTING_SESSION_RESTORE              "preferences.file.session-restore"
#define MOUSEPAD_SETTING_AUTOSAVE_TIMER               "preferences.file.autosave-timer"

#define MOUSEPAD_SETTING_AUTO_INDENT                  "preferences.view.auto-indent"
#define MOUSEPAD_SETTING_FONT                         "preferences.view.font-name"
#define MOUSEPAD_SETTING_USE_DEFAULT_FONT             "preferences.view.use-default-monospace-font"
#define MOUSEPAD_SETTING_SHOW_WHITESPACE              "preferences.view.show-whitespace"
#define MOUSEPAD_SETTING_SHOW_WHITESPACE_LEADING      "preferences.view.show-whitespace.leading"
#define MOUSEPAD_SETTING_SHOW_WHITESPACE_INSIDE       "preferences.view.show-whitespace.inside"
#define MOUSEPAD_SETTING_SHOW_WHITESPACE_TRAILING     "preferences.view.show-whitespace.trailing"
#define MOUSEPAD_SETTING_SHOW_LINE_ENDINGS            "preferences.view.show-line-endings"
#define MOUSEPAD_SETTING_HIGHLIGHT_CURRENT_LINE       "preferences.view.highlight-current-line"
#define MOUSEPAD_SETTING_INDENT_ON_TAB                "preferences.view.indent-on-tab"
#define MOUSEPAD_SETTING_INDENT_WIDTH                 "preferences.view.indent-width"
#define MOUSEPAD_SETTING_INSERT_SPACES                "preferences.view.insert-spaces"
#define MOUSEPAD_SETTING_RIGHT_MARGIN_POSITION        "preferences.view.right-margin-position"
#define MOUSEPAD_SETTING_SHOW_LINE_MARKS              "preferences.view.show-line-marks"
#define MOUSEPAD_SETTING_SHOW_LINE_NUMBERS            "preferences.view.show-line-numbers"
#define MOUSEPAD_SETTING_SHOW_RIGHT_MARGIN            "preferences.view.show-right-margin"
#define MOUSEPAD_SETTING_SMART_BACKSPACE              "preferences.view.smart-backspace"
#define MOUSEPAD_SETTING_SMART_HOME_END               "preferences.view.smart-home-end"
#define MOUSEPAD_SETTING_TAB_WIDTH                    "preferences.view.tab-width"
#define MOUSEPAD_SETTING_WORD_WRAP                    "preferences.view.word-wrap"
#define MOUSEPAD_SETTING_MATCH_BRACES                 "preferences.view.match-braces"
#define MOUSEPAD_SETTING_COLOR_SCHEME                 "preferences.view.color-scheme"

#define MOUSEPAD_SETTING_TOOLBAR_STYLE                "preferences.window.toolbar-style"
#define MOUSEPAD_SETTING_TOOLBAR_ICON_SIZE            "preferences.window.toolbar-icon-size"
#define MOUSEPAD_SETTING_ALWAYS_SHOW_TABS             "preferences.window.always-show-tabs"
#define MOUSEPAD_SETTING_EXPAND_TABS                  "preferences.window.expand-tabs"
#define MOUSEPAD_SETTING_CYCLE_TABS                   "preferences.window.cycle-tabs"
#define MOUSEPAD_SETTING_OPENING_MODE                 "preferences.window.opening-mode"
#define MOUSEPAD_SETTING_DEFAULT_TAB_SIZES            "preferences.window.default-tab-sizes"
#define MOUSEPAD_SETTING_PATH_IN_TITLE                "preferences.window.path-in-title"
#define MOUSEPAD_SETTING_RECENT_MENU_ITEMS            "preferences.window.recent-menu-items"
#define MOUSEPAD_SETTING_REMEMBER_SIZE                "preferences.window.remember-size"
#define MOUSEPAD_SETTING_REMEMBER_POSITION            "preferences.window.remember-position"
#define MOUSEPAD_SETTING_REMEMBER_STATE               "preferences.window.remember-state"
#define MOUSEPAD_SETTING_MENUBAR_VISIBLE              "preferences.window.menubar-visible"
#define MOUSEPAD_SETTING_TOOLBAR_VISIBLE              "preferences.window.toolbar-visible"
#define MOUSEPAD_SETTING_STATUSBAR_VISIBLE            "preferences.window.statusbar-visible"
#define MOUSEPAD_SETTING_MENUBAR_VISIBLE_FULLSCREEN   "preferences.window.menubar-visible-in-fullscreen"
#define MOUSEPAD_SETTING_TOOLBAR_VISIBLE_FULLSCREEN   "preferences.window.toolbar-visible-in-fullscreen"
#define MOUSEPAD_SETTING_STATUSBAR_VISIBLE_FULLSCREEN "preferences.window.statusbar-visible-in-fullscreen"
#define MOUSEPAD_SETTING_OLD_STYLE_MENU               "preferences.window.old-style-menu"
#define MOUSEPAD_SETTING_CLIENT_SIDE_DECORATIONS      "preferences.window.client-side-decorations"

/* State setting names */
#define MOUSEPAD_SETTING_ENABLED_PLUGINS              "state.application.enabled-plugins"
#define MOUSEPAD_SETTING_SESSION                      "state.application.session"

#define MOUSEPAD_SETTING_SEARCH_DIRECTION             "state.search.direction"
#define MOUSEPAD_SETTING_SEARCH_WRAP_AROUND           "state.search.wrap-around"
#define MOUSEPAD_SETTING_SEARCH_MATCH_CASE            "state.search.match-case"
#define MOUSEPAD_SETTING_SEARCH_ENABLE_REGEX          "state.search.enable-regex"
#define MOUSEPAD_SETTING_SEARCH_MATCH_WHOLE_WORD      "state.search.match-whole-word"
#define MOUSEPAD_SETTING_SEARCH_REPLACE_ALL           "state.search.replace-all"
#define MOUSEPAD_SETTING_SEARCH_REPLACE_ALL_LOCATION  "state.search.replace-all-location"
#define MOUSEPAD_SETTING_SEARCH_HIGHLIGHT_ALL         "state.search.highlight-all"
#define MOUSEPAD_SETTING_SEARCH_INCREMENTAL           "state.search.incremental"
#define MOUSEPAD_SETTING_SEARCH_HISTORY_SIZE          "state.search.history-size"
#define MOUSEPAD_SETTING_SEARCH_SEARCH_HISTORY        "state.search.search-history"
#define MOUSEPAD_SETTING_SEARCH_REPLACE_HISTORY       "state.search.replace-history"

#define MOUSEPAD_SETTING_WINDOW_HEIGHT                "state.window.height"
#define MOUSEPAD_SETTING_WINDOW_WIDTH                 "state.window.width"
#define MOUSEPAD_SETTING_WINDOW_TOP                   "state.window.top"
#define MOUSEPAD_SETTING_WINDOW_LEFT                  "state.window.left"
#define MOUSEPAD_SETTING_WINDOW_MAXIMIZED             "state.window.maximized"
#define MOUSEPAD_SETTING_WINDOW_FULLSCREEN            "state.window.fullscreen"

void       mousepad_settings_init          (void);
void       mousepad_settings_finalize      (void);
void       mousepad_settings_add_root      (const gchar        *schema_id);

void       mousepad_setting_bind           (const gchar        *setting,
                                            gpointer            object,
                                            const gchar        *prop,
                                            GSettingsBindFlags  flags);

gulong     mousepad_setting_connect        (const gchar        *setting,
                                            GCallback           callback,
                                            gpointer            user_data,
                                            GConnectFlags       connect_flags);

gulong     mousepad_setting_connect_object (const gchar        *setting,
                                            GCallback           callback,
                                            gpointer            gobject,
                                            GConnectFlags       connect_flags);

void       mousepad_setting_disconnect     (const gchar        *setting,
                                            GCallback           callback,
                                            gpointer            user_data);

/* functions for reading and writing settings */

void       mousepad_setting_reset          (const gchar        *setting);

void       mousepad_setting_get            (const gchar        *setting,
                                            const gchar        *format_string,
                                            ...);

void       mousepad_setting_set            (const gchar        *setting,
                                            const gchar        *format_string,
                                            ...);

/* convenience functions for reading/writing common types */

gboolean   mousepad_setting_get_boolean    (const gchar        *setting);

void       mousepad_setting_set_boolean    (const gchar        *setting,
                                            gboolean            value);

gint       mousepad_setting_get_int        (const gchar        *setting);

void       mousepad_setting_set_int        (const gchar        *setting,
                                            gint                value);

guint      mousepad_setting_get_uint       (const gchar        *setting);

void       mousepad_setting_set_uint       (const gchar        *setting,
                                            guint               value);

gchar     *mousepad_setting_get_string     (const gchar        *setting);

void       mousepad_setting_set_string     (const gchar        *setting,
                                            const gchar        *value);

gchar    **mousepad_setting_get_strv       (const gchar        *setting);

void       mousepad_setting_set_strv       (const gchar        *setting,
                                            const gchar *const *value);

gint       mousepad_setting_get_enum       (const gchar        *setting);

void       mousepad_setting_set_enum       (const gchar        *setting,
                                            gint                value);

GVariant  *mousepad_setting_get_variant    (const gchar        *setting);

void       mousepad_setting_set_variant    (const gchar        *setting,
                                            GVariant           *variant);

/* wrappers for above read/write functions with shorter arguments */

#define MOUSEPAD_SETTING_BIND(setting, object, prop, flags) \
  mousepad_setting_bind (MOUSEPAD_SETTING_##setting, object, prop, flags)

#define MOUSEPAD_SETTING_CONNECT(setting, callback, user_data, connect_flags) \
  mousepad_setting_connect (MOUSEPAD_SETTING_##setting, G_CALLBACK (callback), user_data, connect_flags)

#define MOUSEPAD_SETTING_CONNECT_OBJECT(setting, callback, object, connect_flags) \
  mousepad_setting_connect_object (MOUSEPAD_SETTING_##setting, G_CALLBACK (callback), object, connect_flags)

#define MOUSEPAD_SETTING_DISCONNECT(setting, callback, user_data) \
  mousepad_setting_disconnect (MOUSEPAD_SETTING_##setting, G_CALLBACK (callback), user_data)

#define MOUSEPAD_SETTING_RESET(setting) mousepad_setting_reset (MOUSEPAD_SETTING_##setting)

#define MOUSEPAD_SETTING_GET(setting, ...)           mousepad_setting_get (MOUSEPAD_SETTING_##setting, __VA_ARGS__)
#define MOUSEPAD_SETTING_GET_BOOLEAN(setting)        mousepad_setting_get_boolean (MOUSEPAD_SETTING_##setting)
#define MOUSEPAD_SETTING_GET_INT(setting)            mousepad_setting_get_int (MOUSEPAD_SETTING_##setting)
#define MOUSEPAD_SETTING_GET_UINT(setting)           mousepad_setting_get_uint (MOUSEPAD_SETTING_##setting)
#define MOUSEPAD_SETTING_GET_STRING(setting)         mousepad_setting_get_string (MOUSEPAD_SETTING_##setting)
#define MOUSEPAD_SETTING_GET_STRV(setting)           mousepad_setting_get_strv (MOUSEPAD_SETTING_##setting)
#define MOUSEPAD_SETTING_GET_ENUM(setting)           mousepad_setting_get_enum (MOUSEPAD_SETTING_##setting)

#define MOUSEPAD_SETTING_SET(setting, ...)           mousepad_setting_set (MOUSEPAD_SETTING_##setting, __VA_ARGS__)
#define MOUSEPAD_SETTING_SET_BOOLEAN(setting, value) mousepad_setting_set_boolean (MOUSEPAD_SETTING_##setting, value)
#define MOUSEPAD_SETTING_SET_INT(setting, value)     mousepad_setting_set_int (MOUSEPAD_SETTING_##setting, value)
#define MOUSEPAD_SETTING_SET_UINT(setting, value)    mousepad_setting_set_uint (MOUSEPAD_SETTING_##setting, value)
#define MOUSEPAD_SETTING_SET_STRING(setting, value)  mousepad_setting_set_string (MOUSEPAD_SETTING_##setting, value)
#define MOUSEPAD_SETTING_SET_STRV(setting, value)    mousepad_setting_set_strv (MOUSEPAD_SETTING_##setting, value)
#define MOUSEPAD_SETTING_SET_ENUM(setting, value)    mousepad_setting_set_enum (MOUSEPAD_SETTING_##setting, value)

G_END_DECLS

#endif /* __MOUSEPAD_SETTINGS_H__ */
