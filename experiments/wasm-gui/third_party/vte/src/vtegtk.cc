/*
 * Copyright (C) 2001-2004,2009,2010 Red Hat, Inc.
 * Copyright © 2008, 2009, 2010, 2015 Christian Persch
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * SECTION: vte-terminal
 * @short_description: A terminal widget implementation
 *
 * A VteTerminal is a terminal emulator implemented as a GTK3 widget.
 *
 * Note that altough #VteTerminal implements the #GtkScrollable interface,
 * you should not place a #VteTerminal inside a #GtkScrolledWindow
 * container, since they are incompatible. Instead, pack the terminal in
 * a horizontal #GtkBox together with a #GtkScrollbar which uses the
 * #GtkAdjustment returned from gtk_scrollable_get_vadjustment().
 */

#include "config.h"

#include <new> /* placement new */
#include <exception>
#include <stdexcept>

#include <pwd.h>

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include "vte/vteenums.h"
#include "vte/vtepty.h"
#include "vte/vteterminal.h"
#include "vte/vtetypebuiltins.h"

#include "debug.h"
#include "glib-glue.hh"
#include "gobject-glue.hh"
#include "marshal.h"
#include "reaper.hh"
#include "vtedefines.hh"
#include "vteinternal.hh"
#include "widget.hh"

#include "vtegtk.hh"
#include "vteptyinternal.hh"
#include "vteregexinternal.hh"

#ifdef WITH_A11Y
#if VTE_GTK == 3
#include "vteaccess.h"
#else
#undef WITH_A11Y
#endif /* VTE_GTK == 3 */
#endif /* WITH_A11Y */

#ifdef WITH_ICU
#include "icu-glue.hh"
#endif

#define I_(string) (g_intern_static_string(string))
#define _VTE_PARAM_DEPRECATED (_vte_debug_on(VTE_DEBUG_SIGNALS) ? G_PARAM_DEPRECATED : 0)

#define VTE_TERMINAL_CSS_NAME "vte-terminal"

/* Note that the exact priority used is an implementation detail subject to change
 * and *not* an API guarantee.
 * */
#if VTE_GTK == 3
#define VTE_TERMINAL_CSS_PRIORITY (GTK_STYLE_PROVIDER_PRIORITY_APPLICATION)
#elif VTE_GTK == 4
#define VTE_TERMINAL_CSS_PRIORITY (GTK_STYLE_PROVIDER_PRIORITY_APPLICATION - 2)
#endif

template<typename T>
constexpr bool check_enum_value(T value) noexcept;

struct _VteTerminalClassPrivate {
        GtkStyleProvider *style_provider;
};

#if VTE_GTK == 4

static void
style_provider_parsing_error_cb(GtkCssProvider* provider,
                                void* section,
                                GError* error)
{
        g_assert_no_error(error);
}

#endif


class VteTerminalPrivate {
public:
        VteTerminalPrivate(VteTerminal* terminal)
                : m_widget{std::make_shared<vte::platform::Widget>(terminal)}
        {
        }

        ~VteTerminalPrivate() = default;

        VteTerminalPrivate(VteTerminalPrivate const&) = delete;
        VteTerminalPrivate(VteTerminalPrivate&&) = delete;

        VteTerminalPrivate& operator=(VteTerminalPrivate const&) = delete;
        VteTerminalPrivate& operator=(VteTerminalPrivate&&) = delete;

        auto get() const /* throws */
        {
                if (!m_widget)
                        throw std::runtime_error{"Widget is nullptr"};

                return m_widget.get();
        }

        void reset()
        {
                if (m_widget)
                        m_widget->dispose();

                m_widget.reset();
        }

private:
        std::shared_ptr<vte::platform::Widget> m_widget;
};

#ifdef VTE_DEBUG
G_DEFINE_TYPE_WITH_CODE(VteTerminal, vte_terminal, GTK_TYPE_WIDGET,
                        {
                                VteTerminal_private_offset =
                                        g_type_add_instance_private(g_define_type_id, sizeof(VteTerminalPrivate));
                        }
                        g_type_add_class_private (g_define_type_id, sizeof (VteTerminalClassPrivate));
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_SCROLLABLE, nullptr)
                        if (_vte_debug_on(VTE_DEBUG_LIFECYCLE)) {
                                g_printerr("vte_terminal_get_type()\n");
                        })
#else
G_DEFINE_TYPE_WITH_CODE(VteTerminal, vte_terminal, GTK_TYPE_WIDGET,
                        {
                                VteTerminal_private_offset =
                                        g_type_add_instance_private(g_define_type_id, sizeof(VteTerminalPrivate));
                        }
                        g_type_add_class_private (g_define_type_id, sizeof (VteTerminalClassPrivate));
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_SCROLLABLE, nullptr))
#endif

static inline auto
get_private(VteTerminal* terminal)
{
        return reinterpret_cast<VteTerminalPrivate*>(vte_terminal_get_instance_private(terminal));
}

#define PRIVATE(t) (get_private(t))

static inline auto
get_widget(VteTerminal* terminal) /* throws */
{
        return get_private(terminal)->get();
}

#define WIDGET(t) (get_widget(t))

vte::terminal::Terminal*
_vte_terminal_get_impl(VteTerminal* terminal) /* throws */
{
        return WIDGET(terminal)->terminal();
}

#define IMPL(t) (_vte_terminal_get_impl(t))

guint signals[LAST_SIGNAL];
GParamSpec *pspecs[LAST_PROP];
GTimer *process_timer;
uint64_t g_test_flags = 0;

static bool
valid_color(GdkRGBA const* color) noexcept
{
        return color->red >= 0. && color->red <= 1. &&
               color->green >= 0. && color->green <= 1. &&
               color->blue >= 0. && color->blue <= 1. &&
               color->alpha >= 0. && color->alpha <= 1.;
}

static vte::platform::ClipboardFormat
clipboard_format_from_vte(VteFormat format)
{
        switch (format) {
        case VTE_FORMAT_TEXT: return vte::platform::ClipboardFormat::TEXT;
        case VTE_FORMAT_HTML: return vte::platform::ClipboardFormat::HTML;
        default: throw std::runtime_error{"Unknown VteFormat enum value"};
        }
}

static void
vte_terminal_set_hadjustment(VteTerminal *terminal,
                             GtkAdjustment *adjustment) noexcept
try
{
        g_return_if_fail(adjustment == nullptr || GTK_IS_ADJUSTMENT(adjustment));
        WIDGET(terminal)->set_hadjustment(vte::glib::make_ref_sink(adjustment));
}
catch (...)
{
        vte::log_exception();
}

static void
vte_terminal_set_vadjustment(VteTerminal *terminal,
                             GtkAdjustment *adjustment) noexcept
try
{
        g_return_if_fail(adjustment == nullptr || GTK_IS_ADJUSTMENT(adjustment));
        WIDGET(terminal)->set_vadjustment(vte::glib::make_ref_sink(adjustment));
}
catch (...)
{
        vte::log_exception();
}

static void
vte_terminal_set_hscroll_policy(VteTerminal *terminal,
                                GtkScrollablePolicy policy) noexcept
try
{
        WIDGET(terminal)->set_hscroll_policy(policy);
}
catch (...)
{
        vte::log_exception();
}

static void
vte_terminal_set_vscroll_policy(VteTerminal *terminal,
                                GtkScrollablePolicy policy) noexcept
try
{
        WIDGET(terminal)->set_vscroll_policy(policy);
}
catch (...)
{
        vte::log_exception();
}

static void
vte_terminal_real_copy_clipboard(VteTerminal *terminal) noexcept
try
{
	WIDGET(terminal)->copy(vte::platform::ClipboardType::CLIPBOARD,
                               vte::platform::ClipboardFormat::TEXT);
}
catch (...)
{
        vte::log_exception();
}

static void
vte_terminal_real_paste_clipboard(VteTerminal *terminal) noexcept
try
{
	WIDGET(terminal)->paste(vte::platform::ClipboardType::CLIPBOARD);
}
catch (...)
{
        vte::log_exception();
}

#if VTE_GTK == 3

static void
vte_terminal_style_updated (GtkWidget *widget) noexcept
try
{
	VteTerminal *terminal = VTE_TERMINAL(widget);

        GTK_WIDGET_CLASS (vte_terminal_parent_class)->style_updated (widget);

        WIDGET(terminal)->style_updated();
}
catch (...)
{
        vte::log_exception();
}

static gboolean
vte_terminal_key_press(GtkWidget *widget,
                       GdkEventKey *event) noexcept
try
{
	VteTerminal *terminal = VTE_TERMINAL(widget);

        /* We do NOT want chain up to GtkWidget::key-press-event, since that would
         * cause GtkWidget's keybindings to be handled and consumed. However we'll
         * have to handle the one sane binding (Shift-F10 or MenuKey, to pop up the
         * context menu) ourself, so for now we simply skip the offending keybinding
         * in class_init.
         */

	/* First, check if GtkWidget's behavior already does something with
	 * this key. */
	if (GTK_WIDGET_CLASS(vte_terminal_parent_class)->key_press_event) {
		if ((GTK_WIDGET_CLASS(vte_terminal_parent_class))->key_press_event(widget,
                                                                                   event)) {
			return TRUE;
		}
	}

        return WIDGET(terminal)->event_key_press(event);
}
catch (...)
{
        vte::log_exception();
        return true;
}

static gboolean
vte_terminal_key_release(GtkWidget *widget,
                         GdkEventKey *event) noexcept
try
{
	VteTerminal *terminal = VTE_TERMINAL(widget);
        return WIDGET(terminal)->event_key_release(event);
}
catch (...)
{
        vte::log_exception();
        return true;
}

static gboolean
vte_terminal_motion_notify(GtkWidget *widget,
                           GdkEventMotion *event) noexcept
try
{
        VteTerminal *terminal = VTE_TERMINAL(widget);
        return WIDGET(terminal)->event_motion_notify(event);
}
catch (...)
{
        vte::log_exception();
        return true;
}

static gboolean
vte_terminal_button_press(GtkWidget *widget,
                          GdkEventButton *event) noexcept
try
{
	VteTerminal *terminal = VTE_TERMINAL(widget);
        return WIDGET(terminal)->event_button_press(event);
}
catch (...)
{
        vte::log_exception();
        return true;
}

static gboolean
vte_terminal_button_release(GtkWidget *widget,
                            GdkEventButton *event) noexcept
try
{
	VteTerminal *terminal = VTE_TERMINAL(widget);
        return WIDGET(terminal)->event_button_release(event);
}
catch (...)
{
        vte::log_exception();
        return true;
}

static gboolean
vte_terminal_scroll(GtkWidget *widget,
                    GdkEventScroll *event) noexcept
try
{
        auto terminal = VTE_TERMINAL(widget);
        return WIDGET(terminal)->event_scroll(event);
}
catch (...)
{
        vte::log_exception();
        return true;
}

static gboolean
vte_terminal_focus_in(GtkWidget *widget,
                      GdkEventFocus *event) noexcept
try
{
	VteTerminal *terminal = VTE_TERMINAL(widget);
        WIDGET(terminal)->event_focus_in(event);
        return FALSE;
}
catch (...)
{
        vte::log_exception();
        return false;
}

static gboolean
vte_terminal_focus_out(GtkWidget *widget,
                       GdkEventFocus *event) noexcept
try
{
	VteTerminal *terminal = VTE_TERMINAL(widget);
        WIDGET(terminal)->event_focus_out(event);
        return FALSE;
}
catch (...)
{
        vte::log_exception();
        return false;
}

static gboolean
vte_terminal_enter(GtkWidget *widget,
                   GdkEventCrossing *event) noexcept
try
{
	VteTerminal *terminal = VTE_TERMINAL(widget);
        gboolean ret = FALSE;

	if (GTK_WIDGET_CLASS (vte_terminal_parent_class)->enter_notify_event) {
		ret = GTK_WIDGET_CLASS (vte_terminal_parent_class)->enter_notify_event (widget, event);
	}

        WIDGET(terminal)->event_enter(event);

        return ret;
}
catch (...)
{
        vte::log_exception();
        return false;
}

static gboolean
vte_terminal_leave(GtkWidget *widget,
                   GdkEventCrossing *event) noexcept
try
{
	VteTerminal *terminal = VTE_TERMINAL(widget);
	gboolean ret = FALSE;

	if (GTK_WIDGET_CLASS (vte_terminal_parent_class)->leave_notify_event) {
		ret = GTK_WIDGET_CLASS (vte_terminal_parent_class)->leave_notify_event (widget, event);
	}

        WIDGET(terminal)->event_leave(event);

        return ret;
}
catch (...)
{
        vte::log_exception();
        return false;
}

static void
vte_terminal_get_preferred_width(GtkWidget *widget,
				 int       *minimum_width,
				 int       *natural_width) noexcept
try
{
	VteTerminal *terminal = VTE_TERMINAL(widget);
        WIDGET(terminal)->get_preferred_width(minimum_width, natural_width);
}
catch (...)
{
        vte::log_exception();
}

static void
vte_terminal_get_preferred_height(GtkWidget *widget,
				  int       *minimum_height,
				  int       *natural_height) noexcept
try
{
	VteTerminal *terminal = VTE_TERMINAL(widget);
        WIDGET(terminal)->get_preferred_height(minimum_height, natural_height);
}
catch (...)
{
        vte::log_exception();
}

#endif /* VTE_GTK == 3 */

static void
vte_terminal_realize(GtkWidget *widget) noexcept
try
{
	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_terminal_realize()\n");

        GTK_WIDGET_CLASS(vte_terminal_parent_class)->realize(widget);

        VteTerminal *terminal= VTE_TERMINAL(widget);
        WIDGET(terminal)->realize();
}
catch (...)
{
        vte::log_exception();
}

static void
vte_terminal_unrealize(GtkWidget *widget) noexcept
{
	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_terminal_unrealize()\n");

        try {
                VteTerminal *terminal = VTE_TERMINAL (widget);
                WIDGET(terminal)->unrealize();
        } catch (...) {
                vte::log_exception();
        }

        GTK_WIDGET_CLASS(vte_terminal_parent_class)->unrealize(widget);
}

static void
vte_terminal_map(GtkWidget *widget) noexcept
try
{
        _vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_terminal_map()\n");

        VteTerminal *terminal = VTE_TERMINAL(widget);
        GTK_WIDGET_CLASS(vte_terminal_parent_class)->map(widget);

        WIDGET(terminal)->map();
}
catch (...)
{
        vte::log_exception();
}

static void
vte_terminal_unmap(GtkWidget *widget) noexcept
{
        _vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_terminal_unmap()\n");

        try {
                VteTerminal *terminal = VTE_TERMINAL(widget);
                WIDGET(terminal)->unmap();
        } catch (...) {
                vte::log_exception();
        }

        GTK_WIDGET_CLASS(vte_terminal_parent_class)->unmap(widget);
}

static void
vte_terminal_state_flags_changed(GtkWidget* widget,
                                 GtkStateFlags old_flags) noexcept
try
{
        GTK_WIDGET_CLASS(vte_terminal_parent_class)->state_flags_changed(widget, old_flags);

        auto terminal = VTE_TERMINAL(widget);
        WIDGET(terminal)->state_flags_changed(old_flags);
}
catch (...)
{
        vte::log_exception();
}

static void
vte_terminal_direction_changed(GtkWidget* widget,
                               GtkTextDirection old_direction) noexcept
try
{
        auto const parent_class = GTK_WIDGET_CLASS(vte_terminal_parent_class);
        if (parent_class->direction_changed)
                parent_class->direction_changed(widget, old_direction);

        auto terminal = VTE_TERMINAL(widget);
        WIDGET(terminal)->direction_changed(old_direction);
}
catch (...)
{
        vte::log_exception();
}

