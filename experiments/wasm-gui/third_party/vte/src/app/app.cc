/*
 * Copyright © 2001,2002 Red Hat, Inc.
 * Copyright © 2014, 2017 Christian Persch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>
#include <locale.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include <glib-unix.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <cairo/cairo-gobject.h>
#include <vte/vte.h>

#include <algorithm>
#include <vector>

#include "std-glue.hh"
#include "cairo-glue.hh"
#include "glib-glue.hh"
#include "gtk-glue.hh"
#include "libc-glue.hh"
#include "pango-glue.hh"
#include "pcre2-glue.hh"
#include "refptr.hh"
#include "vte-glue.hh"

/* options */

class Options {
public:
        gboolean allow_window_ops{false};
        gboolean audible_bell{false};
        gboolean backdrop{false};
        gboolean bold_is_bright{false};
        gboolean console{false};
        gboolean debug{false};
        gboolean feed_stdin{false};
        gboolean icon_title{false};
        gboolean keep{false};
        gboolean no_bidi{false};
        gboolean no_bold{false};
        gboolean no_builtin_dingus{false};
        gboolean no_context_menu{false};
        gboolean no_decorations{false};
        gboolean no_fallback_scrolling{false};
        gboolean no_geometry_hints{false};
        gboolean no_hyperlink{false};
        gboolean no_pty{false};
        gboolean no_rewrap{false};
        gboolean no_scrollbar{false};
        gboolean no_shaping{false};
        gboolean no_shell{false};
        gboolean no_systemd_scope{false};
        gboolean no_xfill{false};
        gboolean no_yfill{false};
        gboolean object_notifications{false};
        gboolean require_systemd_scope{false};
        gboolean reverse{false};
        gboolean scroll_unit_is_pixels{false};
        gboolean test_mode{false};
        gboolean track_clipboard_targets{false};
        gboolean use_scrolled_window{false};
        gboolean use_theme_colors{false};
        gboolean version{false};
        gboolean whole_window_transparent{false};
        bool bg_color_set{false};
        bool fg_color_set{false};
        bool cursor_bg_color_set{false};
        bool cursor_fg_color_set{false};
        bool hl_bg_color_set{false};
        bool hl_fg_color_set{false};
        cairo_extend_t background_extend{CAIRO_EXTEND_NONE};
        char* command{nullptr};
        char* encoding{nullptr};
        char* font_string{nullptr};
        char* geometry{nullptr};
        char* output_filename{nullptr};
        char* title{nullptr};
        char* word_char_exceptions{nullptr};
        char* working_directory{nullptr};
        char** dingus{nullptr};
        char** exec_argv{nullptr};
        char** environment{nullptr};
        GdkPixbuf* background_pixbuf{nullptr};
        GdkRGBA bg_color{1.0, 1.0, 1.0, 1.0};
        GdkRGBA fg_color{0.0, 0.0, 0.0, 1.0};
        GdkRGBA cursor_bg_color{};
        GdkRGBA cursor_fg_color{};
        GdkRGBA hl_bg_color{};
        GdkRGBA hl_fg_color{};
        int cjk_ambiguous_width{1};
        int extra_margin{-1};
        int scrollback_lines{-1 /* infinite */};
        int transparency_percent{-1};
        int verbosity{0};
        double cell_height_scale{1.0};
        double cell_width_scale{1.0};
        VteCursorBlinkMode cursor_blink_mode{VTE_CURSOR_BLINK_SYSTEM};
        VteCursorShape cursor_shape{VTE_CURSOR_SHAPE_BLOCK};
        VteTextBlinkMode text_blink_mode{VTE_TEXT_BLINK_ALWAYS};
        vte::glib::RefPtr<GtkCssProvider> css{};

#if VTE_GTK == 3
        gboolean no_argb_visual{false};
        gboolean no_double_buffer{false};
#endif /* VTE_GTK == 3 */

        ~Options() {
                g_clear_object(&background_pixbuf);
                g_free(command);
                g_free(encoding);
                g_free(font_string);
                g_free(geometry);
                g_free(output_filename);
                g_free(word_char_exceptions);
                g_free(working_directory);
                g_strfreev(dingus);
                g_strfreev(exec_argv);
                g_strfreev(environment);
        }

        auto fds()
        {
                auto fds = std::vector<int>{};
                fds.reserve(m_fds.size());
                for (auto& fd : m_fds)
                        fds.emplace_back(fd.get());

                return fds;
        }

        auto map_fds()
        {
                return m_map_fds;
        }

        auto environment_for_spawn() const noexcept
        {
                auto envv = g_get_environ();

                // Merge in extra variables
                if (environment) {
                        for (auto i = 0; environment[i]; ++i) {
                                auto const eq = strchr(environment[i], '=');
                                if (eq) {
                                        auto const var = vte::glib::take_string(g_strndup(environment[i], eq - environment[i]));
                                        envv = g_environ_setenv(envv, var.get(), eq + 1, true);
                                } else {
                                        envv = g_environ_unsetenv(envv, environment[i]);
                                }
                        }
                }

                // Cleanup environment
                // List of variables and prefixes copied from gnome-terminal.
                for (auto const& var : {"COLORTERM",
                                        "COLUMNS",
                                        "DESKTOP_STARTUP_ID",
                                        "EXIT_CODE",
                                        "EXIT_STATUS",
                                        "GIO_LAUNCHED_DESKTOP_FILE",
                                        "GIO_LAUNCHED_DESKTOP_FILE_PID",
                                        "GJS_DEBUG_OUTPUT",
                                        "GJS_DEBUG_TOPICS",
                                        "GNOME_DESKTOP_ICON",
                                        "INVOCATION_ID",
                                        "JOURNAL_STREAM",
                                        "LINES",
                                        "LISTEN_FDNAMES",
                                        "LISTEN_FDS",
                                        "LISTEN_PID",
                                        "MAINPID",
                                        "MANAGERPID",
                                        "NOTIFY_SOCKET",
                                        "NOTIFY_SOCKET",
                                        "PIDFILE",
                                        "PWD",
                                        "REMOTE_ADDR",
                                        "REMOTE_PORT",
                                        "SERVICE_RESULT",
                                        "SHLVL",
                                        "TERM",
                                        "VTE_VERSION",
                                        "WATCHDOG_PID",
                                        "WATCHDOG_USEC",
                                        "WINDOWID"}) {
                        envv = g_environ_unsetenv(envv, var);
                }

                for (auto const& prefix : {"GNOME_TERMINAL_",

                                           // other terminals
                                           "FOOT_",
                                           "ITERM2_",
                                           "MC_",
                                           "MINTTY_",
                                           "PUTTY_",
                                           "RXVT_",
                                           "TERM_",
                                           "URXVT_",
                                           "WEZTERM_",
                                           "XTERM_"}) {
                        for (auto i = 0; envv[i]; ++i) {
                                if (!g_str_has_prefix (envv[i], prefix))
                                        continue;

                                auto const eq = strchr(envv[i], '=');
                                g_assert(eq);
                                auto const var = vte::glib::take_string(g_strndup(envv[i], eq - envv[i]));
                                envv = g_environ_unsetenv(envv, var.get());
                        }
                }

                return vte::glib::take_strv(envv);
        }

private:

        std::vector<vte::libc::FD> m_fds{};
        std::vector<int> m_map_fds{};

        bool parse_enum(GType type,
                        char const* str,
                        int& value,
                        GError** error)
        {
                GEnumClass* enum_klass = reinterpret_cast<GEnumClass*>(g_type_class_ref(type));
                GEnumValue* enum_value = g_enum_get_value_by_nick(enum_klass, str);

                if (enum_value == nullptr) {
                        g_type_class_unref(enum_klass);
                        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                    "Failed to parse enum value \"%s\" as type \"%s\"",
                                    str, g_quark_to_string(g_type_qname(type)));
                        return false;
                }

                value = enum_value->value;
                g_type_class_unref(enum_klass);
                return true;
        }

        bool parse_width_enum(char const* str,
                              int& value,
                              GError** error)
        {
                int v = 1;
                if (str == nullptr || g_str_equal(str, "narrow"))
                        v = 1;
                else if (g_str_equal(str, "wide"))
                        v = 2;
                else {
                        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                    "Failed to parse \"%s\" as width (allowed values are \"narrow\" or \"wide\")", str);
                        return false;
                }

                value = v;
                return true;
        }

        bool parse_color(char const* str,
                         GdkRGBA* value,
                         bool* value_set,
                         GError** error)
        {
                GdkRGBA color;
                if (!gdk_rgba_parse(&color, str)) {
                        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                    "Failed to parse \"%s\" as color", str);
                        *value_set = false;
                        return false;
                }

                *value = color;
                *value_set = true;
                return true;
        }

        int parse_fd_arg(char const* arg,
                         char** end_ptr,
                         GError** error)
        {
                errno = 0;
                char* end = nullptr;
                auto const v = g_ascii_strtoll(arg, &end, 10);
                if (errno || end == arg || v < G_MININT || v > G_MAXINT) {
                        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                     "Failed to parse \"%s\" as file descriptor number", arg);
                        return -1;
                }
                if (v == -1) {
                        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                     "\"%s\" is not a valid file descriptor number", arg);
                        return -1;
                }

                if (end_ptr) {
                        *end_ptr = end;
                } else if (*end) {
                        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                     "Extra characters after number in \"%s\"", arg);
                        return -1;
                }

                return int(v);
        }

        bool parse_fd_arg(char const* str,
                          GError** error)
        {
                char *end = nullptr;
                auto fd = parse_fd_arg(str, &end, error);
                if (fd == -1)
                        return FALSE;

                auto map_to = int{};
                if (*end == '=' || *end == ':') {
                        map_to = parse_fd_arg(end + 1, nullptr, error);
                        if (map_to == -1)
                                return false;

                        if (map_to == STDIN_FILENO ||
                            map_to == STDOUT_FILENO ||
                            map_to == STDERR_FILENO) {
                                g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                             "Cannot map file descriptor to %d (reserved)", map_to);
                                return false;
                        }
                } else if (*end) {
                        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                     "Failed to parse \"%s\" as file descriptor assignment", str);
                        return false;
                } else {
                        map_to = fd;
                }

                /* N:M assigns, N=M assigns a dup of N. Always dup stdin/out/err since
                 * we need to output messages ourself there, too.
                 */
                auto new_fd = int{};
                if (*end == '=' || fd < 3) {
                        new_fd = vte::libc::fd_dup_cloexec(fd, 3);
                        if (new_fd == -1) {
                                auto errsv = vte::libc::ErrnoSaver{};
                                g_set_error (error, G_IO_ERROR, g_io_error_from_errno(errsv),
                                             "Failed to duplicate file descriptor %d: %s",
                                             fd, g_strerror(errsv));
                                return false;
                        }
                } else {
                        new_fd = fd;
                        if (vte::libc::fd_set_cloexec(fd) == -1) {
                                auto errsv = vte::libc::ErrnoSaver{};
                                g_set_error (error, G_IO_ERROR, g_io_error_from_errno(errsv),
                                             "Failed to set cloexec on file descriptor %d: %s",
                                             fd, g_strerror(errsv));
                                return false;
                        }
                }

                m_fds.emplace_back(new_fd);
                m_map_fds.emplace_back(map_to);
                return true;
        }

        bool parse_geometry(char const* str,
                            GError** error)
        {
                g_free(geometry);
                geometry = g_strdup(str);
                return true;
        }

        static gboolean
        parse_background_image(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                g_clear_object(&that->background_pixbuf);
                that->background_pixbuf = gdk_pixbuf_new_from_file(value, error);
                return that->background_pixbuf != nullptr;
        }

        static gboolean
        parse_background_extend(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                int v;
                auto rv = that->parse_enum(CAIRO_GOBJECT_TYPE_EXTEND, value, v, error);
                if (rv)
                        that->background_extend = cairo_extend_t(v);
                return rv;
        }

        static gboolean
        parse_cjk_width(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                return that->parse_width_enum(value, that->cjk_ambiguous_width, error);
        };

        static gboolean
        parse_cursor_blink(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                int v;
                auto rv = that->parse_enum(VTE_TYPE_CURSOR_BLINK_MODE, value, v, error);
                if (rv)
                        that->cursor_blink_mode = VteCursorBlinkMode(v);
                return rv;
        }

        static gboolean
        parse_cursor_shape(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                int v;
                auto rv = that->parse_enum(VTE_TYPE_CURSOR_SHAPE, value, v, error);
                if (rv)
                        that->cursor_shape = VteCursorShape(v);
                return rv;
        }

        static gboolean
        parse_bg_color(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                bool set;
                return that->parse_color(value, &that->bg_color, &set, error);
        }

#if VTE_GTK == 4
        static void
        parse_css_error_cb(GtkCssProvider* provider,
                           void* section,
                           GError* error,
                           GError** ret_error) noexcept
        {
                if (error)
                        *ret_error = g_error_copy(error);
        }
#endif /* VTE_GTK == 4 */

        static gboolean
        parse_css_file(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);

                auto css = vte::glib::take_ref(gtk_css_provider_new());
#if VTE_GTK == 3
                if (!gtk_css_provider_load_from_path(css.get(), value, error))
                    return false;

#elif VTE_GTK == 4
                GError* err = nullptr;
                auto const id = g_signal_connect(css.get(), "parsing-error",
                                                 G_CALLBACK(parse_css_error_cb), &err);

                gtk_css_provider_load_from_path(css.get(), value);
                g_signal_handler_disconnect(css.get(), id);
                if (err) {
                        g_propagate_prefixed_error(error, err,
                                                   "Error parsing CSS file \"%s\": ",
                                                   value);
                        return false;
                }