static GtkSizeRequestMode
vte_terminal_get_request_mode(GtkWidget* widget) noexcept
{
        return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static gboolean
vte_terminal_query_tooltip(GtkWidget* widget,
                           int x,
                           int y,
                           gboolean keyboard,
                           GtkTooltip* tooltip) noexcept
try
{
        auto const parent_class = GTK_WIDGET_CLASS(vte_terminal_parent_class);
        if (parent_class->query_tooltip(widget, x, y, keyboard, tooltip))
                return true;

        auto terminal = VTE_TERMINAL(widget);
        return WIDGET(terminal)->query_tooltip(x, y, keyboard, tooltip);
}
catch (...)
{
        vte::log_exception();
        return false;
}


#if VTE_GTK == 3

static void
vte_terminal_size_allocate(GtkWidget* widget,
                           GtkAllocation* allocation) noexcept
try
{
        auto terminal = VTE_TERMINAL(widget);
        WIDGET(terminal)->size_allocate(allocation);
}
catch (...)
{
        vte::log_exception();
}

static gboolean
vte_terminal_draw(GtkWidget* widget,
                  cairo_t* cr) noexcept
try
{
        auto terminal = VTE_TERMINAL(widget);
        WIDGET(terminal)->draw(cr);
        return FALSE;
}
catch (...)
{
        vte::log_exception();
        return false;
}

static void
vte_terminal_screen_changed(GtkWidget* widget,
                            GdkScreen* previous_screen) noexcept
try
{
        auto const parent_class = GTK_WIDGET_CLASS(vte_terminal_parent_class);
        if (parent_class->screen_changed)
                parent_class->screen_changed(widget, previous_screen);

	auto terminal = VTE_TERMINAL(widget);
        WIDGET(terminal)->screen_changed(previous_screen);
}
catch (...)
{
        vte::log_exception();
}

#endif /* VTE_GTK == 3 */

#if VTE_GTK == 4

static void
vte_terminal_size_allocate(GtkWidget *widget,
                           int width,
                           int height,
                           int baseline) noexcept
try
{
        GTK_WIDGET_CLASS(vte_terminal_parent_class)->size_allocate(widget, width, height, baseline);

	auto terminal = VTE_TERMINAL(widget);
        WIDGET(terminal)->size_allocate(width, height, baseline);
}
catch (...)
{
        vte::log_exception();
}

static void
vte_terminal_root(GtkWidget *widget) noexcept
try
{
        GTK_WIDGET_CLASS(vte_terminal_parent_class)->root(widget);

        auto terminal = VTE_TERMINAL(widget);
        WIDGET(terminal)->root();
}
catch (...)
{
        vte::log_exception();
}

static void
vte_terminal_unroot(GtkWidget *widget) noexcept
{
        _vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_terminal_unroot()\n");

        auto terminal = VTE_TERMINAL(widget);
        WIDGET(terminal)->unroot();

        GTK_WIDGET_CLASS(vte_terminal_parent_class)->unroot(widget);
}

static void
vte_terminal_measure(GtkWidget* widget,
                     GtkOrientation orientation,
                     int for_size,
                     int* minimum,
                     int* natural,
                     int* minimum_baseline,
                     int* natural_baseline) noexcept
try
{
        auto terminal = VTE_TERMINAL(widget);
        WIDGET(terminal)->measure(orientation, for_size,
                                  minimum, natural,
                                  minimum_baseline, natural_baseline);
}
catch (...)
{
        vte::log_exception();
}

static void
vte_terminal_compute_expand(GtkWidget* widget,
                            gboolean* hexpand,
                            gboolean* vexpand) noexcept
try
{
        auto terminal = VTE_TERMINAL(widget);
        auto [h, v] = WIDGET(terminal)->compute_expand();
        *hexpand = h;
        *vexpand = v;
}
catch (...)
{
        vte::log_exception();
        *hexpand = *vexpand = false;
}

static void
vte_terminal_css_changed(GtkWidget* widget,
                         GtkCssStyleChange* change) noexcept
try
{
        GTK_WIDGET_CLASS(vte_terminal_parent_class)->css_changed(widget, change);
        auto terminal = VTE_TERMINAL(widget);
        WIDGET(terminal)->css_changed(change);
}
catch (...)
{
        vte::log_exception();
}

static void
vte_terminal_system_setting_changed(GtkWidget* widget,
                                    GtkSystemSetting setting) noexcept
try
{
        GTK_WIDGET_CLASS(vte_terminal_parent_class)->system_setting_changed(widget, setting);
        auto terminal = VTE_TERMINAL(widget);
        WIDGET(terminal)->system_setting_changed(setting);
}
catch (...)
{
        vte::log_exception();
}

static void
vte_terminal_snapshot(GtkWidget* widget,
                      GtkSnapshot* snapshot_object) noexcept
try
{
        GTK_WIDGET_CLASS(vte_terminal_parent_class)->snapshot(widget, snapshot_object);
        auto terminal = VTE_TERMINAL(widget);
        WIDGET(terminal)->snapshot(snapshot_object);
}
catch (...)
{
        vte::log_exception();
}

static gboolean
vte_terminal_contains(GtkWidget* widget,
                      double x,
                      double y) noexcept
try
{
        auto terminal = VTE_TERMINAL(widget);
        if (WIDGET(terminal)->contains(x, y))
                return true;

        auto const parent_class = GTK_WIDGET_CLASS(vte_terminal_parent_class);
        if (parent_class->contains &&
            parent_class->contains(widget, x, y))
                return true;

        return false;
}
catch (...)
{
        vte::log_exception();
        return false;
}

#endif /* VTE_GTK == 4 */

static void
vte_terminal_constructed (GObject *object) noexcept
try
{
        VteTerminal *terminal = VTE_TERMINAL (object);

        G_OBJECT_CLASS (vte_terminal_parent_class)->constructed (object);

        WIDGET(terminal)->constructed();
}
catch (...)
{
        vte::log_exception();
}

static void
vte_terminal_init(VteTerminal *terminal)
try
{
        void *place;
	GtkStyleContext *context;

	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_terminal_init()\n");

        context = gtk_widget_get_style_context(&terminal->widget);
        gtk_style_context_add_provider (context,
                                        VTE_TERMINAL_GET_CLASS (terminal)->priv->style_provider,
                                        VTE_TERMINAL_CSS_PRIORITY);

#if VTE_GTK == 3
        gtk_widget_set_has_window(&terminal->widget, FALSE);
#endif

	place = vte_terminal_get_instance_private(terminal);
        new (place) VteTerminalPrivate{terminal};
}
catch (...)
{
        vte::log_exception();

        // There's not really anything we can do after the
        // construction of Widget failed... we'll crash soon anyway.
        g_error("Widget::Widget threw\n");
}

static void
vte_terminal_dispose(GObject *object) noexcept
{
	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_terminal_dispose()\n");

        try {
                VteTerminal *terminal = VTE_TERMINAL (object);
                PRIVATE(terminal)->reset();
        } catch (...) {
                vte::log_exception();
        }

	/* Call the inherited dispose() method. */
	G_OBJECT_CLASS(vte_terminal_parent_class)->dispose(object);
}

static void
vte_terminal_finalize(GObject *object) noexcept
{
	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_terminal_finalize()\n");

        auto terminal = VTE_TERMINAL(object);
        PRIVATE(terminal)->~VteTerminalPrivate();

	/* Call the inherited finalize() method. */
	G_OBJECT_CLASS(vte_terminal_parent_class)->finalize(object);
}

static void
vte_terminal_get_property (GObject *object,
                           guint prop_id,
                           GValue *value,
                           GParamSpec *pspec) noexcept
try
{
        VteTerminal *terminal = VTE_TERMINAL (object);
        auto widget = WIDGET(terminal);
        auto impl = IMPL(terminal);

	switch (prop_id)
                {
                case PROP_HADJUSTMENT:
                        g_value_set_object (value, widget->hadjustment());
                        break;
                case PROP_VADJUSTMENT:
                        g_value_set_object (value, widget->vadjustment());
                        break;
                case PROP_HSCROLL_POLICY:
                        g_value_set_enum (value, widget->hscroll_policy());
                        break;
                case PROP_VSCROLL_POLICY:
                        g_value_set_enum (value, widget->vscroll_policy());
                        break;
                case PROP_ALLOW_BOLD:
                        g_value_set_boolean (value, vte_terminal_get_allow_bold (terminal));
                        break;
                case PROP_ALLOW_HYPERLINK:
                        g_value_set_boolean (value, vte_terminal_get_allow_hyperlink (terminal));
                        break;
                case PROP_AUDIBLE_BELL:
                        g_value_set_boolean (value, vte_terminal_get_audible_bell (terminal));
                        break;
                case PROP_BACKSPACE_BINDING:
                        g_value_set_enum (value, widget->backspace_binding());
                        break;
                case PROP_BOLD_IS_BRIGHT:
                        g_value_set_boolean (value, vte_terminal_get_bold_is_bright (terminal));
                        break;
                case PROP_CELL_HEIGHT_SCALE:
                        g_value_set_double (value, vte_terminal_get_cell_height_scale (terminal));
                        break;
                case PROP_CELL_WIDTH_SCALE:
                        g_value_set_double (value, vte_terminal_get_cell_width_scale (terminal));
                        break;
                case PROP_CJK_AMBIGUOUS_WIDTH:
                        g_value_set_int (value, vte_terminal_get_cjk_ambiguous_width (terminal));
                        break;
                case PROP_CURSOR_BLINK_MODE:
                        g_value_set_enum (value, vte_terminal_get_cursor_blink_mode (terminal));
                        break;
                case PROP_CURRENT_DIRECTORY_URI:
                        g_value_set_string (value, vte_terminal_get_current_directory_uri (terminal));
                        break;
                case PROP_CURRENT_FILE_URI:
                        g_value_set_string (value, vte_terminal_get_current_file_uri (terminal));
                        break;
                case PROP_CURSOR_SHAPE:
                        g_value_set_enum (value, vte_terminal_get_cursor_shape (terminal));
                        break;
                case PROP_DELETE_BINDING:
                        g_value_set_enum (value, widget->delete_binding());
                        break;
                case PROP_ENABLE_BIDI:
                        g_value_set_boolean (value, vte_terminal_get_enable_bidi (terminal));
                        break;
                case PROP_ENABLE_FALLBACK_SCROLLING:
                        g_value_set_boolean (value, vte_terminal_get_enable_fallback_scrolling(terminal));
                        break;
                case PROP_ENABLE_SHAPING:
                        g_value_set_boolean (value, vte_terminal_get_enable_shaping (terminal));
                        break;
                case PROP_ENABLE_SIXEL:
                        g_value_set_boolean (value, vte_terminal_get_enable_sixel (terminal));
                        break;
                case PROP_ENCODING:
                        g_value_set_string (value, vte_terminal_get_encoding (terminal));
                        break;
                case PROP_FONT_DESC:
                        g_value_set_boxed (value, vte_terminal_get_font (terminal));
                        break;
                case PROP_FONT_SCALE:
                        g_value_set_double (value, vte_terminal_get_font_scale (terminal));
                        break;
                case PROP_HYPERLINK_HOVER_URI:
                        g_value_set_string (value, impl->m_hyperlink_hover_uri);
                        break;
                case PROP_ICON_TITLE:
                        g_value_set_string (value, vte_terminal_get_icon_title (terminal));
                        break;
                case PROP_INPUT_ENABLED:
                        g_value_set_boolean (value, vte_terminal_get_input_enabled (terminal));
                        break;
                case PROP_MOUSE_POINTER_AUTOHIDE:
                        g_value_set_boolean (value, vte_terminal_get_mouse_autohide (terminal));
                        break;
                case PROP_PTY:
                        g_value_set_object (value, vte_terminal_get_pty(terminal));
                        break;
                case PROP_REWRAP_ON_RESIZE:
                        g_value_set_boolean (value, vte_terminal_get_rewrap_on_resize (terminal));
                        break;
                case PROP_SCROLLBACK_LINES:
                        g_value_set_uint (value, vte_terminal_get_scrollback_lines(terminal));
                        break;
                case PROP_SCROLL_ON_KEYSTROKE:
                        g_value_set_boolean (value, vte_terminal_get_scroll_on_keystroke(terminal));
                        break;
                case PROP_SCROLL_ON_OUTPUT:
                        g_value_set_boolean (value, vte_terminal_get_scroll_on_output(terminal));
                        break;
                case PROP_SCROLL_UNIT_IS_PIXELS:
                        g_value_set_boolean (value, vte_terminal_get_scroll_unit_is_pixels(terminal));
                        break;
                case PROP_TEXT_BLINK_MODE:
                        g_value_set_enum (value, vte_terminal_get_text_blink_mode (terminal));
                        break;
                case PROP_WINDOW_TITLE:
                        g_value_set_string (value, vte_terminal_get_window_title (terminal));
                        break;
                case PROP_WORD_CHAR_EXCEPTIONS:
                        g_value_set_string (value, vte_terminal_get_word_char_exceptions (terminal));
                        break;

                default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			return;
                }
}
catch (...)
{
        vte::log_exception();
}

static void
vte_terminal_set_property (GObject *object,
                           guint prop_id,
                           const GValue *value,
                           GParamSpec *pspec) noexcept
try
{
        VteTerminal *terminal = VTE_TERMINAL (object);

	switch (prop_id)
                {
                case PROP_HADJUSTMENT:
                        vte_terminal_set_hadjustment (terminal, (GtkAdjustment *)g_value_get_object (value));
                        break;
                case PROP_VADJUSTMENT:
                        vte_terminal_set_vadjustment (terminal, (GtkAdjustment *)g_value_get_object (value));
                        break;
                case PROP_HSCROLL_POLICY:
                        vte_terminal_set_hscroll_policy(terminal, (GtkScrollablePolicy)g_value_get_enum(value));
                        break;
                case PROP_VSCROLL_POLICY:
                        vte_terminal_set_vscroll_policy(terminal, (GtkScrollablePolicy)g_value_get_enum(value));
                        break;
                case PROP_ALLOW_BOLD:
                        vte_terminal_set_allow_bold (terminal, g_value_get_boolean (value));
                        break;
                case PROP_ALLOW_HYPERLINK:
                        vte_terminal_set_allow_hyperlink (terminal, g_value_get_boolean (value));
                        break;
                case PROP_AUDIBLE_BELL:
                        vte_terminal_set_audible_bell (terminal, g_value_get_boolean (value));
                        break;
                case PROP_BACKSPACE_BINDING:
                        vte_terminal_set_backspace_binding (terminal, (VteEraseBinding)g_value_get_enum (value));
                        break;
                case PROP_BOLD_IS_BRIGHT:
                        vte_terminal_set_bold_is_bright (terminal, g_value_get_boolean (value));
                        break;
                case PROP_CELL_HEIGHT_SCALE:
                        vte_terminal_set_cell_height_scale (terminal, g_value_get_double (value));
                        break;
                case PROP_CELL_WIDTH_SCALE:
                        vte_terminal_set_cell_width_scale (terminal, g_value_get_double (value));
                        break;
                case PROP_CJK_AMBIGUOUS_WIDTH:
                        vte_terminal_set_cjk_ambiguous_width (terminal, g_value_get_int (value));
                        break;
                case PROP_CURSOR_BLINK_MODE:
                        vte_terminal_set_cursor_blink_mode (terminal, (VteCursorBlinkMode)g_value_get_enum (value));
                        break;
                case PROP_CURSOR_SHAPE:
                        vte_terminal_set_cursor_shape (terminal, (VteCursorShape)g_value_get_enum (value));
                        break;
                case PROP_DELETE_BINDING:
                        vte_terminal_set_delete_binding (terminal, (VteEraseBinding)g_value_get_enum (value));
                        break;
                case PROP_ENABLE_BIDI:
                        vte_terminal_set_enable_bidi (terminal, g_value_get_boolean (value));
                        break;
                case PROP_ENABLE_FALLBACK_SCROLLING:
                        vte_terminal_set_enable_fallback_scrolling (terminal, g_value_get_boolean (value));
                        break;
                case PROP_ENABLE_SHAPING:
                        vte_terminal_set_enable_shaping (terminal, g_value_get_boolean (value));
                        break;
                case PROP_ENABLE_SIXEL:
                        vte_terminal_set_enable_sixel (terminal, g_value_get_boolean (value));
                        break;
                case PROP_ENCODING:
                        vte_terminal_set_encoding (terminal, g_value_get_string (value), NULL);
                        break;
                case PROP_FONT_DESC:
                        vte_terminal_set_font (terminal, (PangoFontDescription *)g_value_get_boxed (value));
                        break;
                case PROP_FONT_SCALE:
                        vte_terminal_set_font_scale (terminal, g_value_get_double (value));
                        break;
                case PROP_INPUT_ENABLED:
                        vte_terminal_set_input_enabled (terminal, g_value_get_boolean (value));
                        break;
                case PROP_MOUSE_POINTER_AUTOHIDE:
                        vte_terminal_set_mouse_autohide (terminal, g_value_get_boolean (value));
                        break;
                case PROP_PTY:
                        vte_terminal_set_pty (terminal, (VtePty *)g_value_get_object (value));
                        break;
                case PROP_REWRAP_ON_RESIZE:
                        vte_terminal_set_rewrap_on_resize (terminal, g_value_get_boolean (value));
                        break;
                case PROP_SCROLLBACK_LINES:
                        vte_terminal_set_scrollback_lines (terminal, g_value_get_uint (value));
                        break;
                case PROP_SCROLL_ON_KEYSTROKE:
                        vte_terminal_set_scroll_on_keystroke(terminal, g_value_get_boolean (value));
                        break;
                case PROP_SCROLL_ON_OUTPUT:
                        vte_terminal_set_scroll_on_output (terminal, g_value_get_boolean (value));
                        break;
                case PROP_SCROLL_UNIT_IS_PIXELS:
                        vte_terminal_set_scroll_unit_is_pixels(terminal, g_value_get_boolean(value));
                        break;
                case PROP_TEXT_BLINK_MODE:
                        vte_terminal_set_text_blink_mode (terminal, (VteTextBlinkMode)g_value_get_enum (value));
                        break;
                case PROP_WORD_CHAR_EXCEPTIONS:
                        vte_terminal_set_word_char_exceptions (terminal, g_value_get_string (value));
                        break;

                        /* Not writable */
                case PROP_CURRENT_DIRECTORY_URI:
                case PROP_CURRENT_FILE_URI:
                case PROP_HYPERLINK_HOVER_URI:
                case PROP_ICON_TITLE:
                case PROP_WINDOW_TITLE:
                        g_assert_not_reached ();
                        break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			return;
                }
}
catch (...)
{
        vte::log_exception();
}

static void
vte_terminal_class_init(VteTerminalClass *klass)
{
#ifdef VTE_DEBUG
	{
                _vte_debug_init();
		_vte_debug_print(VTE_DEBUG_LIFECYCLE,
                                 "vte_terminal_class_init()\n");
		/* print out the legend */
		_vte_debug_print(VTE_DEBUG_WORK,
                                 "Debugging work flow (top input to bottom output):\n"
                                 "  .  _vte_terminal_process_incoming\n"
                                 "  <  start process_timeout\n"
                                 "  {[ start update_timeout  [ => rate limited\n"
                                 "  T  start of terminal in update_timeout\n"
                                 "  (  start _vte_terminal_process_incoming\n"
                                 "  ?  _vte_invalidate_cells (call)\n"
                                 "  !  _vte_invalidate_cells (dirty)\n"
                                 "  *  _vte_invalidate_all\n"
                                 "  )  end _vte_terminal_process_incoming\n"
                                 "  =  vte_terminal_paint\n"
                                 "  ]} end update_timeout\n"
                                 "  >  end process_timeout\n");
	}
#endif

#if VTE_GTK == 3
	_VTE_DEBUG_IF (VTE_DEBUG_UPDATES) gdk_window_set_debug_updates(TRUE);
#endif

	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");

	auto gobject_class = G_OBJECT_CLASS(klass);
	auto widget_class = GTK_WIDGET_CLASS(klass);

	/* Override some of the default handlers. */
        gobject_class->constructed = vte_terminal_constructed;
        gobject_class->dispose = vte_terminal_dispose;
	gobject_class->finalize = vte_terminal_finalize;
        gobject_class->get_property = vte_terminal_get_property;
        gobject_class->set_property = vte_terminal_set_property;

	widget_class->realize = vte_terminal_realize;
	widget_class->unrealize = vte_terminal_unrealize;
        widget_class->map = vte_terminal_map;
        widget_class->unmap = vte_terminal_unmap;

	widget_class->size_allocate = vte_terminal_size_allocate;
        widget_class->state_flags_changed = vte_terminal_state_flags_changed;
        widget_class->direction_changed = vte_terminal_direction_changed;
        widget_class->get_request_mode = vte_terminal_get_request_mode;
        widget_class->query_tooltip = vte_terminal_query_tooltip;

#if VTE_GTK == 3
        widget_class->draw = vte_terminal_draw;
	widget_class->scroll_event = vte_terminal_scroll;
	widget_class->key_press_event = vte_terminal_key_press;
	widget_class->key_release_event = vte_terminal_key_release;
	widget_class->button_press_event = vte_terminal_button_press;
	widget_class->button_release_event = vte_terminal_button_release;
	widget_class->motion_notify_event = vte_terminal_motion_notify;
	widget_class->enter_notify_event = vte_terminal_enter;
	widget_class->leave_notify_event = vte_terminal_leave;
	widget_class->focus_in_event = vte_terminal_focus_in;
	widget_class->focus_out_event = vte_terminal_focus_out;
	widget_class->style_updated = vte_terminal_style_updated;
	widget_class->get_preferred_width = vte_terminal_get_preferred_width;
	widget_class->get_preferred_height = vte_terminal_get_preferred_height;
        widget_class->screen_changed = vte_terminal_screen_changed;
#endif

#if VTE_GTK == 4
        widget_class->root = vte_terminal_root;
        widget_class->unroot = vte_terminal_unroot;
        widget_class->measure = vte_terminal_measure;
        widget_class->compute_expand = vte_terminal_compute_expand;
        widget_class->css_changed = vte_terminal_css_changed;
        widget_class->system_setting_changed = vte_terminal_system_setting_changed;
        widget_class->snapshot = vte_terminal_snapshot;
        widget_class->contains = vte_terminal_contains;
#endif

        gtk_widget_class_set_css_name(widget_class, VTE_TERMINAL_CSS_NAME);

	/* Initialize default handlers. */
	klass->eof = NULL;
	klass->child_exited = NULL;
	klass->encoding_changed = NULL;
	klass->char_size_changed = NULL;
	klass->window_title_changed = NULL;
	klass->icon_title_changed = NULL;
	klass->selection_changed = NULL;
	klass->contents_changed = NULL;
	klass->cursor_moved = NULL;
	klass->commit = NULL;

	klass->deiconify_window = NULL;
	klass->iconify_window = NULL;
	klass->raise_window = NULL;
	klass->lower_window = NULL;
	klass->refresh_window = NULL;
	klass->restore_window = NULL;
	klass->maximize_window = NULL;
	klass->resize_window = NULL;
	klass->move_window = NULL;

	klass->increase_font_size = NULL;
	klass->decrease_font_size = NULL;

#if VTE_GTK == 3
	klass->text_modified = NULL;
	klass->text_inserted = NULL;
	klass->text_deleted = NULL;
	klass->text_scrolled = NULL;
#endif /* VTE_GTK == 3 */

	klass->copy_clipboard = vte_terminal_real_copy_clipboard;
	klass->paste_clipboard = vte_terminal_real_paste_clipboard;

        klass->bell = NULL;

        /* GtkScrollable interface properties */
        g_object_class_override_property (gobject_class, PROP_HADJUSTMENT, "hadjustment");
        g_object_class_override_property (gobject_class, PROP_VADJUSTMENT, "vadjustment");
        g_object_class_override_property (gobject_class, PROP_HSCROLL_POLICY, "hscroll-policy");
        g_object_class_override_property (gobject_class, PROP_VSCROLL_POLICY, "vscroll-policy");

	/* Register some signals of our own. */

        /**
         * VteTerminal::eof:
         * @vteterminal: the object which received the signal
         *
         * Emitted when the terminal receives an end-of-file from a child which
         * is running in the terminal.  This signal is frequently (but not
         * always) emitted with a #VteTerminal::child-exited signal.
         */
        signals[SIGNAL_EOF] =
                g_signal_new(I_("eof"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteTerminalClass, eof),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);
        g_signal_set_va_marshaller(signals[SIGNAL_EOF],
                                   G_OBJECT_CLASS_TYPE(klass),
                                   g_cclosure_marshal_VOID__VOIDv);

        /**
         * VteTerminal::child-exited:
         * @vteterminal: the object which received the signal
         * @status: the child's exit status
         *
         * This signal is emitted when the terminal detects that a child
         * watched using vte_terminal_watch_child() has exited.
         */
        signals[SIGNAL_CHILD_EXITED] =
                g_signal_new(I_("child-exited"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteTerminalClass, child_exited),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__INT,
                             G_TYPE_NONE,
                             1, G_TYPE_INT);
        g_signal_set_va_marshaller(signals[SIGNAL_CHILD_EXITED],
                                   G_OBJECT_CLASS_TYPE(klass),
                                   g_cclosure_marshal_VOID__INTv);

        /**
         * VteTerminal::window-title-changed:
         * @vteterminal: the object which received the signal
         *
         * Emitted when the #VteTerminal:window-title property is modified.
         */
        signals[SIGNAL_WINDOW_TITLE_CHANGED] =
                g_signal_new(I_("window-title-changed"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteTerminalClass, window_title_changed),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);
        g_signal_set_va_marshaller(signals[SIGNAL_WINDOW_TITLE_CHANGED],
                                   G_OBJECT_CLASS_TYPE(klass),
                                   g_cclosure_marshal_VOID__VOIDv);

        /**
         * VteTerminal::icon-title-changed:
         * @vteterminal: the object which received the signal
         *
         * Deprecated: 0.54: This signal is never emitted.
         */
        signals[SIGNAL_ICON_TITLE_CHANGED] =
                g_signal_new(I_("icon-title-changed"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteTerminalClass, icon_title_changed),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);
        g_signal_set_va_marshaller(signals[SIGNAL_ICON_TITLE_CHANGED],
                                   G_OBJECT_CLASS_TYPE(klass),
                                   g_cclosure_marshal_VOID__VOIDv);

        /**
         * VteTerminal::current-directory-uri-changed:
         * @vteterminal: the object which received the signal
         *
         * Emitted when the current directory URI is modified.
         */
        signals[SIGNAL_CURRENT_DIRECTORY_URI_CHANGED] =
                g_signal_new(I_("current-directory-uri-changed"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             0,
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);
        g_signal_set_va_marshaller(signals[SIGNAL_CURRENT_DIRECTORY_URI_CHANGED],
                                   G_OBJECT_CLASS_TYPE(klass),
                                   g_cclosure_marshal_VOID__VOIDv);

        /**
         * VteTerminal::current-file-uri-changed:
         * @vteterminal: the object which received the signal
         *
         * Emitted when the current file URI is modified.
         */
        signals[SIGNAL_CURRENT_FILE_URI_CHANGED] =
                g_signal_new(I_("current-file-uri-changed"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             0,
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);
        g_signal_set_va_marshaller(signals[SIGNAL_CURRENT_FILE_URI_CHANGED],
                                   G_OBJECT_CLASS_TYPE(klass),
                                   g_cclosure_marshal_VOID__VOIDv);

        /**
         * VteTerminal::hyperlink-hover-uri-changed:
         * @vteterminal: the object which received the signal
         * @uri: the nonempty target URI under the mouse, or NULL
         * @bbox: the bounding box of the hyperlink anchor text, or NULL
         *
         * Emitted when the hovered hyperlink changes.
         *
         * @uri and @bbox are owned by VTE, must not be modified, and might
         * change after the signal handlers returns.
         *
         * The signal is not re-emitted when the bounding box changes for the
         * same hyperlink. This might change in a future VTE version without notice.
         *
         * Since: 0.50
         */
        signals[SIGNAL_HYPERLINK_HOVER_URI_CHANGED] =
                g_signal_new(I_("hyperlink-hover-uri-changed"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             0,
                             NULL,
                             NULL,
                             _vte_marshal_VOID__STRING_BOXED,
                             G_TYPE_NONE,
                             2, G_TYPE_STRING, GDK_TYPE_RECTANGLE | G_SIGNAL_TYPE_STATIC_SCOPE);
        g_signal_set_va_marshaller(signals[SIGNAL_HYPERLINK_HOVER_URI_CHANGED],
                                   G_OBJECT_CLASS_TYPE(klass),
                                   _vte_marshal_VOID__STRING_BOXEDv);

        /**
         * VteTerminal::encoding-changed:
         * @vteterminal: the object which received the signal
         *
         * Emitted whenever the terminal's current encoding has changed.
         *
         * Note: support for non-UTF-8 is deprecated.
         */
        signals[SIGNAL_ENCODING_CHANGED] =
                g_signal_new(I_("encoding-changed"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteTerminalClass, encoding_changed),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);
        g_signal_set_va_marshaller(signals[SIGNAL_ENCODING_CHANGED],
                                   G_OBJECT_CLASS_TYPE(klass),
                                   g_cclosure_marshal_VOID__VOIDv);

        /**
         * VteTerminal::commit:
         * @vteterminal: the object which received the signal
         * @text: a string of text
         * @size: the length of that string of text
         *
         * Emitted whenever the terminal receives input from the user and
         * prepares to send it to the child process.
         */
        signals[SIGNAL_COMMIT] =
                g_signal_new(I_("commit"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteTerminalClass, commit),
                             NULL,
                             NULL,
                             _vte_marshal_VOID__STRING_UINT,
                             G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_UINT);
        g_signal_set_va_marshaller(signals[SIGNAL_COMMIT],
                                   G_OBJECT_CLASS_TYPE(klass),
                                   _vte_marshal_VOID__STRING_UINTv);

        /**
         * VteTerminal::char-size-changed:
         * @vteterminal: the object which received the signal
         * @width: the new character cell width
         * @height: the new character cell height
         *
         * Emitted whenever the cell size changes, e.g. due to a change in
         * font, font-scale or cell-width/height-scale.
         *
         * Note that this signal should rather be called "cell-size-changed".
         */
        signals[SIGNAL_CHAR_SIZE_CHANGED] =
                g_signal_new(I_("char-size-changed"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteTerminalClass, char_size_changed),
                             NULL,
                             NULL,
                             _vte_marshal_VOID__UINT_UINT,
                             G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);
        g_signal_set_va_marshaller(signals[SIGNAL_CHAR_SIZE_CHANGED],
                                   G_OBJECT_CLASS_TYPE(klass),
                                   _vte_marshal_VOID__UINT_UINTv);

        /**
         * VteTerminal::selection-changed:
         * @vteterminal: the object which received the signal
         *
         * Emitted whenever the contents of terminal's selection changes.
         */
        signals[SIGNAL_SELECTION_CHANGED] =
                g_signal_new (I_("selection-changed"),
                              G_OBJECT_CLASS_TYPE(klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET(VteTerminalClass, selection_changed),
                              NULL,
                              NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
        g_signal_set_va_marshaller(signals[SIGNAL_SELECTION_CHANGED],
                                   G_OBJECT_CLASS_TYPE(klass),
                                   g_cclosure_marshal_VOID__VOIDv);

        /**
         * VteTerminal::contents-changed:
         * @vteterminal: the object which received the signal
         *
         * Emitted whenever the visible appearance of the terminal has changed.
         * Used primarily by #VteTerminalAccessible.
         */
        signals[SIGNAL_CONTENTS_CHANGED] =
                g_signal_new(I_("contents-changed"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteTerminalClass, contents_changed),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);
        g_signal_set_va_marshaller(signals[SIGNAL_CONTENTS_CHANGED],
                                   G_OBJECT_CLASS_TYPE(klass),
                                   g_cclosure_marshal_VOID__VOIDv);

        /**
         * VteTerminal::cursor-moved:
         * @vteterminal: the object which received the signal
         *
         * Emitted whenever the cursor moves to a new character cell.  Used
         * primarily by #VteTerminalAccessible.
         */
        signals[SIGNAL_CURSOR_MOVED] =
                g_signal_new(I_("cursor-moved"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteTerminalClass, cursor_moved),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);
        g_signal_set_va_marshaller(signals[SIGNAL_CURSOR_MOVED],
                                   G_OBJECT_CLASS_TYPE(klass),
                                   g_cclosure_marshal_VOID__VOIDv);

        /**
         * VteTerminal::deiconify-window:
         * @vteterminal: the object which received the signal
         *
         * Never emitted.
         *
         * Deprecated: 0.60
         */
        signals[SIGNAL_DEICONIFY_WINDOW] =
                g_signal_new(I_("deiconify-window"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteTerminalClass, deiconify_window),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);
        g_signal_set_va_marshaller(signals[SIGNAL_DEICONIFY_WINDOW],
                                   G_OBJECT_CLASS_TYPE(klass),
                                   g_cclosure_marshal_VOID__VOIDv);

        /**
         * VteTerminal::iconify-window:
         * @vteterminal: the object which received the signal
         *
         * Never emitted.
         *
         * Deprecated: 0.60
         */
        signals[SIGNAL_ICONIFY_WINDOW] =
                g_signal_new(I_("iconify-window"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteTerminalClass, iconify_window),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);
        g_signal_set_va_marshaller(signals[SIGNAL_ICONIFY_WINDOW],
                                   G_OBJECT_CLASS_TYPE(klass),
                                   g_cclosure_marshal_VOID__VOIDv);

        /**
         * VteTerminal::raise-window:
         * @vteterminal: the object which received the signal
         *
         * Never emitted.
         *
         * Deprecated: 0.60
         */
        signals[SIGNAL_RAISE_WINDOW] =
                g_signal_new(I_("raise-window"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteTerminalClass, raise_window),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);
        g_signal_set_va_marshaller(signals[SIGNAL_RAISE_WINDOW],
                                   G_OBJECT_CLASS_TYPE(klass),
                                   g_cclosure_marshal_VOID__VOIDv);

        /**
         * VteTerminal::lower-window:
         * @vteterminal: the object which received the signal
         *
         * Never emitted.
         *
         * Deprecated: 0.60
         */
        signals[SIGNAL_LOWER_WINDOW] =
                g_signal_new(I_("lower-window"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteTerminalClass, lower_window),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);
        g_signal_set_va_marshaller(signals[SIGNAL_LOWER_WINDOW],
                                   G_OBJECT_CLASS_TYPE(klass),
                                   g_cclosure_marshal_VOID__VOIDv);

        /**
         * VteTerminal::refresh-window:
         * @vteterminal: the object which received the signal
         *
         * Never emitted.
         *
         * Deprecated: 0.60
         */
        signals[SIGNAL_REFRESH_WINDOW] =
                g_signal_new(I_("refresh-window"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteTerminalClass, refresh_window),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);
        g_signal_set_va_marshaller(signals[SIGNAL_REFRESH_WINDOW],
                                   G_OBJECT_CLASS_TYPE(klass),
                                   g_cclosure_marshal_VOID__VOIDv);

        /**
         * VteTerminal::restore-window:
         * @vteterminal: the object which received the signal
         *
         * Never emitted.
         *
         * Deprecated: 0.60
         */
        signals[SIGNAL_RESTORE_WINDOW] =
                g_signal_new(I_("restore-window"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteTerminalClass, restore_window),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);
        g_signal_set_va_marshaller(signals[SIGNAL_RESTORE_WINDOW],
                                   G_OBJECT_CLASS_TYPE(klass),
                                   g_cclosure_marshal_VOID__VOIDv);

        /**
         * VteTerminal::maximize-window:
         * @vteterminal: the object which received the signal
         *
         * Never emitted.
         *
         * Deprecated: 0.60
         */
        signals[SIGNAL_MAXIMIZE_WINDOW] =
                g_signal_new(I_("maximize-window"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteTerminalClass, maximize_window),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);
        g_signal_set_va_marshaller(signals[SIGNAL_MAXIMIZE_WINDOW],
                                   G_OBJECT_CLASS_TYPE(klass),
                                   g_cclosure_marshal_VOID__VOIDv);

        /**
         * VteTerminal::resize-window:
         * @vteterminal: the object which received the signal
         * @width: the desired number of columns
         * @height: the desired number of rows
         *
         * Emitted at the child application's request.
         */
        signals[SIGNAL_RESIZE_WINDOW] =
                g_signal_new(I_("resize-window"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteTerminalClass, resize_window),
                             NULL,
                             NULL,
                             _vte_marshal_VOID__UINT_UINT,
                             G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);
        g_signal_set_va_marshaller(signals[SIGNAL_RESIZE_WINDOW],
                                   G_OBJECT_CLASS_TYPE(klass),
                                   _vte_marshal_VOID__UINT_UINTv);

        /**
         * VteTerminal::move-window:
         * @vteterminal: the object which received the signal
         * @x: the terminal's desired location, X coordinate
         * @y: the terminal's desired location, Y coordinate
         *
         * Never emitted.
         *
         * Deprecated: 0.60
         */
        signals[SIGNAL_MOVE_WINDOW] =
                g_signal_new(I_("move-window"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteTerminalClass, move_window),
                             NULL,
                             NULL,
                             _vte_marshal_VOID__UINT_UINT,
                             G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);
        g_signal_set_va_marshaller(signals[SIGNAL_MOVE_WINDOW],
                                   G_OBJECT_CLASS_TYPE(klass),
                                   _vte_marshal_VOID__UINT_UINTv);

        /**
         * VteTerminal::increase-font-size:
         * @vteterminal: the object which received the signal
         *
         * Emitted when the user hits the '+' key while holding the Control key.
         */
        signals[SIGNAL_INCREASE_FONT_SIZE] =
                g_signal_new(I_("increase-font-size"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteTerminalClass, increase_font_size),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);
        g_signal_set_va_marshaller(signals[SIGNAL_INCREASE_FONT_SIZE],
                                   G_OBJECT_CLASS_TYPE(klass),
                                   g_cclosure_marshal_VOID__VOIDv);

        /**
         * VteTerminal::decrease-font-size:
         * @vteterminal: the object which received the signal
         *
         * Emitted when the user hits the '-' key while holding the Control key.
         */
        signals[SIGNAL_DECREASE_FONT_SIZE] =
                g_signal_new(I_("decrease-font-size"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteTerminalClass, decrease_font_size),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);
        g_signal_set_va_marshaller(signals[SIGNAL_DECREASE_FONT_SIZE],
                                   G_OBJECT_CLASS_TYPE(klass),
                                   g_cclosure_marshal_VOID__VOIDv);

#if VTE_GTK == 3
        /* These signals are deprecated and never emitted,
         * but need to be kept for ABI compatibility on gtk3.
         */

        /**
         * VteTerminal::text-modified:
         * @vteterminal:
         *
         * Deprecated: 0.66: This signal is never emitted.
         */
        g_signal_new(I_("text-modified"),
                     G_OBJECT_CLASS_TYPE(klass),
                     GSignalFlags(G_SIGNAL_RUN_LAST | G_SIGNAL_DEPRECATED),
                     G_STRUCT_OFFSET(VteTerminalClass, text_modified),
                     NULL,
                     NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

        /**
         * VteTerminal::text-inserted:
         * @vteterminal:
         *
         * Deprecated: 0.66: This signal is never emitted.
         */
        g_signal_new(I_("text-inserted"),
                     G_OBJECT_CLASS_TYPE(klass),
                     GSignalFlags(G_SIGNAL_RUN_LAST | G_SIGNAL_DEPRECATED),
                     G_STRUCT_OFFSET(VteTerminalClass, text_inserted),
                     NULL,
                     NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

        /**
         * VteTerminal::text-deleted:
         * @vteterminal:
         *
         * Deprecated: 0.66: This signal is never emitted.
         */
        g_signal_new(I_("text-deleted"),
                     G_OBJECT_CLASS_TYPE(klass),
                     GSignalFlags(G_SIGNAL_RUN_LAST | G_SIGNAL_DEPRECATED),
                     G_STRUCT_OFFSET(VteTerminalClass, text_deleted),
                     NULL,
                     NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

        /**
         * VteTerminal::text-scrolled:
         * @vteterminal:
         * @delta:
         *
         * Deprecated: 0.66: This signal is never emitted.
         */
        g_signal_new(I_("text-scrolled"),
                     G_OBJECT_CLASS_TYPE(klass),
                     GSignalFlags(G_SIGNAL_RUN_LAST | G_SIGNAL_DEPRECATED),
                     G_STRUCT_OFFSET(VteTerminalClass, text_scrolled),
                     NULL,
                     NULL,
                     g_cclosure_marshal_VOID__INT,
                     G_TYPE_NONE, 1, G_TYPE_INT);

#endif /* VTE_GTK == 3 */

        /**
         * VteTerminal::copy-clipboard:
         * @vteterminal: the object which received the signal
         *
         * Emitted whenever vte_terminal_copy_clipboard() is called.
         */
	signals[SIGNAL_COPY_CLIPBOARD] =
                g_signal_new(I_("copy-clipboard"),
			     G_OBJECT_CLASS_TYPE(klass),
			     (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
			     G_STRUCT_OFFSET(VteTerminalClass, copy_clipboard),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);
        g_signal_set_va_marshaller(signals[SIGNAL_COPY_CLIPBOARD],
                                   G_OBJECT_CLASS_TYPE(klass),
                                   g_cclosure_marshal_VOID__VOIDv);

        /**
         * VteTerminal::paste-clipboard:
         * @vteterminal: the object which received the signal
         *
         * Emitted whenever vte_terminal_paste_clipboard() is called.
         */
	signals[SIGNAL_PASTE_CLIPBOARD] =
                g_signal_new(I_("paste-clipboard"),
			     G_OBJECT_CLASS_TYPE(klass),
			     (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
			     G_STRUCT_OFFSET(VteTerminalClass, paste_clipboard),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);
        g_signal_set_va_marshaller(signals[SIGNAL_PASTE_CLIPBOARD],
                                   G_OBJECT_CLASS_TYPE(klass),
                                   g_cclosure_marshal_VOID__VOIDv);

        /**
         * VteTerminal::bell:
         * @vteterminal: the object which received the signal
         *
         * This signal is emitted when the a child sends a bell request to the
         * terminal.
         */
        signals[SIGNAL_BELL] =
                g_signal_new(I_("bell"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteTerminalClass, bell),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);
        g_signal_set_va_marshaller(signals[SIGNAL_BELL],
                                   G_OBJECT_CLASS_TYPE(klass),
                                   g_cclosure_marshal_VOID__VOIDv);

        /**
         * VteTerminal:allow-bold:
         *
         * Controls whether or not the terminal will attempt to draw bold text,
         * by using a bold font variant.
         *
         * Deprecated: 0.60: There's probably no reason for this feature to exist.
         */
        pspecs[PROP_ALLOW_BOLD] =
                g_param_spec_boolean ("allow-bold", NULL, NULL,
                                      TRUE,
                                      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:allow-hyperlink:
         *
         * Controls whether or not hyperlinks (OSC 8 escape sequence) are recognized and displayed.
         *
         * Since: 0.50
         */
        pspecs[PROP_ALLOW_HYPERLINK] =
                g_param_spec_boolean ("allow-hyperlink", NULL, NULL,
                                      FALSE,
                                      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:audible-bell:
         *
         * Controls whether or not the terminal will beep when the child outputs the
         * "bl" sequence.
         */
        pspecs[PROP_AUDIBLE_BELL] =
                g_param_spec_boolean ("audible-bell", NULL, NULL,
                                      TRUE,
                                      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:backspace-binding:
         *
         * Controls what string or control sequence the terminal sends to its child
         * when the user presses the backspace key.
         */
        pspecs[PROP_BACKSPACE_BINDING] =
                g_param_spec_enum ("backspace-binding", NULL, NULL,
                                   VTE_TYPE_ERASE_BINDING,
                                   VTE_ERASE_AUTO,
                                   (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:bold-is-bright:
         *
         * Whether the SGR 1 attribute also switches to the bright counterpart
         * of the first 8 palette colors, in addition to making them bold (legacy behavior)
         * or if SGR 1 only enables bold and leaves the color intact.
         *
         * Since: 0.52
         */
        pspecs[PROP_BOLD_IS_BRIGHT] =
                g_param_spec_boolean ("bold-is-bright", NULL, NULL,
                                      FALSE,
                                      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:cell-height-scale:
         *
         * Scale factor for the cell height, to increase line spacing. (The font's height is not affected.)
         *
         * Since: 0.52
         */
        pspecs[PROP_CELL_HEIGHT_SCALE] =
                g_param_spec_double ("cell-height-scale", NULL, NULL,
                                     VTE_CELL_SCALE_MIN,
                                     VTE_CELL_SCALE_MAX,
                                     1.,
                                     (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:cell-width-scale:
         *
         * Scale factor for the cell width, to increase letter spacing. (The font's width is not affected.)
         *
         * Since: 0.52
         */
        pspecs[PROP_CELL_WIDTH_SCALE] =
                g_param_spec_double ("cell-width-scale", NULL, NULL,
                                     VTE_CELL_SCALE_MIN,
                                     VTE_CELL_SCALE_MAX,
                                     1.,
                                     (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:cjk-ambiguous-width:
         *
         * This setting controls whether ambiguous-width characters are narrow or wide.
         * (Note that when using a non-UTF-8 encoding set via vte_terminal_set_encoding(),
         * the width of ambiguous-width characters is fixed and determined by the encoding
         * itself.)
         *
         * This setting only takes effect the next time the terminal is reset, either
         * via escape sequence or with vte_terminal_reset().
         */
        pspecs[PROP_CJK_AMBIGUOUS_WIDTH] =
                g_param_spec_int ("cjk-ambiguous-width", NULL, NULL,
                                  1, 2, VTE_DEFAULT_UTF8_AMBIGUOUS_WIDTH,
                                  (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:cursor-blink-mode:
         *
         * Sets whether or not the cursor will blink. Using %VTE_CURSOR_BLINK_SYSTEM
         * will use the #GtkSettings:gtk-cursor-blink setting.
         */
        pspecs[PROP_CURSOR_BLINK_MODE] =
                g_param_spec_enum ("cursor-blink-mode", NULL, NULL,
                                   VTE_TYPE_CURSOR_BLINK_MODE,
                                   VTE_CURSOR_BLINK_SYSTEM,
                                   (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:cursor-shape:
         *
         * Controls the shape of the cursor.
         */
        pspecs[PROP_CURSOR_SHAPE] =
                g_param_spec_enum ("cursor-shape", NULL, NULL,
                                   VTE_TYPE_CURSOR_SHAPE,
                                   VTE_CURSOR_SHAPE_BLOCK,
                                   (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:delete-binding:
         *
         * Controls what string or control sequence the terminal sends to its child
         * when the user presses the delete key.
         */
        pspecs[PROP_DELETE_BINDING] =
                g_param_spec_enum ("delete-binding", NULL, NULL,
                                   VTE_TYPE_ERASE_BINDING,
                                   VTE_ERASE_AUTO,
                                   (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:enable-bidi:
         *
         * Controls whether or not the terminal will perform bidirectional text rendering.
         *
         * Since: 0.58
         */
        pspecs[PROP_ENABLE_BIDI] =
                g_param_spec_boolean ("enable-bidi", NULL, NULL,
                                      TRUE,
                                      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:enable-shaping:
         *
         * Controls whether or not the terminal will shape Arabic text.
         *
         * Since: 0.58
         */
        pspecs[PROP_ENABLE_SHAPING] =
                g_param_spec_boolean ("enable-shaping", NULL, NULL,
                                      TRUE,
                                      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:enable-sixel:
         *
         * Controls whether SIXEL image support is enabled.
         *
         * Since: 0.62
         */
        pspecs[PROP_ENABLE_SIXEL] =
                g_param_spec_boolean ("enable-sixel", nullptr, nullptr,
                                      false,
                                      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));


        /**
         * VteTerminal:font-scale:
         *
         * The terminal's font scale.
         */
        pspecs[PROP_FONT_SCALE] =
                g_param_spec_double ("font-scale", NULL, NULL,
                                     VTE_FONT_SCALE_MIN,
                                     VTE_FONT_SCALE_MAX,
                                     1.,
                                     (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:encoding:
         *
         * Controls the encoding the terminal will expect data from the child to
         * be encoded with.  For certain terminal types, applications executing in the
         * terminal can change the encoding.  The default is defined by the
         * application's locale settings.
         *
         * Deprecated: 0.54: Instead of using this, you should use a tool like
         *   luit(1) when support for non-UTF-8 is required
         */
        pspecs[PROP_ENCODING] =
                g_param_spec_string ("encoding", NULL, NULL,
                                     NULL,
                                     (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY | _VTE_PARAM_DEPRECATED));

        /**
         * VteTerminal:font-desc:
         *
         * Specifies the font used for rendering all text displayed by the terminal,
         * overriding any fonts set using gtk_widget_modify_font().  The terminal
         * will immediately attempt to load the desired font, retrieve its
         * metrics, and attempt to resize itself to keep the same number of rows
         * and columns.
         */
        pspecs[PROP_FONT_DESC] =
                g_param_spec_boxed ("font-desc", NULL, NULL,
                                    PANGO_TYPE_FONT_DESCRIPTION,
                                    (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:icon-title:
         *
         * Deprecated: 0.54: This property is always %NULL.
         */
        pspecs[PROP_ICON_TITLE] =
                g_param_spec_string ("icon-title", NULL, NULL,
                                     NULL,
                                     (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:input-enabled:
         *
         * Controls whether the terminal allows user input. When user input is disabled,
         * key press and mouse button press and motion events are not sent to the
         * terminal's child.
         */
        pspecs[PROP_INPUT_ENABLED] =
                g_param_spec_boolean ("input-enabled", NULL, NULL,
                                      TRUE,
                                      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:pointer-autohide:
         *
         * Controls the value of the terminal's mouse autohide setting.  When autohiding
         * is enabled, the mouse cursor will be hidden when the user presses a key and
         * shown when the user moves the mouse.
         */
        pspecs[PROP_MOUSE_POINTER_AUTOHIDE] =
                g_param_spec_boolean ("pointer-autohide", NULL, NULL,
                                      FALSE,
                                      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:pty:
         *
         * The PTY object for the terminal.
         */
        pspecs[PROP_PTY] =
                g_param_spec_object ("pty", NULL, NULL,
                                     VTE_TYPE_PTY,
                                     (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:rewrap-on-resize:
         *
         * Controls whether or not the terminal will rewrap its contents, including
         * the scrollback buffer, whenever the terminal's width changes.
         *
         * Deprecated: 0.58
         */
        pspecs[PROP_REWRAP_ON_RESIZE] =
                g_param_spec_boolean ("rewrap-on-resize", NULL, NULL,
                                      TRUE,
                                      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:scrollback-lines:
         *
         * The length of the scrollback buffer used by the terminal.  The size of
         * the scrollback buffer will be set to the larger of this value and the number
         * of visible rows the widget can display, so 0 can safely be used to disable
         * scrollback.  Note that this setting only affects the normal screen buffer.
         * For terminal types which have an alternate screen buffer, no scrollback is
         * allowed on the alternate screen buffer.
         */
        pspecs[PROP_SCROLLBACK_LINES] =
                g_param_spec_uint ("scrollback-lines", NULL, NULL,
                                   0, G_MAXUINT,
                                   VTE_SCROLLBACK_INIT,
                                   (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:scroll-on-keystroke:
         *
         * Controls whether or not the terminal will forcibly scroll to the bottom of
         * the viewable history when the user presses a key.  Modifier keys do not
         * trigger this behavior.
         */
        pspecs[PROP_SCROLL_ON_KEYSTROKE] =
                g_param_spec_boolean ("scroll-on-keystroke", NULL, NULL,
                                      FALSE,
                                      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:scroll-on-output:
         *
         * Controls whether or not the terminal will forcibly scroll to the bottom of
         * the viewable history when the new data is received from the child.
         */
        pspecs[PROP_SCROLL_ON_OUTPUT] =
                g_param_spec_boolean ("scroll-on-output", NULL, NULL,
                                      TRUE,
                                      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:handle-scroll:
         *
         * Controls whether or not the terminal manages its own scrolling. This can be
         * disabled when the terminal is the child of a GtkScrolledWindow to take
         * advantage of kinetic scrolling.
         *
         * Since: 0.64
         */
        pspecs[PROP_ENABLE_FALLBACK_SCROLLING] =
                g_param_spec_boolean ("enable-fallback-scrolling", nullptr, nullptr,
                                      true,
                                      GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:scroll-unit-is-pixels:
         *
         * Controls whether the terminal's GtkAdjustment values unit is lines
         * or pixels. This can be enabled when the terminal is the child of a
         * GtkScrolledWindow to fix some bugs with its kinetic scrolling.
         *
         * Since: 0.66
         */
        pspecs[PROP_SCROLL_UNIT_IS_PIXELS] =
                g_param_spec_boolean ("scroll-unit-is-pixels", nullptr, nullptr,
                                      false,
                                      GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:text-blink-mode:
         *
         * Controls whether or not the terminal will allow blinking text.
         *
         * Since: 0.52
         */
        pspecs[PROP_TEXT_BLINK_MODE] =
                g_param_spec_enum ("text-blink-mode", NULL, NULL,
                                   VTE_TYPE_TEXT_BLINK_MODE,
                                   VTE_TEXT_BLINK_ALWAYS,
                                   (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:window-title:
         *
         * The terminal's title.
         */
        pspecs[PROP_WINDOW_TITLE] =
                g_param_spec_string ("window-title", NULL, NULL,
                                     NULL,
                                     (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:current-directory-uri:
         *
         * The current directory URI, or %NULL if unset.
         */
        pspecs[PROP_CURRENT_DIRECTORY_URI] =
                g_param_spec_string ("current-directory-uri", NULL, NULL,
                                     NULL,
                                     (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:current-file-uri:
         *
         * The current file URI, or %NULL if unset.
         */
        pspecs[PROP_CURRENT_FILE_URI] =
                g_param_spec_string ("current-file-uri", NULL, NULL,
                                     NULL,
                                     (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:hyperlink-hover-uri:
         *
         * The currently hovered hyperlink URI, or %NULL if unset.
         *
         * Since: 0.50
         */
        pspecs[PROP_HYPERLINK_HOVER_URI] =
                g_param_spec_string ("hyperlink-hover-uri", NULL, NULL,
                                     NULL,
                                     (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:word-char-exceptions:
         *
         * The set of characters which will be considered parts of a word
         * when doing word-wise selection, in addition to the default which only
         * considers alphanumeric characters part of a word.
         *
         * If %NULL, a built-in set is used.
         *
         * Since: 0.40
         */
        pspecs[PROP_WORD_CHAR_EXCEPTIONS] =
                g_param_spec_string ("word-char-exceptions", NULL, NULL,
                                     NULL,
                                     (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        g_object_class_install_properties(gobject_class, LAST_PROP, pspecs);

#if VTE_GTK == 3
	/* Disable GtkWidget's keybindings except for Shift-F10 and MenuKey
         * which pop up the context menu.
         */
	auto const binding_set = gtk_binding_set_by_class(vte_terminal_parent_class);
	gtk_binding_entry_skip(binding_set, GDK_KEY_F1, GDK_CONTROL_MASK);
	gtk_binding_entry_skip(binding_set, GDK_KEY_F1, GDK_SHIFT_MASK);
	gtk_binding_entry_skip(binding_set, GDK_KEY_KP_F1, GDK_CONTROL_MASK);
	gtk_binding_entry_skip(binding_set, GDK_KEY_KP_F1, GDK_SHIFT_MASK);
#endif /* VTE_GTK == 3 */

        process_timer = g_timer_new();

        klass->priv = G_TYPE_CLASS_GET_PRIVATE (klass, VTE_TYPE_TERMINAL, VteTerminalClassPrivate);

        klass->priv->style_provider = GTK_STYLE_PROVIDER (gtk_css_provider_new ());
#if VTE_GTK == 3
        auto err = vte::glib::Error{};
#elif VTE_GTK == 4
        g_signal_connect(klass->priv->style_provider, "parsing-error",
                         G_CALLBACK(style_provider_parsing_error_cb), nullptr);
#endif
        gtk_css_provider_load_from_data (GTK_CSS_PROVIDER (klass->priv->style_provider),
                                         "VteTerminal, " VTE_TERMINAL_CSS_NAME " {\n"
                                         "padding: 1px 1px 1px 1px;\n"
#if (VTE_GTK == 4) || ((VTE_GTK == 3) && GTK_CHECK_VERSION (3, 24, 22))
                                         "background-color: @text_view_bg;\n"
#else
                                         "background-color: @theme_base_color;\n"
#endif
                                         "color: @theme_text_color;\n"
                                         "}\n",
                                         -1
#if VTE_GTK == 3
                                         , NULL
#endif
                                         );
#if VTE_GTK == 3
        err.assert_no_error();
#endif

#if VTE_GTK == 3
#ifdef WITH_A11Y
        /* a11y */
        gtk_widget_class_set_accessible_type(widget_class, VTE_TYPE_TERMINAL_ACCESSIBLE);
#endif
#endif
}

/* public API */

/**
 * vte_get_features:
 *
 * Gets a list of features vte was compiled with.
 *
 * Returns: (transfer none): a string with features
 *
 * Since: 0.40
 */
const char *
vte_get_features (void) noexcept
{
        return
#ifdef WITH_FRIBIDI
                "+BIDI"
#else
                "-BIDI"
#endif
                " "
#ifdef WITH_GNUTLS
                "+GNUTLS"
#else
                "-GNUTLS"
#endif
                " "
#ifdef WITH_ICU
                "+ICU"
#else
                "-ICU"
#endif
#ifdef __linux__
                " "
#ifdef WITH_SYSTEMD
                "+SYSTEMD"
#else
                "-SYSTEMD"
#endif
#endif // __linux__
                ;
}

/**
 * vte_get_feature_flags:
 *
 * Gets features VTE was compiled with.
 *
 * Returns: (transfer none): flags from #VteFeatureFlags
 *
 * Since: 0.62
 */
VteFeatureFlags
vte_get_feature_flags(void) noexcept
{
        return VteFeatureFlags(0ULL |
#ifdef WITH_FRIBIDI
                               VTE_FEATURE_FLAG_BIDI |
#endif
#ifdef WITH_ICU
                               VTE_FEATURE_FLAG_ICU |
#endif
#ifdef __linux__
#ifdef WITH_SYSTEMD
                               VTE_FEATURE_FLAG_SYSTEMD |
#endif
#endif // __linux__
                               0ULL);
}

/**
 * vte_get_major_version:
 *
 * Returns the major version of the VTE library at runtime.
 * Contrast this with %VTE_MAJOR_VERSION which represents
 * the version of the VTE library that the code was compiled
 * with.
 *
 * Returns: the major version
 *
 * Since: 0.40
 */
guint
vte_get_major_version (void) noexcept
{
        return VTE_MAJOR_VERSION;
}

/**
 * vte_get_minor_version:
 *
 * Returns the minor version of the VTE library at runtime.
 * Contrast this with %VTE_MINOR_VERSION which represents
 * the version of the VTE library that the code was compiled
 * with.
 *
 * Returns: the minor version
 *
 * Since: 0.40
 */
guint
vte_get_minor_version (void) noexcept
{
        return VTE_MINOR_VERSION;
}

/**
 * vte_get_micro_version:
 *
 * Returns the micro version of the VTE library at runtime.
 * Contrast this with %VTE_MICRO_VERSION which represents
 * the version of the VTE library that the code was compiled
 * with.
 *
 * Returns: the micro version
 *
 * Since: 0.40
 */
guint
vte_get_micro_version (void) noexcept
{
        return VTE_MICRO_VERSION;
}

/**
 * vte_get_user_shell:
 *
 * Gets the user's shell, or %NULL. In the latter case, the
 * system default (usually "/bin/sh") should be used.
 *
 * Returns: (transfer full) (type filename): a newly allocated string with the
 *   user's shell, or %NULL
 */
char *
vte_get_user_shell (void) noexcept
{
	struct passwd *pwd;

	pwd = getpwuid(getuid());
        if (pwd && pwd->pw_shell)
                return g_strdup (pwd->pw_shell);

        return NULL;
}

/**
 * vte_set_test_flags: (skip)
 * @flags: flags
 *
 * Sets test flags. This function is only useful for implementing
 * unit tests for vte itself; it is a no-op in non-debug builds.
 *
 * Since: 0.54
 */
void
vte_set_test_flags(guint64 flags) noexcept
{
#ifdef VTE_DEBUG
        g_test_flags = flags;
#endif
}

/**
 * vte_get_encodings:
 * @include_aliases: whether to include alias names
 *
 * Gets the list of supported legacy encodings.
 *
 * If ICU support is not available, this returns an empty vector.
 * Note that UTF-8 is always supported; you can select it by
 * passing %NULL to vte_terminal_set_encoding().
 *
 * Returns: (transfer full): the list of supported encodings; free with
 *   g_strfreev()
 *
 * Since: 0.60
 * Deprecated: 0.60
 */
char **
vte_get_encodings(gboolean include_aliases) noexcept
try
{
#ifdef WITH_ICU
        return vte::base::get_icu_charsets(include_aliases != FALSE);
#else
        char *empty[] = { nullptr };
        return g_strdupv(empty);
#endif
}
catch (...)
{
        vte::log_exception();

        char *empty[] = { nullptr };
        return g_strdupv(empty);
}

/**
 * vte_get_encoding_supported:
 * @encoding: the name of the legacy encoding
 *
 * Queries whether the legacy encoding @encoding is supported.
 *
 * If ICU support is not available, this function always returns %FALSE.
 *
 * Note that UTF-8 is always supported; you can select it by
 * passing %NULL to vte_terminal_set_encoding().
 *
 * Returns: %TRUE iff the legacy encoding @encoding is supported
 *
 * Since: 0.60
 * Deprecated: 0.60
 */
gboolean
vte_get_encoding_supported(const char *encoding) noexcept
try
{
        g_return_val_if_fail(encoding != nullptr, false);

#ifdef WITH_ICU
        return vte::base::get_icu_charset_supported(encoding);
#else
        return false;
#endif
}
catch (...)
{
        vte::log_exception();
        return false;
}

/* VteTerminal public API */

/**
 * vte_terminal_new:
 *
 * Creates a new terminal widget.
 *
 * Returns: (transfer none) (type Vte.Terminal): a new #VteTerminal object
 */
GtkWidget *
vte_terminal_new(void) noexcept
{
	return (GtkWidget *)g_object_new(VTE_TYPE_TERMINAL, nullptr);
}

/**
 * vte_terminal_copy_clipboard:
 * @terminal: a #VteTerminal
 *
 * Places the selected text in the terminal in the #GDK_SELECTION_CLIPBOARD
 * selection.
 *
 * Deprecated: 0.50: Use vte_terminal_copy_clipboard_format() with %VTE_FORMAT_TEXT
 *   instead.
 */
void
vte_terminal_copy_clipboard(VteTerminal *terminal) noexcept
try
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        IMPL(terminal)->emit_copy_clipboard();
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_copy_clipboard_format:
 * @terminal: a #VteTerminal
 * @format: a #VteFormat
 *
 * Places the selected text in the terminal in the #GDK_SELECTION_CLIPBOARD
 * selection in the form specified by @format.
 *
 * For all formats, the selection data (see #GtkSelectionData) will include the
 * text targets (see gtk_target_list_add_text_targets() and
 * gtk_selection_data_targets_includes_text()). For %VTE_FORMAT_HTML,
 * the selection will also include the "text/html" target, which when requested,
 * returns the HTML data in UTF-16 with a U+FEFF BYTE ORDER MARK character at
 * the start.
 *
 * Since: 0.50
 */
void
vte_terminal_copy_clipboard_format(VteTerminal *terminal,
                                   VteFormat format) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(format == VTE_FORMAT_TEXT || format == VTE_FORMAT_HTML);

        WIDGET(terminal)->copy(vte::platform::ClipboardType::CLIPBOARD,
                               clipboard_format_from_vte(format));
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_copy_primary:
 * @terminal: a #VteTerminal
 *
 * Places the selected text in the terminal in the #GDK_SELECTION_PRIMARY
 * selection.
 */
void
vte_terminal_copy_primary(VteTerminal *terminal) noexcept
try
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	_vte_debug_print(VTE_DEBUG_SELECTION, "Copying to PRIMARY.\n");
	WIDGET(terminal)->copy(vte::platform::ClipboardType::PRIMARY,
                               vte::platform::ClipboardFormat::TEXT);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_paste_clipboard:
 * @terminal: a #VteTerminal
 *
 * Sends the contents of the #GDK_SELECTION_CLIPBOARD selection to the
 * terminal's child. It's called on paste menu item, or when
 * user presses Shift+Insert.
 */
void
vte_terminal_paste_clipboard(VteTerminal *terminal) noexcept
try
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        IMPL(terminal)->emit_paste_clipboard();
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_paste_text:
 * @terminal: a #VteTerminal
 * @text: a string to paste
 *
 * Sends @text to the terminal's child as if retrived from the clipboard,
 * this differs from vte_terminal_feed_child() in that it may process
 * @text before passing it to the child (e.g. apply bracketed mode)
 *
 * Since: 0.68
 */
void
vte_terminal_paste_text(VteTerminal *terminal,
                        char const* text) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(text != nullptr);

        WIDGET(terminal)->paste_text(text);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_paste_primary:
 * @terminal: a #VteTerminal
 *
 * Sends the contents of the #GDK_SELECTION_PRIMARY selection to the terminal's
 * child. The terminal will call also paste the
 * #GDK_SELECTION_PRIMARY selection when the user clicks with the the second
 * mouse button.
 */
void
vte_terminal_paste_primary(VteTerminal *terminal) noexcept
try
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	_vte_debug_print(VTE_DEBUG_SELECTION, "Pasting PRIMARY.\n");
	WIDGET(terminal)->paste(vte::platform::ClipboardType::PRIMARY);
}
catch (...)
{
        vte::log_exception();
}

#if VTE_GTK == 3

/**
 * vte_terminal_match_add_gregex:
 * @terminal: a #VteTerminal
 * @gregex: a #GRegex
 * @gflags: the #GRegexMatchFlags to use when matching the regex
 *
 * This function does nothing since version 0.60.
 *
 * Returns: -1
 *
 * Deprecated: 0.46: Use vte_terminal_match_add_regex() instead.
 */
int
vte_terminal_match_add_gregex(VteTerminal *terminal,
                              GRegex *gregex,
                              GRegexMatchFlags gflags) noexcept
{
        return -1;
}

#endif /* VTE_GTK == 3 */

/**
 * vte_terminal_match_add_regex:
 * @terminal: a #VteTerminal
 * @regex: (transfer none): a #VteRegex
 * @flags: PCRE2 match flags, or 0
 *
 * Adds the regular expression @regex to the list of matching expressions.  When the
 * user moves the mouse cursor over a section of displayed text which matches
 * this expression, the text will be highlighted.
 *
 * Note that @regex should have been created using the <literal>PCRE2_MULTILINE</literal>
 * flag.
 *
 * Returns: an integer associated with this expression
 *
 * Since: 0.46
 */
int
vte_terminal_match_add_regex(VteTerminal *terminal,
                             VteRegex    *regex,
                             guint32      flags) noexcept
try
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), -1);
	g_return_val_if_fail(regex != NULL, -1);
        g_return_val_if_fail(_vte_regex_has_purpose(regex, vte::base::Regex::Purpose::eMatch), -1);
        g_warn_if_fail(_vte_regex_has_multiline_compile_flag(regex));

        auto impl = IMPL(terminal);
        return impl->regex_match_add(vte::base::make_ref(regex_from_wrapper(regex)),
                                     flags,
                                     VTE_DEFAULT_CURSOR,
                                     impl->regex_match_next_tag()).tag();
}
catch (...)
{
        vte::log_exception();
        return -1;
}

/**
 * vte_terminal_match_check:
 * @terminal: a #VteTerminal
 * @column: the text column
 * @row: the text row
 * @tag: (out) (allow-none): a location to store the tag, or %NULL
 *
 * Checks if the text in and around the specified position matches any of the
 * regular expressions previously set using vte_terminal_match_add().  If a
 * match exists, the text string is returned and if @tag is not %NULL, the number
 * associated with the matched regular expression will be stored in @tag.
 *
 * If more than one regular expression has been set with
 * vte_terminal_match_add(), then expressions are checked in the order in
 * which they were added.
 *
 * Returns: (transfer full) (nullable): a newly allocated string which matches one of the previously
 *   set regular expressions
 *
 * Deprecated: 0.46: Use vte_terminal_match_check_event() instead.
 */
char *
vte_terminal_match_check(VteTerminal *terminal,
                         long column,
                         long row,
			 int *tag) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);
        return WIDGET(terminal)->regex_match_check(column, row, tag);
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

#if VTE_GTK == 3

/**
 * vte_terminal_match_check_event:
 * @terminal: a #VteTerminal
 * @event: a #GdkEvent
 * @tag: (out) (allow-none): a location to store the tag, or %NULL
 *
 * Checks if the text in and around the position of the event matches any of the
 * regular expressions previously set using vte_terminal_match_add().  If a
 * match exists, the text string is returned and if @tag is not %NULL, the number
 * associated with the matched regular expression will be stored in @tag.
 *
 * If more than one regular expression has been set with
 * vte_terminal_match_add(), then expressions are checked in the order in
 * which they were added.
 *
 * Returns: (transfer full) (nullable): a newly allocated string which matches one of the previously
 *   set regular expressions, or %NULL if there is no match
 */
char *
vte_terminal_match_check_event(VteTerminal *terminal,
                               GdkEvent *event,
                               int *tag) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
        return WIDGET(terminal)->regex_match_check(event, tag);
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

/**
 * vte_terminal_hyperlink_check_event:
 * @terminal: a #VteTerminal
 * @event: a #GdkEvent
 *
 * Returns a nonempty string: the target of the explicit hyperlink (printed using the OSC 8
 * escape sequence) at the position of the event, or %NULL.
 *
 * Proper use of the escape sequence should result in URI-encoded URIs with a proper scheme
 * like "http://", "https://", "file://", "mailto:" etc. This is, however, not enforced by VTE.
 * The caller must tolerate the returned string potentially not being a valid URI.
 *
 * Returns: (transfer full) (nullable): a newly allocated string containing the target of the hyperlink,
 *  or %NULL
 *
 * Since: 0.50
 */
char *
vte_terminal_hyperlink_check_event(VteTerminal *terminal,
                                   GdkEvent *event) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), nullptr);
        return WIDGET(terminal)->hyperlink_check(event);
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

/**
 * vte_terminal_event_check_regex_array: (rename-to vte_terminal_event_check_regex_simple)
 * @terminal: a #VteTerminal
 * @event: a #GdkEvent
 * @regexes: (array length=n_regexes): an array of #VteRegex
 * @n_regexes: number of items in @regexes
 * @match_flags: PCRE2 match flags, or 0
 * @n_matches: (out) (optional): number of items in @matches, which is always equal to @n_regexes
 *
 * Like vte_terminal_event_check_regex_simple(), but returns an array of strings,
 * containing the matching text (or %NULL if no match) corresponding to each of the
 * regexes in @regexes.
 *
 * You must free each string and the array; but note that this is *not* a %NULL-terminated
 * string array, and so you must *not* use g_strfreev() on it.
 *
 * Returns: (nullable) (transfer full) (array length=n_matches): a newly allocated array of strings,
 *   or %NULL if none of the regexes matched
 *
 * Since: 0.62
 */
char**
vte_terminal_event_check_regex_array(VteTerminal *terminal,
                                     GdkEvent *event,
                                     VteRegex **regexes,
                                     gsize n_regexes,
                                     guint32 match_flags,
                                     gsize *n_matches) noexcept
try
{
        if (n_matches)
                *n_matches = n_regexes;

        if (n_regexes == 0)
                return nullptr;

        auto matches = vte::glib::take_free_ptr(g_new0(char*, n_regexes));
        if (!vte_terminal_event_check_regex_simple(terminal,
                                                   event,
                                                   regexes,
                                                   n_regexes,
                                                   match_flags,
                                                   matches.get()))
            return nullptr;

        return matches.release();
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

/**
 * vte_terminal_event_check_regex_simple: (skip)
 * @terminal: a #VteTerminal
 * @event: a #GdkEvent
 * @regexes: (array length=n_regexes): an array of #VteRegex
 * @n_regexes: number of items in @regexes
 * @match_flags: PCRE2 match flags, or 0
 * @matches: (out caller-allocates) (array length=n_regexes) (transfer full): a location to store the matches
 *
 * Checks each regex in @regexes if the text in and around the position of
 * the event matches the regular expressions.  If a match exists, the matched
 * text is stored in @matches at the position of the regex in @regexes; otherwise
 * %NULL is stored there.  Each non-%NULL element of @matches should be freed with
 * g_free().
 *
 * Note that the regexes in @regexes should have been created using the
 * <literal>PCRE2_MULTILINE</literal> flag.
 *
 * Returns: %TRUE iff any of the regexes produced a match
 *
 * Since: 0.46
 */
gboolean
vte_terminal_event_check_regex_simple(VteTerminal *terminal,
                                      GdkEvent *event,
                                      VteRegex **regexes,
                                      gsize n_regexes,
                                      guint32 match_flags,
                                      char **matches) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
        g_return_val_if_fail(event != NULL, FALSE);
        g_return_val_if_fail(regexes != NULL || n_regexes == 0, FALSE);
        for (gsize i = 0; i < n_regexes; i++) {
                g_return_val_if_fail(_vte_regex_has_purpose(regexes[i], vte::base::Regex::Purpose::eMatch), -1);
                g_warn_if_fail(_vte_regex_has_multiline_compile_flag(regexes[i]));
        }
        g_return_val_if_fail(matches != NULL, FALSE);

        return WIDGET(terminal)->regex_match_check_extra(event,
                                                         regex_array_from_wrappers(regexes),
                                                         n_regexes,
                                                         match_flags,
                                                         matches);
}
catch (...)
{
        vte::log_exception();
        return false;
}

#elif VTE_GTK == 4

/**
 * vte_terminal_check_match_at:
 * @terminal: a #VteTerminal
 * @x:
 * @y:
 * @tag: (out) (allow-none): a location to store the tag, or %NULL
 *
 * Checks if the text in and around the position (x, y) matches any of the
 * regular expressions previously set using vte_terminal_match_add().  If a
 * match exists, the text string is returned and if @tag is not %NULL, the number
 * associated with the matched regular expression will be stored in @tag.
 *
 * If more than one regular expression has been set with
 * vte_terminal_match_add(), then expressions are checked in the order in
 * which they were added.
 *
 * Returns: (transfer full) (nullable): a newly allocated string which matches one of the previously
 *   set regular expressions, or %NULL if there is no match
 *
 * Since: 0.70
 */
char*
vte_terminal_check_match_at(VteTerminal* terminal,
                            double x,
                            double y,
                            int* tag) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
        return WIDGET(terminal)->regex_match_check_at(x, y, tag);
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

/**
 * vte_terminal_check_hyperlink_at:
 * @terminal: a #VteTerminal
 * @x:
 * @y:
 *
 * Returns a nonempty string: the target of the explicit hyperlink (printed using the OSC 8
 * escape sequence) at the position (x, y), or %NULL.
 *
 * Proper use of the escape sequence should result in URI-encoded URIs with a proper scheme
 * like "http://", "https://", "file://", "mailto:" etc. This is, however, not enforced by VTE.
 * The caller must tolerate the returned string potentially not being a valid URI.
 *
 * Returns: (transfer full) (nullable): a newly allocated string containing the target of the hyperlink,
 *  or %NULL
 *
 * Since: 0.70
 */
char*
vte_terminal_check_hyperlink_at(VteTerminal* terminal,
                                double x,
                                double y) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), nullptr);
        return WIDGET(terminal)->hyperlink_check_at(x, y);
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

/**
 * vte_terminal_check_regex_array_at: (rename-to vte_terminal_check_regex_simple_at)
 * @terminal: a #VteTerminal
 * @x:
 * @y:
 * @regexes: (array length=n_regexes): an array of #VteRegex
 * @n_regexes: number of items in @regexes
 * @match_flags: PCRE2 match flags, or 0
 * @n_matches: (out) (optional): number of items in @matches, which is always equal to @n_regexes
 *
 * Like vte_terminal_check_regex_simple_at(), but returns an array of strings,
 * containing the matching text (or %NULL if no match) corresponding to each of the
 * regexes in @regexes.
 *
 * You must free each string and the array; but note that this is *not* a %NULL-terminated
 * string array, and so you must *not* use g_strfreev() on it.
 *
 * Returns: (nullable) (transfer full) (array length=n_matches): a newly allocated array of strings,
 *   or %NULL if none of the regexes matched
 *
 * Since: 0.70
 */
char**
vte_terminal_check_regex_array_at(VteTerminal* terminal,
                                  double x,
                                  double y,
                                  VteRegex** regexes,
                                  gsize n_regexes,
                                  guint32 match_flags,
                                  gsize* n_matches) noexcept
try
{
        if (n_matches)
                *n_matches = n_regexes;

        if (n_regexes == 0)
                return nullptr;

        auto matches = vte::glib::take_free_ptr(g_new0(char*, n_regexes));
        if (!vte_terminal_check_regex_simple_at(terminal,
                                                x, y,
                                                regexes,
                                                n_regexes,
                                                match_flags,
                                                matches.get()))
            return nullptr;

        return matches.release();
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

/**
 * vte_terminal_check_regex_simple_at: (skip)
 * @terminal: a #VteTerminal
 * @x:
 * @y:
 * @regexes: (array length=n_regexes): an array of #VteRegex
 * @n_regexes: number of items in @regexes
 * @match_flags: PCRE2 match flags, or 0
 * @matches: (out caller-allocates) (array length=n_regexes) (transfer full): a location to store the matches
 *
 * Checks each regex in @regexes if the text in and around the position (x, y)
 * matches the regular expressions.  If a match exists, the matched
 * text is stored in @matches at the position of the regex in @regexes; otherwise
 * %NULL is stored there.  Each non-%NULL element of @matches should be freed with
 * g_free().
 *
 * Note that the regexes in @regexes should have been created using the %PCRE2_MULTILINE flag.
 *
 * Returns: %TRUE iff any of the regexes produced a match
 *
 * Since: 0.70
 */
gboolean
vte_terminal_check_regex_simple_at(VteTerminal* terminal,
                                   double x,
                                   double y,
                                   VteRegex** regexes,
                                   gsize n_regexes,
                                   guint32 match_flags,
                                   char** matches) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
        g_return_val_if_fail(regexes != NULL || n_regexes == 0, FALSE);
        for (gsize i = 0; i < n_regexes; i++) {
                g_return_val_if_fail(_vte_regex_has_purpose(regexes[i], vte::base::Regex::Purpose::eMatch), -1);
                g_warn_if_fail(_vte_regex_has_multiline_compile_flag(regexes[i]));
        }
        g_return_val_if_fail(matches != NULL, FALSE);

        return WIDGET(terminal)->regex_match_check_extra_at(x, y,
                                                            regex_array_from_wrappers(regexes),
                                                            n_regexes,
                                                            match_flags,
                                                            matches);
}
catch (...)
{
        vte::log_exception();
        return false;
}

#endif /* VTE_GTK */

#if VTE_GTK == 3

/**
 * vte_terminal_event_check_gregex_simple:
 * @terminal: a #VteTerminal
 * @event: a #GdkEvent
 * @regexes: (array length=n_regexes): an array of #GRegex
 * @n_regexes: number of items in @regexes
 * @match_flags: the #GRegexMatchFlags to use when matching the regexes
 * @matches: (out caller-allocates) (array length=n_regexes): a location to store the matches
 *
 * This function does nothing.
 *
 * Returns: %FALSE
 *
 * Since: 0.44
 * Deprecated: 0.46: Use vte_terminal_event_check_regex_simple() instead.
 */
gboolean
vte_terminal_event_check_gregex_simple(VteTerminal *terminal,
                                       GdkEvent *event,
                                       GRegex **regexes,
                                       gsize n_regexes,
                                       GRegexMatchFlags match_flags,
                                       char **matches) noexcept
{
        return FALSE;
}

#endif /* VTE_GTK == 3 */

/**
 * vte_terminal_match_set_cursor:
 * @terminal: a #VteTerminal
 * @tag: the tag of the regex which should use the specified cursor
 * @cursor: (allow-none): the #GdkCursor which the terminal should use when the pattern is
 *   highlighted, or %NULL to use the standard cursor
 *
 * Sets which cursor the terminal will use if the pointer is over the pattern
 * specified by @tag.  The terminal keeps a reference to @cursor.
 *
 * Deprecated: 0.40: Use vte_terminal_match_set_cursor_name() instead.
 */
void
vte_terminal_match_set_cursor(VteTerminal *terminal,
                              int tag,
                              GdkCursor *cursor) noexcept
try
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(tag >= 0);
        if (auto rem = IMPL(terminal)->regex_match_get(tag))
                rem->set_cursor(vte::glib::make_ref<GdkCursor>(cursor));
}
catch (...)
{
        vte::log_exception();
}

#if VTE_GTK == 3

/**
 * vte_terminal_match_set_cursor_type:
 * @terminal: a #VteTerminal
 * @tag: the tag of the regex which should use the specified cursor
 * @cursor_type: a #GdkCursorType
 *
 * Sets which cursor the terminal will use if the pointer is over the pattern
 * specified by @tag.
 *
 * Deprecated: 0.54: Use vte_terminal_match_set_cursor_name() instead.
 */
void
vte_terminal_match_set_cursor_type(VteTerminal *terminal,
				   int tag,
                                   GdkCursorType cursor_type) noexcept
try
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(tag >= 0);
        if (auto rem = IMPL(terminal)->regex_match_get(tag))
                rem->set_cursor(cursor_type);
}
catch (...)
{
        vte::log_exception();
}
#endif /* VTE_GTK == 3 */

/**
 * vte_terminal_match_set_cursor_name:
 * @terminal: a #VteTerminal
 * @tag: the tag of the regex which should use the specified cursor
 * @cursor_name: the name of the cursor
 *
 * Sets which cursor the terminal will use if the pointer is over the pattern
 * specified by @tag.
 */
void
vte_terminal_match_set_cursor_name(VteTerminal *terminal,
				   int tag,
                                   const char *cursor_name) noexcept
try
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(tag >= 0);
        if (auto rem = IMPL(terminal)->regex_match_get(tag))
                rem->set_cursor(cursor_name);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_match_remove:
 * @terminal: a #VteTerminal
 * @tag: the tag of the regex to remove
 *
 * Removes the regular expression which is associated with the given @tag from
 * the list of expressions which the terminal will highlight when the user
 * moves the mouse cursor over matching text.
 */
void
vte_terminal_match_remove(VteTerminal *terminal,
                          int tag) noexcept
try
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
        IMPL(terminal)->regex_match_remove(tag);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_match_remove_all:
 * @terminal: a #VteTerminal
 *
 * Clears the list of regular expressions the terminal uses to highlight text
 * when the user moves the mouse cursor.
 */
void
vte_terminal_match_remove_all(VteTerminal *terminal) noexcept
try
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
        IMPL(terminal)->regex_match_remove_all();
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_search_find_previous:
 * @terminal: a #VteTerminal
 *
 * Searches the previous string matching the search regex set with
 * vte_terminal_search_set_regex().
 *
 * Returns: %TRUE if a match was found
 */
gboolean
vte_terminal_search_find_previous (VteTerminal *terminal) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), false);
	return IMPL(terminal)->search_find(true);
}
catch (...)
{
        vte::log_exception();
        return false;
}

/**
 * vte_terminal_search_find_next:
 * @terminal: a #VteTerminal
 *
 * Searches the next string matching the search regex set with
 * vte_terminal_search_set_regex().
 *
 * Returns: %TRUE if a match was found
 */
gboolean
vte_terminal_search_find_next (VteTerminal *terminal) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), false);
	return IMPL(terminal)->search_find(false);
}
catch (...)
{
        vte::log_exception();
        return false;
}

/**
 * vte_terminal_search_set_regex:
 * @terminal: a #VteTerminal
 * @regex: (allow-none): a #VteRegex, or %NULL
 * @flags: PCRE2 match flags, or 0
 *
 * Sets the regex to search for. Unsets the search regex when passed %NULL.
 *
 * Note that @regex should have been created using the
 * <literal>PCRE2_MULTILINE</literal> flag.
 *
 * Since: 0.46
 */
void
vte_terminal_search_set_regex (VteTerminal *terminal,
                               VteRegex    *regex,
                               guint32      flags) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(regex == nullptr || _vte_regex_has_purpose(regex, vte::base::Regex::Purpose::eSearch));
        g_warn_if_fail(regex == nullptr || _vte_regex_has_multiline_compile_flag(regex));

        IMPL(terminal)->search_set_regex(vte::base::make_ref(regex_from_wrapper(regex)), flags);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_search_get_regex:
 * @terminal: a #VteTerminal
 *
 * Returns: (transfer none): the search #VteRegex regex set in @terminal, or %NULL
 *
 * Since: 0.46
 */
VteRegex *
vte_terminal_search_get_regex(VteTerminal *terminal) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), nullptr);

        return wrapper_from_regex(IMPL(terminal)->search_regex());
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

#if VTE_GTK == 3

/**
 * vte_terminal_search_set_gregex:
 * @terminal: a #VteTerminal
 * @gregex: (allow-none): a #GRegex, or %NULL
 * @gflags: flags from #GRegexMatchFlags
 *
 * This function does nothing since version 0.60.
 *
 * Deprecated: 0.46: use vte_terminal_search_set_regex() instead.
 */
void
vte_terminal_search_set_gregex (VteTerminal *terminal,
				GRegex      *gregex,
                                GRegexMatchFlags gflags) noexcept
{
}

/**
 * vte_terminal_search_get_gregex:
 * @terminal: a #VteTerminal
 *
 * Returns: (transfer none): %NULL
 *
 * Deprecated: 0.46: use vte_terminal_search_get_regex() instead.
 */
GRegex *
vte_terminal_search_get_gregex (VteTerminal *terminal) noexcept
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), nullptr);

        return nullptr;
}

#endif /* VTE_GTK == 3 */

/**
 * vte_terminal_search_set_wrap_around:
 * @terminal: a #VteTerminal
 * @wrap_around: whether search should wrap
 *
 * Sets whether search should wrap around to the beginning of the
 * terminal content when reaching its end.
 */
void
vte_terminal_search_set_wrap_around (VteTerminal *terminal,
				     gboolean     wrap_around) noexcept
try
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        IMPL(terminal)->search_set_wrap_around(wrap_around != FALSE);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_search_get_wrap_around:
 * @terminal: a #VteTerminal
 *
 * Returns: whether searching will wrap around
 */
gboolean
vte_terminal_search_get_wrap_around (VteTerminal *terminal) noexcept
try
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), false);
	return IMPL(terminal)->m_search_wrap_around;
}
catch (...)
{
        vte::log_exception();
        return false;
}

/**
 * vte_terminal_select_all:
 * @terminal: a #VteTerminal
 *
 * Selects all text within the terminal (not including the scrollback buffer).
 */
void
vte_terminal_select_all (VteTerminal *terminal) noexcept
try
{
	g_return_if_fail (VTE_IS_TERMINAL (terminal));

        IMPL(terminal)->select_all();
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_unselect_all:
 * @terminal: a #VteTerminal
 *
 * Clears the current selection.
 */
void
vte_terminal_unselect_all(VteTerminal *terminal) noexcept
try
{
	g_return_if_fail (VTE_IS_TERMINAL (terminal));

        IMPL(terminal)->deselect_all();
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_get_cursor_position:
 * @terminal: a #VteTerminal
 * @column: (out) (allow-none): a location to store the column, or %NULL
 * @row: (out) (allow-none): a location to store the row, or %NULL
 *
 * Reads the location of the insertion cursor and returns it.  The row
 * coordinate is absolute.
 *
 * This method is unaware of BiDi. The returned column is logical column.
 */
void
vte_terminal_get_cursor_position(VteTerminal *terminal,
				 long *column,
                                 long *row) noexcept
try
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        auto impl = IMPL(terminal);
	if (column) {
                *column = impl->m_screen->cursor.col;
	}
	if (row) {
                *row = impl->m_screen->cursor.row;
	}
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_pty_new_sync:
 * @terminal: a #VteTerminal
 * @flags: flags from #VtePtyFlags
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @error: (allow-none): return location for a #GError, or %NULL
 *
 * Creates a new #VtePty, sets the emulation property
 * from #VteTerminal:emulation, and sets the size using
 * @terminal's size.
 *
 * See vte_pty_new() for more information.
 *
 * Returns: (transfer full): a new #VtePty
 */
VtePty *
vte_terminal_pty_new_sync(VteTerminal *terminal,
                          VtePtyFlags flags,
                          GCancellable *cancellable,
                          GError **error) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);

        auto pty = vte::glib::take_ref(vte_pty_new_sync(flags, cancellable, error));
        if (!pty)
                return nullptr;

        auto impl = IMPL(terminal);
        _vte_pty_set_size(pty.get(),
                          impl->m_row_count,
                          impl->m_column_count,
                          impl->m_cell_height_unscaled,
                          impl->m_cell_width_unscaled,
                          nullptr);

        return pty.release();
}
catch (...)
{
        vte::glib::set_error_from_exception(error);
        return nullptr;
}

/**
 * vte_terminal_watch_child:
 * @terminal: a #VteTerminal
 * @child_pid: a #GPid
 *
 * Watches @child_pid. When the process exists, the #VteTerminal::child-exited
 * signal will be called with the child's exit status.
 *
 * Prior to calling this function, a #VtePty must have been set in @terminal
 * using vte_terminal_set_pty().
 * When the child exits, the terminal's #VtePty will be set to %NULL.
 *
 * Note: g_child_watch_add() or g_child_watch_add_full() must not have
 * been called for @child_pid, nor a #GSource for it been created with
 * g_child_watch_source_new().
 *
 * Note: when using the g_spawn_async() family of functions,
 * the %G_SPAWN_DO_NOT_REAP_CHILD flag MUST have been passed.
 */
void
vte_terminal_watch_child (VteTerminal *terminal,
                          GPid child_pid) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(child_pid != -1);

        g_return_if_fail(WIDGET(terminal)->pty() != nullptr);

        IMPL(terminal)->watch_child(child_pid);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_spawn_sync:
 * @terminal: a #VteTerminal
 * @pty_flags: flags from #VtePtyFlags
 * @working_directory: (allow-none): the name of a directory the command should start
 *   in, or %NULL to use the current working directory
 * @argv: (array zero-terminated=1) (element-type filename): child's argument vector
 * @envv: (allow-none) (array zero-terminated=1) (element-type filename): a list of environment
 *   variables to be added to the environment before starting the process, or %NULL
 * @spawn_flags: flags from #GSpawnFlags
 * @child_setup: (allow-none) (scope call): an extra child setup function to run in the child just before exec(), or %NULL
 * @child_setup_data: user data for @child_setup
 * @child_pid: (out) (allow-none) (transfer full): a location to store the child PID, or %NULL
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @error: (allow-none): return location for a #GError, or %NULL
 *
 * Starts the specified command under a newly-allocated controlling
 * pseudo-terminal.  The @argv and @envv lists should be %NULL-terminated.
 * The "TERM" environment variable is automatically set to a default value,
 * but can be overridden from @envv.
 * @pty_flags controls logging the session to the specified system log files.
 *
 * Note that %G_SPAWN_DO_NOT_REAP_CHILD will always be added to @spawn_flags.
 *
 * Note also that %G_SPAWN_STDOUT_TO_DEV_NULL, %G_SPAWN_STDERR_TO_DEV_NULL,
 * and %G_SPAWN_CHILD_INHERITS_STDIN are not supported in @spawn_flags, since
 * stdin, stdout and stderr of the child process will always be connected to
 * the PTY.
 *
 * Note that all open file descriptors will be closed in the child. If you want
 * to keep some file descriptor open for use in the child process, you need to
 * use a child setup function that unsets the FD_CLOEXEC flag on that file
 * descriptor.
 *
 * See vte_pty_new(), g_spawn_async() and vte_terminal_watch_child() for more information.
 *
 * Beginning with 0.52, sets PWD to @working_directory in order to preserve symlink components.
 * The caller should also make sure that symlinks were preserved while constructing the value of @working_directory,
 * e.g. by using vte_terminal_get_current_directory_uri(), g_get_current_dir() or get_current_dir_name().
 *
 * Returns: %TRUE on success, or %FALSE on error with @error filled in
 *
 * Deprecated: 0.48: Use vte_terminal_spawn_async() instead.
 */
gboolean
vte_terminal_spawn_sync(VteTerminal *terminal,
                        VtePtyFlags pty_flags,
                        const char *working_directory,
                        char **argv,
                        char **envv,
                        GSpawnFlags spawn_flags,
                        GSpawnChildSetupFunc child_setup,
                        gpointer child_setup_data,
                        GPid *child_pid /* out */,
                        GCancellable *cancellable,
                        GError **error) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
        g_return_val_if_fail(argv != NULL, FALSE);
        g_return_val_if_fail(argv[0] != nullptr, FALSE);
        g_return_val_if_fail(envv == nullptr ||_vte_pty_check_envv(envv), false);
        g_return_val_if_fail((spawn_flags & (VTE_SPAWN_NO_SYSTEMD_SCOPE | VTE_SPAWN_REQUIRE_SYSTEMD_SCOPE)) == 0, FALSE);
        g_return_val_if_fail(child_setup_data == NULL || child_setup, FALSE);
        g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

        auto new_pty = vte::glib::take_ref(vte_terminal_pty_new_sync(terminal, pty_flags, cancellable, error));
        if (!new_pty)
                return false;

        GPid pid;
        if (!_vte_pty_spawn_sync(new_pty.get(),
                                 working_directory,
                                 argv,
                                 envv,
                                 spawn_flags,
                                 child_setup, child_setup_data, nullptr,
                                 &pid,
                                 -1 /* default timeout */,
                                 cancellable,
                                 error))
                return false;

        vte_terminal_set_pty(terminal, new_pty.get());
        vte_terminal_watch_child(terminal, pid);

        if (child_pid)
                *child_pid = pid;

        return true;
}
catch (...)
{
        return vte::glib::set_error_from_exception(error);
}

typedef struct {
        GWeakRef wref;
        VteTerminalSpawnAsyncCallback callback;
        gpointer user_data;
} SpawnAsyncCallbackData;

static gpointer
spawn_async_callback_data_new(VteTerminal *terminal,
                              VteTerminalSpawnAsyncCallback callback,
                              gpointer user_data) noexcept
{
        SpawnAsyncCallbackData *data = g_new0 (SpawnAsyncCallbackData, 1);

        g_weak_ref_init(&data->wref, terminal);
        data->callback = callback;
        data->user_data = user_data;

        return data;
}

static void
spawn_async_callback_data_free(SpawnAsyncCallbackData* data) noexcept
{
        g_weak_ref_clear(&data->wref);
        g_free(data);
}

static void
spawn_async_cb(GObject *source,
               GAsyncResult *result,
               gpointer user_data) noexcept
{
        SpawnAsyncCallbackData *data = reinterpret_cast<SpawnAsyncCallbackData*>(user_data);
        VtePty *pty = VTE_PTY(source);

        auto pid = pid_t{-1};
        auto error = vte::glib::Error{};
        if (source) {
                vte_pty_spawn_finish(pty, result, &pid, error);
        } else {
                (void)g_task_propagate_int(G_TASK(result), error);
                assert(error.error());
        }

        /* Now get a ref to the terminal */
        auto terminal = vte::glib::acquire_ref<VteTerminal>(&data->wref);

        if (terminal) {
                if (pid != -1) {
                        vte_terminal_set_pty(terminal.get(), pty);
                        vte_terminal_watch_child(terminal.get(), pid);
                } else {
                        vte_terminal_set_pty(terminal.get(), nullptr);
                }
        }

        if (data->callback) {
                try {
                        data->callback(terminal.get(), pid, error, data->user_data);
                } catch (...) {
                        vte::log_exception();
                }
        }

        if (!terminal) {
                /* If the terminal was destroyed, we need to abort the child process, if any */
                if (pid != -1) {
                        pid_t pgrp;
                        pgrp = getpgid(pid);
                        if (pgrp != -1 && pgrp != getpgid(getpid())) {
                                kill(-pgrp, SIGHUP);
                        }

                        kill(pid, SIGHUP);
                }
        }

        spawn_async_callback_data_free(data);
}

/**
 * VteTerminalSpawnAsyncCallback:
 * @terminal: the #VteTerminal
 * @pid: a #GPid
 * @error: a #GError, or %NULL
 * @user_data: user data that was passed to vte_terminal_spawn_async
 *
 * Callback for vte_terminal_spawn_async().
 *
 * On success, @pid contains the PID of the spawned process, and @error
 * is %NULL.
 * On failure, @pid is -1 and @error contains the error information.
 *
 * Since: 0.48
 */

/**
 * vte_terminal_spawn_with_fds_async:
 * @terminal: a #VteTerminal
 * @pty_flags: flags from #VtePtyFlags
 * @working_directory: (allow-none): the name of a directory the command should start
 *   in, or %NULL to use the current working directory
 * @argv: (array zero-terminated=1) (element-type filename): child's argument vector
 * @envv: (allow-none) (array zero-terminated=1) (element-type filename): a list of environment
 *   variables to be added to the environment before starting the process, or %NULL
 * @fds: (nullable) (array length=n_fds) (transfer none) (scope call): an array of file descriptors, or %NULL
 * @n_fds: the number of file descriptors in @fds, or 0 if @fds is %NULL
 * @map_fds: (nullable) (array length=n_map_fds) (transfer none) (scope call): an array of integers, or %NULL
 * @n_map_fds: the number of elements in @map_fds, or 0 if @map_fds is %NULL
 * @spawn_flags: flags from #GSpawnFlags
 * @child_setup: (allow-none) (scope async): an extra child setup function to run in the child just before exec(), or %NULL
 * @child_setup_data: (nullable) (closure child_setup): user data for @child_setup, or %NULL
 * @child_setup_data_destroy: (nullable) (destroy child_setup_data): a #GDestroyNotify for @child_setup_data, or %NULL
 * @timeout: a timeout value in ms, -1 for the default timeout, or G_MAXINT to wait indefinitely
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @callback: (nullable) (scope async): a #VteTerminalSpawnAsyncCallback, or %NULL
 * @user_data: (nullable) (closure callback): user data for @callback, or %NULL
 *
 * A convenience function that wraps creating the #VtePty and spawning
 * the child process on it. See vte_pty_new_sync(), vte_pty_spawn_with_fds_async(),
 * and vte_pty_spawn_finish() for more information.
 *
 * When the operation is finished successfully, @callback will be called
 * with the child #GPid, and a %NULL #GError. The child PID will already be
 * watched via vte_terminal_watch_child().
 *
 * When the operation fails, @callback will be called with a -1 #GPid,
 * and a non-%NULL #GError containing the error information.
 *
 * Note that %G_SPAWN_STDOUT_TO_DEV_NULL, %G_SPAWN_STDERR_TO_DEV_NULL,
 * and %G_SPAWN_CHILD_INHERITS_STDIN are not supported in @spawn_flags, since
 * stdin, stdout and stderr of the child process will always be connected to
 * the PTY.
 *
 * If @fds is not %NULL, the child process will map the file descriptors from
 * @fds according to @map_fds; @n_map_fds must be less or equal to @n_fds.
 * This function will take ownership of the file descriptors in @fds;
 * you must not use or close them after this call.
 *
 * Note that all  open file descriptors apart from those mapped as above
 * will be closed in the child. (If you want to keep some other file descriptor
 * open for use in the child process, you need to use a child setup function
 * that unsets the FD_CLOEXEC flag on that file descriptor manually.)
 *
 * Beginning with 0.60, and on linux only, and unless %VTE_SPAWN_NO_SYSTEMD_SCOPE is
 * passed in @spawn_flags, the newly created child process will be moved to its own
 * systemd user scope; and if %VTE_SPAWN_REQUIRE_SYSTEMD_SCOPE is passed, and creation
 * of the systemd user scope fails, the whole spawn will fail.
 * You can override the options used for the systemd user scope by
 * providing a systemd override file for 'vte-spawn-.scope' unit. See man:systemd.unit(5)
 * for further information.
 *
 * Note that if @terminal has been destroyed before the operation is called,
 * @callback will be called with a %NULL @terminal; you must not do anything
 * in the callback besides freeing any resources associated with @user_data,
 * but taking care not to access the now-destroyed #VteTerminal. Note that
 * in this case, if spawning was successful, the child process will be aborted
 * automatically.
 *
 * Beginning with 0.52, sets PWD to @working_directory in order to preserve symlink components.
 * The caller should also make sure that symlinks were preserved while constructing the value of @working_directory,
 * e.g. by using vte_terminal_get_current_directory_uri(), g_get_current_dir() or get_current_dir_name().
 *
 * Since: 0.62
 */
void
vte_terminal_spawn_with_fds_async(VteTerminal *terminal,
                                  VtePtyFlags pty_flags,
                                  const char *working_directory,
                                  char const* const* argv,
                                  char const* const* envv,
                                  int const* fds,
                                  int n_fds,
                                  int const* fd_map_to,
                                  int n_fd_map_to,
                                  GSpawnFlags spawn_flags,
                                  GSpawnChildSetupFunc child_setup,
                                  gpointer child_setup_data,
                                  GDestroyNotify child_setup_data_destroy,
                                  int timeout,
                                  GCancellable *cancellable,
                                  VteTerminalSpawnAsyncCallback callback,
                                  gpointer user_data) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(cancellable == nullptr || G_IS_CANCELLABLE (cancellable));

        auto error = vte::glib::Error{};
        auto pty = vte::glib::take_ref(vte_terminal_pty_new_sync(terminal, pty_flags, cancellable, error));
        if (!pty) {
                auto task = vte::glib::take_ref(g_task_new(nullptr,
                                                           cancellable,
                                                           spawn_async_cb,
                                                           spawn_async_callback_data_new(terminal, callback, user_data)));
                g_task_return_error(task.get(), error.release());
                return;
        }

        vte_pty_spawn_with_fds_async(pty.get(),
                                     working_directory,
                                     argv,
                                     envv,
                                     fds, n_fds, fd_map_to, n_fd_map_to,
                                     spawn_flags,
                                     child_setup, child_setup_data, child_setup_data_destroy,
                                     timeout, cancellable,
                                     spawn_async_cb,
                                     spawn_async_callback_data_new(terminal, callback, user_data));
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_spawn_async:
 * @terminal: a #VteTerminal
 * @pty_flags: flags from #VtePtyFlags
 * @working_directory: (allow-none): the name of a directory the command should start
 *   in, or %NULL to use the current working directory
 * @argv: (array zero-terminated=1) (element-type filename): child's argument vector
 * @envv: (allow-none) (array zero-terminated=1) (element-type filename): a list of environment
 *   variables to be added to the environment before starting the process, or %NULL
 * @spawn_flags: flags from #GSpawnFlags
 * @child_setup: (allow-none) (scope async): an extra child setup function to run in the child just before exec(), or %NULL
 * @child_setup_data: (nullable) (closure child_setup): user data for @child_setup, or %NULL
 * @child_setup_data_destroy: (nullable) (destroy child_setup_data): a #GDestroyNotify for @child_setup_data, or %NULL
 * @timeout: a timeout value in ms, -1 for the default timeout, or G_MAXINT to wait indefinitely
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @callback: (nullable) (scope async): a #VteTerminalSpawnAsyncCallback, or %NULL
 * @user_data: (nullable) (closure callback): user data for @callback, or %NULL
 *
 * A convenience function that wraps creating the #VtePty and spawning
 * the child process on it. Like vte_terminal_spawn_with_fds_async(),
 * except that this function does not allow passing file descriptors to
 * the child process. See vte_terminal_spawn_with_fds_async() for more
 * information.
 *
 * Since: 0.48
 */
void
vte_terminal_spawn_async(VteTerminal *terminal,
                         VtePtyFlags pty_flags,
                         const char *working_directory,
                         char **argv,
                         char **envv,
                         GSpawnFlags spawn_flags,
                         GSpawnChildSetupFunc child_setup,
                         gpointer child_setup_data,
                         GDestroyNotify child_setup_data_destroy,
                         int timeout,
                         GCancellable *cancellable,
                         VteTerminalSpawnAsyncCallback callback,
                         gpointer user_data) noexcept
{
        vte_terminal_spawn_with_fds_async(terminal, pty_flags, working_directory, argv, envv,
                                          nullptr, 0, nullptr, 0,
                                          spawn_flags,
                                          child_setup, child_setup_data, child_setup_data_destroy,
                                          timeout, cancellable,
                                          callback, user_data);
}

/**
 * vte_terminal_feed:
 * @terminal: a #VteTerminal
 * @data: (array length=length) (element-type guint8) (allow-none): a string in the terminal's current encoding
 * @length: the length of the string, or -1 to use the full length or a nul-terminated string
 *
 * Interprets @data as if it were data received from a child process.
 */
void
vte_terminal_feed(VteTerminal *terminal,
                  const char *data,
                  gssize length) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(length == 0 || data != NULL);

        if (length == 0)
                return;

        auto const len = size_t{length == -1 ? strlen(data) : size_t(length)};
        WIDGET(terminal)->feed({data, len});
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_feed_child:
 * @terminal: a #VteTerminal
 * @text: (array length=length) (element-type guint8) (allow-none): data to send to the child
 * @length: length of @text in bytes, or -1 if @text is NUL-terminated
 *
 * Sends a block of UTF-8 text to the child as if it were entered by the user
 * at the keyboard.
 */
void
vte_terminal_feed_child(VteTerminal *terminal,
                        const char *text,
                        gssize length) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(length == 0 || text != NULL);

        if (length == 0)
                return;

        auto const len = size_t{length == -1 ? strlen(text) : size_t(length)};
        WIDGET(terminal)->feed_child({text, len});
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_feed_child_binary:
 * @terminal: a #VteTerminal
 * @data: (array length=length) (element-type guint8) (allow-none): data to send to the child
 * @length: length of @data
 *
 * Sends a block of binary data to the child.
 *
 * Deprecated: 0.60: Don't send binary data. Use vte_terminal_feed_child() instead to send
 *   UTF-8 text
 */
void
vte_terminal_feed_child_binary(VteTerminal *terminal,
                               const guint8 *data,
                               gsize length) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(length == 0 || data != NULL);

        if (length == 0)
                return;

        WIDGET(terminal)->feed_child_binary({(char*)data, length});
}
catch (...)
{
        vte::log_exception();
}

/**
 * VteSelectionFunc:
 * @terminal: terminal in which the cell is.
 * @column: column in which the cell is.
 * @row: row in which the cell is.
 * @data: (closure): user data.
 *
 * Specifies the type of a selection function used to check whether
 * a cell has to be selected or not.
 *
 * Returns: %TRUE if cell has to be selected; %FALSE if otherwise.
 */

static void
warn_if_callback(VteSelectionFunc func,
                 char const* caller = __builtin_FUNCTION()) noexcept
{
        if (!func)
                return;

#ifndef VTE_DEBUG
        static gboolean warned = FALSE;
        if (warned)
                return;
        warned = TRUE;
#endif
        g_warning ("%s: VteSelectionFunc callback ignored.\n", caller);
}

/**
 * VteCharAttributes:
 *
 * Deprecated: 0.68
 */

static void
warn_if_attributes(void* array,
                   char const* caller = __builtin_FUNCTION()) noexcept
{
        if (!array)
                return;

#ifndef VTE_DEBUG
        static gboolean warned = FALSE;
        if (warned)
                return;
        warned = TRUE;
#endif
        g_warning ("%s: Passing a GArray to retrieve attributes is deprecated. In a future version, passing non-NULL as attributes array will make the function return NULL.\n", caller);
}

/**
 * vte_terminal_get_text:
 * @terminal: a #VteTerminal
 * @is_selected: (scope call) (allow-none): a #VteSelectionFunc callback
 * @user_data: (closure): user data to be passed to the callback
 * @attributes: (nullable) (out caller-allocates) (transfer full) (array) (element-type Vte.CharAttributes): location for storing text attributes. Deprecated: 0.68: Always pass %NULL here.
 *
 * Extracts a view of the visible part of the terminal.  If @is_selected is not
 * %NULL, characters will only be read if @is_selected returns %TRUE after being
 * passed the column and row, respectively.  A #VteCharAttributes structure
 * is added to @attributes for each byte added to the returned string detailing
 * the character's position, colors, and other characteristics.
 *
 * This method is unaware of BiDi. The columns returned in @attributes are
 * logical columns.
 *
 * Note: since 0.68, passing a non-%NULL @array parameter is deprecated. Starting with
 * 0.72, passing a non-%NULL @array parameter will make this function itself return %NULL.
 *
 * Returns: (transfer full) (nullable): a newly allocated text string, or %NULL.
 */
char *
vte_terminal_get_text(VteTerminal *terminal,
		      VteSelectionFunc is_selected,
		      gpointer user_data,
		      GArray *attributes) noexcept
try
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);
        warn_if_callback(is_selected);
        warn_if_attributes(attributes);
        auto text = IMPL(terminal)->get_text_displayed(true /* wrap */,
                                                       attributes);
        if (text == nullptr)
                return nullptr;
        return (char*)g_string_free(text, FALSE);
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

/**
 * vte_terminal_get_text_include_trailing_spaces:
 * @terminal: a #VteTerminal
 * @is_selected: (scope call) (allow-none): a #VteSelectionFunc callback
 * @user_data: (closure): user data to be passed to the callback
 * @attributes: (out caller-allocates) (transfer full) (array) (element-type Vte.CharAttributes): location for storing text attributes. Deprecated: 0.68: Always pass %NULL here.
 *
 * Extracts a view of the visible part of the terminal.  If @is_selected is not
 * %NULL, characters will only be read if @is_selected returns %TRUE after being
 * passed the column and row, respectively.  A #VteCharAttributes structure
 * is added to @attributes for each byte added to the returned string detailing
 * the character's position, colors, and other characteristics.
 *
 * This method is unaware of BiDi. The columns returned in @attributes are
 * logical columns.
 *
 * Note: since 0.68, passing a non-%NULL @array parameter is deprecated. Starting with
 * 0.72, passing a non-%NULL @array parameter will make this function itself return %NULL.
 *
 * Returns: (transfer full): a newly allocated text string, or %NULL.
 *
 * Deprecated: 0.56: Use vte_terminal_get_text() instead.
 */
char *
vte_terminal_get_text_include_trailing_spaces(VteTerminal *terminal,
					      VteSelectionFunc is_selected,
					      gpointer user_data,
					      GArray *attributes) noexcept
{
        return vte_terminal_get_text(terminal, is_selected, user_data, attributes);
}

/**
 * vte_terminal_get_text_range:
 * @terminal: a #VteTerminal
 * @start_row: first row to search for data
 * @start_col: first column to search for data
 * @end_row: last row to search for data
 * @end_col: last column to search for data
 * @is_selected: (scope call) (allow-none): a #VteSelectionFunc callback
 * @user_data: (closure): user data to be passed to the callback
 * @attributes: (nullable) (out caller-allocates) (transfer full) (array) (element-type Vte.CharAttributes): location for storing text attributes. Deprecated: 0.68: Always pass %NULL here.
 *
 * Extracts a view of the visible part of the terminal.  If @is_selected is not
 * %NULL, characters will only be read if @is_selected returns %TRUE after being
 * passed the column and row, respectively.  A #VteCharAttributes structure
 * is added to @attributes for each byte added to the returned string detailing
 * the character's position, colors, and other characteristics.  The
 * entire scrollback buffer is scanned, so it is possible to read the entire
 * contents of the buffer using this function.
 *
 * This method is unaware of BiDi. The columns passed in @start_col and @end_row,
 * and returned in @attributes are logical columns.
 *
 * Note: since 0.68, passing a non-%NULL @array parameter is deprecated. Starting with
 * 0.72, passing a non-%NULL @array parameter will make this function itself return %NULL.
 *
 * Returns: (transfer full) (nullable): a newly allocated text string, or %NULL.
 */
char *
vte_terminal_get_text_range(VteTerminal *terminal,
			    long start_row,
                            long start_col,
			    long end_row,
                            long end_col,
			    VteSelectionFunc is_selected,
			    gpointer user_data,
			    GArray *attributes) noexcept
try
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);
        warn_if_callback(is_selected);
        warn_if_attributes(attributes);
        auto text = IMPL(terminal)->get_text(start_row, start_col,
                                             end_row, end_col,
                                             false /* block */,
                                             true /* wrap */,
                                             attributes);
        if (text == nullptr)
                return nullptr;
        return (char*)g_string_free(text, FALSE);
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

/**
 * vte_terminal_reset:
 * @terminal: a #VteTerminal
 * @clear_tabstops: whether to reset tabstops
 * @clear_history: whether to empty the terminal's scrollback buffer
 *
 * Resets as much of the terminal's internal state as possible, discarding any
 * unprocessed input data, resetting character attributes, cursor state,
 * national character set state, status line, terminal modes (insert/delete),
 * selection state, and encoding.
 *
 */
void
vte_terminal_reset(VteTerminal *terminal,
                   gboolean clear_tabstops,
                   gboolean clear_history) noexcept
try
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
        IMPL(terminal)->reset(clear_tabstops, clear_history, true);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_set_size:
 * @terminal: a #VteTerminal
 * @columns: the desired number of columns
 * @rows: the desired number of rows
 *
 * Attempts to change the terminal's size in terms of rows and columns.  If
 * the attempt succeeds, the widget will resize itself to the proper size.
 */
void
vte_terminal_set_size(VteTerminal *terminal,
                      long columns,
                      long rows) noexcept
try
{
        g_return_if_fail(columns >= 1);
        g_return_if_fail(rows >= 1);

        IMPL(terminal)->set_size(columns, rows, false);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_get_text_blink_mode:
 * @terminal: a #VteTerminal
 *
 * Checks whether or not the terminal will allow blinking text.
 *
 * Returns: the blinking setting
 *
 * Since: 0.52
 */
VteTextBlinkMode
vte_terminal_get_text_blink_mode(VteTerminal *terminal) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), VTE_TEXT_BLINK_ALWAYS);
        return WIDGET(terminal)->text_blink_mode();
}
catch (...)
{
        vte::log_exception();
        return VTE_TEXT_BLINK_ALWAYS;
}

/**
 * vte_terminal_set_text_blink_mode:
 * @terminal: a #VteTerminal
 * @text_blink_mode: the #VteTextBlinkMode to use
 *
 * Controls whether or not the terminal will allow blinking text.
 *
 * Since: 0.52
 */
void
vte_terminal_set_text_blink_mode(VteTerminal *terminal,
                                 VteTextBlinkMode text_blink_mode) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));

        if (WIDGET(terminal)->set_text_blink_mode(text_blink_mode))
                g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_TEXT_BLINK_MODE]);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_get_allow_bold:
 * @terminal: a #VteTerminal
 *
 * Checks whether or not the terminal will attempt to draw bold text,
 * by using a bold font variant.
 *
 * Returns: %TRUE if bolding is enabled, %FALSE if not
 *
 * Deprecated: 0.60: There's probably no reason for this feature to exist.
 */
gboolean
vte_terminal_get_allow_bold(VteTerminal *terminal) noexcept
try
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), false);
	return IMPL(terminal)->m_allow_bold;
}
catch (...)
{
        vte::log_exception();
        return false;
}

/**
 * vte_terminal_set_allow_bold:
 * @terminal: a #VteTerminal
 * @allow_bold: %TRUE if the terminal should attempt to draw bold text
 *
 * Controls whether or not the terminal will attempt to draw bold text,
 * by using a bold font variant.
 *
 * Deprecated: 0.60: There's probably no reason for this feature to exist.
 */
void
vte_terminal_set_allow_bold(VteTerminal *terminal,
                            gboolean allow_bold) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));

        if (IMPL(terminal)->set_allow_bold(allow_bold != FALSE))
                g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_ALLOW_BOLD]);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_get_allow_hyperlink:
 * @terminal: a #VteTerminal
 *
 * Checks whether or not hyperlinks (OSC 8 escape sequence) are allowed.
 *
 * Returns: %TRUE if hyperlinks are enabled, %FALSE if not
 *
 * Since: 0.50
 */
gboolean
vte_terminal_get_allow_hyperlink(VteTerminal *terminal) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
        return IMPL(terminal)->m_allow_hyperlink;
}
catch (...)
{
        vte::log_exception();
        return false;
}

/**
 * vte_terminal_set_allow_hyperlink:
 * @terminal: a #VteTerminal
 * @allow_hyperlink: %TRUE if the terminal should allow hyperlinks
 *
 * Controls whether or not hyperlinks (OSC 8 escape sequence) are allowed.
 *
 * Since: 0.50
 */
void
vte_terminal_set_allow_hyperlink(VteTerminal *terminal,
                                 gboolean allow_hyperlink) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));

        if (IMPL(terminal)->set_allow_hyperlink(allow_hyperlink != FALSE))
                g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_ALLOW_HYPERLINK]);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_get_audible_bell:
 * @terminal: a #VteTerminal
 *
 * Checks whether or not the terminal will beep when the child outputs the
 * "bl" sequence.
 *
 * Returns: %TRUE if audible bell is enabled, %FALSE if not
 */
gboolean
vte_terminal_get_audible_bell(VteTerminal *terminal) noexcept
try
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), false);
	return IMPL(terminal)->m_audible_bell;
}
catch (...)
{
        vte::log_exception();
        return false;
}

/**
 * vte_terminal_set_audible_bell:
 * @terminal: a #VteTerminal
 * @is_audible: %TRUE if the terminal should beep
 *
 * Controls whether or not the terminal will beep when the child outputs the
 * "bl" sequence.
 */
void
vte_terminal_set_audible_bell(VteTerminal *terminal,
                              gboolean is_audible) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));

        if (IMPL(terminal)->set_audible_bell(is_audible != FALSE))
                g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_AUDIBLE_BELL]);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_set_backspace_binding:
 * @terminal: a #VteTerminal
 * @binding: a #VteEraseBinding for the backspace key
 *
 * Modifies the terminal's backspace key binding, which controls what
 * string or control sequence the terminal sends to its child when the user
 * presses the backspace key.
 */
void
vte_terminal_set_backspace_binding(VteTerminal *terminal,
                                   VteEraseBinding binding) noexcept
try
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(binding >= VTE_ERASE_AUTO && binding <= VTE_ERASE_TTY);

        if (WIDGET(terminal)->set_backspace_binding(binding))
                g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_BACKSPACE_BINDING]);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_get_bold_is_bright:
 * @terminal: a #VteTerminal
 *
 * Checks whether the SGR 1 attribute also switches to the bright counterpart
 * of the first 8 palette colors, in addition to making them bold (legacy behavior)
 * or if SGR 1 only enables bold and leaves the color intact.
 *
 * Returns: %TRUE if bold also enables bright, %FALSE if not
 *
 * Since: 0.52
 */
gboolean
vte_terminal_get_bold_is_bright(VteTerminal *terminal) noexcept
try
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), false);
	return IMPL(terminal)->m_bold_is_bright;
}
catch (...)
{
        vte::log_exception();
        return false;
}

/**
 * vte_terminal_set_bold_is_bright:
 * @terminal: a #VteTerminal
 * @bold_is_bright: %TRUE if bold should also enable bright
 *
 * Sets whether the SGR 1 attribute also switches to the bright counterpart
 * of the first 8 palette colors, in addition to making them bold (legacy behavior)
 * or if SGR 1 only enables bold and leaves the color intact.
 *
 * Since: 0.52
 */
void
vte_terminal_set_bold_is_bright(VteTerminal *terminal,
                                gboolean bold_is_bright) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));

        if (IMPL(terminal)->set_bold_is_bright(bold_is_bright != FALSE))
                g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_BOLD_IS_BRIGHT]);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_get_char_height:
 * @terminal: a #VteTerminal
 *
 * Returns: the height of a character cell
 *
 * Note that this method should rather be called vte_terminal_get_cell_height,
 * because the return value takes cell-height-scale into account.
 */
glong
vte_terminal_get_char_height(VteTerminal *terminal) noexcept
try
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), -1);
	return IMPL(terminal)->get_cell_height();
}
catch (...)
{
        vte::log_exception();
        return -1;
}

/**
 * vte_terminal_get_char_width:
 * @terminal: a #VteTerminal
 *
 * Returns: the width of a character cell
 *
 * Note that this method should rather be called vte_terminal_get_cell_width,
 * because the return value takes cell-width-scale into account.
 */
glong
vte_terminal_get_char_width(VteTerminal *terminal) noexcept
try
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), -1);
	return IMPL(terminal)->get_cell_width();
}
catch (...)
{
        vte::log_exception();
        return -1;
}

/**
 * vte_terminal_get_cjk_ambiguous_width:
 * @terminal: a #VteTerminal
 *
 *  Returns whether ambiguous-width characters are narrow or wide.
 * (Note that when using a non-UTF-8 encoding set via vte_terminal_set_encoding(),
 * the width of ambiguous-width characters is fixed and determined by the encoding
 * itself.)
 *
 * Returns: 1 if ambiguous-width characters are narrow, or 2 if they are wide
 */
int
vte_terminal_get_cjk_ambiguous_width(VteTerminal *terminal) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), 1);
        return IMPL(terminal)->m_utf8_ambiguous_width;
}
catch (...)
{
        vte::log_exception();
        return 1;
}