#endif /* VTE_GTK */

                that->css = std::move(css);
                return true;
        }

        static gboolean
        parse_fd(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                return that->parse_fd_arg(value, error);
        }

        static gboolean
        parse_fg_color(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                bool set;
                return that->parse_color(value, &that->fg_color, &set, error);
        }

        static gboolean
        parse_cursor_bg_color(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                return that->parse_color(value, &that->cursor_bg_color, &that->cursor_bg_color_set, error);
        }

        static gboolean
        parse_cursor_fg_color(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                return that->parse_color(value, &that->cursor_fg_color, &that->cursor_fg_color_set, error);
        }

        static gboolean
        parse_hl_bg_color(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                return that->parse_color(value, &that->hl_bg_color, &that->hl_bg_color_set, error);
        }

        static gboolean
        parse_hl_fg_color(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                return that->parse_color(value, &that->hl_fg_color, &that->hl_fg_color_set, error);
        }

        static gboolean
        parse_text_blink(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                int v;
                auto rv = that->parse_enum(VTE_TYPE_TEXT_BLINK_MODE, value, v, error);
                if (rv)
                        that->text_blink_mode = VteTextBlinkMode(v);
                return rv;
        }

        static gboolean
        parse_verbosity(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                that->verbosity++;
                return true;
        }

public:

        double get_alpha() const
        {
                return double(100 - CLAMP(transparency_percent, 0, 100)) / 100.0;
        }

        double get_alpha_bg() const
        {
                double alpha;
                if (background_pixbuf != nullptr)
                        alpha = 0.0;
                else if (whole_window_transparent)
                        alpha = 1.0;
                else
                        alpha = get_alpha();

                return alpha;
        }

        double get_alpha_bg_for_draw() const
        {
                double alpha;
                if (whole_window_transparent)
                        alpha = 1.0;
                else
                        alpha = get_alpha();

                return alpha;
        }

        GdkRGBA get_color_bg() const
        {
                GdkRGBA color{bg_color};
                color.alpha = get_alpha_bg();
                return color;
        }

        GdkRGBA get_color_fg() const
        {
                return fg_color;
        }

        bool parse_argv(int argc,
                        char* argv[],
                        GError** error)
        {
                char* dummy_string = nullptr;
                GOptionEntry const entries[] = {
                        { "allow-window-ops", 0, 0, G_OPTION_ARG_NONE, &allow_window_ops,
                          "Allow window operations (resize, move, raise/lower, (de)iconify)", nullptr },
                        { "audible-bell", 'a', 0, G_OPTION_ARG_NONE, &audible_bell,
                          "Use audible terminal bell", nullptr },
                        { "backdrop", 0, 0,G_OPTION_ARG_NONE, &backdrop,
                          "Dim when toplevel unfocused", nullptr },
                        { "background-color", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_bg_color,
                          "Set default background color", "COLOR" },
                        { "background-image", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_background_image,
                          "Set background image from file", "FILE" },
                        { "background-extend", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_background_extend,
                          "Set background image extend", "EXTEND" },
                        { "blink", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_text_blink,
                          "Text blink mode (never|focused|unfocused|always)", "MODE" },
                        { "bold-is-bright", 'B', 0, G_OPTION_ARG_NONE, &bold_is_bright,
                          "Bold also brightens colors", nullptr },
                        { "cell-height-scale", 0, 0, G_OPTION_ARG_DOUBLE, &cell_height_scale,
                          "Add extra line spacing", "1.0..2.0" },
                        { "cell-width-scale", 0, 0, G_OPTION_ARG_DOUBLE, &cell_width_scale,
                          "Add extra letter spacing", "1.0..2.0" },
                        { "cjk-width", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_cjk_width,
                          "Specify the cjk ambiguous width to use for UTF-8 encoding", "NARROW|WIDE" },
                        { "cursor-blink", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_cursor_blink,
                          "Cursor blink mode (system|on|off)", "MODE" },
                        { "cursor-background-color", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_cursor_bg_color,
                          "Enable a colored cursor background", "COLOR" },
                        { "cursor-foreground-color", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_cursor_fg_color,
                          "Enable a colored cursor foreground", "COLOR" },
                        { "cursor-shape", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_cursor_shape,
                          "Set cursor shape (block|underline|ibeam)", nullptr },
                        { "css-file", 0, G_OPTION_FLAG_FILENAME, G_OPTION_ARG_CALLBACK, (void*)parse_css_file,
                          "Load CSS from FILE", "FILE" },
                        { "dingu", 'D', 0, G_OPTION_ARG_STRING_ARRAY, &dingus,
                          "Add regex highlight", nullptr },
                        { "debug", 'd', 0,G_OPTION_ARG_NONE, &debug,
                          "Enable various debugging checks", nullptr },
                        { "encoding", 0, 0, G_OPTION_ARG_STRING, &encoding,
                          "Specify the terminal encoding to use", "ENCODING" },
                        { "env", 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &environment,
                          "Add environment variable to the child\'s environment", "VAR=VALUE" },
                        { "extra-margin", 0, 0, G_OPTION_ARG_INT, &extra_margin,
                          "Add extra margin around the terminal widget", "MARGIN" },
                        { "fd", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_fd,
                          "Pass file descriptor N (as M) to the child process", "N[:M]|N[=]M]" },
                        { "feed-stdin", 'B', 0, G_OPTION_ARG_NONE, &feed_stdin,
                          "Feed input to the terminal", nullptr },
                        { "font", 'f', 0, G_OPTION_ARG_STRING, &font_string,
                          "Specify a font to use", nullptr },
                        { "foreground-color", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_fg_color,
                          "Set default foreground color", "COLOR" },
                        { "geometry", 'g', 0, G_OPTION_ARG_STRING, &geometry,
                          "Set the size (in characters) and position", "GEOMETRY" },
                        { "highlight-background-color", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_hl_bg_color,
                          "Enable distinct highlight background color for selection", "COLOR" },
                        { "highlight-foreground-color", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_hl_fg_color,
                          "Enable distinct highlight foreground color for selection", "COLOR" },
                        { "icon-title", 'i', 0, G_OPTION_ARG_NONE, &icon_title,
                          "Enable the setting of the icon title", nullptr },
                        { "keep", 'k', 0, G_OPTION_ARG_NONE, &keep,
                          "Live on after the command exits", nullptr },
                        { "no-bidi", 0, 0, G_OPTION_ARG_NONE, &no_bidi,
                          "Disable BiDi", nullptr },
                        { "no-bold", 0, 0, G_OPTION_ARG_NONE, &no_bold,
                          "Disable bold", nullptr },
                        { "no-builtin-dingus", 0, 0, G_OPTION_ARG_NONE, &no_builtin_dingus,
                          "Highlight URLs inside the terminal", nullptr },
                        { "no-context-menu", 0, 0, G_OPTION_ARG_NONE, &no_context_menu,
                          "Disable context menu", nullptr },
                        { "no-decorations", 0, 0, G_OPTION_ARG_NONE, &no_decorations,
                          "Disable window decorations", nullptr },
                        { "no-fallback-scrolling", 0, 0, G_OPTION_ARG_NONE, &no_fallback_scrolling,
                          "Disable fallback scrolling", nullptr },
                        { "no-geometry-hints", 'G', 0, G_OPTION_ARG_NONE, &no_geometry_hints,
                          "Allow the terminal to be resized to any dimension, not constrained to fit to an integer multiple of characters", nullptr },
                        { "no-hyperlink", 'H', 0, G_OPTION_ARG_NONE, &no_hyperlink,
                          "Disable hyperlinks", nullptr },
                        { "no-pty", 0, 0, G_OPTION_ARG_NONE, &no_pty,
                          "Disable PTY creation with --no-shell", nullptr },
                        { "no-rewrap", 'R', 0, G_OPTION_ARG_NONE, &no_rewrap,
                          "Disable rewrapping on resize", nullptr },
                        { "no-scrollbar", 0, 0, G_OPTION_ARG_NONE, &no_scrollbar,
                          "Disable scrollbar", nullptr },
                        { "no-shaping", 0, 0, G_OPTION_ARG_NONE, &no_shaping,
                          "Disable Arabic shaping", nullptr },
                        { "no-shell", 'S', 0, G_OPTION_ARG_NONE, &no_shell,
                          "Disable spawning a shell inside the terminal", nullptr },
                        { "no-systemd-scope", 0, 0, G_OPTION_ARG_NONE, &no_systemd_scope,
                          "Don't use systemd user scope", nullptr },
                        { "object-notifications", 'N', 0, G_OPTION_ARG_NONE, &object_notifications,
                          "Print VteTerminal object notifications", nullptr },
                        { "output-file", 0, 0, G_OPTION_ARG_FILENAME, &output_filename,
                          "Save terminal contents to file at exit", nullptr },
                        { "reverse", 0, 0, G_OPTION_ARG_NONE, &reverse,
                          "Reverse foreground/background colors", nullptr },
                        { "require-systemd-scope", 0, 0, G_OPTION_ARG_NONE, &require_systemd_scope,
                          "Require use of a systemd user scope", nullptr },
                        { "scroll-unit-is-pixels", 0, 0, G_OPTION_ARG_NONE, &scroll_unit_is_pixels,
                          "Use pixels as scroll unit", nullptr },
                        { "scrollback-lines", 'n', 0, G_OPTION_ARG_INT, &scrollback_lines,
                          "Specify the number of scrollback-lines (-1 for infinite)", nullptr },
                        { "title", 0, 0, G_OPTION_ARG_STRING, &title, "Set the initial title of the window", "TITLE" },
                        { "transparent", 'T', 0, G_OPTION_ARG_INT, &transparency_percent,
                          "Enable the use of a transparent background", "0..100" },
                        { "track-clipboard-targets", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &track_clipboard_targets,
                          "Track clipboard targets", nullptr },
                        { "verbose", 'v', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
                          (void*)parse_verbosity,
                          "Enable verbose debug output", nullptr },
                        { "version", 0, 0, G_OPTION_ARG_NONE, &version,
                          "Show version", nullptr },
                        { "whole-window-transparent", 0, 0, G_OPTION_ARG_NONE, &whole_window_transparent,
                          "Make the whole window transparent", NULL },
                        { "word-char-exceptions", 0, 0, G_OPTION_ARG_STRING, &word_char_exceptions,
                          "Specify the word char exceptions", "CHARS" },
                        { "working-directory", 'w', 0, G_OPTION_ARG_FILENAME, &working_directory,
                          "Specify the initial working directory of the terminal", nullptr },

                        /* Options for compatibility with the old vteapp test application */
                        { "border-width", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_INT, &extra_margin,
                          nullptr, nullptr },
                        { "command", 'c', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_FILENAME, &command,
                          nullptr, nullptr },
                        { "console", 'C', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &console,
                          nullptr, nullptr },
                        { "pty-flags", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &dummy_string,
                          nullptr, nullptr },
                        { "scrollbar-policy", 'P', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING,
                          &dummy_string, nullptr, nullptr },
                        { "scrolled-window", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,
                          &use_scrolled_window,
                          "Use a GtkScrolledWindow", nullptr },
                        { "shell", 'S', G_OPTION_FLAG_REVERSE | G_OPTION_FLAG_HIDDEN,
                          G_OPTION_ARG_NONE, &no_shell, nullptr, nullptr },
#ifdef VTE_DEBUG
                        { "test-mode", 0, 0, G_OPTION_ARG_NONE, &test_mode,
                          "Enable test mode", nullptr },
#endif
                        { "use-theme-colors", 0, 0, G_OPTION_ARG_NONE, &use_theme_colors,
                          "Use foreground and background colors from the gtk+ theme", nullptr },
#if VTE_GTK == 3
                        { "no-argb-visual", 0, 0, G_OPTION_ARG_NONE, &no_argb_visual,
                          "Don't use an ARGB visual", nullptr },
                        { "double-buffer", '2', G_OPTION_FLAG_REVERSE | G_OPTION_FLAG_HIDDEN,
                          G_OPTION_ARG_NONE, &no_double_buffer, nullptr, nullptr },
                        { "no-double-buffer", '2', 0, G_OPTION_ARG_NONE, &no_double_buffer,
                          "Disable double-buffering", nullptr },
#endif /* VTE_GTK == 3 */
                        { nullptr }
                };

                /* Look for '--' */
                for (int i = 0; i < argc; i++) {
                        if (!g_str_equal(argv[i], "--"))
                                continue;

                        i++; /* skip it */
                        if (i == argc) {
                                g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                                            "No command specified after -- terminator");
                                return false;
                        }

                        exec_argv = g_new(char*, argc - i + 1);
                        int j;
                        for (j = 0; i < argc; i++, j++)
                                exec_argv[j] = g_strdup(argv[i]);
                        exec_argv[j] = nullptr;
                        break;
                }

                auto context = vte::take_freeable
                        (g_option_context_new("[-- COMMAND …] — VTE test application"));
                g_option_context_set_help_enabled(context.get(), true);
                g_option_context_set_translation_domain(context.get(), GETTEXT_PACKAGE);

                auto group = g_option_group_new(nullptr, nullptr, nullptr, this, nullptr);
                g_option_group_set_translation_domain(group, GETTEXT_PACKAGE);
                g_option_group_add_entries(group, entries);
                g_option_context_set_main_group(context.get(), group);

#if VTE_GTK == 3
                g_option_context_add_group(context.get(), gtk_get_option_group(true));
#endif

                bool rv = g_option_context_parse(context.get(), &argc, &argv, error);

                g_free(dummy_string);

                if (reverse) {
                        using std::swap;
                        swap(fg_color, bg_color);
                }

                if (use_scrolled_window) {
                        no_geometry_hints = true;
                        no_scrollbar = true;
                }

#if VTE_GTK == 4
                if (rv && !gtk_init_check()) {
                        g_set_error_literal(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                                            "Failed to initialise gtk+");
                        rv = false;
                }
#endif /* VTE_GTK == 4 */

                return rv;
        }
}; // class Options

Options options{}; /* global */

/* debug output */

static void G_GNUC_PRINTF(2, 3)
verbose_fprintf(FILE* fp,
                char const* format,
                ...)
{
        if (options.verbosity == 0)
                return;

        va_list args;
        va_start(args, format);
        g_vfprintf(fp, format, args);
        va_end(args);
}