/**
 * vte_terminal_set_cjk_ambiguous_width:
 * @terminal: a #VteTerminal
 * @width: either 1 (narrow) or 2 (wide)
 *
 * This setting controls whether ambiguous-width characters are narrow or wide.
 * (Note that when using a non-UTF-8 encoding set via vte_terminal_set_encoding(),
 * the width of ambiguous-width characters is fixed and determined by the encoding
 * itself.)
 */
void
vte_terminal_set_cjk_ambiguous_width(VteTerminal *terminal, int width) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(width == 1 || width == 2);

        if (IMPL(terminal)->set_cjk_ambiguous_width(width))
                g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_CJK_AMBIGUOUS_WIDTH]);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_set_color_background:
 * @terminal: a #VteTerminal
 * @background: the new background color
 *
 * Sets the background color for text which does not have a specific background
 * color assigned.  Only has effect when no background image is set and when
 * the terminal is not transparent.
 */
void
vte_terminal_set_color_background(VteTerminal *terminal,
                                  const GdkRGBA *background) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(background != NULL);
        g_return_if_fail(valid_color(background));

        auto impl = IMPL(terminal);
        impl->set_color_background(vte::color::rgb(background));
        impl->set_background_alpha(background->alpha);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_set_color_bold:
 * @terminal: a #VteTerminal
 * @bold: (allow-none): the new bold color or %NULL
 *
 * Sets the color used to draw bold text in the default foreground color.
 * If @bold is %NULL then the default color is used.
 */
void
vte_terminal_set_color_bold(VteTerminal *terminal,
                            const GdkRGBA *bold) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(bold == nullptr || valid_color(bold));

        auto impl = IMPL(terminal);
        if (bold)
                impl->set_color_bold(vte::color::rgb(bold));
        else
                impl->reset_color_bold();
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_set_color_cursor:
 * @terminal: a #VteTerminal
 * @cursor_background: (allow-none): the new color to use for the text cursor, or %NULL
 *
 * Sets the background color for text which is under the cursor.  If %NULL, text
 * under the cursor will be drawn with foreground and background colors
 * reversed.
 */
void
vte_terminal_set_color_cursor(VteTerminal *terminal,
                              const GdkRGBA *cursor_background) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(cursor_background == nullptr || valid_color(cursor_background));

        auto impl = IMPL(terminal);
        if (cursor_background)
                impl->set_color_cursor_background(vte::color::rgb(cursor_background));
        else
                impl->reset_color_cursor_background();
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_set_color_cursor_foreground:
 * @terminal: a #VteTerminal
 * @cursor_foreground: (allow-none): the new color to use for the text cursor, or %NULL
 *
 * Sets the foreground color for text which is under the cursor.  If %NULL, text
 * under the cursor will be drawn with foreground and background colors
 * reversed.
 *
 * Since: 0.44
 */