#define verbose_print(...) verbose_fprintf(stdout, __VA_ARGS__)
#define verbose_printerr(...) verbose_fprintf(stderr, __VA_ARGS__)

/* regex */

static void
jit_regex(VteRegex* regex,
          char const* pattern)
{
        auto error = vte::glib::Error{};
        if (!vte_regex_jit(regex, PCRE2_JIT_COMPLETE, error) ||
            !vte_regex_jit(regex, PCRE2_JIT_PARTIAL_SOFT, error)) {
                if (!error.matches(VTE_REGEX_ERROR, -45 /* PCRE2_ERROR_JIT_BADOPTION: JIT not supported */))
                        verbose_printerr("JITing regex \"%s\" failed: %s\n", pattern, error.message());
        }
}

static vte::Freeable<VteRegex>
compile_regex_for_search(char const* pattern,
                         bool caseless,
                         GError** error)
{
        uint32_t flags = PCRE2_UTF | PCRE2_NO_UTF_CHECK | PCRE2_MULTILINE;
        if (caseless)
                flags |= PCRE2_CASELESS;

        auto regex = vte_regex_new_for_search(pattern, strlen(pattern), flags, error);
        if (regex != nullptr)
                jit_regex(regex, pattern);

        return vte::take_freeable(regex);
}

static vte::Freeable<VteRegex>
compile_regex_for_match(char const* pattern,
                        bool caseless,
                        GError** error)
{
        uint32_t flags = PCRE2_UTF | PCRE2_NO_UTF_CHECK | PCRE2_MULTILINE;
        if (caseless)
                flags |= PCRE2_CASELESS;

        auto regex = vte_regex_new_for_match(pattern, strlen(pattern), flags, error);
        if (regex != nullptr)
                jit_regex(regex, pattern);

        return vte::take_freeable(regex);
}

/* search popover */

#define VTEAPP_TYPE_SEARCH_POPOVER         (vteapp_search_popover_get_type())
#define VTEAPP_SEARCH_POPOVER(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), VTEAPP_TYPE_SEARCH_POPOVER, VteappSearchPopover))
#define VTEAPP_SEARCH_POPOVER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), VTEAPP_TYPE_SEARCH_POPOVER, VteappSearchPopoverClass))
#define VTEAPP_IS_SEARCH_POPOVER(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), VTEAPP_TYPE_SEARCH_POPOVER))
#define VTEAPP_IS_SEARCH_POPOVER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), VTEAPP_TYPE_SEARCH_POPOVER))
#define VTEAPP_SEARCH_POPOVER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), VTEAPP_TYPE_SEARCH_POPOVER, VteappSearchPopoverClass))

typedef struct _VteappSearchPopover        VteappSearchPopover;
typedef struct _VteappSearchPopoverClass   VteappSearchPopoverClass;

struct _VteappSearchPopover {
        GtkPopover parent;

        /* from GtkWidget template */
        GtkWidget* search_entry;
        GtkWidget* search_prev_button;
        GtkWidget* search_next_button;
        GtkWidget* close_button;
        GtkToggleButton* match_case_checkbutton;
        GtkToggleButton* entire_word_checkbutton;
        GtkToggleButton* regex_checkbutton;
        GtkToggleButton* wrap_around_checkbutton;
        GtkWidget* reveal_button;
        GtkWidget* revealer;
        /* end */

        VteTerminal *terminal;
        bool regex_caseless;
        bool has_regex;
        char* regex_pattern;
};

struct _VteappSearchPopoverClass {
        GtkPopoverClass parent;
};

static GType vteapp_search_popover_get_type(void);

enum {
        SEARCH_POPOVER_PROP0,
        SEARCH_POPOVER_PROP_TERMINAL,
        SEARCH_POPOVER_N_PROPS
};

static GParamSpec* search_popover_pspecs[SEARCH_POPOVER_N_PROPS];

static void
vteapp_search_popover_update_sensitivity(VteappSearchPopover* popover)
{
        bool can_search = popover->has_regex;

        gtk_widget_set_sensitive(popover->search_next_button, can_search);
        gtk_widget_set_sensitive(popover->search_prev_button, can_search);
}

static void
vteapp_search_popover_update_regex(VteappSearchPopover* popover)
{
#if VTE_GTK == 3
        auto search_text = gtk_entry_get_text(GTK_ENTRY(popover->search_entry));
#elif VTE_GTK == 4
        auto search_text = gtk_editable_get_text(GTK_EDITABLE(popover->search_entry));
#endif /* VTE_GTK */
        bool caseless = gtk_toggle_button_get_active(popover->match_case_checkbutton) == FALSE;

        char* pattern;
        if (gtk_toggle_button_get_active(popover->regex_checkbutton))
                pattern = g_strdup(search_text);
        else
                pattern = g_regex_escape_string(search_text, -1);

        if (gtk_toggle_button_get_active(popover->regex_checkbutton)) {
                char* tmp = g_strdup_printf("\\b%s\\b", pattern);
                g_free(pattern);
                pattern = tmp;
        }

        if (caseless == popover->regex_caseless &&
            g_strcmp0(pattern, popover->regex_pattern) == 0)
                return;

        popover->regex_caseless = caseless;
        g_free(popover->regex_pattern);
        popover->regex_pattern = nullptr;

        if (search_text[0] != '\0') {
                auto error = vte::glib::Error{};
                auto regex = compile_regex_for_search(pattern, caseless, error);
                vte_terminal_search_set_regex(popover->terminal, regex.get(), 0);

                if (error.error()) {
                        popover->has_regex = false;
                        popover->regex_pattern = pattern; /* adopt */
                        pattern = nullptr; /* adopted */
                        gtk_widget_set_tooltip_text(popover->search_entry, nullptr);
                } else {
                        popover->has_regex = true;
                        gtk_widget_set_tooltip_text(popover->search_entry, error.message());
                }
        }

        g_free(pattern);

        vteapp_search_popover_update_sensitivity(popover);
}

static void
vteapp_search_popover_wrap_around_toggled(GtkToggleButton* button,
                                          VteappSearchPopover* popover)
{
        vte_terminal_search_set_wrap_around(popover->terminal, gtk_toggle_button_get_active(button));
}

static void
vteapp_search_popover_search_forward(VteappSearchPopover* popover)
{
        if (!popover->has_regex)
                return;
        vte_terminal_search_find_next(popover->terminal);
}

static void
vteapp_search_popover_search_backward(VteappSearchPopover* popover)
{
        if (!popover->has_regex)
                return;
        vte_terminal_search_find_previous(popover->terminal);
}

G_DEFINE_TYPE(VteappSearchPopover, vteapp_search_popover, GTK_TYPE_POPOVER)

static void
vteapp_search_popover_init(VteappSearchPopover* popover)
{
        gtk_widget_init_template(GTK_WIDGET(popover));

        popover->regex_caseless = false;
        popover->has_regex = false;
        popover->regex_pattern = nullptr;

        g_signal_connect_swapped(popover->close_button, "clicked", G_CALLBACK(gtk_widget_hide), popover);

        g_object_bind_property(popover->reveal_button, "active",
                               popover->revealer, "reveal-child",
                               GBindingFlags(G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE));

        g_signal_connect_swapped(popover->search_entry, "next-match", G_CALLBACK(vteapp_search_popover_search_forward), popover);
        g_signal_connect_swapped(popover->search_entry, "previous-match", G_CALLBACK(vteapp_search_popover_search_backward), popover);
        g_signal_connect_swapped(popover->search_entry, "search-changed", G_CALLBACK(vteapp_search_popover_update_regex), popover);

        g_signal_connect_swapped(popover->search_next_button,"clicked", G_CALLBACK(vteapp_search_popover_search_forward), popover);
        g_signal_connect_swapped(popover->search_prev_button,"clicked", G_CALLBACK(vteapp_search_popover_search_backward), popover);

        g_signal_connect_swapped(popover->match_case_checkbutton,"toggled", G_CALLBACK(vteapp_search_popover_update_regex), popover);
        g_signal_connect_swapped(popover->entire_word_checkbutton,"toggled", G_CALLBACK(vteapp_search_popover_update_regex), popover);
        g_signal_connect_swapped(popover->regex_checkbutton,"toggled", G_CALLBACK(vteapp_search_popover_update_regex), popover);
        g_signal_connect_swapped(popover->match_case_checkbutton, "toggled", G_CALLBACK(vteapp_search_popover_update_regex), popover);

        g_signal_connect(popover->wrap_around_checkbutton, "toggled", G_CALLBACK(vteapp_search_popover_wrap_around_toggled), popover);

        vteapp_search_popover_update_sensitivity(popover);
}

static void
vteapp_search_popover_finalize(GObject *object)
{
        VteappSearchPopover* popover = VTEAPP_SEARCH_POPOVER(object);

        g_free(popover->regex_pattern);

        G_OBJECT_CLASS(vteapp_search_popover_parent_class)->finalize(object);
}