void
vte_terminal_set_color_cursor_foreground(VteTerminal *terminal,
                                         const GdkRGBA *cursor_foreground) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(cursor_foreground == nullptr || valid_color(cursor_foreground));

        auto impl = IMPL(terminal);
        if (cursor_foreground)
                impl->set_color_cursor_foreground(vte::color::rgb(cursor_foreground));
        else
                impl->reset_color_cursor_foreground();
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_set_color_foreground:
 * @terminal: a #VteTerminal
 * @foreground: the new foreground color
 *
 * Sets the foreground color used to draw normal text.
 */
void
vte_terminal_set_color_foreground(VteTerminal *terminal,
                                  const GdkRGBA *foreground) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(foreground != nullptr);
        g_return_if_fail(valid_color(foreground));

        IMPL(terminal)->set_color_foreground(vte::color::rgb(foreground));
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_set_color_highlight:
 * @terminal: a #VteTerminal
 * @highlight_background: (allow-none): the new color to use for highlighted text, or %NULL
 *
 * Sets the background color for text which is highlighted.  If %NULL,
 * it is unset.  If neither highlight background nor highlight foreground are set,
 * highlighted text (which is usually highlighted because it is selected) will
 * be drawn with foreground and background colors reversed.
 */
void
vte_terminal_set_color_highlight(VteTerminal *terminal,
                                 const GdkRGBA *highlight_background) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(highlight_background == nullptr || valid_color(highlight_background));

        auto impl = IMPL(terminal);
        if (highlight_background)
                impl->set_color_highlight_background(vte::color::rgb(highlight_background));
        else
                impl->reset_color_highlight_background();
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_set_color_highlight_foreground:
 * @terminal: a #VteTerminal
 * @highlight_foreground: (allow-none): the new color to use for highlighted text, or %NULL
 *
 * Sets the foreground color for text which is highlighted.  If %NULL,
 * it is unset.  If neither highlight background nor highlight foreground are set,
 * highlighted text (which is usually highlighted because it is selected) will
 * be drawn with foreground and background colors reversed.
 */