static void
vteapp_search_popover_set_property(GObject* object,
                                   guint prop_id,
                                   const GValue* value,
                                   GParamSpec* pspec)
{
        VteappSearchPopover* popover = VTEAPP_SEARCH_POPOVER(object);

        switch (prop_id) {
        case SEARCH_POPOVER_PROP_TERMINAL:
                popover->terminal = reinterpret_cast<VteTerminal*>(g_value_get_object(value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
                break;
        }
}

static void
vteapp_search_popover_class_init(VteappSearchPopoverClass* klass)
{
        GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
        gobject_class->finalize = vteapp_search_popover_finalize;
        gobject_class->set_property = vteapp_search_popover_set_property;

        search_popover_pspecs[SEARCH_POPOVER_PROP_TERMINAL] =
                g_param_spec_object("terminal", nullptr, nullptr,
                                    VTE_TYPE_TERMINAL,
                                    GParamFlags(G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY));

        g_object_class_install_properties(gobject_class, G_N_ELEMENTS(search_popover_pspecs), search_popover_pspecs);

        GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
        gtk_widget_class_set_template_from_resource(widget_class, "/org/gnome/vte/app/ui/search-popover.ui");
        gtk_widget_class_bind_template_child(widget_class, VteappSearchPopover, search_entry);
        gtk_widget_class_bind_template_child(widget_class, VteappSearchPopover, search_prev_button);
        gtk_widget_class_bind_template_child(widget_class, VteappSearchPopover, search_next_button);
        gtk_widget_class_bind_template_child(widget_class, VteappSearchPopover, reveal_button);
        gtk_widget_class_bind_template_child(widget_class, VteappSearchPopover, close_button);
        gtk_widget_class_bind_template_child(widget_class, VteappSearchPopover, revealer);
        gtk_widget_class_bind_template_child(widget_class, VteappSearchPopover, match_case_checkbutton);
        gtk_widget_class_bind_template_child(widget_class, VteappSearchPopover, entire_word_checkbutton);
        gtk_widget_class_bind_template_child(widget_class, VteappSearchPopover, regex_checkbutton);
        gtk_widget_class_bind_template_child(widget_class, VteappSearchPopover, wrap_around_checkbutton);
}

static GtkWidget*
vteapp_search_popover_new(VteTerminal* terminal,
                          GtkWidget* relative_to)
{
        auto popover = reinterpret_cast<GtkWidget*>(g_object_new(VTEAPP_TYPE_SEARCH_POPOVER,
                                                                 "terminal", terminal,
#if VTE_GTK == 3
                                                                 "relative-to", relative_to,
#endif
                                                                 nullptr));

#if VTE_GTK == 4
        gtk_widget_set_parent(popover, relative_to);
#endif

        return popover;
}

/* terminal */

#define VTEAPP_TYPE_TERMINAL         (vteapp_terminal_get_type())
#define VTEAPP_TERMINAL(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), VTEAPP_TYPE_TERMINAL, VteappTerminal))
#define VTEAPP_TERMINAL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), VTEAPP_TYPE_TERMINAL, VteappTerminalClass))
#define VTEAPP_IS_TERMINAL(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), VTEAPP_TYPE_TERMINAL))
#define VTEAPP_IS_TERMINAL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), VTEAPP_TYPE_TERMINAL))
#define VTEAPP_TERMINAL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), VTEAPP_TYPE_TERMINAL, VteappTerminalClass))

typedef struct _VteappTerminal       VteappTerminal;
typedef struct _VteappTerminalClass  VteappTerminalClass;

struct _VteappTerminal {
        VteTerminal parent;

        cairo_pattern_t* background_pattern;
        bool has_backdrop;
        bool use_backdrop;
};

struct _VteappTerminalClass {
        VteTerminalClass parent;
};

static GType vteapp_terminal_get_type(void);

G_DEFINE_TYPE(VteappTerminal, vteapp_terminal, VTE_TYPE_TERMINAL)

#define BACKDROP_ALPHA (0.2)

static void
vteapp_terminal_realize(GtkWidget* widget)
{
        GTK_WIDGET_CLASS(vteapp_terminal_parent_class)->realize(widget);

        if (!options.background_pixbuf)
                return;

        auto terminal = VTEAPP_TERMINAL(widget);

#if VTE_GTK == 3
        auto surface = vte::take_freeable
                (gdk_cairo_surface_create_from_pixbuf(options.background_pixbuf,
                                                      0 /* take scale from window */,
                                                      gtk_widget_get_window(widget)));
#elif VTE_GTK == 4
        auto const width = gdk_pixbuf_get_width(options.background_pixbuf);
        auto const height = gdk_pixbuf_get_height(options.background_pixbuf);
        auto surface = vte::take_freeable(cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                                     width, height));
        auto cr = vte::take_freeable(cairo_create(surface.get()));
        gdk_cairo_set_source_pixbuf(cr.get(), options.background_pixbuf, 0, 0);
        cairo_paint(cr.get());
        cairo_surface_flush(surface.get()); // FIXME necessary?
#endif
        terminal->background_pattern = cairo_pattern_create_for_surface(surface.get());

        cairo_pattern_set_extend(terminal->background_pattern, options.background_extend);
}

static void
vteapp_terminal_unrealize(GtkWidget* widget)
{
#if VTE_GTK == 3
        auto terminal = VTEAPP_TERMINAL(widget);

        if (terminal->background_pattern != nullptr) {
                cairo_pattern_destroy(terminal->background_pattern);
                terminal->background_pattern = nullptr;
        }
#endif /* VTE_GTK */

        GTK_WIDGET_CLASS(vteapp_terminal_parent_class)->unrealize(widget);
}

static void
vteapp_terminal_draw_background(GtkWidget* widget,
                                cairo_t* cr)
{
#if VTE_GTK == 3
        auto terminal = VTEAPP_TERMINAL(widget);

        if (terminal->background_pattern != nullptr) {
                cairo_push_group(cr);

                /* Draw background colour */
                cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
                cairo_rectangle(cr, 0.0, 0.0,
                                gtk_widget_get_allocated_width(widget),
                                gtk_widget_get_allocated_height(widget));
                GdkRGBA bg;
                vte_terminal_get_color_background_for_draw(VTE_TERMINAL(terminal), &bg);
                cairo_set_source_rgba(cr, bg.red, bg.green, bg.blue, 1.0);
                cairo_paint(cr);

                /* Draw background image */
                cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
                cairo_set_source(cr, terminal->background_pattern);
                cairo_paint(cr);

                cairo_pop_group_to_source(cr);
                cairo_paint_with_alpha(cr, options.get_alpha_bg_for_draw());

        }
#endif /* VTE_GTK == 3 */
}

#if VTE_GTK == 4

static void
vteapp_terminal_draw_background(GtkWidget* widget,
                                GtkSnapshot* snapshot)
{
        auto grect = GRAPHENE_RECT_INIT(float(0), float(0),
                                        float(gtk_widget_get_allocated_width(widget)),
                                        float(gtk_widget_get_allocated_height(widget)));
        auto cr = vte::take_freeable(gtk_snapshot_append_cairo(snapshot, &grect));
        vteapp_terminal_draw_background(widget, cr.get());
}

#endif /* VTE_GTK  == 4 */

static void
vteapp_terminal_draw_backdrop(GtkWidget* widget,
                              cairo_t* cr)
{
        auto terminal = VTEAPP_TERMINAL(widget);

        if (terminal->use_backdrop && terminal->has_backdrop) {
                cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
                cairo_set_source_rgba(cr, 0, 0, 0, BACKDROP_ALPHA);
                cairo_rectangle(cr, 0.0, 0.0,
                                gtk_widget_get_allocated_width(widget),
                                gtk_widget_get_allocated_height(widget));
                cairo_paint(cr);
        }
}

#if VTE_GTK == 4

static void
vteapp_terminal_draw_backdrop(GtkWidget* widget,
                              GtkSnapshot* snapshot)
{
        auto grect = GRAPHENE_RECT_INIT(float(0), float(0),
                                        float(gtk_widget_get_allocated_width(widget)),
                                        float(gtk_widget_get_allocated_height(widget)));
        auto cr = vte::take_freeable(gtk_snapshot_append_cairo(snapshot, &grect));
        vteapp_terminal_draw_backdrop(widget, cr.get());
}

#endif /* VTE_GTK  == 4 */

#if VTE_GTK == 3

static gboolean
vteapp_terminal_draw(GtkWidget* widget,
                     cairo_t* cr)
{
        vteapp_terminal_draw_background(widget, cr);

        auto const rv = GTK_WIDGET_CLASS(vteapp_terminal_parent_class)->draw(widget, cr);

        vteapp_terminal_draw_backdrop(widget, cr);

        return rv;
}

#endif /* VTE_GTK == 3 */

static void
vteapp_terminal_update_theme_colors(GtkWidget* widget)
{
        if (!options.use_theme_colors)
                return;

        auto terminal = VTEAPP_TERMINAL(widget);
        auto context = gtk_widget_get_style_context(widget);

#if VTE_GTK == 3
        auto const flags = gtk_style_context_get_state(context);
#endif

        auto theme_fg = GdkRGBA{};
        gtk_style_context_get_color(context,
#if VTE_GTK == 3
                                    flags,
#endif
                                    &theme_fg);

        auto theme_bg = GdkRGBA{};
#if VTE_GTK == 3
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        gtk_style_context_get_background_color(context, flags, &theme_bg);
        G_GNUC_END_IGNORE_DEPRECATIONS;
#elif VTE_GTK == 4
        // FIXMEgtk4 "background-color" lookup always fails
        if (!gtk_style_context_lookup_color(context, "text_view_bg", &theme_bg)) {
                verbose_print("Failed to get theme background color\n");
                return;
        }
#endif

        auto dti = [](double d) -> unsigned { return std::clamp(unsigned(d*255), 0u, 255u); };

        verbose_print("Theme colors: foreground is #%02X%02X%02X, background is #%02X%02X%02X\n",
                      dti(theme_fg.red), dti(theme_fg.green), dti(theme_fg.blue),
                      dti(theme_bg.red), dti(theme_bg.green), dti(theme_bg.blue));

        theme_fg.alpha = 1.;
        theme_bg.alpha = options.get_alpha_bg();
        vte_terminal_set_colors(VTE_TERMINAL(terminal), &theme_fg, &theme_bg, nullptr, 0);
}

#if VTE_GTK == 3

static void
vteapp_terminal_style_updated(GtkWidget* widget)
{
        GTK_WIDGET_CLASS(vteapp_terminal_parent_class)->style_updated(widget);

        auto terminal = VTEAPP_TERMINAL(widget);

        auto context = gtk_widget_get_style_context(widget);
        auto const flags = gtk_style_context_get_state(context);
        terminal->has_backdrop = (flags & GTK_STATE_FLAG_BACKDROP) != 0;

        vteapp_terminal_update_theme_colors(widget);
}

#endif /* VTE_GTK == 3 */

#if VTE_GTK == 4

static void
vteapp_terminal_snapshot(GtkWidget* widget,
                         GtkSnapshot* snapshot_object)
{
        vteapp_terminal_draw_background(widget, snapshot_object);

        GTK_WIDGET_CLASS(vteapp_terminal_parent_class)->snapshot(widget, snapshot_object);

        vteapp_terminal_draw_backdrop(widget, snapshot_object);
}

static void
vteapp_terminal_css_changed(GtkWidget* widget,
                            GtkCssStyleChange* change)
{
        GTK_WIDGET_CLASS(vteapp_terminal_parent_class)->css_changed(widget, change);

        vteapp_terminal_update_theme_colors(widget);
}

static void
vteapp_terminal_state_flags_changed(GtkWidget* widget,
                                    GtkStateFlags old_flags)
{
        GTK_WIDGET_CLASS(vteapp_terminal_parent_class)->state_flags_changed(widget, old_flags);

        auto terminal = VTEAPP_TERMINAL(widget);
        auto const flags = gtk_widget_get_state_flags(widget);
        terminal->has_backdrop = (flags & GTK_STATE_FLAG_BACKDROP) != 0;
}

static void
vteapp_terminal_system_setting_changed(GtkWidget* widget,
                                       GtkSystemSetting setting)
{
        GTK_WIDGET_CLASS(vteapp_terminal_parent_class)->system_setting_changed(widget, setting);

        // FIXMEgtk4 find a way to update colours on theme change like gtk3 above
}

#endif /* VTE_GTK == 4 */

static void
vteapp_terminal_class_init(VteappTerminalClass *klass)
{
        auto widget_class = GTK_WIDGET_CLASS(klass);
        widget_class->realize = vteapp_terminal_realize;
        widget_class->unrealize = vteapp_terminal_unrealize;

#if VTE_GTK == 3
        widget_class->draw = vteapp_terminal_draw;
        widget_class->style_updated = vteapp_terminal_style_updated;
#elif VTE_GTK == 4
        widget_class->snapshot = vteapp_terminal_snapshot;
        widget_class->css_changed = vteapp_terminal_css_changed;
        widget_class->state_flags_changed = vteapp_terminal_state_flags_changed;
        widget_class->system_setting_changed = vteapp_terminal_system_setting_changed;
#endif
}

static void
vteapp_terminal_init(VteappTerminal *terminal)
{
        terminal->background_pattern = nullptr;
        terminal->has_backdrop = false;
        terminal->use_backdrop = options.backdrop;

#if VTE_GTK == 3
        if (options.background_pixbuf != nullptr)
                vte_terminal_set_clear_background(VTE_TERMINAL(terminal), false);
#endif /* VTE_GTK == 3 */
}

static GtkWidget *
vteapp_terminal_new(void)
{
        return reinterpret_cast<GtkWidget*>(g_object_new(VTEAPP_TYPE_TERMINAL, nullptr));
}

/* terminal window */

#define VTEAPP_TYPE_WINDOW         (vteapp_window_get_type())
#define VTEAPP_WINDOW(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), VTEAPP_TYPE_WINDOW, VteappWindow))
#define VTEAPP_WINDOW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), VTEAPP_TYPE_WINDOW, VteappWindowClass))
#define VTEAPP_IS_WINDOW(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), VTEAPP_TYPE_WINDOW))
#define VTEAPP_IS_WINDOW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), VTEAPP_TYPE_WINDOW))
#define VTEAPP_WINDOW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), VTEAPP_TYPE_WINDOW, VteappWindowClass))

typedef struct _VteappWindow       VteappWindow;
typedef struct _VteappWindowClass  VteappWindowClass;

struct _VteappWindow {
        GtkApplicationWindow parent;

        /* from GtkWidget template */
        GtkWidget* window_grid;
        GtkScrollbar* scrollbar;
        /* GtkGrid* notifications_grid; */
        GtkWidget* readonly_emblem;
        /* GtkButton* copy_button; */
        /* GtkButton* paste_button; */
        GtkToggleButton* find_button;
        GtkMenuButton* gear_button;
        /* end */

        VteTerminal* terminal;
        GPid child_pid;
        GtkWidget* search_popover;

        /* used for updating the geometry hints */
        int cached_cell_width{0};
        int cached_cell_height{0};
        int cached_chrome_width{0};
        int cached_chrome_height{0};
        int cached_csd_width{0};
        int cached_csd_height{0};

#if VTE_GTK == 3
        GtkClipboard* clipboard;
        GdkWindowState window_state{GdkWindowState(0)};
#endif
#if VTE_GTK == 4
        GdkClipboard* clipboard;
        GdkToplevelState toplevel_state{GdkToplevelState(0)};
#endif
};

struct _VteappWindowClass {
        GtkApplicationWindowClass parent;
};

static GType vteapp_window_get_type(void);

static char const* const builtin_dingus[] = {
        "(((gopher|news|telnet|nntp|file|http|ftp|https)://)|(www|ftp)[-A-Za-z0-9]*\\.)[-A-Za-z0-9\\.]+(:[0-9]*)?",
        "(((gopher|news|telnet|nntp|file|http|ftp|https)://)|(www|ftp)[-A-Za-z0-9]*\\.)[-A-Za-z0-9\\.]+(:[0-9]*)?/[-A-Za-z0-9_\\$\\.\\+\\!\\*\\(\\),;:@&=\\?/~\\#\\%]*[^]'\\.}>\\) ,\\\"]",
        nullptr,
};

/* Just some arbitrary minimum values */
#define MIN_COLUMNS (16)
#define MIN_ROWS    (2)

static void
vteapp_window_add_dingus(VteappWindow* window,
                         char const* const* dingus)
{
        for (auto i = 0; dingus[i] != nullptr; i++) {
                auto tag = -1;
                auto error = vte::glib::Error{};
                auto regex = compile_regex_for_match(dingus[i], true, error);
                if (regex) {
                        tag = vte_terminal_match_add_regex(window->terminal, regex.get(), 0);
                }

                if (error.error()) {
                        verbose_printerr("Failed to compile regex \"%s\": %s\n",
                                         dingus[i], error.message());
                }

                if (tag != -1)
                        vte_terminal_match_set_cursor_name(window->terminal, tag, "pointer");
        }
}

static void
vteapp_window_update_geometry(VteappWindow* window)
{
        if (options.no_geometry_hints)
                return;

#if VTE_GTK == 3
        GtkWidget* window_widget = GTK_WIDGET(window);
        GtkWidget* terminal_widget = GTK_WIDGET(window->terminal);

        int columns = vte_terminal_get_column_count(window->terminal);
        int rows = vte_terminal_get_row_count(window->terminal);
        int cell_width = vte_terminal_get_char_width(window->terminal);
        int cell_height = vte_terminal_get_char_height(window->terminal);

        /* Calculate the chrome size as difference between the content's
         * natural size requisition and the terminal grid's size.
         * This includes the terminal's padding in the chrome.
         */
        GtkRequisition contents_req;
        gtk_widget_get_preferred_size(window->window_grid, nullptr, &contents_req);
        int chrome_width = contents_req.width - cell_width * columns;
        int chrome_height = contents_req.height - cell_height * rows;
        g_assert_cmpint(chrome_width, >=, 0);
        g_assert_cmpint(chrome_height, >=, 0);

        int csd_width = 0;
        int csd_height = 0;
        if (gtk_widget_get_realized(terminal_widget)) {
                /* Calculate the CSD size as difference between the toplevel's
                 * and content's allocation.
                 */
                GtkAllocation toplevel, contents;
                gtk_widget_get_allocation(window_widget, &toplevel);
                gtk_widget_get_allocation(window->window_grid, &contents);

                csd_width = toplevel.width - contents.width;
                csd_height = toplevel.height - contents.height;
                g_assert_cmpint(csd_width, >=, 0);
                g_assert_cmpint(csd_height, >=, 0);

                /* Only actually set the geometry hints once the window is realized,
                 * since only then we know the CSD size. Only set the geometry when
                 * anything has changed.
                 */
                if (!options.no_geometry_hints &&
                    (cell_height != window->cached_cell_height ||
                     cell_width != window->cached_cell_width ||
                     chrome_width != window->cached_chrome_width ||
                     chrome_height != window->cached_chrome_height ||
                     csd_width != window->cached_csd_width ||
                     csd_width != window->cached_csd_height)) {
                        GdkGeometry geometry;

                        geometry.base_width = csd_width + chrome_width;
                        geometry.base_height = csd_height + chrome_height;
                        geometry.width_inc = cell_width;
                        geometry.height_inc = cell_height;
                        geometry.min_width = geometry.base_width + cell_width * MIN_COLUMNS;
                        geometry.min_height = geometry.base_height + cell_height * MIN_ROWS;

                        gtk_window_set_geometry_hints(GTK_WINDOW(window),
                                                      nullptr,
                                                      &geometry,
                                                      GdkWindowHints(GDK_HINT_RESIZE_INC |
                                                                     GDK_HINT_MIN_SIZE |
                                                                     GDK_HINT_BASE_SIZE));

                        verbose_print("Updating geometry hints base %dx%d inc %dx%d min %dx%d\n",
                                      geometry.base_width, geometry.base_height,
                                      geometry.width_inc, geometry.height_inc,
                                      geometry.min_width, geometry.min_height);
                }
        }

        window->cached_csd_width = csd_width;
        window->cached_csd_height = csd_height;
        window->cached_cell_width = cell_width;
        window->cached_cell_height = cell_height;
        window->cached_chrome_width = chrome_width;
        window->cached_chrome_height = chrome_height;

        verbose_print("Cached grid %dx%d cell-size %dx%d chrome %dx%d csd %dx%d\n",
                      columns, rows,
                      window->cached_cell_width, window->cached_cell_height,
                      window->cached_chrome_width, window->cached_chrome_height,
                      window->cached_csd_width, window->cached_csd_height);
#elif VTE_GTK == 4
        // FIXMEgtk4 there appears to be no way to do this with gtk4 ? maybe go to X/wayland
        // directly to set the geometry hints?
#endif
}

#include <gdk/gdk.h>

static void
vteapp_window_resize(VteappWindow* window)
{
        /* Can't do this when not using geometry hints */
        if (options.no_geometry_hints)
                return;

        /* Don't do this for fullscreened, maximised, or tiled windows. */
#if VTE_GTK == 3
        if (window->window_state & (GDK_WINDOW_STATE_MAXIMIZED |
                                    GDK_WINDOW_STATE_FULLSCREEN |
                                    GDK_WINDOW_STATE_TILED |
#if GTK_CHECK_VERSION(3,22,23)
                                    GDK_WINDOW_STATE_TOP_TILED |
                                    GDK_WINDOW_STATE_BOTTOM_TILED |
                                    GDK_WINDOW_STATE_LEFT_TILED |
                                    GDK_WINDOW_STATE_RIGHT_TILED |
#endif
                                    0))
                return;
#elif VTE_GTK == 4
        if (window->toplevel_state & (GDK_TOPLEVEL_STATE_MAXIMIZED |
                                      GDK_TOPLEVEL_STATE_FULLSCREEN |
                                      GDK_TOPLEVEL_STATE_TILED |
                                      GDK_TOPLEVEL_STATE_TOP_TILED |
                                      GDK_TOPLEVEL_STATE_BOTTOM_TILED |
                                      GDK_TOPLEVEL_STATE_LEFT_TILED |
                                      GDK_TOPLEVEL_STATE_RIGHT_TILED))
                return;
#endif /* VTE_GTK */

#if VTE_GTK == 3
        // FIXMEgtk4

        /* First, update the geometry hints, so that the cached_* members are up-to-date */
        vteapp_window_update_geometry(window);

        /* Calculate the window's pixel size corresponding to the terminal's grid size */
        int columns = vte_terminal_get_column_count(window->terminal);
        int rows = vte_terminal_get_row_count(window->terminal);
        int pixel_width = window->cached_chrome_width + window->cached_cell_width * columns;
        int pixel_height = window->cached_chrome_height + window->cached_cell_height * rows;

        verbose_print("VteappWindow resize grid %dx%d pixel %dx%d\n",
                      columns, rows, pixel_width, pixel_height);

        gtk_window_resize(GTK_WINDOW(window), pixel_width, pixel_height);
#endif /* VTE_GTK == 3 FIXMEgtk4 */
}

static void
vteapp_window_parse_geometry(VteappWindow* window)
{
#if VTE_GTK == 3
        /* First update the geometry hints, so that gtk_window_parse_geometry()
         * knows the char width/height and base size increments.
         */
        vteapp_window_update_geometry(window);

        if (options.geometry != nullptr) {
                auto rv = gtk_window_parse_geometry(GTK_WINDOW(window), options.geometry);

                if (!rv)
                        verbose_printerr("Failed to parse geometry spec \"%s\"\n", options.geometry);
                else if (!options.no_geometry_hints) {
                        /* After parse_geometry(), we can get the default size in
                         * width/height increments, i.e. in grid size.
                         */
                        int columns, rows;
                        gtk_window_get_default_size(GTK_WINDOW(window), &columns, &rows);
                        vte_terminal_set_size(window->terminal, columns, rows);
                        gtk_window_resize_to_geometry(GTK_WINDOW(window), columns, rows);
                } else {
                        /* Approximate the grid width from the passed pixel size. */
                        int width, height;
                        gtk_window_get_default_size(GTK_WINDOW(window), &width, &height);
                        width -= window->cached_csd_width + window->cached_chrome_width;
                        height -= window->cached_csd_height + window->cached_chrome_height;
                        int columns = width / window->cached_cell_width;
                        int rows = height / window->cached_cell_height;
                        vte_terminal_set_size(window->terminal,
                                              MAX(columns, MIN_COLUMNS),
                                              MAX(rows, MIN_ROWS));
                }
        } else {
                /* In GTK+ 3.0, the default size of a window comes from its minimum
                 * size not its natural size, so we need to set the right default size
                 * explicitly */
                if (!options.no_geometry_hints) {
                        /* Grid based */
                        gtk_window_set_default_geometry(GTK_WINDOW(window),
                                                        vte_terminal_get_column_count(window->terminal),
                                                        vte_terminal_get_row_count(window->terminal));
                } else {
                        /* Pixel based */
                        vteapp_window_resize(window);
                }
        }
#elif VTE_GTK == 4
        // FIXMEgtk4 ????
#endif /* VTE_GTK */
}

static void
vteapp_window_adjust_font_size(VteappWindow* window,
                               double factor)
{
        vte_terminal_set_font_scale(window->terminal,
                                    vte_terminal_get_font_scale(window->terminal) * factor);

        vteapp_window_resize(window);
}

static void
window_spawn_cb(VteTerminal* terminal,
                GPid child_pid,
                GError* error,
                void* data)
{
        if (terminal == nullptr) /* terminal destroyed while spawning */
                return;

        VteappWindow* window = VTEAPP_WINDOW(data);
        window->child_pid = child_pid;

        if (child_pid != -1)
                verbose_printerr("Spawning succeded, PID=%ld\n", (long)child_pid);

        if (error != nullptr) {
                verbose_printerr("Spawning failed: %s\n", error->message);

                auto msg = vte::glib::take_string(g_strdup_printf("Spawning failed: %s", error->message));
                if (options.keep)
                        vte_terminal_feed(window->terminal, msg.get(), -1);
                else {
#if VTE_GTK == 3
                        gtk_widget_destroy(GTK_WIDGET(window));
#elif VTE_GTK == 4
                        gtk_window_destroy(GTK_WINDOW(window));
#endif
                }
        }
}

static bool
vteapp_window_launch_argv(VteappWindow* window,
                          char** argv,
                          GError** error)
{
        auto const spawn_flags = GSpawnFlags(G_SPAWN_SEARCH_PATH_FROM_ENVP |
                                             VTE_SPAWN_NO_PARENT_ENVV |
                                             (options.no_systemd_scope ? VTE_SPAWN_NO_SYSTEMD_SCOPE : 0) |
                                             (options.require_systemd_scope ? VTE_SPAWN_REQUIRE_SYSTEMD_SCOPE : 0));
        auto fds = options.fds();
        auto map_fds = options.map_fds();
        vte_terminal_spawn_with_fds_async(window->terminal,
                                          VTE_PTY_DEFAULT,
                                          options.working_directory,
                                          argv,
                                          options.environment_for_spawn().get(),
                                          fds.data(), fds.size(),
                                          map_fds.data(), map_fds.size(),
                                          spawn_flags,
                                          nullptr, nullptr, nullptr, /* child setup, data and destroy */
                                          -1 /* default timeout of 30s */,
                                          nullptr /* cancellable */,
                                          window_spawn_cb, window);
        return true;
}

static bool
vteapp_window_launch_commandline(VteappWindow* window,
                                 char* commandline,
                                 GError** error)
{
        int argc;
        char** argv;
        if (!g_shell_parse_argv(commandline, &argc, &argv, error))
            return false;

        bool rv = vteapp_window_launch_argv(window, argv, error);

        g_strfreev(argv);
        return rv;
}

static bool
vteapp_window_launch_shell(VteappWindow* window,
                           GError** error)
{
        char* shell = vte_get_user_shell();
        if (shell == nullptr || shell[0] == '\0') {
                g_free(shell);
                shell = g_strdup(g_getenv("SHELL"));
        }
        if (shell == nullptr || shell[0] == '\0') {
                g_free(shell);
                shell = g_strdup("/bin/sh");
        }

        char* argv[2] = { shell, nullptr };

        bool rv = vteapp_window_launch_argv(window, argv, error);

        g_free(shell);
        return rv;
}

static bool
vteapp_window_fork(VteappWindow* window,
                   GError** error)
{
        auto pty = vte::glib::take_ref(vte_pty_new_sync(VTE_PTY_DEFAULT, nullptr, error));
        if (!pty)
                return false;

        auto pid = fork();
        switch (pid) {
        case -1: { /* error */
                auto errsv = vte::libc::ErrnoSaver{};
                g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errsv),
                            "Error forking: %s", g_strerror(errsv));
                return false;
        }

        case 0: /* child */ {
                vte_pty_child_setup(pty.get());

                for (auto i = 0; ; i++) {
                        switch (i % 3) {
                        case 0:
                        case 1:
                                g_print("%d\n", i);
                                break;
                        case 2:
                                g_printerr("%d\n", i);
                                break;
                        }
                        g_usleep(G_USEC_PER_SEC);
                }
        }
        default: /* parent */
                vte_terminal_set_pty(window->terminal, pty.get());
                vte_terminal_watch_child(window->terminal, pid);
                verbose_print("Child PID is %d (mine is %d).\n", (int)pid, (int)getpid());
                break;
        }

        return true;
}