void
vte_terminal_set_color_highlight_foreground(VteTerminal *terminal,
                                            const GdkRGBA *highlight_foreground) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(highlight_foreground == nullptr || valid_color(highlight_foreground));

        auto impl = IMPL(terminal);
        if (highlight_foreground)
                impl->set_color_highlight_foreground(vte::color::rgb(highlight_foreground));
        else
                impl->reset_color_highlight_foreground();
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_set_colors:
 * @terminal: a #VteTerminal
 * @foreground: (allow-none): the new foreground color, or %NULL
 * @background: (allow-none): the new background color, or %NULL
 * @palette: (array length=palette_size zero-terminated=0) (element-type Gdk.RGBA) (allow-none): the color palette
 * @palette_size: the number of entries in @palette
 *
 * @palette specifies the new values for the 256 palette colors: 8 standard colors,
 * their 8 bright counterparts, 6x6x6 color cube, and 24 grayscale colors.
 * Omitted entries will default to a hardcoded value.
 *
 * @palette_size must be 0, 8, 16, 232 or 256.
 *
 * If @foreground is %NULL and @palette_size is greater than 0, the new foreground
 * color is taken from @palette[7].  If @background is %NULL and @palette_size is
 * greater than 0, the new background color is taken from @palette[0].
 */
void
vte_terminal_set_colors(VteTerminal *terminal,
                        const GdkRGBA *foreground,
                        const GdkRGBA *background,
                        const GdkRGBA *palette,
                        gsize palette_size) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_return_if_fail((palette_size == 0) ||
			 (palette_size == 8) ||
			 (palette_size == 16) ||
			 (palette_size == 232) ||
			 (palette_size == 256));
        g_return_if_fail(foreground == nullptr || valid_color(foreground));
        g_return_if_fail(background == nullptr || valid_color(background));
        for (gsize i = 0; i < palette_size; ++i)
                g_return_if_fail(valid_color(&palette[i]));

        vte::color::rgb fg;
        if (foreground)
                fg = vte::color::rgb(foreground);
        vte::color::rgb bg;
        if (background)
                bg = vte::color::rgb(background);

        vte::color::rgb* pal = nullptr;
        if (palette_size) {
                pal = g_new0(vte::color::rgb, palette_size);
                for (gsize i = 0; i < palette_size; ++i)
                        pal[i] = vte::color::rgb(palette[i]);
        }

        auto impl = IMPL(terminal);
        impl->set_colors(foreground ? &fg : nullptr,
                         background ? &bg : nullptr,
                         pal, palette_size);
        impl->set_background_alpha(background ? background->alpha : 1.0);
        g_free(pal);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_set_default_colors:
 * @terminal: a #VteTerminal
 *
 * Reset the terminal palette to reasonable compiled-in default color.
 */
void
vte_terminal_set_default_colors(VteTerminal *terminal) noexcept
try
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
        IMPL(terminal)->set_colors_default();
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_get_column_count:
 * @terminal: a #VteTerminal
 *
 * Returns: the number of columns
 */
glong
vte_terminal_get_column_count(VteTerminal *terminal) noexcept
try
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), -1);
	return IMPL(terminal)->m_column_count;
}
catch (...)
{
        vte::log_exception();
        return -1;
}

/**
 * vte_terminal_get_current_directory_uri:
 * @terminal: a #VteTerminal
 *
 * Returns: (nullable) (transfer none): the URI of the current directory of the
 *   process running in the terminal, or %NULL
 */
const char *
vte_terminal_get_current_directory_uri(VteTerminal *terminal) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);
        auto impl = IMPL(terminal);
        return impl->m_current_directory_uri.size() ? impl->m_current_directory_uri.data() : nullptr;
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

/**
 * vte_terminal_get_current_file_uri:
 * @terminal: a #VteTerminal
 *
 * Returns: (nullable) (transfer none): the URI of the current file the
 *   process running in the terminal is operating on, or %NULL if
 *   not set
 */
const char *
vte_terminal_get_current_file_uri(VteTerminal *terminal) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);
        auto impl = IMPL(terminal);
        return impl->m_current_file_uri.size() ? impl->m_current_file_uri.data() : nullptr;
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

/**
 * vte_terminal_get_cursor_blink_mode:
 * @terminal: a #VteTerminal
 *
 * Returns the currently set cursor blink mode.
 *
 * Return value: cursor blink mode.
 */
VteCursorBlinkMode
vte_terminal_get_cursor_blink_mode(VteTerminal *terminal) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), VTE_CURSOR_BLINK_SYSTEM);

        return WIDGET(terminal)->cursor_blink_mode();
}
catch (...)
{
        vte::log_exception();
        return VTE_CURSOR_BLINK_SYSTEM;
}

/**
 * vte_terminal_set_cursor_blink_mode:
 * @terminal: a #VteTerminal
 * @mode: the #VteCursorBlinkMode to use
 *
 * Sets whether or not the cursor will blink. Using %VTE_CURSOR_BLINK_SYSTEM
 * will use the #GtkSettings::gtk-cursor-blink setting.
 */
void
vte_terminal_set_cursor_blink_mode(VteTerminal *terminal,
                                   VteCursorBlinkMode mode) noexcept
try
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(mode >= VTE_CURSOR_BLINK_SYSTEM && mode <= VTE_CURSOR_BLINK_OFF);

        if (WIDGET(terminal)->set_cursor_blink_mode(mode))
                g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_CURSOR_BLINK_MODE]);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_get_cursor_shape:
 * @terminal: a #VteTerminal
 *
 * Returns the currently set cursor shape.
 *
 * Return value: cursor shape.
 */
VteCursorShape
vte_terminal_get_cursor_shape(VteTerminal *terminal) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), VTE_CURSOR_SHAPE_BLOCK);

        return WIDGET(terminal)->cursor_shape();
}
catch (...)
{
        vte::log_exception();
        return VTE_CURSOR_SHAPE_BLOCK;
}

/**
 * vte_terminal_set_cursor_shape:
 * @terminal: a #VteTerminal
 * @shape: the #VteCursorShape to use
 *
 * Sets the shape of the cursor drawn.
 */
void
vte_terminal_set_cursor_shape(VteTerminal *terminal,
                              VteCursorShape shape) noexcept
try
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(shape >= VTE_CURSOR_SHAPE_BLOCK && shape <= VTE_CURSOR_SHAPE_UNDERLINE);

        if (WIDGET(terminal)->set_cursor_shape(shape))
                g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_CURSOR_SHAPE]);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_set_delete_binding:
 * @terminal: a #VteTerminal
 * @binding: a #VteEraseBinding for the delete key
 *
 * Modifies the terminal's delete key binding, which controls what
 * string or control sequence the terminal sends to its child when the user
 * presses the delete key.
 */
void
vte_terminal_set_delete_binding(VteTerminal *terminal,
                                VteEraseBinding binding) noexcept
try
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(binding >= VTE_ERASE_AUTO && binding <= VTE_ERASE_TTY);

        if (WIDGET(terminal)->set_delete_binding(binding))
                g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_DELETE_BINDING]);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_get_enable_bidi:
 * @terminal: a #VteTerminal
 *
 * Checks whether the terminal performs bidirectional text rendering.
 *
 * Returns: %TRUE if BiDi is enabled, %FALSE if not
 *
 * Since: 0.58
 */
gboolean
vte_terminal_get_enable_bidi(VteTerminal *terminal) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), false);
        return IMPL(terminal)->m_enable_bidi;
}
catch (...)
{
        vte::log_exception();
        return false;
}

/**
 * vte_terminal_set_enable_bidi:
 * @terminal: a #VteTerminal
 * @enable_bidi: %TRUE to enable BiDi support
 *
 * Controls whether or not the terminal will perform bidirectional text rendering.
 *
 * Since: 0.58
 */
void
vte_terminal_set_enable_bidi(VteTerminal *terminal,
                             gboolean enable_bidi) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));

        if (IMPL(terminal)->set_enable_bidi(enable_bidi != FALSE))
                g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_ENABLE_BIDI]);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_get_enable_shaping:
 * @terminal: a #VteTerminal
 *
 * Checks whether the terminal shapes Arabic text.
 *
 * Returns: %TRUE if Arabic shaping is enabled, %FALSE if not
 *
 * Since: 0.58
 */
gboolean
vte_terminal_get_enable_shaping(VteTerminal *terminal) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), false);
        return IMPL(terminal)->m_enable_shaping;
}
catch (...)
{
        vte::log_exception();
        return false;
}

/**
 * vte_terminal_set_enable_shaping:
 * @terminal: a #VteTerminal
 * @enable_shaping: %TRUE to enable Arabic shaping
 *
 * Controls whether or not the terminal will shape Arabic text.
 *
 * Since: 0.58
 */
void
vte_terminal_set_enable_shaping(VteTerminal *terminal,
                                gboolean enable_shaping) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));

        if (IMPL(terminal)->set_enable_shaping(enable_shaping != FALSE))
                g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_ENABLE_SHAPING]);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_get_encoding:
 * @terminal: a #VteTerminal
 *
 * Determines the name of the encoding in which the terminal expects data to be
 * encoded, or %NULL if UTF-8 is in use.
 *
 * Returns: (nullable) (transfer none): the current encoding for the terminal
 *
 * Deprecated: 0.54: Support for non-UTF-8 is deprecated.
 */
const char *
vte_terminal_get_encoding(VteTerminal *terminal) noexcept
try
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);
	return WIDGET(terminal)->encoding();
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

/**
 * vte_terminal_set_encoding:
 * @terminal: a #VteTerminal
 * @codeset: (allow-none): target charset, or %NULL to use UTF-8
 * @error: (allow-none): return location for a #GError, or %NULL
 *
 * Changes the encoding the terminal will expect data from the child to
 * be encoded with.  For certain terminal types, applications executing in the
 * terminal can change the encoding. If @codeset is %NULL, it uses "UTF-8".
 *
 * Note: Support for non-UTF-8 is deprecated and may get removed altogether.
 * Instead of this function, you should use a wrapper like luit(1) when
 * spawning the child process.
 *
 * Returns: %TRUE if the encoding could be changed to the specified one,
 *  or %FALSE with @error set to %G_CONVERT_ERROR_NO_CONVERSION.
 *
 * Deprecated: 0.54: Support for non-UTF-8 is deprecated.
 */
gboolean
vte_terminal_set_encoding(VteTerminal *terminal,
                          const char *codeset,
                          GError **error) noexcept
try
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
        g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

        auto const freezer = vte::glib::FreezeObjectNotify{terminal};

        auto const rv = IMPL(terminal)->set_encoding(codeset, error);
        if (rv) {
                g_signal_emit(freezer.get(), signals[SIGNAL_ENCODING_CHANGED], 0);
                g_object_notify_by_pspec(freezer.get(), pspecs[PROP_ENCODING]);
        }

        return rv;
}
catch (...)
{
        return vte::glib::set_error_from_exception(error);
}

/**
 * vte_terminal_get_font:
 * @terminal: a #VteTerminal
 *
 * Queries the terminal for information about the fonts which will be
 * used to draw text in the terminal.  The actual font takes the font scale
 * into account, this is not reflected in the return value, the unscaled
 * font is returned.
 *
 * Returns: (transfer none): a #PangoFontDescription describing the font the
 * terminal uses to render text at the default font scale of 1.0.
 */
const PangoFontDescription *
vte_terminal_get_font(VteTerminal *terminal) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), nullptr);

        return IMPL(terminal)->unscaled_font_description();
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

/**
 * vte_terminal_set_font:
 * @terminal: a #VteTerminal
 * @font_desc: (allow-none): a #PangoFontDescription for the desired font, or %NULL
 *
 * Sets the font used for rendering all text displayed by the terminal,
 * overriding any fonts set using gtk_widget_modify_font().  The terminal
 * will immediately attempt to load the desired font, retrieve its
 * metrics, and attempt to resize itself to keep the same number of rows
 * and columns.  The font scale is applied to the specified font.
 */
void
vte_terminal_set_font(VteTerminal *terminal,
                      const PangoFontDescription* font_desc) noexcept
try
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        if (IMPL(terminal)->set_font_desc(vte::take_freeable(pango_font_description_copy(font_desc))))
                g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_FONT_DESC]);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_get_font_scale:
 * @terminal: a #VteTerminal
 *
 * Returns: the terminal's font scale
 */
gdouble
vte_terminal_get_font_scale(VteTerminal *terminal) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), 1.);

        return IMPL(terminal)->m_font_scale;
}
catch (...)
{
        vte::log_exception();
        return 1.;
}

/**
 * vte_terminal_set_font_scale:
 * @terminal: a #VteTerminal
 * @scale: the font scale
 *
 * Sets the terminal's font scale to @scale.
 */
void
vte_terminal_set_font_scale(VteTerminal *terminal,
                            double scale) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));

        scale = CLAMP(scale, VTE_FONT_SCALE_MIN, VTE_FONT_SCALE_MAX);
        if (IMPL(terminal)->set_font_scale(scale))
                g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_FONT_SCALE]);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_get_cell_height_scale:
 * @terminal: a #VteTerminal
 *
 * Returns: the terminal's cell height scale
 *
 * Since: 0.52
 */
double
vte_terminal_get_cell_height_scale(VteTerminal *terminal) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), 1.);

        return IMPL(terminal)->m_cell_height_scale;
}
catch (...)
{
        vte::log_exception();
        return 1.;
}

/**
 * vte_terminal_set_cell_height_scale:
 * @terminal: a #VteTerminal
 * @scale: the cell height scale
 *
 * Sets the terminal's cell height scale to @scale.
 *
 * This can be used to increase the line spacing. (The font's height is not affected.)
 * Valid values go from 1.0 (default) to 2.0 ("double spacing").
 *
 * Since: 0.52
 */
void
vte_terminal_set_cell_height_scale(VteTerminal *terminal,
                                   double scale) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));

        scale = CLAMP(scale, VTE_CELL_SCALE_MIN, VTE_CELL_SCALE_MAX);
        if (IMPL(terminal)->set_cell_height_scale(scale))
                g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_CELL_HEIGHT_SCALE]);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_get_cell_width_scale:
 * @terminal: a #VteTerminal
 *
 * Returns: the terminal's cell width scale
 *
 * Since: 0.52
 */