static gboolean
tick_cb(VteappWindow* window)
{
        static int i = 0;
        char buf[256];
        auto s = g_snprintf(buf, sizeof(buf), "%d\r\n", i++);
        vte_terminal_feed(window->terminal, buf, s);
        return G_SOURCE_CONTINUE;
}

static bool
vteapp_window_tick(VteappWindow* window,
                   GError** error)
{
        g_timeout_add_seconds(1, (GSourceFunc) tick_cb, window);
        return true;
}

static void
vteapp_window_launch(VteappWindow* window)
{
        auto rv = bool{};
        auto error = vte::glib::Error{};

        if (options.exec_argv != nullptr)
                rv = vteapp_window_launch_argv(window, options.exec_argv, error);
        else if (options.command != nullptr)
                rv = vteapp_window_launch_commandline(window, options.command, error);
        else if (!options.no_shell)
                rv = vteapp_window_launch_shell(window, error);
        else if (!options.no_pty)
                rv = vteapp_window_fork(window, error);
        else
                rv = vteapp_window_tick(window, error);

        if (!rv)
                verbose_printerr("Error launching: %s\n", error.message());
}

static void
window_update_copy_sensitivity(VteappWindow* window)
{
        auto action = g_action_map_lookup_action(G_ACTION_MAP(window), "copy");
        g_simple_action_set_enabled(G_SIMPLE_ACTION(action),
                                    vte_terminal_get_has_selection(window->terminal));
}

static void
window_update_fullscreen_state(VteappWindow* window)
{
#if VTE_GTK == 3
        auto const fullscreen = (window->window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0;
#elif VTE_GTK == 4
        auto const fullscreen = (window->toplevel_state & GDK_TOPLEVEL_STATE_FULLSCREEN) != 0;
#endif
        auto action = g_action_map_lookup_action(G_ACTION_MAP(window), "fullscreen");
        g_simple_action_set_state(G_SIMPLE_ACTION(action), g_variant_new_boolean (fullscreen));
}

static void
window_update_paste_sensitivity(VteappWindow* window)
{
        bool can_paste = false;

#if VTE_GTK == 3
        GdkAtom* targets;
        int n_targets;

        if (gtk_clipboard_wait_for_targets(window->clipboard, &targets, &n_targets)) {
                can_paste = gtk_targets_include_text(targets, n_targets);
                g_free(targets);
        }
#elif VTE_GTK == 4
        auto formats = gdk_clipboard_get_formats(window->clipboard);
        can_paste = gdk_content_formats_contain_gtype(formats, G_TYPE_STRING);
#endif /* VTE_GTK */

        auto action = g_action_map_lookup_action(G_ACTION_MAP(window), "paste");
        g_simple_action_set_enabled(G_SIMPLE_ACTION(action), can_paste);
}

/* Callbacks */

static void
window_action_copy_cb(GSimpleAction* action,
                      GVariant* parameter,
                      void* data)
{
        VteappWindow* window = VTEAPP_WINDOW(data);
        char const* str = g_variant_get_string(parameter, nullptr);

        vte_terminal_copy_clipboard_format(window->terminal,
                                           g_str_equal(str, "html") ? VTE_FORMAT_HTML : VTE_FORMAT_TEXT);

}

static void
window_action_copy_match_cb(GSimpleAction* action,
                            GVariant* parameter,
                            void* data)
{
        auto window = VTEAPP_WINDOW(data);

        auto len = size_t{};
        auto str = g_variant_get_string(parameter, &len);
#if VTE_GTK == 3
        gtk_clipboard_set_text(window->clipboard, str, len);
#elif VTE_GTK == 4
        gdk_clipboard_set_text(window->clipboard, str);
#endif
}

static void
window_action_paste_cb(GSimpleAction* action,
                       GVariant* parameter,
                       void* data)
{
        VteappWindow* window = VTEAPP_WINDOW(data);
        vte_terminal_paste_clipboard(window->terminal);
}

static void
window_action_reset_cb(GSimpleAction* action,
                       GVariant* parameter,
                       void* data)
{
        auto window = VTEAPP_WINDOW(data);
        auto clear = false;

        if (parameter != nullptr)
                clear = g_variant_get_boolean(parameter);
        else {
                auto modifiers = GdkModifierType{};
#if VTE_GTK == 3
                if (!gtk_get_current_event_state(&modifiers))
                        modifiers = GdkModifierType(0);
#elif VTE_GTK == 4
                // FIXMEgtk4!
                modifiers = GdkModifierType(0);
#endif

                clear = (modifiers & GDK_CONTROL_MASK) != 0;
        }

        vte_terminal_reset(window->terminal, true, clear);
}

static void
window_action_find_cb(GSimpleAction* action,
                      GVariant* parameter,
                      void* data)
{
        VteappWindow* window = VTEAPP_WINDOW(data);
        gtk_toggle_button_set_active(window->find_button, true);
}

static void
window_action_fullscreen_state_cb (GSimpleAction *action,
                                   GVariant *state,
                                   void* data)
{
        VteappWindow* window = VTEAPP_WINDOW(data);

        if (!gtk_widget_get_realized(GTK_WIDGET(window)))
                return;

        if (g_variant_get_boolean(state))
                gtk_window_fullscreen(GTK_WINDOW(window));
        else
                gtk_window_unfullscreen(GTK_WINDOW(window));

        /* The window-state-changed callback will update the action's actual state */
}

#if VTE_GTK == 3
static bool
vteapp_window_show_context_menu(VteappWindow* window,
                                guint button,
                                guint32 timestamp,
                                GdkEvent* event)
{
        if (options.no_context_menu)
                return false;

        auto menu = g_menu_new();
        g_menu_append(menu, "_Copy", "win.copy::text");
        g_menu_append(menu, "Copy As _HTML", "win.copy::html");

        if (event != nullptr)
        {
                auto hyperlink = vte::glib::take_string(vte_terminal_hyperlink_check_event(window->terminal, event));
                if (hyperlink) {
                        verbose_print("Hyperlink: %s\n", hyperlink.get());
                        auto target = g_variant_new_string(hyperlink.get()); /* floating */
                        auto item = vte::glib::take_ref(g_menu_item_new("Copy _Hyperlink", nullptr));
                        g_menu_item_set_action_and_target_value(item.get(), "win.copy-match", target);
                        g_menu_append_item(menu, item.get());
                }

                auto match = vte::glib::take_string(vte_terminal_match_check_event(window->terminal, event, nullptr));
                if (match) {
                        verbose_print("Match: %s\n", match.get());
                        auto target = g_variant_new_string(match.get()); /* floating */
                        auto item = vte::glib::take_ref(g_menu_item_new("Copy _Match", nullptr));
                        g_menu_item_set_action_and_target_value(item.get(), "win.copy-match", target);
                        g_menu_append_item(menu, item.get());
                }

                /* Test extra match API */
                static const char extra_pattern[] = "(\\d+)\\s*(\\w+)";
                char* extra_match = nullptr;
                char *extra_subst = nullptr;
                auto error = vte::glib::Error{};
                auto regex = compile_regex_for_match(extra_pattern, false, error);
                error.assert_no_error();

                VteRegex* regexes[1] = {regex.get()};
                vte_terminal_event_check_regex_simple(window->terminal, event,
                                                      regexes, G_N_ELEMENTS(regexes),
                                                      0,
                                                      &extra_match);

                if (extra_match != nullptr &&
                    (extra_subst = vte_regex_substitute(regex.get(), extra_match, "$2 $1",
                                                        PCRE2_SUBSTITUTE_EXTENDED |
                                                        PCRE2_SUBSTITUTE_GLOBAL,
                                                        error)) == nullptr) {
                        verbose_printerr("Substitution failed: %s\n", error.message());
                }

                if (extra_match != nullptr) {
                        if (extra_subst != nullptr)
                                verbose_print("%s match: %s => %s\n", extra_pattern, extra_match, extra_subst);
                        else
                                verbose_print("%s match: %s\n", extra_pattern, extra_match);
                }
                g_free(extra_match);
                g_free(extra_subst);
        }

        g_menu_append(menu, "_Paste", "win.paste");

        auto const fullscreen = (window->window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0;
        if (fullscreen)
                g_menu_append(menu, "_Fullscreen", "win.fullscreen");

        auto popup = gtk_menu_new_from_model(G_MENU_MODEL(menu));
        gtk_style_context_add_class(gtk_widget_get_style_context (popup),
                                    GTK_STYLE_CLASS_CONTEXT_MENU);

        gtk_menu_attach_to_widget(GTK_MENU(popup), GTK_WIDGET(window->terminal), nullptr);
        gtk_menu_popup(GTK_MENU(popup), nullptr, nullptr, nullptr, nullptr, button, timestamp);
        if (button == 0)
                gtk_menu_shell_select_first(GTK_MENU_SHELL(popup), true);

        return true;
}
#endif /* VTE_GTK */

#if VTE_GTK == 3

static gboolean
window_popup_menu_cb(GtkWidget* widget,
                     VteappWindow* window)
{
        auto const timestamp = gtk_get_current_event_time();

        return vteapp_window_show_context_menu(window, 0, timestamp , nullptr);
}
// FIXMEgtk4

#endif /* VTE_GTK == 3 */

#if VTE_GTK == 3

static gboolean
window_button_press_cb(GtkWidget* widget,
                       GdkEventButton* event,
                       VteappWindow* window)
{
        if (event->button != GDK_BUTTON_SECONDARY)
                return false;

        return vteapp_window_show_context_menu(window, event->button, event->time,
                                               reinterpret_cast<GdkEvent*>(event));
}

#endif /* VTE_GTK == 3 */

static void
window_cell_size_changed_cb(VteTerminal* term,
                            guint width,
                            guint height,
                            VteappWindow* window)
{
        vteapp_window_update_geometry(window);
}

static void
window_child_exited_cb(VteTerminal* term,
                       int status,
                       VteappWindow* window)
{
        if (WIFEXITED(status))
                verbose_printerr("Child exited with status %x\n", WEXITSTATUS(status));
        else if (WIFSIGNALED(status))
                verbose_printerr("Child terminated by signal %d\n", WTERMSIG(status));
        else
                verbose_printerr("Child terminated\n");

        if (options.output_filename != nullptr) {
                auto file = vte::glib::take_ref
                        (g_file_new_for_commandline_arg(options.output_filename));
                auto error = vte::glib::Error{};
                auto stream = vte::glib::take_ref(g_file_replace(file.get(),
                                                                 nullptr,
                                                                 false,
                                                                 G_FILE_CREATE_NONE,
                                                                 nullptr,
                                                                 error));

                if (stream) {
                        vte_terminal_write_contents_sync(window->terminal,
                                                         G_OUTPUT_STREAM(stream.get()),
                                                         VTE_WRITE_DEFAULT,
                                                         nullptr,
                                                         error);
                }

                if (error.error()) {
                        verbose_printerr("Failed to write output to \"%s\": %s\n",
                                         options.output_filename, error.message());
                }
        }

        window->child_pid = -1;

        if (options.keep)
                return;

#if VTE_GTK == 3
        gtk_widget_destroy(GTK_WIDGET(window));
#elif VTE_GTK == 4
        gtk_window_destroy(GTK_WINDOW(window));
#endif
}

#if VTE_GTK == 3

static void
window_clipboard_owner_change_cb(GtkClipboard* clipboard,
                                 GdkEvent* event,
                                 VteappWindow* window)
{
        window_update_paste_sensitivity(window);
}

#elif VTE_GTK == 4

static void
window_clipboard_formats_notify_cb(GdkClipboard* clipboard,
                                   GParamSpec* pspec,
                                   VteappWindow* window)
{
        window_update_paste_sensitivity(window);
}

#endif /* VTE_GTK */

static void
window_decrease_font_size_cb(VteTerminal* terminal,
                             VteappWindow* window)
{
        vteapp_window_adjust_font_size(window, 1.0 / 1.2);
}

static void
window_increase_font_size_cb(VteTerminal* terminal,
                             VteappWindow* window)
{
        vteapp_window_adjust_font_size(window, 1.2);
}

static void
window_deiconify_window_cb(VteTerminal* terminal,
                           VteappWindow* window)
{
        if (!options.allow_window_ops)
                return;

#if VTE_GTK == 3
        gtk_window_deiconify(GTK_WINDOW(window));
#elif VTE_GTK == 4
        auto toplevel = GDK_TOPLEVEL(gtk_native_get_surface(GTK_NATIVE(window)));
        gdk_toplevel_present(toplevel, nullptr); // FIXMEgtk4 nullptr not allowed
#endif
}

static void
window_iconify_window_cb(VteTerminal* terminal,
                         VteappWindow* window)
{
        if (!options.allow_window_ops)
                return;

#if VTE_GTK == 3
        gtk_window_iconify(GTK_WINDOW(window));
#elif VTE_GTK == 4
        auto toplevel = GDK_TOPLEVEL(gtk_native_get_surface(GTK_NATIVE(window)));
        gdk_toplevel_minimize(toplevel);
#endif
}

static void
window_window_title_changed_cb(VteTerminal* terminal,
                               VteappWindow* window)
{
        auto const title = vte_terminal_get_window_title(window->terminal);
        gtk_window_set_title(GTK_WINDOW(window), title && title[0] ? title : "Terminal");
}

static void
window_lower_window_cb(VteTerminal* terminal,
                       VteappWindow* window)
{
        if (!options.allow_window_ops)
                return;
        if (!gtk_widget_get_realized(GTK_WIDGET(window)))
                return;

#if VTE_GTK == 3
        gdk_window_lower(gtk_widget_get_window(GTK_WIDGET(window)));
#elif VTE_GTK == 4
        auto toplevel = GDK_TOPLEVEL(gtk_native_get_surface(GTK_NATIVE(window)));
        gdk_toplevel_lower(toplevel);
#endif
}

static void
window_raise_window_cb(VteTerminal* terminal,
                       VteappWindow* window)
{
        if (!options.allow_window_ops)
                return;
        if (!gtk_widget_get_realized(GTK_WIDGET(window)))
                return;

#if VTE_GTK == 3
        gdk_window_raise(gtk_widget_get_window(GTK_WIDGET(window)));
#elif VTE_GTK == 4
        auto toplevel = GDK_TOPLEVEL(gtk_native_get_surface(GTK_NATIVE(window)));
        gdk_toplevel_present(toplevel, nullptr); // FIXMEgtk4 gdk_toplevel_raise() doesn't exist??
#endif
}

static void
window_maximize_window_cb(VteTerminal* terminal,
                          VteappWindow* window)
{
        if (!options.allow_window_ops)
                return;

        gtk_window_maximize(GTK_WINDOW(window));
}

static void
window_restore_window_cb(VteTerminal* terminal,
                         VteappWindow* window)
{
        if (!options.allow_window_ops)
                return;

        gtk_window_unmaximize(GTK_WINDOW(window));
}

static void
window_move_window_cb(VteTerminal* terminal,
                      guint x,
                      guint y,
                      VteappWindow* window)
{
        if (!options.allow_window_ops)
                return;

#if VTE_GTK == 3
        gtk_window_move(GTK_WINDOW(window), x, y);
#elif VTE_GTK == 4
        // FIXMEgtk4
#endif
}

#if VTE_GTK == 4

static void
window_toplevel_notify_state_cb(GdkToplevel* toplevel,
                                GParamSpec* pspec,
                                VteappWindow* window)
{
        window->toplevel_state = gdk_toplevel_get_state(toplevel);
        window_update_fullscreen_state(window);
}

#endif /* VTE_GTK == 4 */

static void
window_notify_cb(GObject* object,
                 GParamSpec* pspec,
                 VteappWindow* window)
{
        if (pspec->owner_type != VTE_TYPE_TERMINAL)
                return;

        GValue value G_VALUE_INIT;
        g_object_get_property(object, pspec->name, &value);
        auto str = g_strdup_value_contents(&value);
        g_value_unset(&value);

        verbose_print("NOTIFY property \"%s\" value %s\n", pspec->name, str);
        g_free(str);
}

static void
window_refresh_window_cb(VteTerminal* terminal,
                         VteappWindow* window)
{
        gtk_widget_queue_draw(GTK_WIDGET(window));
}

static void
window_resize_window_cb(VteTerminal* terminal,
                        guint columns,
                        guint rows,
                        VteappWindow* window)
{
        if (!options.allow_window_ops)
                return;
        if (columns < MIN_COLUMNS || rows < MIN_ROWS)
                return;

        vte_terminal_set_size(window->terminal, columns, rows);
        vteapp_window_resize(window);
}

static void
window_selection_changed_cb(VteTerminal* terminal,
                            VteappWindow* window)
{
        window_update_copy_sensitivity(window);
}

static void
window_input_enabled_state_cb(GAction* action,
                              GParamSpec* pspec,
                              VteappWindow* window)
{
        gtk_widget_set_visible(window->readonly_emblem,
                               !g_variant_get_boolean(g_action_get_state(action)));
}

static void
window_search_popover_closed_cb(GtkPopover* popover,
                                VteappWindow* window)
{
        if (gtk_toggle_button_get_active(window->find_button))
                gtk_toggle_button_set_active(window->find_button, false);
}

static void
window_find_button_toggled_cb(GtkToggleButton* button,
                              VteappWindow* window)
{
        auto active = gtk_toggle_button_get_active(button);

        if (gtk_widget_get_visible(GTK_WIDGET(window->search_popover)) != active)
                gtk_widget_set_visible(GTK_WIDGET(window->search_popover), active);
}

G_DEFINE_TYPE(VteappWindow, vteapp_window, GTK_TYPE_APPLICATION_WINDOW)

static void
vteapp_window_init(VteappWindow* window)
{
        gtk_widget_init_template(GTK_WIDGET(window));

        window->child_pid = -1;
}

static void
vteapp_window_constructed(GObject *object)
{
        VteappWindow* window = VTEAPP_WINDOW(object);

        G_OBJECT_CLASS(vteapp_window_parent_class)->constructed(object);

        gtk_window_set_title(GTK_WINDOW(window),
                             options.title && options.title[0] ? options.title : "Terminal");

        if (options.no_decorations)
                gtk_window_set_decorated(GTK_WINDOW(window), false);

        /* Create terminal and connect scrollbar */
        window->terminal = reinterpret_cast<VteTerminal*>(vteapp_terminal_new());

        gtk_widget_set_hexpand(GTK_WIDGET(window->terminal), true);
        gtk_widget_set_vexpand(GTK_WIDGET(window->terminal), true);

        auto margin = options.extra_margin;
        if (margin >= 0) {
                gtk_widget_set_margin_start(GTK_WIDGET(window->terminal), margin);
                gtk_widget_set_margin_end(GTK_WIDGET(window->terminal), margin);
                gtk_widget_set_margin_top(GTK_WIDGET(window->terminal), margin);
                gtk_widget_set_margin_bottom(GTK_WIDGET(window->terminal), margin);
        }

        if (!options.no_scrollbar) {
                auto vadj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(window->terminal));
#if VTE_GTK == 3
                gtk_range_set_adjustment(GTK_RANGE(window->scrollbar), vadj);
#elif VTE_GTK == 4
                gtk_scrollbar_set_adjustment(GTK_SCROLLBAR(window->scrollbar), vadj);
#endif
        }

        if (options.no_scrollbar) {
#if VTE_GTK == 3
                gtk_widget_destroy(GTK_WIDGET(window->scrollbar));
#elif VTE_GTK == 4
                gtk_grid_remove(GTK_GRID(window->window_grid), GTK_WIDGET(window->scrollbar));
#endif
                window->scrollbar = nullptr;
        }

        /* Create actions */
        GActionEntry const entries[] = {
                { "copy",        window_action_copy_cb,       "s", nullptr, nullptr },
                { "copy-match",  window_action_copy_match_cb, "s", nullptr, nullptr },
                { "paste",       window_action_paste_cb,  nullptr, nullptr, nullptr },
                { "reset",       window_action_reset_cb,      "b", nullptr, nullptr },
                { "find",        window_action_find_cb,   nullptr, nullptr, nullptr },
                { "fullscreen",  nullptr,                 nullptr, "false", window_action_fullscreen_state_cb },
        };

        GActionMap* map = G_ACTION_MAP(window);
        g_action_map_add_action_entries(map, entries, G_N_ELEMENTS(entries), window);

        /* Property actions */
        auto action = vte::glib::take_ref
                (g_property_action_new("input-enabled", window->terminal, "input-enabled"));
        g_action_map_add_action(map, G_ACTION(action.get()));
        g_signal_connect(action.get(), "notify::state", G_CALLBACK(window_input_enabled_state_cb), window);

#if VTE_GTK == 4
        auto gear_popover = gtk_menu_button_get_popover(GTK_MENU_BUTTON(window->gear_button));
        gtk_widget_set_halign(GTK_WIDGET(gear_popover), GTK_ALIGN_END);
#endif

        /* Find */
        window->search_popover = vteapp_search_popover_new(window->terminal,
                                                           GTK_WIDGET(window->find_button));

        g_signal_connect(window->search_popover, "closed",
                         G_CALLBACK(window_search_popover_closed_cb), window);
        g_signal_connect(window->find_button, "toggled",
                         G_CALLBACK(window_find_button_toggled_cb), window);

        /* Clipboard */
#if VTE_GTK == 3
        window->clipboard = gtk_widget_get_clipboard(GTK_WIDGET(window), GDK_SELECTION_CLIPBOARD);
        g_signal_connect(window->clipboard, "owner-change", G_CALLBACK(window_clipboard_owner_change_cb), window);
#elif VTE_GTK == 4
        window->clipboard = gtk_widget_get_clipboard(GTK_WIDGET(window));
        g_signal_connect(window->clipboard, "notify::formats", G_CALLBACK(window_clipboard_formats_notify_cb), window);
#endif /* VTE_GTK */

        /* Set ARGB visual */
        if (options.transparency_percent >= 0) {
#if VTE_GTK == 3
                if (!options.no_argb_visual) {
                        auto screen = gtk_widget_get_screen(GTK_WIDGET(window));
                        auto visual = gdk_screen_get_rgba_visual(screen);
                        if (visual != nullptr)
                                gtk_widget_set_visual(GTK_WIDGET(window), visual);
                }

                /* Without this transparency doesn't work; see bug #729884. */
                gtk_widget_set_app_paintable(GTK_WIDGET(window), true);

#elif VTE_GTK == 4
                // FIXMEgtk4
#endif /* VTE_GTK == 3 */
        }

        /* Signals */
#if VTE_GTK == 3
        g_signal_connect(window->terminal, "popup-menu", G_CALLBACK(window_popup_menu_cb), window);
        g_signal_connect(window->terminal, "button-press-event", G_CALLBACK(window_button_press_cb), window);
#elif VTE_GTK == 4
        // FIXMEgtk4
#endif
        g_signal_connect(window->terminal, "char-size-changed", G_CALLBACK(window_cell_size_changed_cb), window);
        g_signal_connect(window->terminal, "child-exited", G_CALLBACK(window_child_exited_cb), window);
        g_signal_connect(window->terminal, "decrease-font-size", G_CALLBACK(window_decrease_font_size_cb), window);
        g_signal_connect(window->terminal, "deiconify-window", G_CALLBACK(window_deiconify_window_cb), window);
        g_signal_connect(window->terminal, "iconify-window", G_CALLBACK(window_iconify_window_cb), window);
        g_signal_connect(window->terminal, "increase-font-size", G_CALLBACK(window_increase_font_size_cb), window);
        g_signal_connect(window->terminal, "lower-window", G_CALLBACK(window_lower_window_cb), window);
        g_signal_connect(window->terminal, "maximize-window", G_CALLBACK(window_maximize_window_cb), window);
        g_signal_connect(window->terminal, "move-window", G_CALLBACK(window_move_window_cb), window);
        g_signal_connect(window->terminal, "raise-window", G_CALLBACK(window_raise_window_cb), window);
        g_signal_connect(window->terminal, "refresh-window", G_CALLBACK(window_refresh_window_cb), window);
        g_signal_connect(window->terminal, "resize-window", G_CALLBACK(window_resize_window_cb), window);
        g_signal_connect(window->terminal, "restore-window", G_CALLBACK(window_restore_window_cb), window);
        g_signal_connect(window->terminal, "selection-changed", G_CALLBACK(window_selection_changed_cb), window);
        g_signal_connect(window->terminal, "window-title-changed", G_CALLBACK(window_window_title_changed_cb), window);
        if (options.object_notifications)
                g_signal_connect(window->terminal, "notify", G_CALLBACK(window_notify_cb), window);

        /* Settings */
#if VTE_GTK == 3
        if (options.no_double_buffer) {
                G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
                gtk_widget_set_double_buffered(GTK_WIDGET(window->terminal), false);
                G_GNUC_END_IGNORE_DEPRECATIONS;
        }
#endif /* VTE_GTK == 3 */

        if (options.encoding != nullptr) {
                auto error = vte::glib::Error{};
                if (!vte_terminal_set_encoding(window->terminal, options.encoding, error))
                        g_printerr("Failed to set encoding: %s\n", error.message());
        }

        if (options.word_char_exceptions != nullptr)
                vte_terminal_set_word_char_exceptions(window->terminal, options.word_char_exceptions);

        vte_terminal_set_allow_hyperlink(window->terminal, !options.no_hyperlink);
        vte_terminal_set_audible_bell(window->terminal, options.audible_bell);
        vte_terminal_set_allow_bold(window->terminal, !options.no_bold);
        vte_terminal_set_bold_is_bright(window->terminal, options.bold_is_bright);
        vte_terminal_set_cell_height_scale(window->terminal, options.cell_height_scale);
        vte_terminal_set_cell_width_scale(window->terminal, options.cell_width_scale);
        vte_terminal_set_cjk_ambiguous_width(window->terminal, options.cjk_ambiguous_width);
        vte_terminal_set_cursor_blink_mode(window->terminal, options.cursor_blink_mode);
        vte_terminal_set_cursor_shape(window->terminal, options.cursor_shape);
        vte_terminal_set_enable_bidi(window->terminal, !options.no_bidi);
        vte_terminal_set_enable_shaping(window->terminal, !options.no_shaping);
        vte_terminal_set_enable_fallback_scrolling(window->terminal, !options.no_fallback_scrolling);
        vte_terminal_set_mouse_autohide(window->terminal, true);
        vte_terminal_set_rewrap_on_resize(window->terminal, !options.no_rewrap);
        vte_terminal_set_scroll_on_output(window->terminal, false);
        vte_terminal_set_scroll_on_keystroke(window->terminal, true);
        vte_terminal_set_scroll_unit_is_pixels(window->terminal, options.scroll_unit_is_pixels);
        vte_terminal_set_scrollback_lines(window->terminal, options.scrollback_lines);
        vte_terminal_set_text_blink_mode(window->terminal, options.text_blink_mode);

        /* Style */
        if (options.font_string != nullptr) {
                auto desc = vte::take_freeable(pango_font_description_from_string(options.font_string));
                vte_terminal_set_font(window->terminal, desc.get());
        }

        auto fg = options.get_color_fg();
        auto bg = options.get_color_bg();
        vte_terminal_set_colors(window->terminal, &fg, &bg, nullptr, 0);
        if (options.cursor_bg_color_set)
                vte_terminal_set_color_cursor(window->terminal, &options.cursor_bg_color);
        if (options.cursor_fg_color_set)
                vte_terminal_set_color_cursor_foreground(window->terminal, &options.cursor_fg_color);
        if (options.hl_bg_color_set)
                vte_terminal_set_color_highlight(window->terminal, &options.hl_bg_color);
        if (options.hl_fg_color_set)
                vte_terminal_set_color_highlight_foreground(window->terminal, &options.hl_fg_color);

        if (options.whole_window_transparent)
                gtk_widget_set_opacity (GTK_WIDGET (window), options.get_alpha());

        /* Dingus */
        if (!options.no_builtin_dingus)
                vteapp_window_add_dingus(window, builtin_dingus);
        if (options.dingus != nullptr)
                vteapp_window_add_dingus(window, options.dingus);

        /* Done! */
        if (options.use_scrolled_window) {
#if VTE_GTK == 3
                auto sw = gtk_scrolled_window_new(nullptr, nullptr);
                gtk_container_add(GTK_CONTAINER(sw), GTK_WIDGET(window->terminal));
#elif VTE_GTK == 4
                auto sw = gtk_scrolled_window_new();
                gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), GTK_WIDGET(window->terminal));
#endif /* VTE_GTK */
                gtk_grid_attach(GTK_GRID(window->window_grid), sw,
                                0, 0, 1, 1);
                gtk_widget_set_halign(GTK_WIDGET(sw), GTK_ALIGN_FILL);
                gtk_widget_set_valign(GTK_WIDGET(sw), GTK_ALIGN_FILL);
                gtk_widget_show(sw);
        } else {
                gtk_grid_attach(GTK_GRID(window->window_grid), GTK_WIDGET(window->terminal),
                                0, 0, 1, 1);
        }

        gtk_widget_set_halign(GTK_WIDGET(window->terminal), GTK_ALIGN_FILL);
        gtk_widget_set_valign(GTK_WIDGET(window->terminal), GTK_ALIGN_FILL);
        gtk_widget_show(GTK_WIDGET(window->terminal));

        window_update_paste_sensitivity(window);
        window_update_copy_sensitivity(window);

        gtk_widget_grab_focus(GTK_WIDGET(window->terminal));

        /* Sanity check */
        g_assert(!gtk_widget_get_realized(GTK_WIDGET(window)));
}