double
vte_terminal_get_cell_width_scale(VteTerminal *terminal) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), 1.);

        return IMPL(terminal)->m_cell_width_scale;
}
catch (...)
{
        vte::log_exception();
        return 1.;
}

/**
 * vte_terminal_set_cell_width_scale:
 * @terminal: a #VteTerminal
 * @scale: the cell width scale
 *
 * Sets the terminal's cell width scale to @scale.
 *
 * This can be used to increase the letter spacing. (The font's width is not affected.)
 * Valid values go from 1.0 (default) to 2.0.
 *
 * Since: 0.52
 */
void
vte_terminal_set_cell_width_scale(VteTerminal *terminal,
                                  double scale) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));

        scale = CLAMP(scale, VTE_CELL_SCALE_MIN, VTE_CELL_SCALE_MAX);
        if (IMPL(terminal)->set_cell_width_scale(scale))
                g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_CELL_WIDTH_SCALE]);
}
catch (...)
{
        vte::log_exception();
}

#if VTE_GTK == 3

/* Just some arbitrary minimum values */
#define MIN_COLUMNS (16)
#define MIN_ROWS    (2)

/**
 * vte_terminal_get_geometry_hints:
 * @terminal: a #VteTerminal
 * @hints: (out caller-allocates): a #GdkGeometry to fill in
 * @min_rows: the minimum number of rows to request
 * @min_columns: the minimum number of columns to request
 *
 * Fills in some @hints from @terminal's geometry. The hints
 * filled are those covered by the %GDK_HINT_RESIZE_INC,
 * %GDK_HINT_MIN_SIZE and %GDK_HINT_BASE_SIZE flags.
 *
 * See gtk_window_set_geometry_hints() for more information.
 *
 * @terminal must be realized (see gtk_widget_get_realized()).
 *
 * Deprecated: 0.52
 */
void
vte_terminal_get_geometry_hints(VteTerminal *terminal,
                                GdkGeometry *hints,
                                int min_rows,
                                int min_columns) noexcept
{
        GtkWidget *widget;
        GtkBorder padding;

        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(hints != NULL);
        widget = &terminal->widget;
        g_return_if_fail(gtk_widget_get_realized(widget));

        auto impl = IMPL(terminal);

        auto context = gtk_widget_get_style_context(widget);
        gtk_style_context_get_padding(context, gtk_style_context_get_state(context),
                                      &padding);

        hints->base_width  = padding.left + padding.right;
        hints->base_height = padding.top  + padding.bottom;
        hints->width_inc   = impl->m_cell_width;
        hints->height_inc  = impl->m_cell_height;
        hints->min_width   = hints->base_width  + hints->width_inc  * min_columns;
        hints->min_height  = hints->base_height + hints->height_inc * min_rows;

	_vte_debug_print(VTE_DEBUG_WIDGET_SIZE,
                         "[Terminal %p] Geometry cell       width %ld height %ld\n"
                         "                       base       width %d height %d\n"
                         "                       increments width %d height %d\n"
                         "                       minimum    width %d height %d\n",
                         terminal,
                         impl->m_cell_width, impl->m_cell_height,
                         hints->base_width, hints->base_height,
                         hints->width_inc, hints->height_inc,
                         hints->min_width, hints->min_height);
}

/**
 * vte_terminal_set_geometry_hints_for_window:
 * @terminal: a #VteTerminal
 * @window: a #GtkWindow
 *
 * Sets @terminal as @window's geometry widget. See
 * gtk_window_set_geometry_hints() for more information.
 *
 * @terminal must be realized (see gtk_widget_get_realized()).
 *
 * Deprecated: 0.52
 */
void
vte_terminal_set_geometry_hints_for_window(VteTerminal *terminal,
                                           GtkWindow *window) noexcept
{
        GdkGeometry hints;

        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(gtk_widget_get_realized(&terminal->widget));

        vte_terminal_get_geometry_hints(terminal, &hints, MIN_ROWS, MIN_COLUMNS);
        gtk_window_set_geometry_hints(window,
                                      NULL,
                                      &hints,
                                      (GdkWindowHints)(GDK_HINT_RESIZE_INC |
                                                       GDK_HINT_MIN_SIZE |
                                                       GDK_HINT_BASE_SIZE));
}

#endif /* VTE_GTK == 3 */

/**
 * vte_terminal_get_has_selection:
 * @terminal: a #VteTerminal
 *
 * Checks if the terminal currently contains selected text.  Note that this
 * is different from determining if the terminal is the owner of any
 * #GtkClipboard items.
 *
 * Returns: %TRUE if part of the text in the terminal is selected.
 */
gboolean
vte_terminal_get_has_selection(VteTerminal *terminal) noexcept
try
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), false);
        return !IMPL(terminal)->m_selection_resolved.empty();
}
catch (...)
{
        vte::log_exception();
        return false;
}

/**
 * vte_terminal_get_text_selected:
 * @terminal: a #VteTerminal
 * @format: the #VteFormat to use
 *
 * Gets the currently selected text in the format specified by @format.
 * Note that currently, only %VTE_FORMAT_TEXT is supported.
 *
 * Returns: (transfer full) (nullable): a newly allocated string containing the selected text, or %NULL if there is no selection or the format is not supported
 *
 * Since: 0.70
 */
char*
vte_terminal_get_text_selected(VteTerminal* terminal,
                               VteFormat format) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), nullptr);

        if (format != VTE_FORMAT_TEXT)
                return nullptr;

        auto const selection = IMPL(terminal)->m_selection_resolved;
        return vte_terminal_get_text_range(terminal,
                                           selection.start_row(),
                                           selection.start_column(),
                                           selection.end_row(),
                                           selection.end_column(),
                                           nullptr,
                                           nullptr,
                                           nullptr);
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

/**
 * vte_terminal_get_icon_title:
 * @terminal: a #VteTerminal
 *
 * Returns: (nullable) (transfer none): %NULL
 *
 * Deprecated: 0.54:
 */
const char *
vte_terminal_get_icon_title(VteTerminal *terminal) noexcept
{
	return nullptr;
}

/**
 * vte_terminal_get_input_enabled:
 * @terminal: a #VteTerminal
 *
 * Returns whether the terminal allow user input.
 */
gboolean
vte_terminal_get_input_enabled (VteTerminal *terminal) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), false);

        return IMPL(terminal)->m_input_enabled;
}
catch (...)
{
        vte::log_exception();
        return false;
}

/**
 * vte_terminal_set_input_enabled:
 * @terminal: a #VteTerminal
 * @enabled: whether to enable user input
 *
 * Enables or disables user input. When user input is disabled,
 * the terminal's child will not receive any key press, or mouse button
 * press or motion events sent to it.
 */
void
vte_terminal_set_input_enabled (VteTerminal *terminal,
                                gboolean enabled) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));

        if (IMPL(terminal)->set_input_enabled(enabled != FALSE))
                g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_INPUT_ENABLED]);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_get_mouse_autohide:
 * @terminal: a #VteTerminal
 *
 * Determines the value of the terminal's mouse autohide setting.  When
 * autohiding is enabled, the mouse cursor will be hidden when the user presses
 * a key and shown when the user moves the mouse.  This setting can be changed
 * using vte_terminal_set_mouse_autohide().
 *
 * Returns: %TRUE if autohiding is enabled, %FALSE if not
 */
gboolean
vte_terminal_get_mouse_autohide(VteTerminal *terminal) noexcept
try
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), false);
	return IMPL(terminal)->m_mouse_autohide;
}
catch (...)
{
        vte::log_exception();
        return false;
}

/**
 * vte_terminal_set_mouse_autohide:
 * @terminal: a #VteTerminal
 * @setting: whether the mouse pointer should autohide
 *
 * Changes the value of the terminal's mouse autohide setting.  When autohiding
 * is enabled, the mouse cursor will be hidden when the user presses a key and
 * shown when the user moves the mouse.  This setting can be read using
 * vte_terminal_get_mouse_autohide().
 */
void
vte_terminal_set_mouse_autohide(VteTerminal *terminal,
                                gboolean setting) noexcept
try
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        if (IMPL(terminal)->set_mouse_autohide(setting != FALSE))
                g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_MOUSE_POINTER_AUTOHIDE]);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_set_pty:
 * @terminal: a #VteTerminal
 * @pty: (allow-none): a #VtePty, or %NULL
 *
 * Sets @pty as the PTY to use in @terminal.
 * Use %NULL to unset the PTY.
 */
void
vte_terminal_set_pty(VteTerminal *terminal,
                     VtePty *pty) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(pty == NULL || VTE_IS_PTY(pty));

        auto const freezer = vte::glib::FreezeObjectNotify{terminal};

        if (WIDGET(terminal)->set_pty(pty))
                g_object_notify_by_pspec(freezer.get(), pspecs[PROP_PTY]);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_get_pty:
 * @terminal: a #VteTerminal
 *
 * Returns the #VtePty of @terminal.
 *
 * Returns: (transfer none): a #VtePty, or %NULL
 */
VtePty *
vte_terminal_get_pty(VteTerminal *terminal) noexcept
try
{
        g_return_val_if_fail (VTE_IS_TERMINAL (terminal), nullptr);
        return WIDGET(terminal)->pty();
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

/**
 * vte_terminal_get_rewrap_on_resize:
 * @terminal: a #VteTerminal
 *
 * Checks whether or not the terminal will rewrap its contents upon resize.
 *
 * Returns: %TRUE if rewrapping is enabled, %FALSE if not
 *
 * Deprecated: 0.58
 */
gboolean
vte_terminal_get_rewrap_on_resize(VteTerminal *terminal) noexcept
try
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), false);
	return IMPL(terminal)->m_rewrap_on_resize;
}
catch (...)
{
        vte::log_exception();
        return false;
}

/**
 * vte_terminal_set_rewrap_on_resize:
 * @terminal: a #VteTerminal
 * @rewrap: %TRUE if the terminal should rewrap on resize
 *
 * Controls whether or not the terminal will rewrap its contents, including
 * the scrollback history, whenever the terminal's width changes.
 *
 * Deprecated: 0.58
 */
void
vte_terminal_set_rewrap_on_resize(VteTerminal *terminal,
                                  gboolean rewrap) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));

        if (IMPL(terminal)->set_rewrap_on_resize(rewrap != FALSE))
                g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_REWRAP_ON_RESIZE]);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_get_row_count:
 * @terminal: a #VteTerminal
 *
 *
 * Returns: the number of rows
 */
glong
vte_terminal_get_row_count(VteTerminal *terminal) noexcept
try
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), -1);
	return IMPL(terminal)->m_row_count;
}
catch (...)
{
        vte::log_exception();
        return -1;
}

/**
 * vte_terminal_set_scrollback_lines:
 * @terminal: a #VteTerminal
 * @lines: the length of the history buffer
 *
 * Sets the length of the scrollback buffer used by the terminal.  The size of
 * the scrollback buffer will be set to the larger of this value and the number
 * of visible rows the widget can display, so 0 can safely be used to disable
 * scrollback.
 *
 * A negative value means "infinite scrollback".
 *
 * Note that this setting only affects the normal screen buffer.
 * No scrollback is allowed on the alternate screen buffer.
 */
void
vte_terminal_set_scrollback_lines(VteTerminal *terminal,
                                  glong lines) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(lines >= -1);

        auto const freezer = vte::glib::FreezeObjectNotify{terminal};

        if (IMPL(terminal)->set_scrollback_lines(lines))
                g_object_notify_by_pspec(freezer.get(), pspecs[PROP_SCROLLBACK_LINES]);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_get_scrollback_lines:
 * @terminal: a #VteTerminal
 *
 * Returns: length of the scrollback buffer used by the terminal.
 * A negative value means "infinite scrollback".
 *
 * Since: 0.52
 */
glong
vte_terminal_get_scrollback_lines(VteTerminal *terminal) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), 0);
        return IMPL(terminal)->m_scrollback_lines;
}
catch (...)
{
        vte::log_exception();
        return 0;
}

/**
 * vte_terminal_set_scroll_on_keystroke:
 * @terminal: a #VteTerminal
 * @scroll: whether the terminal should scroll on keystrokes
 *
 * Controls whether or not the terminal will forcibly scroll to the bottom of
 * the viewable history when the user presses a key.  Modifier keys do not
 * trigger this behavior.
 */
void
vte_terminal_set_scroll_on_keystroke(VteTerminal *terminal,
                                     gboolean scroll) noexcept
try
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        if (IMPL(terminal)->set_scroll_on_keystroke(scroll != FALSE))
                g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_SCROLL_ON_KEYSTROKE]);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_get_scroll_on_keystroke:
 * @terminal: a #VteTerminal
 *
 * Returns: whether or not the terminal will forcibly scroll to the bottom of
 * the viewable history when the user presses a key.  Modifier keys do not
 * trigger this behavior.
 *
 * Since: 0.52
 */
gboolean
vte_terminal_get_scroll_on_keystroke(VteTerminal *terminal) noexcept
try
{
    g_return_val_if_fail(VTE_IS_TERMINAL(terminal), false);
    return IMPL(terminal)->m_scroll_on_keystroke;
}
catch (...)
{
        vte::log_exception();
        return false;
}

/**
 * vte_terminal_set_scroll_on_output:
 * @terminal: a #VteTerminal
 * @scroll: whether the terminal should scroll on output
 *
 * Controls whether or not the terminal will forcibly scroll to the bottom of
 * the viewable history when the new data is received from the child.
 */
void
vte_terminal_set_scroll_on_output(VteTerminal *terminal,
                                  gboolean scroll) noexcept
try
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        if (IMPL(terminal)->set_scroll_on_output(scroll != FALSE))
                g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_SCROLL_ON_OUTPUT]);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_get_scroll_on_output:
 * @terminal: a #VteTerminal
 *
 * Returns: whether or not the terminal will forcibly scroll to the bottom of
 * the viewable history when the new data is received from the child.
 *
 * Since: 0.52
 */
gboolean
vte_terminal_get_scroll_on_output(VteTerminal *terminal) noexcept
try
{
    g_return_val_if_fail(VTE_IS_TERMINAL(terminal), false);
    return IMPL(terminal)->m_scroll_on_output;
}
catch (...)
{
        vte::log_exception();
        return false;
}

/**
 * vte_terminal_set_enable_fallback_scrolling:
 * @terminal: a #VteTerminal
 * @enable: whether to enable fallback scrolling
 *
 * Controls whether the terminal uses scroll events to scroll the history
 * if the event was not otherwise consumed by it.
 *
 * This function is rarely useful, except when the terminal is added to a
 * #GtkScrolledWindow, to perform kinetic scrolling (while vte itself does
 * not, yet, implement kinetic scrolling by itself).
 *
 * Since: 0.64
 */
void
vte_terminal_set_enable_fallback_scrolling(VteTerminal *terminal,
                                           gboolean enable) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));

        if (WIDGET(terminal)->set_fallback_scrolling(enable != false))
                g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_ENABLE_FALLBACK_SCROLLING]);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_get_enable_fallback_scrolling:
 * @terminal: a #VteTerminal
 *
 * Returns: %TRUE if fallback scrolling is enabled
 *
 * Since: 0.64
 */
gboolean
vte_terminal_get_enable_fallback_scrolling(VteTerminal *terminal) noexcept
try
{
    g_return_val_if_fail(VTE_IS_TERMINAL(terminal), false);
    return WIDGET(terminal)->fallback_scrolling();
}
catch (...)
{
        vte::log_exception();
        return true;
}

/**
 * vte_terminal_set_scroll_unit_is_pixels:
 * @terminal: a #VteTerminal
 * @enable: whether to use pixels as scroll unit
 *
 * Controls whether the terminal's scroll unit is lines or pixels.
 *
 * This function is rarely useful, except when the terminal is added to a
 * #GtkScrolledWindow.
 *
 * Since: 0.66
 */
void
vte_terminal_set_scroll_unit_is_pixels(VteTerminal *terminal,
                                       gboolean enable) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));

        if (WIDGET(terminal)->set_scroll_unit_is_pixels(enable != false))
                g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_SCROLL_UNIT_IS_PIXELS]);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_get_scroll_unit_is_pixels:
 * @terminal: a #VteTerminal
 *
 * Returns: %TRUE if the scroll unit is pixels; or %FALSE if the unit is lines
 *
 * Since: 0.66
 */
gboolean
vte_terminal_get_scroll_unit_is_pixels(VteTerminal *terminal) noexcept
try
{
    g_return_val_if_fail(VTE_IS_TERMINAL(terminal), false);
    return WIDGET(terminal)->scroll_unit_is_pixels();
}
catch (...)
{
        vte::log_exception();
        return false;
}

/**
 * vte_terminal_get_window_title:
 * @terminal: a #VteTerminal
 *
 * Returns: (nullable) (transfer none): the window title, or %NULL
 */
const char *
vte_terminal_get_window_title(VteTerminal *terminal) noexcept
try
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), nullptr);
	return IMPL(terminal)->m_window_title.data();
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

/**
 * vte_terminal_get_word_char_exceptions:
 * @terminal: a #VteTerminal
 *
 * Returns the set of characters which will be considered parts of a word
 * when doing word-wise selection, in addition to the default which only
 * considers alphanumeric characters part of a word.
 *
 * If %NULL, a built-in set is used.
 *
 * Returns: (nullable) (transfer none): a string, or %NULL
 *
 * Since: 0.40
 */
const char *
vte_terminal_get_word_char_exceptions(VteTerminal *terminal) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);

        return WIDGET(terminal)->word_char_exceptions();
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

/**
 * vte_terminal_set_word_char_exceptions:
 * @terminal: a #VteTerminal
 * @exceptions: a string of ASCII punctuation characters, or %NULL
 *
 * With this function you can provide a set of characters which will
 * be considered parts of a word when doing word-wise selection, in
 * addition to the default which only considers alphanumeric characters
 * part of a word.
 *
 * The characters in @exceptions must be non-alphanumeric, each character
 * must occur only once, and if @exceptions contains the character
 * U+002D HYPHEN-MINUS, it must be at the start of the string.
 *
 * Use %NULL to reset the set of exception characters to the default.
 *
 * Since: 0.40
 */
void
vte_terminal_set_word_char_exceptions(VteTerminal *terminal,
                                      const char *exceptions) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));

        auto stropt = exceptions ? std::make_optional<std::string_view>(exceptions) : std::nullopt;
        if (WIDGET(terminal)->set_word_char_exceptions(stropt))
                g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_WORD_CHAR_EXCEPTIONS]);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_write_contents_sync:
 * @terminal: a #VteTerminal
 * @stream: a #GOutputStream to write to
 * @flags: a set of #VteWriteFlags
 * @cancellable: (allow-none): a #GCancellable object, or %NULL
 * @error: (allow-none): a #GError location to store the error occuring, or %NULL
 *
 * Write contents of the current contents of @terminal (including any
 * scrollback history) to @stream according to @flags.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by triggering
 * the cancellable object from another thread. If the operation was cancelled,
 * the error %G_IO_ERROR_CANCELLED will be returned in @error.
 *
 * This is a synchronous operation and will make the widget (and input
 * processing) during the write operation, which may take a long time
 * depending on scrollback history and @stream availability for writing.
 *
 * Returns: %TRUE on success, %FALSE if there was an error
 */
gboolean
vte_terminal_write_contents_sync (VteTerminal *terminal,
                                  GOutputStream *stream,
                                  VteWriteFlags flags,
                                  GCancellable *cancellable,
                                  GError **error) noexcept
try
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), false);
        g_return_val_if_fail(G_IS_OUTPUT_STREAM(stream), false);

        return IMPL(terminal)->write_contents_sync(stream, flags, cancellable, error);
}
catch (...)
{
        return vte::glib::set_error_from_exception(error);
}

/**
 * vte_terminal_set_clear_background:
 * @terminal: a #VteTerminal
 * @setting: whether to clear the background
 *
 * Sets whether to paint the background with the background colour.
 * The default is %TRUE.
 *
 * This function is rarely useful. One use for it is to add a background
 * image to the terminal.
 *
 * Since: 0.52
 */
void
vte_terminal_set_clear_background(VteTerminal* terminal,
                                  gboolean setting) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));

        IMPL(terminal)->set_clear_background(setting != FALSE);
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_get_color_background_for_draw:
 * @terminal: a #VteTerminal
 * @color: (out): a location to store a #GdkRGBA color
 *
 * Returns the background colour, as used by @terminal when
 * drawing the background, which may be different from
 * the color set by vte_terminal_set_color_background().
 *
 * Note: you must only call this function while handling the
 * GtkWidget::draw signal.
 *
 * This function is rarely useful. One use for it is if you disable
 * drawing the background (see vte_terminal_set_clear_background())
 * and then need to draw the background yourself.
 *
 * Since: 0.54
 */
void
vte_terminal_get_color_background_for_draw(VteTerminal* terminal,
                                           GdkRGBA* color) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(color != nullptr);

        auto impl = IMPL(terminal);
        auto const c = impl->get_color(VTE_DEFAULT_BG);
        color->red = c->red / 65535.;
        color->green = c->green / 65535.;
        color->blue = c->blue / 65535.;
        color->alpha = impl->m_background_alpha;
}
catch (...)
{
        vte::log_exception();
        *color = {0., 0., 0., 1.};
}

/**
 * vte_terminal_set_enable_sixel:
 * @terminal: a #VteTerminal
 * @enabled: whether to enable SIXEL images
 *
 * Set whether to enable SIXEL images.
 *
 * Since: 0.62
 */
void
vte_terminal_set_enable_sixel(VteTerminal *terminal,
                              gboolean enabled) noexcept
try
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));
}
catch (...)
{
        vte::log_exception();
}

/**
 * vte_terminal_get_enable_sixel:
 * @terminal: a #VteTerminal
 *
 * Returns: %FALSE
 *
 * Since: 0.62
 */
gboolean
vte_terminal_get_enable_sixel(VteTerminal *terminal) noexcept
try
{
        return false;
}
catch (...)
{
        vte::log_exception();
        return false;
}

template<>
constexpr bool check_enum_value<VteAlign>(VteAlign value) noexcept
{
        switch (value) {
        case VTE_ALIGN_START:
        case VTE_ALIGN_CENTER:
        case VTE_ALIGN_END:
                return true;
        default:
                return false;
        }
}