static void
vteapp_window_dispose(GObject *object)
{
        VteappWindow* window = VTEAPP_WINDOW(object);

        if (window->clipboard != nullptr) {
                g_signal_handlers_disconnect_by_func(window->clipboard,
#if VTE_GTK == 3
                                                     (void*)window_clipboard_owner_change_cb,
#elif VTE_GTK == 4
                                                     (void*)window_clipboard_formats_notify_cb,
#endif
                                                     window);
                window->clipboard = nullptr;
        }

        if (window->search_popover != nullptr) {
#if VTE_GTK == 3
                gtk_widget_destroy(window->search_popover);
#elif VTE_GTK == 4
                gtk_widget_unparent(window->search_popover); // this destroys the popover
#endif /* VTE_GTK */
                window->search_popover = nullptr;
        }

        // Disconnect all signal handlers from the terminal
        g_signal_handlers_disconnect_matched(window->terminal,
                                             GSignalMatchType(G_SIGNAL_MATCH_DATA),
                                             0, // signal id
                                             0, // detail quark
                                             nullptr, // closure
                                             nullptr, // func
                                             window);

        G_OBJECT_CLASS(vteapp_window_parent_class)->dispose(object);
}

static void
vteapp_window_realize(GtkWidget* widget)
{
        GTK_WIDGET_CLASS(vteapp_window_parent_class)->realize(widget);

        /* Now we can know the CSD size, and thus apply the geometry. */
        VteappWindow* window = VTEAPP_WINDOW(widget);
        verbose_print("VteappWindow::realize\n");

#if VTE_GTK == 3
        auto win = gtk_widget_get_window(GTK_WIDGET(window));
        window->window_state = gdk_window_get_state(win);
#elif VTE_GTK == 4
        auto surface = gtk_native_get_surface(GTK_NATIVE(widget));
        window->toplevel_state = gdk_toplevel_get_state(GDK_TOPLEVEL(surface));
        g_signal_connect(surface, "notify::state",
                         G_CALLBACK(window_toplevel_notify_state_cb), window);
#endif

        window_update_fullscreen_state(window);

        vteapp_window_resize(window);
}

static void
vteapp_window_unrealize(GtkWidget* widget)
{
#if VTE_GTK == 4
        auto window = VTEAPP_WINDOW(widget);
        auto toplevel = gtk_native_get_surface(GTK_NATIVE(widget));
        g_signal_handlers_disconnect_by_func(toplevel,
                                             (void*)window_toplevel_notify_state_cb,
                                             window);
#endif

        GTK_WIDGET_CLASS(vteapp_window_parent_class)->unrealize(widget);
}

static void
vteapp_window_show(GtkWidget* widget)
{
        GTK_WIDGET_CLASS(vteapp_window_parent_class)->show(widget);

        /* Re-apply the geometry. */
        VteappWindow* window = VTEAPP_WINDOW(widget);
        verbose_print("VteappWindow::show\n");
        vteapp_window_resize(window);
}

#if VTE_GTK == 3

static void
vteapp_window_style_updated(GtkWidget* widget)
{
        GTK_WIDGET_CLASS(vteapp_window_parent_class)->style_updated(widget);

        /* Re-apply the geometry. */
        VteappWindow* window = VTEAPP_WINDOW(widget);
        verbose_print("VteappWindow::style-update\n");
        vteapp_window_resize(window);
}

static gboolean
vteapp_window_state_event (GtkWidget* widget,
                           GdkEventWindowState* event)
{
        auto window = VTEAPP_WINDOW(widget);
        window->window_state = event->new_window_state;

        if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
                window_update_fullscreen_state(window);

        return GTK_WIDGET_CLASS(vteapp_window_parent_class)->window_state_event(widget, event);
}

#endif /* VTE_GTK == 3 */

static void
vteapp_window_class_init(VteappWindowClass* klass)
{
        GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
        gobject_class->constructed  = vteapp_window_constructed;
        gobject_class->dispose  = vteapp_window_dispose;

        GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
        widget_class->realize = vteapp_window_realize;
        widget_class->unrealize = vteapp_window_unrealize;
        widget_class->show = vteapp_window_show;

#if VTE_GTK == 3
        widget_class->style_updated = vteapp_window_style_updated;
        widget_class->window_state_event = vteapp_window_state_event;
#elif VTE_GTK == 4
        // FIXMEgtk4 window state event
#endif

        gtk_widget_class_set_template_from_resource(widget_class, "/org/gnome/vte/app/ui/window.ui");

        gtk_widget_class_bind_template_child(widget_class, VteappWindow, window_grid);
        gtk_widget_class_bind_template_child(widget_class, VteappWindow, scrollbar);
        /* gtk_widget_class_bind_template_child(widget_class, VteappWindow, notification_grid); */
        gtk_widget_class_bind_template_child(widget_class, VteappWindow, readonly_emblem);
        /* gtk_widget_class_bind_template_child(widget_class, VteappWindow, copy_button); */
        /* gtk_widget_class_bind_template_child(widget_class, VteappWindow, paste_button); */
        gtk_widget_class_bind_template_child(widget_class, VteappWindow, find_button);
        gtk_widget_class_bind_template_child(widget_class, VteappWindow, gear_button);
}

static VteappWindow*
vteapp_window_new(GApplication* app)
{
        return reinterpret_cast<VteappWindow*>(g_object_new(VTEAPP_TYPE_WINDOW,
                                                            "application", app,
#if VTE_GTK == 4 && GTK_CHECK_VERSION(4, 2, 0)
                                                            "handle-menubar-accel", false,
#endif
                                                            nullptr));
}

/* application */

#define VTEAPP_TYPE_APPLICATION         (vteapp_application_get_type())
#define VTEAPP_APPLICATION(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), VTEAPP_TYPE_APPLICATION, VteappApplication))
#define VTEAPP_APPLICATION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), VTEAPP_TYPE_APPLICATION, VteappApplicationClass))
#define VTEAPP_IS_APPLICATION(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), VTEAPP_TYPE_APPLICATION))
#define VTEAPP_IS_APPLICATION_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), VTEAPP_TYPE_APPLICATION))
#define VTEAPP_APPLICATION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), VTEAPP_TYPE_APPLICATION, VteappApplicationClass))

typedef struct _VteappApplication       VteappApplication;
typedef struct _VteappApplicationClass  VteappApplicationClass;

struct _VteappApplication {
        GtkApplication parent;

        guint input_source;
};

struct _VteappApplicationClass {
        GtkApplicationClass parent;
};

static GType vteapp_application_get_type(void);

static void
app_action_new_cb(GSimpleAction* action,
                  GVariant* parameter,
                  void* data)
{
        GApplication* application = G_APPLICATION(data);
        auto window = vteapp_window_new(application);
        vteapp_window_parse_geometry(window);
        gtk_window_present(GTK_WINDOW(window));
        vteapp_window_launch(window);
}

static void
app_action_close_cb(GSimpleAction* action,
                    GVariant* parameter,
                    void* data)
{
        GtkApplication* application = GTK_APPLICATION(data);
        auto window = gtk_application_get_active_window(application);
        if (window == nullptr)
                return;

#if VTE_GTK == 3
        gtk_widget_destroy(GTK_WIDGET(window));
#elif VTE_GTK == 4
        gtk_window_destroy(GTK_WINDOW(window));
#endif
}

static gboolean
app_stdin_readable_cb(int fd,
                      GIOCondition condition,
                      VteappApplication* application)
{
        auto eos = bool{false};
        if (condition & G_IO_IN) {
                auto window = gtk_application_get_active_window(GTK_APPLICATION(application));
                auto terminal = VTEAPP_IS_WINDOW(window) ? VTEAPP_WINDOW(window)->terminal : nullptr;

                char buf[4096];
                auto r = int{0};
                do {
                        errno = 0;
                        r = read(fd, buf, sizeof(buf));
                        if (r > 0 && terminal != nullptr)
                                vte_terminal_feed(terminal, buf, r);
                } while (r > 0 || errno == EINTR);

                if (r == 0)
                        eos = true;
        }

        if (eos) {
                application->input_source = 0;
                return G_SOURCE_REMOVE;
        }

        return G_SOURCE_CONTINUE;
}

#if VTE_GTK == 3

static void
app_clipboard_targets_received_cb(GtkClipboard* clipboard,
                                  GdkAtom* targets,
                                  int n_targets,
                                  VteappApplication* application)
{
        verbose_print("Clipboard has %d targets:", n_targets);

        for (auto i = 0; i < n_targets; ++i) {
                auto atom_name = vte::glib::take_string(gdk_atom_name(targets[i]));
                verbose_print(" %s", atom_name.get());
        }
        verbose_print("\n");
}

static void
app_clipboard_owner_change_cb(GtkClipboard* clipboard,
                              GdkEvent* event G_GNUC_UNUSED,
                              VteappApplication* application)
{
        verbose_print("Clipboard owner-change, requesting targets\n");

        /* We can do this without holding a reference to @application since
         * the application lives as long as the process.
         */
        gtk_clipboard_request_targets(clipboard,
                                      (GtkClipboardTargetsReceivedFunc)app_clipboard_targets_received_cb,
                                      application);
}

#elif VTE_GTK == 4

static void
app_clipboard_changed_cb(GdkClipboard* clipboard,
                         VteappApplication* application)
{
        auto formats = gdk_clipboard_get_formats(clipboard);
        auto str = vte::glib::take_string(gdk_content_formats_to_string(formats));

        verbose_print("Clipboard owner changed, targets now %s\n", str.get());
}

#endif /* VTE_GTK */

G_DEFINE_TYPE(VteappApplication, vteapp_application, GTK_TYPE_APPLICATION)

static void
vteapp_application_init(VteappApplication* application)
{
        g_object_set(gtk_settings_get_default(),
                     "gtk-enable-accels", FALSE,
#if VTE_GTK == 3
                     "gtk-enable-mnemonics", FALSE,
                     /* Make gtk+ CSD not steal F10 from the terminal */
                     "gtk-menu-bar-accel", nullptr,
#endif
                     nullptr);

        if (options.css) {
#if VTE_GTK == 3
                gtk_style_context_add_provider_for_screen(gdk_screen_get_default (),
                                                          GTK_STYLE_PROVIDER(options.css.get()),
                                                          GTK_STYLE_PROVIDER_PRIORITY_USER);
#elif VTE_GTK == 4
                gtk_style_context_add_provider_for_display(gdk_display_get_default (),
                                                          GTK_STYLE_PROVIDER(options.css.get()),
                                                          GTK_STYLE_PROVIDER_PRIORITY_USER);
#endif
        }

        if (options.feed_stdin) {
                g_unix_set_fd_nonblocking(STDIN_FILENO, true, nullptr);
                application->input_source = g_unix_fd_add(STDIN_FILENO,
                                                          GIOCondition(G_IO_IN | G_IO_HUP | G_IO_ERR),
                                                          (GUnixFDSourceFunc)app_stdin_readable_cb,
                                                          application);
        }

        if (options.track_clipboard_targets) {
#if VTE_GTK == 3
                auto clipboard = gtk_clipboard_get_for_display(gdk_display_get_default(),
                                                               GDK_SELECTION_CLIPBOARD);
                app_clipboard_owner_change_cb(clipboard, nullptr, application);
                g_signal_connect(clipboard, "owner-change",
                                 G_CALLBACK(app_clipboard_owner_change_cb), application);

#elif VTE_GTK == 4
                auto clipboard = gdk_display_get_clipboard(gdk_display_get_default());
                app_clipboard_changed_cb(clipboard, application);
                g_signal_connect(clipboard, "changed",
                                 G_CALLBACK(app_clipboard_changed_cb), application);
#endif /* VTE_GTK */
        }
}

static void
vteapp_application_dispose(GObject* object)
{
        VteappApplication* application = VTEAPP_APPLICATION(object);

        if (application->input_source != 0) {
                g_source_remove(application->input_source);
                application->input_source = 0;
        }

        G_OBJECT_CLASS(vteapp_application_parent_class)->dispose(object);
}

static void
vteapp_application_startup(GApplication* application)
{
        /* Create actions */
        GActionEntry const entries[] = {
                { "new",   app_action_new_cb,   nullptr, nullptr, nullptr },
                { "close", app_action_close_cb, nullptr, nullptr, nullptr },
        };

        GActionMap* map = G_ACTION_MAP(application);
        g_action_map_add_action_entries(map, entries, G_N_ELEMENTS(entries), application);

        g_application_set_resource_base_path (application, "/org/gnome/vte/app");

        G_APPLICATION_CLASS(vteapp_application_parent_class)->startup(application);
}

static void
vteapp_application_activate(GApplication* application)
{
        auto action = g_action_map_lookup_action(G_ACTION_MAP(application), "new");
        g_action_activate(action, nullptr);
}

static void
vteapp_application_class_init(VteappApplicationClass* klass)
{
        GObjectClass* object_class = G_OBJECT_CLASS(klass);
        object_class->dispose = vteapp_application_dispose;

        GApplicationClass* application_class = G_APPLICATION_CLASS(klass);
        application_class->startup = vteapp_application_startup;
        application_class->activate = vteapp_application_activate;
}

static GApplication*
vteapp_application_new(void)
{
        return reinterpret_cast<GApplication*>(g_object_new(VTEAPP_TYPE_APPLICATION,
                                                            "application-id", "org.gnome.Vte.Application",
                                                            "flags", guint(G_APPLICATION_NON_UNIQUE),
                                                            nullptr));
}

/* main */

int
main(int argc,
     char *argv[])
{
        setlocale(LC_ALL, "");

	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");

        /* Not interested in silly debug spew, bug #749195 */
        if (g_getenv("G_ENABLE_DIAGNOSTIC") == nullptr)
                g_setenv("G_ENABLE_DIAGNOSTIC", "0", true);

       g_set_prgname("Terminal");
       g_set_application_name("Terminal");

       auto error = vte::glib::Error{};
       if (!options.parse_argv(argc, argv, error)) {
               g_printerr("Error parsing arguments: %s\n", error.message());
               return EXIT_FAILURE;
       }

        if (g_getenv("VTE_CJK_WIDTH") != nullptr)
                verbose_printerr("VTE_CJK_WIDTH is not supported anymore, use --cjk-width instead\n");

       if (options.version) {
               g_print("VTE Application %s %s\n", VERSION, vte_get_features());
               return EXIT_SUCCESS;
       }

#if VTE_GTK == 3
       if (options.debug)
               gdk_window_set_debug_updates(true);
#endif /* VTE_GTK == 3 */

#ifdef VTE_DEBUG
       if (options.test_mode) {
               vte_set_test_flags(VTE_TEST_FLAGS_ALL);
               options.allow_window_ops = true;
       }
#endif

       auto reset_termios = bool{false};
       struct termios saved_tcattr;
       if (options.feed_stdin && isatty(STDIN_FILENO)) {
               /* Put terminal in raw mode */

               struct termios tcattr;
               if (tcgetattr(STDIN_FILENO, &tcattr) == 0) {
                       saved_tcattr = tcattr;
                       cfmakeraw(&tcattr);
                       if (tcsetattr(STDIN_FILENO, TCSANOW, &tcattr) == 0)
                               reset_termios = true;
               }
       }

       auto app = vte::glib::take_ref(vteapp_application_new());
       auto rv = g_application_run(app.get(), 0, nullptr);

       if (reset_termios)
               tcsetattr(STDIN_FILENO, TCSANOW, &saved_tcattr);

       return rv;
}
