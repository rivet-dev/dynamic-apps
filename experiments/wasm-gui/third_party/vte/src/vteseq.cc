/*
 * Copyright © 2001-2004 Red Hat, Inc.
 * Copyright © 2015 David Herrmann <dh.herrmann@gmail.com>
 * Copyright © 2008-2018 Christian Persch
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

#include "config.h"

#include <search.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifdef HAVE_SYS_SYSLIMITS_H
#include <sys/syslimits.h>
#endif

#include <glib.h>

#include <vte/vte.h>
#include "vteinternal.hh"
#include "vtegtk.hh"
#include "caps.hh"
#include "debug.h"

#define BEL_C0 "\007"
#define ST_C0 _VTE_CAP_ST

#include <algorithm>

using namespace std::literals;

enum {
        VTE_XTERM_WM_RESTORE_WINDOW = 1,
        VTE_XTERM_WM_MINIMIZE_WINDOW = 2,
        VTE_XTERM_WM_SET_WINDOW_POSITION = 3,
        VTE_XTERM_WM_SET_WINDOW_SIZE_PIXELS = 4,
        VTE_XTERM_WM_RAISE_WINDOW = 5,
        VTE_XTERM_WM_LOWER_WINDOW = 6,
        VTE_XTERM_WM_REFRESH_WINDOW = 7,
        VTE_XTERM_WM_SET_WINDOW_SIZE_CELLS = 8,
        VTE_XTERM_WM_MAXIMIZE_WINDOW = 9,
        VTE_XTERM_WM_FULLSCREEN_WINDOW = 10,
        VTE_XTERM_WM_GET_WINDOW_STATE = 11,
        VTE_XTERM_WM_GET_WINDOW_POSITION = 13,
        VTE_XTERM_WM_GET_WINDOW_SIZE_PIXELS = 14,
        VTE_XTERM_WM_GET_WINDOW_SIZE_CELLS = 18,
        VTE_XTERM_WM_GET_SCREEN_SIZE_CELLS = 19,
        VTE_XTERM_WM_GET_ICON_TITLE = 20,
        VTE_XTERM_WM_GET_WINDOW_TITLE = 21,
        VTE_XTERM_WM_TITLE_STACK_PUSH = 22,
        VTE_XTERM_WM_TITLE_STACK_POP = 23,
};

enum {
        VTE_SGR_COLOR_SPEC_RGB    = 2,
        VTE_SGR_COLOR_SPEC_LEGACY = 5
};

void
vte::parser::Sequence::print() const noexcept
{
#ifdef VTE_DEBUG
        auto c = m_seq != nullptr ? terminator() : 0;
        char c_buf[7];
        g_snprintf(c_buf, sizeof(c_buf), "%lc", c);
        g_printerr("%s:%s [%s]", type_string(), command_string(),
                   g_unichar_isprint(c) ? c_buf : _vte_debug_sequence_to_string(c_buf, -1));
        if (m_seq != nullptr && m_seq->n_args > 0) {
                g_printerr("[ ");
                for (unsigned int i = 0; i < m_seq->n_args; i++) {
                        if (i > 0)
                                g_print(", ");
                        g_printerr("%d", vte_seq_arg_value(m_seq->args[i]));
                }
                g_printerr(" ]");
        }
        if (m_seq->type == VTE_SEQ_OSC || m_seq->type == VTE_SEQ_DCS) {
                char* str = string_param();
                g_printerr(" \"%s\"", str);
                g_free(str);
        }
        g_printerr("\n");
#endif
}

char const*
vte::parser::Sequence::type_string() const
{
        if (G_UNLIKELY(m_seq == nullptr))
                return "(nil)";

        switch (type()) {
        case VTE_SEQ_NONE:    return "NONE";
        case VTE_SEQ_IGNORE:  return "IGNORE";
        case VTE_SEQ_GRAPHIC: return "GRAPHIC";
        case VTE_SEQ_CONTROL: return "CONTROL";
        case VTE_SEQ_ESCAPE:  return "ESCAPE";
        case VTE_SEQ_CSI:     return "CSI";
        case VTE_SEQ_DCS:     return "DCS";
        case VTE_SEQ_OSC:     return "OSC";
        case VTE_SEQ_SCI:     return "SCI";
        case VTE_SEQ_APC:     return "APC";
        case VTE_SEQ_PM:      return "PM";
        case VTE_SEQ_SOS:     return "SOS";
        default:
                g_assert(false);
                return nullptr;
        }
}

char const*
vte::parser::Sequence::command_string() const
{
        if (G_UNLIKELY(m_seq == nullptr))
                return "(nil)";

        switch (command()) {
#define _VTE_CMD(cmd) case VTE_CMD_##cmd: return #cmd;
#define _VTE_NOP(cmd)
#include "parser-cmd.hh"
#undef _VTE_CMD
#undef _VTE_NOP
        default:
                static char buf[32];
                snprintf(buf, sizeof(buf), "NOP OR UNKOWN(%u)", command());
                return buf;
        }
}

// FIXMEchpe optimise this
std::string
vte::parser::Sequence::string_utf8() const noexcept
{
        std::string str;

        size_t len;
        auto buf = vte_seq_string_get(&m_seq->arg_str, &len);

        char u[6];
        for (size_t i = 0; i < len; ++i) {
                auto ulen = g_unichar_to_utf8(buf[i], u);
                str.append((char const*)u, ulen);
        }

        return str;
}

/* A couple are duplicated from vte.c, to keep them static... */

/* Check how long a string of unichars is.  Slow version. */
static gsize
vte_unichar_strlen(gunichar const* c)
{
	gsize i;
	for (i = 0; c[i] != 0; i++) ;
	return i;
}

/* Convert a wide character string to a multibyte string */
/* Simplified from glib's g_ucs4_to_utf8() to simply allocate the maximum
 * length instead of walking the input twice.
 */
char*
vte::parser::Sequence::ucs4_to_utf8(gunichar const* str,
                                    ssize_t len) const noexcept
{
        if (len < 0)
                len = vte_unichar_strlen(str);
        auto outlen = (len * VTE_UTF8_BPC) + 1;

        auto result = (char*)g_try_malloc(outlen);
        if (result == nullptr)
                return nullptr;

        auto end = str + len;
        auto p = result;
        for (auto i = str; i < end; i++)
                p += g_unichar_to_utf8(*i, p);
        *p = '\0';

        return result;
}

namespace vte {
namespace terminal {

/* Emit a "bell" signal. */
void
Terminal::emit_bell()
{
        _vte_debug_print(VTE_DEBUG_SIGNALS, "Emitting `bell'.\n");
        g_signal_emit(m_terminal, signals[SIGNAL_BELL], 0);
}

/* Emit a "resize-window" signal.  (Grid size.) */
void
Terminal::emit_resize_window(guint columns,
                                       guint rows)
{
        _vte_debug_print(VTE_DEBUG_SIGNALS, "Emitting `resize-window'.\n");
        g_signal_emit(m_terminal, signals[SIGNAL_RESIZE_WINDOW], 0, columns, rows);
}

/* Some common functions */

/* In Xterm, upon printing a character in the last column the cursor doesn't
 * advance.  It's special cased that printing the following letter will first
 * wrap to the next row.
 *
 * As a rule of thumb, escape sequences that move the cursor (e.g. cursor up)
 * or immediately update the visible contents (e.g. clear in line) disable
 * this special mode, whereas escape sequences with no immediate visible
 * effect (e.g. color change) leave this special mode on.  There are
 * exceptions of course (e.g. scroll up).
 *
 * In VTE, a different technical approach is used.  The cursor is advanced to
 * the invisible column on the right, but it's set back to the visible
 * rightmost column whenever necessary (that is, before handling any of the
 * sequences that disable the special cased mode in xterm).  (Bug 731155.)
 */
void
Terminal::ensure_cursor_is_onscreen()
{
        if (G_UNLIKELY (m_screen->cursor.col >= m_column_count))
                m_screen->cursor.col = m_column_count - 1;
}

void
Terminal::home_cursor()
{
        set_cursor_coords(0, 0);
}

void
Terminal::clear_screen()
{
        auto row = m_screen->cursor.row - m_screen->insert_delta;
        auto initial = _vte_ring_next(m_screen->row_data);
	/* Add a new screen's worth of rows. */
        for (auto i = 0; i < m_row_count; i++)
                ring_append(true);
	/* Move the cursor and insertion delta to the first line in the
	 * newly-cleared area and scroll if need be. */
        m_screen->insert_delta = initial;
        m_screen->cursor.row = row + m_screen->insert_delta;
        adjust_adjustments();
	/* Redraw everything. */
        invalidate_all();
	/* We've modified the display.  Make a note of it. */
        m_text_deleted_flag = TRUE;
}

/* Clear the current line. */
void
Terminal::clear_current_line()
{
	VteRowData *rowdata;

	/* If the cursor is actually on the screen, clear data in the row
	 * which corresponds to the cursor. */
        if (_vte_ring_next(m_screen->row_data) > m_screen->cursor.row) {
		/* Get the data for the row which the cursor points to. */
                rowdata = _vte_ring_index_writable(m_screen->row_data, m_screen->cursor.row);
		g_assert(rowdata != NULL);
		/* Remove it. */
		_vte_row_data_shrink (rowdata, 0);
		/* Add enough cells to the end of the line to fill out the row. */
                _vte_row_data_fill (rowdata, &m_color_defaults, m_column_count);
                set_hard_wrapped(m_screen->cursor.row);
                rowdata->attr.bidi_flags = get_bidi_flags();
                /* Repaint this row's paragraph (might need to extend upwards). */
                invalidate_row_and_context(m_screen->cursor.row);
	}

	/* We've modified the display.  Make a note of it. */
        m_text_deleted_flag = TRUE;
}

/* Clear above the current line. */
void
Terminal::clear_above_current()
{
        /* Make the line just above the writable area hard wrapped. */
        if (m_screen->insert_delta > _vte_ring_delta(m_screen->row_data)) {
                set_hard_wrapped(m_screen->insert_delta - 1);
        }
        /* Clear data in all the writable rows above (excluding) the cursor's. */
        for (auto i = m_screen->insert_delta; i < m_screen->cursor.row; i++) {
                if (_vte_ring_next(m_screen->row_data) > i) {
			/* Get the data for the row we're erasing. */
                        auto rowdata = _vte_ring_index_writable(m_screen->row_data, i);
			g_assert(rowdata != NULL);
			/* Remove it. */
			_vte_row_data_shrink (rowdata, 0);
			/* Add new cells until we fill the row. */
                        _vte_row_data_fill (rowdata, &m_color_defaults, m_column_count);
                        set_hard_wrapped(i);
                        rowdata->attr.bidi_flags = get_bidi_flags();
		}
	}
        /* Repaint the cleared area. No need to extend, set_hard_wrapped() took care of
         * invalidating the context lines if necessary. */
        invalidate_rows(m_screen->insert_delta, m_screen->cursor.row - 1);
	/* We've modified the display.  Make a note of it. */
        m_text_deleted_flag = TRUE;
}

/* Scroll the text, but don't move the cursor.  Negative = up, positive = down. */
void
Terminal::scroll_text(vte::grid::row_t scroll_amount)
{
        vte::grid::row_t start, end;
        if (m_scrolling_restricted) {
                start = m_screen->insert_delta + m_scrolling_region.start;
                end = m_screen->insert_delta + m_scrolling_region.end;
	} else {
                start = m_screen->insert_delta;
                end = start + m_row_count - 1;
	}

        while (_vte_ring_next(m_screen->row_data) <= end)
                ring_append(false);

	if (scroll_amount > 0) {
                /* Scroll down. */
		for (auto i = 0; i < scroll_amount; i++) {
                        ring_remove(end);
                        ring_insert(start, true);
		}
                /* Set the boundaries to hard wrapped where we tore apart the contents.
                 * Need to do it after scrolling down, for the end row to be the desired one. */
                set_hard_wrapped(start - 1);
                set_hard_wrapped(end);
	} else {
                /* Set the boundaries to hard wrapped where we're about to tear apart the contents.
                 * Need to do it before scrolling up, for the end row to be the desired one. */
                set_hard_wrapped(start - 1);
                set_hard_wrapped(end);
                /* Scroll up. */
		for (auto i = 0; i < -scroll_amount; i++) {
                        ring_remove(start);
                        ring_insert(end, true);
		}
	}

        /* Repaint the affected lines. No need to extend, set_hard_wrapped() took care of
         * invalidating the context lines if necessary. */
        invalidate_rows(start, end);

	/* Adjust the scrollbars if necessary. */
        adjust_adjustments();

	/* We've modified the display.  Make a note of it. */
        m_text_inserted_flag = TRUE;
        m_text_deleted_flag = TRUE;
}

void
Terminal::restore_cursor()
{
        restore_cursor(m_screen);
        ensure_cursor_is_onscreen();
}

void
Terminal::save_cursor()
{
        save_cursor(m_screen);
}

/* Switch to normal screen. */
void
Terminal::switch_normal_screen()
{
        switch_screen(&m_normal_screen);
}

void
Terminal::switch_screen(VteScreen *new_screen)
{
        /* if (new_screen == m_screen) return; ? */

        /* The two screens use different hyperlink pools, so carrying on the idx
         * wouldn't make sense and could lead to crashes.
         * Ideally we'd carry the target URI itself, but I'm just lazy.
         * Also, run a GC before we switch away from that screen. */
        m_hyperlink_hover_idx = _vte_ring_get_hyperlink_at_position(m_screen->row_data, -1, -1, true, NULL);
        g_assert (m_hyperlink_hover_idx == 0);
        m_hyperlink_hover_uri = NULL;
        emit_hyperlink_hover_uri_changed(NULL);  /* FIXME only emit if really changed */
        m_defaults.attr.hyperlink_idx = _vte_ring_get_hyperlink_idx(m_screen->row_data, NULL);
        g_assert (m_defaults.attr.hyperlink_idx == 0);

        /* cursor.row includes insert_delta, adjust accordingly */
        auto cr = m_screen->cursor.row - m_screen->insert_delta;
        auto cc = m_screen->cursor.col;
        m_screen = new_screen;
        m_screen->cursor.row = cr + m_screen->insert_delta;
        m_screen->cursor.col = cc;

        /* Make sure the ring is large enough */
        ensure_row();
}

/* Switch to alternate screen. */
void
Terminal::switch_alternate_screen()
{
        switch_screen(&m_alternate_screen);
}

void
Terminal::set_mode_ecma(vte::parser::Sequence const& seq,
                                  bool set) noexcept
{
        auto const n_params = seq.size();
        for (unsigned int i = 0; i < n_params; i = seq.next(i)) {
                auto const param = seq.collect1(i);
                auto const mode = m_modes_ecma.mode_from_param(param);

                _vte_debug_print(VTE_DEBUG_MODES,
                                 "Mode %d (%s) %s\n",
                                 param, m_modes_ecma.mode_to_cstring(mode),
                                 set ? "set" : "reset");

                if (mode < 0)
                        continue;

                m_modes_ecma.set(mode, set);

                if (mode == m_modes_ecma.eBDSM) {
                        _vte_debug_print(VTE_DEBUG_BIDI,
                                         "BiDi %s mode\n",
                                         set ? "implicit" : "explicit");
                        maybe_apply_bidi_attributes(VTE_BIDI_FLAG_IMPLICIT);
                }
        }
}

void
Terminal::update_mouse_protocol() noexcept
{
        if (m_modes_private.XTERM_MOUSE_ANY_EVENT())
                m_mouse_tracking_mode = MouseTrackingMode::eALL_MOTION_TRACKING;
        else if (m_modes_private.XTERM_MOUSE_BUTTON_EVENT())
                m_mouse_tracking_mode = MouseTrackingMode::eCELL_MOTION_TRACKING;
        else if (m_modes_private.XTERM_MOUSE_VT220_HIGHLIGHT())
                m_mouse_tracking_mode = MouseTrackingMode::eHILITE_TRACKING;
        else if (m_modes_private.XTERM_MOUSE_VT220())
                m_mouse_tracking_mode = MouseTrackingMode::eSEND_XY_ON_BUTTON;
        else if (m_modes_private.XTERM_MOUSE_X10())
                m_mouse_tracking_mode = MouseTrackingMode::eSEND_XY_ON_CLICK;
        else
                m_mouse_tracking_mode = MouseTrackingMode::eNONE;

        m_mouse_smooth_scroll_delta = 0.0;

        /* Mouse pointer might change */
        apply_mouse_cursor();

        _vte_debug_print(VTE_DEBUG_MODES,
                         "Mouse protocol is now %d\n", (int)m_mouse_tracking_mode);
}

void
Terminal::set_mode_private(int mode,
                           bool set) noexcept
{
        /* Pre actions */
        switch (mode) {
        default:
                break;
        }

        m_modes_private.set(mode, set);

        /* Post actions */
        switch (mode) {
        case vte::terminal::modes::Private::eDEC_132_COLUMN:
                /* DECCOLM: set/reset to 132/80 columns mode, clear screen and cursor home */
                // FIXMEchpe don't do clear screen if DECNCSM is set
                /* FIXMEchpe!!!
                 * Changing this mode resets the top, bottom, left, right margins;
                 * clears the screen (unless DECNCSM is set); resets DECLRMM; and clears
                 * the status line if host-writable.
                 */
                if (m_modes_private.XTERM_DECCOLM()) {
                        emit_resize_window(set ? 132 : 80, m_row_count);
                        clear_screen();
                        home_cursor();
                }
                break;

        case vte::terminal::modes::Private::eDEC_REVERSE_IMAGE:
                invalidate_all();
                break;

        case vte::terminal::modes::Private::eDEC_ORIGIN:
                /* Reposition the cursor in its new home position. */
                home_cursor();
                break;

        case vte::terminal::modes::Private::eDEC_TEXT_CURSOR:
                /* No need to invalidate the cursor here, this is done
                 * in process_incoming().
                 */
                break;

        case vte::terminal::modes::Private::eXTERM_ALTBUF:
                [[fallthrough]];
        case vte::terminal::modes::Private::eXTERM_OPT_ALTBUF:
                [[fallthrough]];
        case vte::terminal::modes::Private::eXTERM_OPT_ALTBUF_SAVE_CURSOR:
                if (set) {
                        if (mode == vte::terminal::modes::Private::eXTERM_OPT_ALTBUF_SAVE_CURSOR)
                                save_cursor();

                        switch_alternate_screen();

                        /* Clear the alternate screen */
                        if (mode == vte::terminal::modes::Private::eXTERM_OPT_ALTBUF_SAVE_CURSOR)
                                clear_screen();
                } else {
                        if (mode == vte::terminal::modes::Private::eXTERM_OPT_ALTBUF &&
                            m_screen == &m_alternate_screen)
                                clear_screen();

                        switch_normal_screen();

                        if (mode == vte::terminal::modes::Private::eXTERM_OPT_ALTBUF_SAVE_CURSOR)
                                restore_cursor();
                }

                /* Reset scrollbars and repaint everything. */
                queue_adjustment_value_changed(m_screen->scroll_delta);
                set_scrollback_lines(m_scrollback_lines);
                queue_contents_changed();
                invalidate_all();
                break;

        case vte::terminal::modes::Private::eXTERM_SAVE_CURSOR:
                if (set)
                        save_cursor();
                else
                        restore_cursor();
                break;

        case vte::terminal::modes::Private::eXTERM_MOUSE_X10:
        case vte::terminal::modes::Private::eXTERM_MOUSE_VT220:
        case vte::terminal::modes::Private::eXTERM_MOUSE_VT220_HIGHLIGHT:
        case vte::terminal::modes::Private::eXTERM_MOUSE_BUTTON_EVENT:
        case vte::terminal::modes::Private::eXTERM_MOUSE_ANY_EVENT:
        case vte::terminal::modes::Private::eXTERM_MOUSE_EXT:
        case vte::terminal::modes::Private::eXTERM_MOUSE_EXT_SGR:
                update_mouse_protocol();
                break;

        case vte::terminal::modes::Private::eXTERM_FOCUS:
                if (set)
                        feed_focus_event_initial();
                break;

        case vte::terminal::modes::Private::eVTE_BIDI_BOX_MIRROR:
                _vte_debug_print(VTE_DEBUG_BIDI,
                                 "BiDi box drawing mirroring %s\n",
                                 set ? "enabled" : "disabled");
                maybe_apply_bidi_attributes(VTE_BIDI_FLAG_BOX_MIRROR);
                break;

        case vte::terminal::modes::Private::eVTE_BIDI_AUTO:
                        _vte_debug_print(VTE_DEBUG_BIDI,
                                         "BiDi dir autodetection %s\n",
                                         set ? "enabled" : "disabled");
                maybe_apply_bidi_attributes(VTE_BIDI_FLAG_AUTO);
                break;

        default:
                break;
        }
}

void
Terminal::set_mode_private(vte::parser::Sequence const& seq,
                                     bool set) noexcept
{
        auto const n_params = seq.size();
        for (unsigned int i = 0; i < n_params; i = seq.next(i)) {
                auto const param = seq.collect1(i);
                auto const mode = m_modes_private.mode_from_param(param);

                _vte_debug_print(VTE_DEBUG_MODES,
                                 "Private mode %d (%s) %s\n",
                                 param, m_modes_private.mode_to_cstring(mode),
                                 set ? "set" : "reset");

                if (mode < 0)
                        continue;

                set_mode_private(mode, set);
        }
}

void
Terminal::save_mode_private(vte::parser::Sequence const& seq,
                                      bool save) noexcept
{
        auto const n_params = seq.size();
        for (unsigned int i = 0; i < n_params; i = seq.next(i)) {
                auto const param = seq.collect1(i);
                auto const mode = m_modes_private.mode_from_param(param);

                if (mode < 0) {
                        _vte_debug_print(VTE_DEBUG_MODES,
                                         "Saving private mode %d (%s)\n",
                                         param, m_modes_private.mode_to_cstring(mode));
                        continue;
                }

                if (save) {
                        _vte_debug_print(VTE_DEBUG_MODES,
                                         "Saving private mode %d (%s) is %s\n",
                                         param, m_modes_private.mode_to_cstring(mode),
                                         m_modes_private.get(mode) ? "set" : "reset");

                        m_modes_private.push_saved(mode);
                } else {
                        bool const set = m_modes_private.pop_saved(mode);

                        _vte_debug_print(VTE_DEBUG_MODES,
                                         "Restoring private mode %d (%s) to %s\n",
                                         param, m_modes_private.mode_to_cstring(mode),
                                         set ? "set" : "reset");

                        set_mode_private(mode, set);
                }
        }
}

void
Terminal::set_character_replacement(unsigned slot)
{
        g_assert(slot < G_N_ELEMENTS(m_character_replacements));
        m_character_replacement = &m_character_replacements[slot];
}

/* Clear from the cursor position (inclusive!) to the beginning of the line. */
void
Terminal::clear_to_bol()
{
        ensure_cursor_is_onscreen();

	/* Get the data for the row which the cursor points to. */
	auto rowdata = ensure_row();
        /* Clean up Tab/CJK fragments. */
        cleanup_fragments(0, m_screen->cursor.col + 1);
	/* Clear the data up to the current column with the default
	 * attributes.  If there is no such character cell, we need
	 * to add one. */
        vte::grid::column_t i;
        for (i = 0; i <= m_screen->cursor.col; i++) {
                if (i < (glong) _vte_row_data_length (rowdata)) {
			/* Muck with the cell in this location. */
                        auto pcell = _vte_row_data_get_writable(rowdata, i);
                        *pcell = m_color_defaults;
		} else {
			/* Add new cells until we have one here. */
                        _vte_row_data_append (rowdata, &m_color_defaults);
		}
	}
        /* Repaint this row's paragraph. */
        invalidate_row_and_context(m_screen->cursor.row);

	/* We've modified the display.  Make a note of it. */
        m_text_deleted_flag = TRUE;
}

/* Clear to the right of the cursor and below the current line. */
void
Terminal::clear_below_current()
{
        ensure_cursor_is_onscreen();

	/* If the cursor is actually on the screen, clear the rest of the
	 * row the cursor is on and all of the rows below the cursor. */
        VteRowData *rowdata;
        auto i = m_screen->cursor.row;
	if (i < _vte_ring_next(m_screen->row_data)) {
		/* Get the data for the row we're clipping. */
                rowdata = _vte_ring_index_writable(m_screen->row_data, i);
                /* Clean up Tab/CJK fragments. */
                if ((glong) _vte_row_data_length(rowdata) > m_screen->cursor.col)
                        cleanup_fragments(m_screen->cursor.col, _vte_row_data_length(rowdata));
		/* Clear everything to the right of the cursor. */
		if (rowdata)
                        _vte_row_data_shrink(rowdata, m_screen->cursor.col);
	}
	/* Now for the rest of the lines. */
        for (i = m_screen->cursor.row + 1;
	     i < _vte_ring_next(m_screen->row_data);
	     i++) {
		/* Get the data for the row we're removing. */
		rowdata = _vte_ring_index_writable(m_screen->row_data, i);
		/* Remove it. */
		if (rowdata)
			_vte_row_data_shrink (rowdata, 0);
	}
	/* Now fill the cleared areas. */
        bool const not_default_bg = (m_color_defaults.attr.back() != VTE_DEFAULT_BG);

        for (i = m_screen->cursor.row;
	     i < m_screen->insert_delta + m_row_count;
	     i++) {
		/* Retrieve the row's data, creating it if necessary. */
		if (_vte_ring_contains(m_screen->row_data, i)) {
			rowdata = _vte_ring_index_writable (m_screen->row_data, i);
			g_assert(rowdata != NULL);
		} else {
			rowdata = ring_append(false);
		}
		/* Pad out the row. */
                if (not_default_bg) {
                        _vte_row_data_fill(rowdata, &m_color_defaults, m_column_count);
		}
                set_hard_wrapped(i);
                if (i > m_screen->cursor.row)
                        rowdata->attr.bidi_flags = get_bidi_flags();
	}
        /* Repaint the cleared area (might need to extend upwards). */
        invalidate_rows_and_context(m_screen->cursor.row, m_screen->insert_delta + m_row_count - 1);

	/* We've modified the display.  Make a note of it. */
	m_text_deleted_flag = TRUE;
}

/* Clear from the cursor position to the end of the line. */
void
Terminal::clear_to_eol()
{
	/* If we were to strictly emulate xterm, we'd ensure the cursor is onscreen.
	 * But due to https://bugzilla.gnome.org/show_bug.cgi?id=740789 we intentionally
	 * deviate and do instead what konsole does. This way emitting a \e[K doesn't
	 * influence the text flow, and serves as a perfect workaround against a new line
	 * getting painted with the active background color (except for a possible flicker).
	 */
	/* ensure_cursor_is_onscreen(); */

	/* Get the data for the row which the cursor points to. */
        auto rowdata = ensure_cursor();
	g_assert(rowdata != NULL);
        if ((glong) _vte_row_data_length(rowdata) > m_screen->cursor.col) {
                /* Clean up Tab/CJK fragments. */
                cleanup_fragments(m_screen->cursor.col, _vte_row_data_length(rowdata));
                /* Remove the data at the end of the array until the current column
                 * is the end of the array. */
                _vte_row_data_shrink(rowdata, m_screen->cursor.col);
		/* We've modified the display.  Make a note of it. */
		m_text_deleted_flag = TRUE;
	}
        bool const not_default_bg = (m_color_defaults.attr.back() != VTE_DEFAULT_BG);

        if (not_default_bg) {
		/* Add enough cells to fill out the row. */
                _vte_row_data_fill(rowdata, &m_color_defaults, m_column_count);
	}
        set_hard_wrapped(m_screen->cursor.row);
        /* Repaint this row's paragraph. */
        invalidate_row_and_context(m_screen->cursor.row);
}

/*
 * Terminal::set_cursor_column:
 * @col: the column. 0-based from 0 to m_column_count - 1
 *
 * Sets the cursor column to @col, clamped to the range 0..m_column_count-1.
 */
void
Terminal::set_cursor_column(vte::grid::column_t col)
{
	_vte_debug_print(VTE_DEBUG_PARSER,
                         "Moving cursor to column %ld.\n", col);
        m_screen->cursor.col = CLAMP(col, 0, m_column_count - 1);
}

void
Terminal::set_cursor_column1(vte::grid::column_t col)
{
        set_cursor_column(col - 1);
}

/*
 * Terminal::set_cursor_row:
 * @row: the row. 0-based and relative to the scrolling region
 *
 * Sets the cursor row to @row. @row is relative to the scrolling region
 * (0 if restricted scrolling is off).
 */
void
Terminal::set_cursor_row(vte::grid::row_t row)
{
        vte::grid::row_t start_row, end_row;
        if (m_modes_private.DEC_ORIGIN() &&
            m_scrolling_restricted) {
                start_row = m_scrolling_region.start;
                end_row = m_scrolling_region.end;
        } else {
                start_row = 0;
                end_row = m_row_count - 1;
        }
        row += start_row;
        row = CLAMP(row, start_row, end_row);

        m_screen->cursor.row = row + m_screen->insert_delta;
}

void
Terminal::set_cursor_row1(vte::grid::row_t row)
{
        set_cursor_row(row - 1);
}

/*
 * Terminal::get_cursor_row:
 *
 * Returns: the relative cursor row, 0-based and relative to the scrolling region
 * if set (regardless of origin mode).
 */
vte::grid::row_t
Terminal::get_cursor_row_unclamped() const
{
        auto row = m_screen->cursor.row - m_screen->insert_delta;
        /* Note that we do NOT check DEC_ORIGIN mode here! */
        if (m_scrolling_restricted) {
                row -= m_scrolling_region.start;
        }
        return row;
}

vte::grid::column_t
Terminal::get_cursor_column_unclamped() const
{
        return m_screen->cursor.col;
}

/*
 * Terminal::set_cursor_coords:
 * @row: the row. 0-based and relative to the scrolling region
 * @col: the column. 0-based from 0 to m_column_count - 1
 *
 * Sets the cursor row to @row. @row is relative to the scrolling region
 * (0 if restricted scrolling is off).
 *
 * Sets the cursor column to @col, clamped to the range 0..m_column_count-1.
 */
void
Terminal::set_cursor_coords(vte::grid::row_t row,
                                      vte::grid::column_t column)
{
        set_cursor_column(column);
        set_cursor_row(row);
}

void
Terminal::set_cursor_coords1(vte::grid::row_t row,
                                      vte::grid::column_t column)
{
        set_cursor_column1(column);
        set_cursor_row1(row);
}

/* Delete a character at the current cursor position. */
void
Terminal::delete_character()
{
	VteRowData *rowdata;
	long col;

        ensure_cursor_is_onscreen();

        if (_vte_ring_next(m_screen->row_data) > m_screen->cursor.row) {
		long len;
		/* Get the data for the row which the cursor points to. */
                rowdata = _vte_ring_index_writable(m_screen->row_data, m_screen->cursor.row);
		g_assert(rowdata != NULL);
                col = m_screen->cursor.col;
		len = _vte_row_data_length (rowdata);

                bool const not_default_bg = (m_color_defaults.attr.back() != VTE_DEFAULT_BG);
                if (not_default_bg) {
                        _vte_row_data_fill(rowdata, &basic_cell, m_column_count);
                        len = m_column_count;
                }

		/* Remove the column. */
		if (col < len) {
                        /* Clean up Tab/CJK fragments. */
                        cleanup_fragments(col, col + 1);
			_vte_row_data_remove (rowdata, col);

                        if (not_default_bg) {
                                _vte_row_data_fill(rowdata, &m_color_defaults, m_column_count);
                                len = m_column_count;
			}
                        set_hard_wrapped(m_screen->cursor.row);
                        /* Repaint this row's paragraph. */
                        invalidate_row_and_context(m_screen->cursor.row);
		}
	}

	/* We've modified the display.  Make a note of it. */
        m_text_deleted_flag = TRUE;
}

void
Terminal::move_cursor_down(vte::grid::row_t rows)
{
        rows = CLAMP(rows, 1, m_row_count);

        // FIXMEchpe why not do this afterwards?
        ensure_cursor_is_onscreen();

        vte::grid::row_t end;
        // FIXMEchpe why not check DEC_ORIGIN here?
        if (m_scrolling_restricted && m_screen->cursor.row <= m_screen->insert_delta + m_scrolling_region.end) {
                end = m_screen->insert_delta + m_scrolling_region.end;
	} else {
                end = m_screen->insert_delta + m_row_count - 1;
	}

        m_screen->cursor.row = MIN(m_screen->cursor.row + rows, end);
}

void
Terminal::erase_characters(long count,
                           bool use_basic)
{
	VteCell *cell;
	long col, i;

        ensure_cursor_is_onscreen();

	/* Clear out the given number of characters. */
	auto rowdata = ensure_row();
        if (_vte_ring_next(m_screen->row_data) > m_screen->cursor.row) {
		g_assert(rowdata != NULL);
                /* Clean up Tab/CJK fragments. */
                cleanup_fragments(m_screen->cursor.col, m_screen->cursor.col + count);
		/* Write over the characters.  (If there aren't enough, we'll
		 * need to create them.) */
                _vte_row_data_fill (rowdata, &basic_cell, m_screen->cursor.col);
		for (i = 0; i < count; i++) {
                        col = m_screen->cursor.col + i;
			if (col >= 0) {
				if (col < (glong) _vte_row_data_length (rowdata)) {
					/* Replace this cell with the current
					 * defaults. */
					cell = _vte_row_data_get_writable (rowdata, col);
                                        *cell = use_basic ? basic_cell : m_color_defaults;
				} else {
					/* Add new cells until we have one here. */
                                        _vte_row_data_fill (rowdata, use_basic ? &basic_cell : &m_color_defaults, col + 1);
				}
			}
		}
                /* Repaint this row's paragraph. */
                invalidate_row_and_context(m_screen->cursor.row);
	}

	/* We've modified the display.  Make a note of it. */
        m_text_deleted_flag = TRUE;
}

/* Insert a blank character. */
void
Terminal::insert_blank_character()
{
        ensure_cursor_is_onscreen();

        auto save = m_screen->cursor;
        insert_char(' ', true, true);
        m_screen->cursor = save;
}

void
Terminal::move_cursor_backward(vte::grid::column_t columns)
{
        ensure_cursor_is_onscreen();

        auto col = get_cursor_column_unclamped();
        columns = CLAMP(columns, 1, col);
        set_cursor_column(col - columns);
}

void
Terminal::move_cursor_forward(vte::grid::column_t columns)
{
        columns = CLAMP(columns, 1, m_column_count);

        ensure_cursor_is_onscreen();

        /* The cursor can be further to the right, don't move in that case. */
        auto col = get_cursor_column_unclamped();
        if (col < m_column_count) {
		/* There's room to move right. */
                set_cursor_column(col + columns);
	}
}

void
Terminal::line_feed()
{
        ensure_cursor_is_onscreen();
        cursor_down(true);
        maybe_apply_bidi_attributes(VTE_BIDI_FLAG_ALL);
}

void
Terminal::move_cursor_tab_backward(int count)
{
        if (count == 0)
                return;

        auto const newcol = m_tabstops.get_previous(get_cursor_column(), count, 0);
        set_cursor_column(newcol);
}

void
Terminal::move_cursor_tab_forward(int count)
{
        if (count == 0)
                return;

        auto const col = get_cursor_column();

	/* Find the next tabstop, but don't go beyond the end of the line */
        int const newcol = m_tabstops.get_next(col, count, m_column_count - 1);

	/* Make sure we don't move cursor back (see bug #340631) */
        // FIXMEchpe how could this happen!?
	if (col >= newcol)
                return;

        /* Smart tab handling: bug 353610
         *
         * If we currently don't have any cells in the space this
         * tab creates, we try to make the tab character copyable,
         * by appending a single tab char with lots of fragment
         * cells following it.
         *
         * Otherwise, just append empty cells that will show up
         * as a space each.
         */

        VteRowData *rowdata = ensure_row();
        auto const old_len = _vte_row_data_length (rowdata);
        _vte_row_data_fill (rowdata, &basic_cell, newcol);

        /* Insert smart tab if there's nothing in the line after
         * us, not even empty cells (with non-default background
         * color for example).
         *
         * Notable bugs here: 545924, 597242, 764330
         */
        if (col >= old_len && (newcol - col) <= VTE_TAB_WIDTH_MAX) {
                glong i;
                VteCell *cell = _vte_row_data_get_writable (rowdata, col);
                VteCell tab = *cell;
                tab.attr.set_columns(newcol - col);
                tab.c = '\t';
                /* Save tab char */
                *cell = tab;
                /* And adjust the fragments */
                for (i = col + 1; i < newcol; i++) {
                        cell = _vte_row_data_get_writable (rowdata, i);
                        cell->c = '\t';
                        cell->attr.set_columns(1);
                        cell->attr.set_fragment(true);
                }
        }

        /* Repaint the cursor. */
        invalidate_row(m_screen->cursor.row);
        m_screen->cursor.col = newcol;
}

void
Terminal::move_cursor_up(vte::grid::row_t rows)
{
        // FIXMEchpe allow 0 as no-op?
        rows = CLAMP(rows, 1, m_row_count);

        //FIXMEchpe why not do this afterward?
        ensure_cursor_is_onscreen();

        vte::grid::row_t start;
        //FIXMEchpe why not check DEC_ORIGIN mode here?
        if (m_scrolling_restricted && m_screen->cursor.row >= m_screen->insert_delta + m_scrolling_region.start) {
                start = m_screen->insert_delta + m_scrolling_region.start;
	} else {
		start = m_screen->insert_delta;
	}

        m_screen->cursor.row = MAX(m_screen->cursor.row - rows, start);
}

/*
 * Parse parameters of SGR 38, 48 or 58, starting at @index within @seq.
 * Returns %true if @seq contained colour parameters at @index, or %false otherwise.
 * In each case, @idx is set to last consumed parameter,
 * and the colour is returned in @color.
 *
 * The format looks like:
 * - 256 color indexed palette:
 *   - ^[[38:5:INDEXm  (de jure standard: ITU-T T.416 / ISO/IEC 8613-6; we also allow and ignore further parameters)
 *   - ^[[38;5;INDEXm  (de facto standard, understood by probably all terminal emulators that support 256 colors)
 * - true colors:
 *   - ^[[38:2:[id]:RED:GREEN:BLUE[:...]m  (de jure standard: ITU-T T.416 / ISO/IEC 8613-6)
 *   - ^[[38:2:RED:GREEN:BLUEm             (common misinterpretation of the standard, FIXME: stop supporting it at some point)
 *   - ^[[38;2;RED;GREEN;BLUEm             (de facto standard, understood by probably all terminal emulators that support true colors)
 * See bugs 685759 and 791456 for details.
 */
template<unsigned int redbits, unsigned int greenbits, unsigned int bluebits>
bool
Terminal::seq_parse_sgr_color(vte::parser::Sequence const& seq,
                                        unsigned int &idx,
                                        uint32_t& color) const noexcept
{
        /* Note that we don't have to check if the index is after the end of
         * the parameters list, since dereferencing is safe and returns -1.
         */

        if (seq.param_nonfinal(idx)) {
                /* Colon version */
                switch (seq.param(++idx)) {
                case VTE_SGR_COLOR_SPEC_RGB: {
                        auto const n = seq.next(idx) - idx;
                        if (n < 4)
                                return false;
                        if (n > 4) {
                                /* Consume a colourspace parameter; it must be default */
                                if (!seq.param_default(++idx))
                                        return false;
                        }

                        int red = seq.param(++idx);
                        int green = seq.param(++idx);
                        int blue = seq.param(++idx);
                        if ((red & 0xff) != red ||
                            (green & 0xff) != green ||
                            (blue & 0xff) != blue)
                                return false;

                        color = VTE_RGB_COLOR(redbits, greenbits, bluebits, red, green, blue);
                        return true;
                }
                case VTE_SGR_COLOR_SPEC_LEGACY: {
                        auto const n = seq.next(idx) - idx;
                        if (n < 2)
                                return false;

                        int v = seq.param(++idx);
                        if (v < 0 || v >= 256)
                                return false;

                        color = (uint32_t)v;
                        return true;
                }
                }
        } else {
                /* Semicolon version */

                idx = seq.next(idx);
                switch (seq.param(idx)) {
                case VTE_SGR_COLOR_SPEC_RGB: {
                        /* Consume 3 more parameters */
                        idx = seq.next(idx);
                        int red = seq.param(idx);
                        idx = seq.next(idx);
                        int green = seq.param(idx);
                        idx = seq.next(idx);
                        int blue = seq.param(idx);

                        if ((red & 0xff) != red ||
                            (green & 0xff) != green ||
                            (blue & 0xff) != blue)
                                return false;

                        color = VTE_RGB_COLOR(redbits, greenbits, bluebits, red, green, blue);
                        return true;
                }
                case VTE_SGR_COLOR_SPEC_LEGACY: {
                        /* Consume 1 more parameter */
                        idx = seq.next(idx);
                        int v = seq.param(idx);

                        if ((v & 0xff) != v)
                                return false;

                        color = (uint32_t)v;
                        return true;
                }
                }
        }

        return false;
}

void
Terminal::erase_in_display(vte::parser::Sequence const& seq)
{
        /* We don't implement the protected attribute, so we can ignore selective:
         * bool selective = (seq.command() == VTE_CMD_DECSED);
         */

        switch (seq.collect1(0)) {
        case -1: /* default */
	case 0:
		/* Clear below the current line. */
                clear_below_current();
		break;
	case 1:
		/* Clear above the current line. */
                clear_above_current();
		/* Clear everything to the left of the cursor, too. */
		/* FIXME: vttest. */
                clear_to_bol();
		break;
	case 2:
		/* Clear the entire screen. */
                clear_screen();
		break;
        case 3:
                /* Drop the scrollback. */
                drop_scrollback();
                break;
	default:
		break;
	}
	/* We've modified the display.  Make a note of it. */
        m_text_deleted_flag = TRUE;
}

void
Terminal::erase_in_line(vte::parser::Sequence const& seq)
{
        /* We don't implement the protected attribute, so we can ignore selective:
         * bool selective = (seq.command() == VTE_CMD_DECSEL);
         */

        switch (seq.collect1(0)) {
        case -1: /* default */
	case 0:
		/* Clear to end of the line. */
                clear_to_eol();
		break;
	case 1:
		/* Clear to start of the line. */
                clear_to_bol();
		break;
	case 2:
		/* Clear the entire line. */
                clear_current_line();
		break;
	default:
		break;
	}
	/* We've modified the display.  Make a note of it. */
        m_text_deleted_flag = TRUE;
}

void
Terminal::insert_lines(vte::grid::row_t param)
{
        vte::grid::row_t start, end, i;

	/* Find the region we're messing with. */
        auto row = m_screen->cursor.row;
        if (m_scrolling_restricted) {
                start = m_screen->insert_delta + m_scrolling_region.start;
                end = m_screen->insert_delta + m_scrolling_region.end;
	} else {
                start = m_screen->insert_delta;
                end = m_screen->insert_delta + m_row_count - 1;
	}

        /* Don't do anything if the cursor is outside of the scrolling region: DEC STD 070 & bug #199. */
        if (m_screen->cursor.row < start || m_screen->cursor.row > end)
                return;

	/* Only allow to insert as many lines as there are between this row
         * and the end of the scrolling region. See bug #676090.
         */
        auto limit = end - row + 1;
        param = MIN (param, limit);

	for (i = 0; i < param; i++) {
		/* Clear a line off the end of the region and add one to the
		 * top of the region. */
                ring_remove(end);
                ring_insert(row, true);
	}

        /* Set the boundaries to hard wrapped where we tore apart the contents.
         * Need to do it after scrolling down, for the end row to be the desired one. */
        set_hard_wrapped(row - 1);
        set_hard_wrapped(end);

        m_screen->cursor.col = 0;

        /* Repaint the affected lines. No need to extend, set_hard_wrapped() took care of
         * invalidating the context lines if necessary. */
        invalidate_rows(row, end);
	/* Adjust the scrollbars if necessary. */
        adjust_adjustments();
	/* We've modified the display.  Make a note of it. */
        m_text_inserted_flag = TRUE;
}

void
Terminal::delete_lines(vte::grid::row_t param)
{
        vte::grid::row_t start, end, i;

	/* Find the region we're messing with. */
        auto row = m_screen->cursor.row;
        if (m_scrolling_restricted) {
                start = m_screen->insert_delta + m_scrolling_region.start;
                end = m_screen->insert_delta + m_scrolling_region.end;
	} else {
                start = m_screen->insert_delta;
                end = m_screen->insert_delta + m_row_count - 1;
	}

        /* Don't do anything if the cursor is outside of the scrolling region: DEC STD 070 & bug #199. */
        if (m_screen->cursor.row < start || m_screen->cursor.row > end)
                return;

        /* Set the boundaries to hard wrapped where we're about to tear apart the contents.
         * Need to do it before scrolling up, for the end row to be the desired one. */
        set_hard_wrapped(row - 1);
        set_hard_wrapped(end);

        /* Only allow to delete as many lines as there are between this row
         * and the end of the scrolling region. See bug #676090.
         */
        auto limit = end - row + 1;
        param = MIN (param, limit);

	/* Clear them from below the current cursor. */
	for (i = 0; i < param; i++) {
		/* Insert a line at the end of the region and remove one from
		 * the top of the region. */
                ring_remove(row);
                ring_insert(end, true);
	}
        m_screen->cursor.col = 0;

        /* Repaint the affected lines. No need to extend, set_hard_wrapped() took care of
         * invalidating the context lines if necessary. */
        invalidate_rows(row, end);
	/* Adjust the scrollbars if necessary. */
        adjust_adjustments();
	/* We've modified the display.  Make a note of it. */
        m_text_deleted_flag = TRUE;
}

bool
Terminal::get_osc_color_index(int osc,
                                        int value,
                                        int& index) const noexcept
{
        if (value < 0)
                return false;

        if (osc == VTE_OSC_XTERM_SET_COLOR ||
            osc == VTE_OSC_XTERM_RESET_COLOR) {
                if (value < VTE_DEFAULT_FG) {
                        index = value;
                        return true;
                }

                index = value - VTE_DEFAULT_FG;
        } else {
                index = value;
        }

        /* Translate OSC 5 numbers to color index.
         *
         * We return -1 for known but umimplemented special colors
         * so that we can send a dummy reply when queried.
         */
        switch (index) {
        case 0: index = VTE_BOLD_FG; return true; /* Bold */
        case 1: index = -1; return true; /* Underline */
        case 2: index = -1; return true; /* Blink */
        case 3: index = -1; return true; /* Reverse */
        case 4: index = -1; return true; /* Italic */
        default: return false;
        }
}

void
Terminal::set_color(vte::parser::Sequence const& seq,
                              vte::parser::StringTokeniser::const_iterator& token,
                              vte::parser::StringTokeniser::const_iterator const& endtoken,
                              int osc) noexcept
{
        while (token != endtoken) {
                int value;
                bool has_value = token.number(value);

                if (++token == endtoken)
                        break;

                int index;
                if (!has_value ||
                    !get_osc_color_index(osc, value, index)) {
                        ++token;
                        continue;
                }

                set_color_index(seq, token, endtoken, value, index, -1, osc);
                ++token;
        }
}

void
Terminal::set_color_index(vte::parser::Sequence const& seq,
                                    vte::parser::StringTokeniser::const_iterator& token,
                                    vte::parser::StringTokeniser::const_iterator const& endtoken,
                                    int number,
                                    int index,
                                    int index_fallback,
                                    int osc) noexcept
{
        auto const str = *token;

        if (str == "?"s) {
                vte::color::rgb color{0, 0, 0};
                if (index != -1) {
                        auto const* c = get_color(index);
                        if (c == nullptr && index_fallback != -1)
                                c = get_color(index_fallback);
                        if (c != nullptr)
                                color = *c;
                }

                if (number != -1)
                        reply(seq, VTE_REPLY_OSC, {}, "%d;%d;rgb:%04x/%04x/%04x",
                              osc, number, color.red, color.green, color.blue);
                else
                        reply(seq, VTE_REPLY_OSC, {}, "%d;rgb:%04x/%04x/%04x",
                              osc, color.red, color.green, color.blue);
        } else {
                vte::color::rgb color;

                if (index != -1 &&
                    color.parse(str.data())) {
                        set_color(index, VTE_COLOR_SOURCE_ESCAPE, color);
                }
        }
}

void
Terminal::set_special_color(vte::parser::Sequence const& seq,
                                      vte::parser::StringTokeniser::const_iterator& token,
                                      vte::parser::StringTokeniser::const_iterator const& endtoken,
                                      int index,
                                      int index_fallback,
                                      int osc) noexcept
{
        if (token == endtoken)
                return;

        set_color_index(seq, token, endtoken, -1, index, index_fallback, osc);
}

void
Terminal::reset_color(vte::parser::Sequence const& seq,
                                vte::parser::StringTokeniser::const_iterator& token,
                                vte::parser::StringTokeniser::const_iterator const& endtoken,
                                int osc) noexcept
{
        /* Empty param? Reset all */
        if (token == endtoken ||
            token.size_remaining() == 0) {
                if (osc == VTE_OSC_XTERM_RESET_COLOR) {
                        for (unsigned int idx = 0; idx < VTE_DEFAULT_FG; idx++)
                                reset_color(idx, VTE_COLOR_SOURCE_ESCAPE);
                }

                reset_color(VTE_BOLD_FG, VTE_COLOR_SOURCE_ESCAPE);
                /* Add underline/blink/reverse/italic here if/when implemented */

                return;
        }

        while (token != endtoken) {
                int value;
                if (!token.number(value)) {
                        ++token;
                        continue;
                }

                int index;
                if (get_osc_color_index(osc, value, index) &&
                    index != -1) {
                        reset_color(index, VTE_COLOR_SOURCE_ESCAPE);
                }

                ++token;
        }
}

void
Terminal::set_current_directory_uri(vte::parser::Sequence const& seq,
                                              vte::parser::StringTokeniser::const_iterator& token,
                                              vte::parser::StringTokeniser::const_iterator const& endtoken) noexcept
{
        std::string uri;
        if (token != endtoken && token.size_remaining() > 0) {
                uri = token.string_remaining();

                auto filename = g_filename_from_uri(uri.data(), nullptr, nullptr);
                if (filename != nullptr) {
                        g_free(filename);
                } else {
                        /* invalid URI */
                        uri.clear();
                }
        }

        m_current_directory_uri_pending.swap(uri);
        m_pending_changes |= vte::to_integral(PendingChanges::CWD);
}

void
Terminal::set_current_file_uri(vte::parser::Sequence const& seq,
                                         vte::parser::StringTokeniser::const_iterator& token,
                                         vte::parser::StringTokeniser::const_iterator const& endtoken) noexcept

{
        std::string uri;
        if (token != endtoken && token.size_remaining() > 0) {
                uri = token.string_remaining();

                auto filename = g_filename_from_uri(uri.data(), nullptr, nullptr);
                if (filename != nullptr) {
                        g_free(filename);
                } else {
                        /* invalid URI */
                        uri.clear();
                }
        }

        m_current_file_uri_pending.swap(uri);
        m_pending_changes |= vte::to_integral(PendingChanges::CWF);
}

void
Terminal::set_current_hyperlink(vte::parser::Sequence const& seq,
                                          vte::parser::StringTokeniser::const_iterator& token,
                                          vte::parser::StringTokeniser::const_iterator const& endtoken) noexcept
{
        if (token == endtoken)
                return; // FIXMEchpe or should we treat this as a reset?

        /* Handle OSC 8 hyperlinks.
         * See bug 779734 and https://gist.github.com/egmontkob/eb114294efbcd5adb1944c9f3cb5feda
         */

        if (!m_allow_hyperlink)
                return;

        /* The hyperlink, as we carry around and store in the streams, is "id;uri" */
        std::string hyperlink;

        /* First, find the ID */
        auto tokenstr = *token;
        vte::parser::StringTokeniser subtokeniser{tokenstr, ':'};
        for (auto subtoken : subtokeniser) {
                auto const len = subtoken.size();
                if (len < 3)
                        continue;

                if (subtoken[0] != 'i' || subtoken[1] != 'd' || subtoken[2] != '=')
                        continue;

                if (len > 3 + VTE_HYPERLINK_ID_LENGTH_MAX) {
                        _vte_debug_print (VTE_DEBUG_HYPERLINK, "Overlong \"id\" ignored: \"%s\"\n",
                                          subtoken.data());
                        break;
                }

                hyperlink = subtoken.substr(3);
                break;
        }

        if (hyperlink.size() == 0) {
                /* Automatically generate a unique ID string. The colon makes sure
                 * it cannot conflict with an explicitly specified one.
                 */
                char idbuf[24];
                auto len = g_snprintf(idbuf, sizeof(idbuf), ":%ld", m_hyperlink_auto_id++);
                hyperlink.append(idbuf, len);
                _vte_debug_print (VTE_DEBUG_HYPERLINK, "Autogenerated id=\"%s\"\n", hyperlink.data());
        }

        /* Now get the URI */
        if (++token == endtoken)
                return; // FIXMEchpe or should we treat this the same as 0-length URI ?

        hyperlink.push_back(';');
        guint idx;
        auto const len = token.size_remaining();
        if (len > 0 && len <= VTE_HYPERLINK_URI_LENGTH_MAX) {
                token.append_remaining(hyperlink);

                _vte_debug_print (VTE_DEBUG_HYPERLINK, "OSC 8: id;uri=\"%s\"\n", hyperlink.data());

                idx = _vte_ring_get_hyperlink_idx(m_screen->row_data, hyperlink.data());
        } else {
                if (G_UNLIKELY(len > VTE_HYPERLINK_URI_LENGTH_MAX))
                        _vte_debug_print (VTE_DEBUG_HYPERLINK, "Overlong URI ignored (len %" G_GSIZE_FORMAT ")\n", len);

                /* idx = 0; also remove the previous current_idx so that it can be GC'd now. */
                idx = _vte_ring_get_hyperlink_idx(m_screen->row_data, nullptr);
        }

        m_defaults.attr.hyperlink_idx = idx;
}

/*
 * Command Handlers
 * This is the unofficial documentation of all the VTE_CMD_* definitions.
 * Each handled command has a separate function with an extensive comment on
 * the semantics of the command.
 * Note that many semantics are unknown and need to be verified. This is mostly
 * about error-handling, though. Applications rarely rely on those features.
 */

void
Terminal::NONE(vte::parser::Sequence const& seq)
{
}

void
Terminal::GRAPHIC(vte::parser::Sequence const& seq)
{
#if 0
        struct vte_char ch = VTE_CHAR_NULL;

        if (screen->state.cursor_x + 1 == screen->page->width
            && screen->flags & VTE_FLAG_PENDING_WRAP
            && screen->state.auto_wrap) {
                screen_cursor_down(screen, 1, true);
                screen_cursor_set(screen, 0, screen->state.cursor_y);
        }

        screen_cursor_clear_wrap(screen);

        ch = vte_char_merge(ch, screen_map(screen, seq->terminator));
        vte_page_write(screen->page,
                          screen->state.cursor_x,
                          screen->state.cursor_y,
                          ch,
                          1,
                          &screen->state.attr,
                          screen->age,
                          false);

        if (screen->state.cursor_x + 1 == screen->page->width)
                screen->flags |= VTE_FLAG_PENDING_WRAP;
        else
                screen_cursor_right(screen, 1);

        return 0;
#endif

        insert_char(seq.terminator(), false, false);
}

void
Terminal::ACK(vte::parser::Sequence const& seq)
{
        /*
         * ACK - acknowledge
         *
         * References: ECMA-48 § 8.3.1
         *             ECMA-16 § 3.1.6
         */

        m_bell_pending = true;
}

void
Terminal::ACS(vte::parser::Sequence const& seq)
{
        /* ACS - announce-code-structure
         *
         * The final byte of the sequence identifies the facility number
         * from 1 to 62 starting with 4/01.
         * DEC uses some final characters in the 3/00..3/15 range for
         * private purposes.
         *
         * References: ECMA-35 § 15.2
         *             DEC VT525
         *             DEC PPLV2
         */

        /* Since we mostly don't implement ECMA-35 anymore, we can mostly ignore this */

        switch (int(seq.terminator()) - 0x40) {
        case -10: /* '6' */
                /* S7C1R/DECTC1 - truncate C1 controls
                 *
                 * Masks the high bit from C1 controls and then
                 * processes them as if received like that.
                 *
                 * References: DEC PPLV2
                 */
                 break;
        case -9: /* '7' */
                /* S8C1R/DECAC1 - accept C1 controls
                 *
                 * Accept both C0 and C1 controls.
                 *
                 * References: DEC PPLV2
                 */
                 break;
        case 6:
                /*
                 * This causes the terminal to start sending C1 controls as 7bit
                 * sequences instead of 8bit C1 controls.
                 * This is ignored if the terminal is below level-2 emulation mode
                 * (VT100 and below), the terminal already sends 7bit controls then.
                 *
                 * References: ECMA-35
                 *             VT525
                 */
#if 0
                if (screen->conformance_level > VTE_CONFORMANCE_LEVEL_VT100)
                        screen->flags |= VTE_FLAG_7BIT_MODE;
#endif
                break;

        case 7:
                /*
                 * This causes the terminal to start sending C1 controls as 8bit C1
                 * control instead of 7bit sequences.
                 * This is ignored if the terminal is below level-2 emulation mode
                 * (VT100 and below). The terminal always sends 7bit controls in those
                 * modes.
                 *
                 * References: ECMA-35
                 *             VT525
                 */
#if 0
                if (screen->conformance_level > VTE_CONFORMANCE_LEVEL_VT100)
                        screen->flags &= ~VTE_FLAG_7BIT_MODE;
#endif
                break;

        case 12:
                /* Use Level 1 of ECMA-43
                 *
                 * Probably not worth implementing.
                 */
                break;
        case 13:
                /* Use Level 2 of ECMA-43
                 *
                 * Probably not worth implementing.
                 *
                 * On a VTxxx, both levels 1 and 2 designate as follows:
                 * G0 = ASCII (IR #6)
                 * G1 = ISO_LATIN1_SUPPLEMENTAL
                 * with G0 mapped to GL, G1 to GR.
                 *
                 * References: VT525
                 */
                break;
        case 14:
                /* Use Level 3 of ECMA-43
                 *
                 * Probably not worth implementing.
                 *
                 * On a VTxxx, this designates as follows:
                 * G0 = ASCII (IR #6)
                 * with G0 mapped to GL.
                 *
                 *
                 * References: VT525
                 */
                break;
        }
}

void
Terminal::BEL(vte::parser::Sequence const& seq)
{
        /*
         * BEL - sound bell tone
         * This command should trigger an acoustic bell.
         *
         * References: ECMA-48 § 8.3.3
         */

        m_bell_pending = true;
}

void
Terminal::BPH(vte::parser::Sequence const& seq)
{
        /*
         * BPH - break permitted here
         *
         * References: ECMA-48 § 8.3.4
         *
         * Not worth implementing.
         */
}

void
Terminal::BS(vte::parser::Sequence const& seq)
{
        /*
         * BS - backspace
         * Move cursor one cell to the left. If already at the left margin,
         * nothing happens.
         *
         * References: ECMA-48 § 8.3.5
         */

#if 0
        screen_cursor_clear_wrap(screen);
        screen_cursor_left(screen, 1);
#endif

        ensure_cursor_is_onscreen();

        if (m_screen->cursor.col > 0) {
		/* There's room to move left, so do so. */
                m_screen->cursor.col--;
	}
}

void
Terminal::CBT(vte::parser::Sequence const& seq)
{
        /*
         * CBT - cursor-backward-tabulation
         * Move the cursor @args[0] tabs backwards (to the left). The
         * current cursor cell, in case it's a tab, is not counted.
         * Furthermore, the cursor cannot be moved beyond position 0 and
         * it will stop there.
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.7
         */
#if 0
        screen_cursor_clear_wrap(screen);
#endif

        move_cursor_tab_backward(seq.collect1(0, 1));
}

void
Terminal::CCH(vte::parser::Sequence const& seq)
{
        /*
         * CCH - cancel character
         * Indicates that the CCH and the preceding graphic character
         * (including SPACE (2/0)) in the data stream should be ignored.
         * If CCH is not preceded by a graphic character but by a
         * control function instead, CCH is ignored.
         *
         * References: ECMA-48 § 8.3.8
         *
         * Not worth implementing.
         */
}

void
Terminal::CHA(vte::parser::Sequence const& seq)
{
        /*
         * CHA - cursor-horizontal-absolute
         * Move the cursor to position @args[0] in the current line
         * (presentation).
         * The cursor cannot be moved beyond the rightmost cell; it will
         * stop there.
         *
         * Arguments:
         *   args[0]: column
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.9
         */

#if 0
        unsigned int pos = 1;

        if (seq->args[0] > 0)
                pos = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_set(screen, pos - 1, screen->state.cursor_y);
#endif

        auto value = seq.collect1(0, 1, 1, m_column_count);
        set_cursor_column1(value);
}

void
Terminal::CHT(vte::parser::Sequence const& seq)
{
        /*
         * CHT - cursor-horizontal-forward-tabulation
         * Move the cursor @args[0] tabs forward (to the right) (presentation).
         * The current cursor cell, in case it's a tab, is not counted.
         * Furthermore, the cursor cannot be moved beyond the rightmost cell
         * and will stop there.
         *
         * Arguments:
         *   args[0]: count
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.10
         */
#if 0
        screen_cursor_clear_wrap(screen);
#endif

        move_cursor_tab_forward(seq.collect1(0, 1));
}

void
Terminal::CMD(vte::parser::Sequence const& seq)
{
        /*
         * CMD - coding method delimiter
         *
         * References: ECMA-35 § 15.3
         *             ECMA-48 § 8.3.11
         *
         * Not worth implementing.
         */
}

void
Terminal::CNL(vte::parser::Sequence const& seq)
{
        /*
         * CNL - cursor-next-line
         * Move the cursor @args[0] lines down.
         *
         * TODO: Does this stop at the bottom or cause a scroll-up?
         *
         * Arguments:
         *   args[0]: number of lines
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 §8.3.12
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_down(screen, num, false);
#endif

        set_cursor_column1(1);

        auto value = seq.collect1(0, 1);
        move_cursor_down(value);
}

void
Terminal::CPL(vte::parser::Sequence const& seq)
{
        /*
         * CPL - cursor-preceding-line
         * Move the cursor @args[0] lines up, without scrolling.
         *
         * Arguments:
         *   args[0]: number of lines
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.13
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_up(screen, num, false);
#endif

        set_cursor_column(0);

        auto const value = seq.collect1(0, 1);
        move_cursor_up(value);
}

void
Terminal::CR(vte::parser::Sequence const& seq)
{
        /*
         * CR - carriage-return
         * Move the cursor to the left margin on the current line.
         *
         * References: ECMA-48 § 8.3.15
         */
#if 0
        screen_cursor_clear_wrap(screen);
        screen_cursor_set(screen, 0, screen->state.cursor_y);
#endif

        set_cursor_column(0);
}

void
Terminal::CTC(vte::parser::Sequence const& seq)
{
        /*
         * CTC - cursor tabulation control
         * Set/clear tabstops.
         *
         * For the cases @args[0] = 0, 2, 4, the effect depends on TSM mode.
         *
         * References: ECMA-48 § 8.3.17
         */

        switch (seq.collect1(0)) {
        case -1:
        case 0:
                /* Set tabstop at the current cursor position */
                m_tabstops.set(get_cursor_column());
                break;

        case 1:
                /* Sets line tabstop in the ative line (presentation) */
                break;

        case 2:
                /* Clear tabstop at the current cursor position */
                m_tabstops.unset(get_cursor_column());
                break;

        case 3:
                /* Clear line tabstop in the active line */
                break;

        case 4:
                /* Clear all tabstops in the active line */
                [[fallthrough]];
        case 5:
                /* Clear all tabstops */
                m_tabstops.clear();
                break;

        case 6:
                /* Clear all line tabstops */
                break;

        default:
                break;
        }
}

void
Terminal::CUB(vte::parser::Sequence const& seq)
{
        /*
         * CUB - cursor-backward
         * Move the cursor @args[0] positions to the left. The cursor stops
         * at the left-most position. (presentation)
         *
         * Arguments:
         *   args[0]: number of positions
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.18
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_left(screen, num);
#endif

        auto value = seq.collect1(0, 1);
        move_cursor_backward(value);
}

void
Terminal::CUD(vte::parser::Sequence const& seq)
{
        /*
         * CUD - cursor-down
         * Move the cursor @args[0] positions down. The cursor stops at the
         * bottom margin. If it was already moved further, it stops at the
         * bottom line. (presentation)
         *
         * Arguments:
         *   args[0]: number of positions
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.19
         *             DEC STD 070 page 5-43
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_down(screen, num, false);
#endif

        auto value = seq.collect1(0, 1);
        move_cursor_down(value);
}

void
Terminal::CUF(vte::parser::Sequence const& seq)
{
        /*
         * CUF -cursor-forward
         * Move the cursor @args[0] positions to the right. The cursor stops
         * at the right-most position. (presentation)
         *
         * Arguments:
         *   args[0]: number of positions
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.20
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_right(screen, num);
#endif

        auto value = seq.collect1(0, 1);
        move_cursor_forward(value);
}

void
Terminal::CUP(vte::parser::Sequence const& seq)
{
        /*
         * CUP - cursor-position
         * Moves the cursor to position @args[1] x @args[0]. If either is 0, it
         * is treated as 1. The positions are subject to the origin-mode and
         * clamped to the addressable width/height. (presentation)
         *
         * Arguments:
         *   args[0]: line
         *   args[0]: column
         *
         * Defaults:
         *   args[0]: 1
         *   args[1]: 1
         *
         * References: ECMA-48 § 8.3.21
         */
#if 0
        unsigned int x = 1, y = 1;

        if (seq->args[0] > 0)
                y = seq->args[0];
        if (seq->args[1] > 0)
                x = seq->args[1];

        screen_cursor_clear_wrap(screen);
        screen_cursor_set_rel(screen, x - 1, y - 1);
#endif

        /* The first is the row, the second is the column. */
        auto rowvalue = seq.collect1(0, 1, 1, m_row_count);
        auto colvalue = seq.collect1(seq.next(0), 1, 1, m_column_count);
        set_cursor_coords1(rowvalue, colvalue);
}

void
Terminal::CUU(vte::parser::Sequence const& seq)
{
        /*
         * CUU - cursor-up
         * Move the cursor @args[0] positions up. The cursor stops at the
         * top margin. If it was already moved further, it stops at the
         * top line. (presentation)
         *
         * Arguments:
         *   args[0]: number of positions
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.22
         *             DEC STD 070 page 5-41
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_up(screen, num, false);
#endif

        auto const value = seq.collect1(0, 1);
        move_cursor_up(value);
}

void
Terminal::CVT(vte::parser::Sequence const& seq)
{
        /*
         * CVT - cursor line tabulation
         * Move the cursor @args[0] positions down. The cursor stops at the
         * bottom margin. If it was already moved further, it stops at the
         * bottom line. (presentation)
         *
         * Arguments:
         *   args[0]: number of positions
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.23
         */

        /* FIXME: implement this? */
}

void
Terminal::CnD(vte::parser::Sequence const& seq)
{
        /*
         * CnD - Cn-designate
         *
         * Designate a set of control functions.
         *
         * References: ECMA-35 § 14.2
         *             ISO 2375 IR
         */

        /* Since we mostly don't implement ECMA-35 anymore, we can ignore this */
}

void
Terminal::DA1(vte::parser::Sequence const& seq)
{
        /*
         * DA1 - primary-device-attributes
         * The primary DA asks for basic terminal features. We simply return
         * a hard-coded list of features we implement.
         * Note that the primary DA asks for supported features, not currently
         * enabled features.
         *
         * Reply: DECDA1R (CSI ? 65 ; ARGS c)
         *
         * The first argument, 65, is fixed and denotes a VT520 (a Level 5
         * terminal), the last DEC-term that extended this number.
         * All following arguments denote supported features. Note
         * that at most 15 features can be sent (max CSI args). It is safe to
         * send more, but clients might not be able to parse them. This is a
         * client's problem and we shouldn't care. There is no other way to
         * send those feature lists, so we have to extend them beyond 15 in
         * those cases.
         *
         * Known modes:
         *    1: 132 column mode
         *       The 132 column mode is supported by the terminal.
         *    2: printer port
         *       A priner-port is supported and can be addressed via
         *       control-codes.
         *    3: ReGIS graphics
         *       Support for ReGIS graphics is available. The ReGIS routines
         *       provide the "remote graphics instruction set" and allow basic
         *       vector-rendering.
         *    4: Sixel
         *       Support of Sixel graphics is available. This provides access
         *       to the sixel bitmap routines.
         *    6: selective erase
         *       The terminal supports DECSCA and related selective-erase
         *       functions. This allows to protect specific cells from being
         *       erased, if specified.
         *    7: soft character set (DRCS)
         *       TODO: ?
         *    8: user-defined keys (UDKs)
         *       TODO: ?
         *    9: national-replacement character sets (NRCS)
         *       National-replacement character-sets are available.
         *   12: Serbo-Croatian (SCS)
         *       TODO: ?
         *   15: technical character set
         *       The DEC technical-character-set is available.
         *   18: windowing capability
         *       TODO: ?
         *   19: sessions capability
         *       TODO: ?
         *   21: horizontal scrolling
         *       TODO: ?
         *   22: ANSI color
         *       TODO: ?
         *   23: Greek
         *       TODO: ?
         *   24: Turkish
         *       TODO: ?
         *   29: DECterm text locator
         *       TODO: ?
         *   42: ISO Latin-2 character set
         *       TODO: ?
         *   44: PCTerm
         *       TODO: ?
         *   45: soft key mapping
         *       TODO: ?
         *   46: ASCII emulation
         *       TODO: ?
         *
         * Extensions which are implied by the level are not reported explicity
         * (e.g. 6, 8, 15 in level 5).
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.24
         *             VT525
         */

        if (seq.collect1(0, 0) != 0)
                return;

        reply(seq, VTE_REPLY_DECDA1R, {65, 1,
                                       9});
}

void
Terminal::DA2(vte::parser::Sequence const& seq)
{
        /*
         * DA2 - secondary-device-attributes
         * The secondary DA asks for the terminal-ID, firmware versions and
         * other non-primary attributes. All these values are
         * informational-only and should not be used by the host to detect
         * terminal features.
         *
         * Reply: DECDA2R (CSI > 65 ; FIRMWARE ; KEYBOARD c)
         * where 65 is fixed for VT525 color terminals, the last terminal-line that
         * increased this number (64 for VT520). FIRMWARE is the firmware
         * version encoded as major/minor (20 == 2.0) and KEYBOARD is 0 for STD
         * keyboard and 1 for PC keyboards.
         *
         * We replace the firmware-version with the VTE version so clients
         * can decode it again.
         *
         * References: VT525
         */

        /* Param != 0 means this is a reply, not a request */
        if (seq.collect1(0, 0) != 0)
                return;

        int const version = (VTE_MAJOR_VERSION * 100 + VTE_MINOR_VERSION) * 100 + VTE_MICRO_VERSION;
        reply(seq, VTE_REPLY_DECDA2R, {65, version, 1});
}

void
Terminal::DA3(vte::parser::Sequence const& seq)
{
        /*
         * DA3 - tertiary-device-attributes
         * The tertiary DA is used to query the terminal-ID.
         *
         * Reply: DECRPTUI
         *   DATA: four pairs of are hexadecimal number, encoded 4 bytes.
         *   The first byte denotes the manufacturing site, the remaining
         *   three is the terminal's ID.
         *
         * We always reply with '~VTE' encoded in hex.
         */

        if (seq.collect1(0, 0) != 0)
                return;

        reply(seq, VTE_REPLY_DECRPTUI, {});
}

void
Terminal::DAQ(vte::parser::Sequence const& seq)
{
        /*
         * DAQ - define area qualification
         *
         * Arguments:
         *   args[0]: type
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.25, § 6.5.2
         */
}

void
Terminal::DC1(vte::parser::Sequence const& seq)
{
        /*
         * DC1 - device-control-1 or XON
         * This clears any previous XOFF and resumes terminal-transmission.
         *
         * References: ECMA-48 § 8.3.28
         */

        /* we do not support XON */
}

void
Terminal::DC2(vte::parser::Sequence const& seq)
{
        /*
         * DC2 - device-control-2
         *
         * References: ECMA-48 § 8.3.29
         *
         * Not implemented.
         */
}

void
Terminal::DC3(vte::parser::Sequence const& seq)
{
        /*
         * DC3 - device-control-3 or XOFF
         * Stops terminal transmission. No further characters are sent until
         * an XON is received.
         *
         * References: ECMA-48 § 8.3.30
         */

        /* we do not support XOFF */
}

void
Terminal::DC4(vte::parser::Sequence const& seq)
{
        /*
         * DC4 - device-control-4
         *
         * References: ECMA-48 § 8.3.31
         *
         * Not implemented.
         */
}

void
Terminal::DCH(vte::parser::Sequence const& seq)
{
        /*
         * DCH - delete-character
         * This deletes @argv[0] characters at the current cursor position.
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.26
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        vte_page_delete_cells(screen->page,
                                 screen->state.cursor_x,
                                 screen->state.cursor_y,
                                 num,
                                 &screen->state.attr,
                                 screen->age);
#endif

        auto const value = seq.collect1(0, 1, 1, int(m_column_count - m_screen->cursor.col));

        // FIXMEchpe pass count to delete_character() and simplify
        // to only cleanup fragments once
        for (auto i = 0; i < value; i++)
                delete_character();
}

void
Terminal::DECAC(vte::parser::Sequence const& seq)
{
        /*
         * DECAC - assign color
         * Assign the color used for normal text.
         *
         * Arguments:
         *   @args[0]: item; 1 for normal text, 2 for the text in the window frame
         *   @args[1]: foreground color palette index (0..15)
         *   @args[2]: background color palette index (0..15)
         *
         * References: VT525
         */

        // FIXMEchpe maybe implement this, allowing our extended color
        // format instead of just palette colors
}

void
Terminal::DECALN(vte::parser::Sequence const& seq)
{
        /*
         * DECALN - screen-alignment-pattern
         * Resets the margins, homes the cursor, and fills the screen
         * with 'E's.
         *
         * References: VT525
         */

        // FIXMEchpe! reset margins and home cursor

	for (auto row = m_screen->insert_delta;
	     row < m_screen->insert_delta + m_row_count;
	     row++) {
		/* Find this row. */
                while (_vte_ring_next(m_screen->row_data) <= row)
                        ring_append(false);
                adjust_adjustments();
                auto rowdata = _vte_ring_index_writable (m_screen->row_data, row);
		g_assert(rowdata != NULL);
		/* Clear this row. */
		_vte_row_data_shrink (rowdata, 0);

                emit_text_deleted();
		/* Fill this row. */
                VteCell cell;
		cell.c = 'E';
		cell.attr = basic_cell.attr;
		cell.attr.set_columns(1);
                _vte_row_data_fill(rowdata, &cell, m_column_count);
                emit_text_inserted();
	}
        invalidate_all();

	/* We modified the display, so make a note of it for completeness. */
        m_text_modified_flag = TRUE;
}

void
Terminal::DECARR(vte::parser::Sequence const& seq)
{
        /*
         * DECARR - auto repeat rate
         * Sets the key autorepeat rate in from @args[0] in keys/s.
         * 0…5 are mapped to 0/s, 6…15 to 10/s, 16…30 to 30/s.
         * Other values are ignored. The default is 30.
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECATC(vte::parser::Sequence const& seq)
{
        /*
         * DECATC - alternate text color
         * Assign the color used for attribute combinations text.
         *
         * Arguments:
         *   @args[0]: selects the attribute combinations from a
         *     value table (0 = normal, 1 = bold, 2 = reverse,
         *     3 = (single) underline, 4 = blink; then 5…15
         *     encode the combinations)
         *   @args[1]: foreground color palette index (0..15)
         *   @args[2]: background color palette index (0..15)
         *
         * References: VT525
         */

        // FIXMEchpe maybe implement this, allowing our extended color
        // format instead of just palette colors
}

void
Terminal::DECAUPSS(vte::parser::Sequence const& seq)
{
        /*
         * DECAUPSS - assign user preferred supplemental sets
         * Sets a supplemental charset as user preferred.
         * Arguments:
         *   @args[0]: charset designator:
         *     0 = DEC, Latin 1/2
         *     1 = Latin 5/7, ISO Cyrillic, ISO Hebrew
         *   DATA: the charset, as in a ECMA-35 charset designation
         *     sequence (sans the ESC); but only some charsets are
         *     supported.
         *
         * Default: DEC Supplemental Graphic set.
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECBI(vte::parser::Sequence const& seq)
{
        /*
         * DECBI - back-index
         * This control function moves the cursor backward one column. If the
         * cursor is at the left margin, then all screen data within the margin
         * moves one column to the right. The column that shifted past the right
         * margin is lost.
         * DECBI adds a new column at the left margin with no visual attributes.
         * DECBI does not affect the margins. If the cursor is beyond the
         * left-margin at the left border, then the terminal ignores DECBI.
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECCARA(vte::parser::Sequence const& seq)
{
        /*
         * DECCARA - change-attributes-in-rectangular-area
         * Change some character attributes (bold, blink, reverse,
         * (single) underline) in the specified rectangle.
         * The characters in the area are unchanged.
         *
         * Arguments;
         *   args[0..3]: top, left, bottom, right of the rectangle (1-based)
         *   args[4:]: the character attributes to change; values as in SGR
         *
         * Defaults:
         *   args[0]: 1
         *   args[1]: 1
         *   args[2]: height of current page
         *   args[3]: width of current page
         *   args[4:]: no defaults
         *
         * If the top > bottom or left > right, the command is ignored.
         *
         * These coordinates are interpreted according to origin mode (DECOM),
         * but unaffected by the page margins (DECSLRM?). Current SGR defaults
         * and cursor position are unchanged.
         *
         * Note: DECSACE selects whether this function operates on the
         * rectangular area or the data stream between the star and end
         * positions.
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECCKD(vte::parser::Sequence const& seq)
{
        /*
         * DECCKD - copy key default
         * Copy the defaults from one key to another.
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECCRA(vte::parser::Sequence const& seq)
{
        /*
         * DECCRA - copy-rectangular-area
         * Copies characters and their attributes from one rectangle to
         * another.
         *
         * Arguments;
         *   args[0..3]: top, left, bottom, right of the source rectangle (1-based)
         *   args[4..7]: top, left, bottom, right of the target rectangle (1-based)
         *
         * Defaults:
         *   args[0]: 1
         *   args[1]: 1
         *   args[2]: height of current page
         *   args[3]: width of current page
         *   args[4]: 1
         *   args[5]: 1
         *   args[6]: height of current page
         *   args[7]: width of current page
         *
         * If the top > bottom or left > right for either of the rectangles,
         * the command is ignored.
         *
         * These coordinates are interpreted according to origin mode (DECOM),
         * but unaffected by the page margins (DECSLRM?). Current SGR defaults
         * and cursor position are unchanged.
         *
         * Note: DECSACE selects whether this function operates on the
         * rectangular area or the data stream between the star and end
         * positions.
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECCRTST(vte::parser::Sequence const& seq)
{
        /*
         * DECCRTST - CRT saver time
         * Sets the CRT saver timer. When DECCRTSM is set, the
         * screen blanks when the time elapsed since the last
         * keystroke or output is greater than the time set here.
         *
         * Arguments:
         *   args[0]: the time in minutes (0…60) (0 = never)
         *
         * Default: 15
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECDC(vte::parser::Sequence const& seq)
{
        /*
         * DECDC - delete-column
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECDHL_BH(vte::parser::Sequence const& seq)
{
        /*
         * DECDHL_BH - double-width-double-height-line: bottom half
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECDHL_TH(vte::parser::Sequence const& seq)
{
        /*
         * DECDHL_TH - double-width-double-height-line: top half
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECDLD(vte::parser::Sequence const& seq)
{
        /*
         * DECDLD - dynamically redefinable character sets extension
         * Loads a soft font for a DRCS charset from SIXEL data
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECDLDA(vte::parser::Sequence const& seq)
{
        /*
         * DECDLD - down line load allocation
         * Sets the number of DRCSes allowed per sesion
         * (monochrome terminals only).
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECDMAC(vte::parser::Sequence const& seq)
{
        /*
         * DECDMAC - define-macro
         * Define a macro that can be executed by DECINVM.
         *
         * References: VT525
         *
         * For security reasons, VTE does not implement this.
         */
}

void
Terminal::DECDWL(vte::parser::Sequence const& seq)
{
        /*
         * DECDWL - double-width-single-height-line
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECEFR(vte::parser::Sequence const& seq)
{
        /*
         * DECEFR - enable-filter-rectangle
         * Defines the coordinates of a filter rectangle (top, left, bottom,
         * right as @args[0] to @args[3]) and activates it.
         * Anytime the locator is detected outside of the filter rectangle, an
         * outside rectangle event is generated and the rectangle is disabled.
         * Filter rectangles are always treated as "one-shot" events. Any
         * parameters that are omitted default to the current locator position.
         * If all parameters are omitted, any locator motion will be reported.
         * DECELR always cancels any prevous rectangle definition.
         *
         * The locator is usually associated with the mouse-cursor, but based
         * on cells instead of pixels. See DECELR how to initialize and enable
         * it. DECELR can also enable pixel-mode instead of cell-mode.
         *
         * References: VT525
         *
         * TODO: implement
         */
}

void
Terminal::DECELF(vte::parser::Sequence const& seq)
{
        /*
         * DECELF - enable-local-functions
         * Enable or disable keys to perform local functions like
         * copy/paster, panning and window resize.
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECELR(vte::parser::Sequence const& seq)
{
        /*
         * DECELR - enable-locator-reporting
         * This changes the locator-reporting mode. @args[0] specifies the mode
         * to set, 0 disables locator-reporting, 1 enables it continuously, 2
         * enables it for a single report. @args[1] specifies the
         * precision-mode. 0 and 2 set the reporting to cell-precision, 1 sets
         * pixel-precision.
         *
         * Defaults:
         *   args[0]: 0
         *   args[1]: 0
         *
         * References: VT525
         *
         * TODO: implement
         */
}

void
Terminal::DECERA(vte::parser::Sequence const& seq)
{
        /*
         * DECERA - erase-rectangular-area
         * Erases characters in the specified rectangle, replacing
         * them with SPACE (2/0). Character attributes are erased
         * too, but not line attributes (DECDHL, DECDWL).
         *
         * Arguments;
         *   args[0..3]: top, left, bottom, right of the rectangle (1-based)
         *
         * Defaults:
         *   args[0]: 1
         *   args[1]: 1
         *   args[2]: height of current page
         *   args[3]: width of current page
         *
         * If the top > bottom or left > right, the command is ignored.
         *
         * These coordinates are interpreted according to origin mode (DECOM),
         * but unaffected by the page margins (DECSLRM?). Current SGR defaults
         * and cursor position are unchanged.
         *
         * Note: DECSACE selects whether this function operates on the
         * rectangular area or the data stream between the star and end
         * positions.
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECES(vte::parser::Sequence const& seq)
{
        /*
         * DECES - enable session
         * Makes this session active as if by the Session key;
         * that is, makes the session receiving this command the
         * session receiving keyboard input.
         *
         * References: VT525
         *
         * VTE does not support sessions.
         */
}

void
Terminal::DECFI(vte::parser::Sequence const& seq)
{
        /*
         * DECFI - forward-index
         * This control function moves the cursor forward one column. If the
         * cursor is at the right margin, then all screen data within the
         * margins moves one column to the left. The column shifted past the
         * left margin is lost.
         * DECFI adds a new column at the right margin, with no visual
         * attributes. DECFI does not affect margins. If the cursor is beyond
         * the right margin at the border of the page when the terminal
         * receives DECFI, then the terminal ignores DECFI.
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECFNK(vte::parser::Sequence const& seq)
{
        /*
         * DECFNK - function key (or XTERM bracketed paste)
         *
         * References: VT525
         *             XTERM
         */
}

void
Terminal::DECFRA(vte::parser::Sequence const& seq)
{
        /*
         * DECFRA - fill-rectangular-area
         * Fills the specified rectangle with the specified character,
         * replacing the current characters in it. Character attributes
         * are replaced by the current default SGR. Does not change
         * line attributes (DECDHL, DECDWL).
         *
         * Arguments;
         *   args[0]: the decimal value of the replacement character (GL or GR)
         *   args[0..3]: top, left, bottom, right of the rectangle (1-based)
         *
         * Defaults:
         *   args[0]: 1
         *   args[1]: 1
         *   args[2]: height of current page
         *   args[3]: width of current page
         *
         * If the top > bottom or left > right, the command is ignored.
         * If the character is not in the GL or GR area, the command is ignored.
         *
         * These coordinates are interpreted according to origin mode (DECOM),
         * but unaffected by the page margins (DECSLRM?). Current SGR defaults
         * and cursor position are unchanged.
         *
         * Note: DECSACE selects whether this function operates on the
         * rectangular area or the data stream between the start and end
         * positions.
         *
         * References: VT525
         *
         * Probably not worth implementing.
         *
         * *If* we were to implement it, we should find a way to allow any
         * UTF-8 character, perhaps by using subparams to encode it. E.g.
         * either each UTF-8 byte in a subparam of its own, or just split
         * the unicode plane off into the leading subparam (plane:remaining 16 bits).
         * Or by using the last graphic character for it, like REP.
         * See also the changes wrt. UTF-8 handling here in XTERM 357.
         */
}

void
Terminal::DECIC(vte::parser::Sequence const& seq)
{
        /*
         * DECIC - insert-column
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECINVM(vte::parser::Sequence const& seq)
{
        /*
         * DECINVM - invoke-macro
         * Invokes a macro defined by DECDMAC.
         *
         * References: VT525
         *
         * For security reasons, VTE does not implement this.
         */
}

void
Terminal::DECKBD(vte::parser::Sequence const& seq)
{
        /*
         * DECKBD - keyboard-language-selection
         * Selects a keyboard language.
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECKPAM(vte::parser::Sequence const& seq)
{
        /*
         * DECKPAM - keypad-application-mode
         * Enables the keypad-application mode. If enabled, the keypad sends
         * special characters instead of the printed characters. This way,
         * applications can detect whether a numeric key was pressed on the
         * top-row or on the keypad.
         * Default is keypad-numeric-mode.
         *
         * References: VT525
         */

        set_mode_private(vte::terminal::modes::Private::eDEC_APPLICATION_KEYPAD, true);
}

void
Terminal::DECKPNM(vte::parser::Sequence const& seq)
{
        /*
         * DECKPNM - keypad-numeric-mode
         * This disables the keypad-application-mode (DECKPAM) and returns to
         * the keypad-numeric-mode. Keypresses on the keypad generate the same
         * sequences as corresponding keypresses on the main keyboard.
         * Default is keypad-numeric-mode.
         *
         * References: VT525
         */
        set_mode_private(vte::terminal::modes::Private::eDEC_APPLICATION_KEYPAD, false);
}

void
Terminal::DECLANS(vte::parser::Sequence const& seq)
{
        /*
         * DECLANS - load answerback message
         *
         * References: VT525
         *
         * For security reasons, VTE does not implement this.
         */
}

void
Terminal::DECLBAN(vte::parser::Sequence const& seq)
{
        /*
         * DECLBAN - load banner message
         * Loads a banner message that will be displayed in double size
         * characters when the terminal powers up.
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECLBD(vte::parser::Sequence const& seq)
{
        /*
         * DECLBD - locator button define
         *
         * References: VT330
         */
}

void
Terminal::DECLFKC(vte::parser::Sequence const& seq)
{
        /*
         * DECLFKC - local-function-key-control
         * Select the action for local function keys.
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECLL(vte::parser::Sequence const& seq)
{
        /*
         * DECLL - load-leds
         * Sets the keyboard LEDs when in DECKLHIM mode.
         *
         * Arguments:
         *   args[0]: which LED to change to which state
         *     0: NumLock, CapsLock, ScrollLock off
         *     1, 21: NumLock on/off
         *     2, 22: CapsLock on/off
         *     3, 23: ScrollLock on/off
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECLTOD(vte::parser::Sequence const& seq)
{
        /*
         * DECLTOD - load-time-of-day
         * Sets the clock.
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECPAK(vte::parser::Sequence const& seq)
{
        /*
         * DECPAK - program alphanumeric key
         * Program alphanumeric keys to send different codes or perform actions.
         *
         * References: VT525
         *
         * For security reasons, VTE does not implement this.
         */
}

void
Terminal::DECPCTERM(vte::parser::Sequence const& seq)
{
        /*
         * DECPCTERM - pcterm-mode
         * This enters/exits the PCTerm mode. Default mode is VT-mode. It can
         * also select parameters for scancode/keycode mappings in SCO mode.
         *
         * References: VT525
         *
         * Definitely not worth implementing.
         */
}

void
Terminal::DECPCTERM_OR_XTERM_RPM(vte::parser::Sequence const& seq)
{
        /*
         * There's a conflict between DECPCTERM and XTERM_RPM.
         * XTERM_RPM takes a single argument, DECPCTERM takes 2.
         * Note that since both admit default values (which may be
         * omitted at the end of the sequence), this only an approximation.
         */
        if (seq.size_final() <= 1)
                XTERM_RPM(seq);
        #ifdef PARSER_INCLUDE_NOP
        else
                DECPCTERM(seq);
        #endif
}

void
Terminal::DECPFK(vte::parser::Sequence const& seq)
{
        /*
         * DECPFK - program function key
         * Program function keys to send different codes or perform actions.
         *
         * References: VT525
         *
         * For security reasons, VTE does not implement this.
         */
}

void
Terminal::DECPKA(vte::parser::Sequence const& seq)
{
        /*
         * DECPKA - program-key-action
         * Sets whether DECPFK, DECPAK, DECCD, DECUDK can reprogram keys.
         *
         * Arguments:
         *   args[0]:
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: VT525
         *
         * For security reasons, VTE does not implement this.
         */
}

void
Terminal::DECPKFMR(vte::parser::Sequence const& seq)
{
        /*
         * DECPKFMR - program-key-free-memory-report
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECPS(vte::parser::Sequence const& seq)
{
        /*
         * DECPS - play sound
         * Plays a note. Arguments:
         *   @args[0]: the volume. 0 = off, 1…3 = low, 4…7 = high
         *   @args[1]: the duration, in multiples of 1s/32
         *   @args[2..]: the note(s); from 1 = C5, 2 = C♯5 … to 25 = C7
         *
         * Defaults:
         *   @args[0]: no default
         *   @args[1]: no default
         *   @args[2..]: no default
         *
         * Note that a VT525 is specified to store only 16 notes at a time.
         *
         * Note that while the VT520/525 programming manual documents the
         * DECPS sequence on page 5-89 with only one note, in the Setup
         * section on page 2-60 it shows the sequence taking multiple notes
         * (likely up to the maximum number or parameters the VT525
         * supports in CSI sequences, which is at least 16 as per DEC STD 070).
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECRARA(vte::parser::Sequence const& seq)
{
        /*
         * DECRARA - reverse-attributes-in-rectangular-area
         * Reverse some character attributes (bold, blink, reverse,
         * (single) underline) in the specified rectangle.
         * The characters in the area are unchanged, as are the
         * other character attributes.
         *
         * Arguments;
         *   args[0..3]: top, left, bottom, right of the rectangle (1-based)
         *   args[4:]: the character attributes to change; values as in SGR
         *     except that only bold, blink, reverse, (single) underline are
         *     supported; 0 to reverse all of these.
         *
         * Defaults:
         *   args[0]: 1
         *   args[1]: 1
         *   args[2]: height of current page
         *   args[3]: width of current page
         *   args[4:]: no defaults
         *
         * If the top > bottom or left > right, the command is ignored.
         *
         * These coordinates are interpreted according to origin mode (DECOM),
         * but unaffected by the page margins (DECSLRM?). Current SGR defaults
         * and cursor position are unchanged.
         *
         * Note: DECSACE selects whether this function operates on the
         * rectangular area or the data stream between the star and end
         * positions.
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECRC(vte::parser::Sequence const& seq)
{
        /*
         * DECRC - restore-cursor
         * Restores the terminal to the state saved by the save cursor (DECSC)
         * function. If there was not a previous DECSC, then this does:
         *   * Home the cursor
         *   * Resets DECOM
         *   * Resets the SGR attributes
         *   * Designates ASCII (IR #6) to GL, and DEC Supplemental Graphics to GR
         *
         * Note that the status line has its own DECSC buffer.
         *
         * References: VT525
         */
#if 0
        screen_restore_state(screen, &screen->saved);
#endif

        restore_cursor();
}

void
Terminal::DECREGIS(vte::parser::Sequence const& seq)
{
        /*
         * DECREGIS - ReGIS graphics
         *
         * References: VT330
         */
}

void
Terminal::DECREQTPARM(vte::parser::Sequence const& seq)
{
        /*
         * DECREQTPARM - request-terminal-parameters
         * The sequence DECREPTPARM is sent by the terminal controller to notify
         * the host of the status of selected terminal parameters. The status
         * sequence may be sent when requested by the host or at the terminal's
         * discretion. DECREPTPARM is sent upon receipt of a DECREQTPARM.
         *
         * If @args[0] is 0, this marks a request and the terminal is allowed
         * to send DECREPTPARM messages without request. If it is 1, the same
         * applies but the terminal should no longer send DECREPTPARM
         * unrequested.
         * 2 and 3 mark a report, but 3 is only used if the terminal answers as
         * an explicit request with @args[0] == 1.
         *
         * The other arguments are ignored in requests, but have the following
         * meaning in responses:
         *   args[1]: 1=no-parity-set 4=parity-set-and-odd 5=parity-set-and-even
         *   args[2]: 1=8bits-per-char 2=7bits-per-char
         *   args[3]: transmission-speed
         *   args[4]: receive-speed
         *   args[5]: 1=bit-rate-multiplier-is-16
         *   args[6]: This value communicates the four switch values in block 5
         *            of SETUP B, which are only visible to the user when an STP
         *            option is installed. These bits may be assigned for an STP
         *            device. The four bits are a decimal-encoded binary number.
         *            Value between 0-15.
         *
         * The transmission/receive speeds have mappings for number => bits/s
         * which are quite weird. Examples are: 96->3600, 112->9600, 120->19200
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: VT100
         */

        switch (seq.collect1(0)) {
        case -1:
        case 0:
                #if 0
                screen->flags &= ~VTE_FLAG_INHIBIT_TPARM;
                #endif
                reply(seq, VTE_REPLY_DECREPTPARM,
                      {2, 1, 1, 120, 120, 1, 0});
                break;
        case 1:
                #if 0
                screen->flags |= VTE_FLAG_INHIBIT_TPARM;
                #endif
                reply(seq, VTE_REPLY_DECREPTPARM,
                      {3, 1, 1, 120, 120, 1, 0});
                break;
        case 2:
        case 3:
                /* This is a report, not a request */
        default:
                break;
        }
}

void
Terminal::DECREQTPARM_OR_WYCDIR(vte::parser::Sequence const& seq)
{
        /*
         * There's a conflict between DECREQTPERM and WYCDIR.
         * A DECTPARM request (_not_ response!) only has at most one
         * parameter, while WYCDIR takes three. Although both
         * commands admit default values to all parameters, using
         * the number of parameters to disambiguate should be good
         * enough here.
         */
        if (seq.size_final() <= 1)
                DECREQTPARM(seq);
#ifdef PARSER_INCLUDE_NOP
        else
                WYCDIR(seq);
#endif
}

void
Terminal::DECRQCRA(vte::parser::Sequence const& seq)
{
        /*
         * DECRQCRA - request checksum of rectangular area
         * Computes a simple checksum of the characters in the rectangular
         * area. args[0] is an identifier, which the response must use.
         * args[1] is the page number; if it's 0 or default then the
         * checksum is computed over all pages; if it's greater than the
         * number of pages, then the checksum is computed only over the
         * last page. args[2]..args[5] describe the area to compute the
         * checksum from, denoting the top, left, bottom, right, resp
         * (1-based). It's required that top ≤ bottom, and left ≤ right.
         * These coordinates are interpreted according to origin mode.
         *
         * NOTE: Since this effectively allows to read the screen
         * (by using a 1x1 rectangle on each cell), we normally only
         * send a dummy reply, and only reply with the actual checksum
         * when in test mode.
         *
         * Defaults:
         *   args[0]: no default
         *   args[1]: 0
         *   args[2]: 1
         *   args[3]: no default (?)
         *   args[4]: height of current page
         *   args[5]: width of current page
         *
         * Reply: DECCKSR
         *   @args[0]: the identifier from the request
         *   DATA: the checksum as a 4-digit hex number
         *
         * References: VT525
         *             XTERM
         */

        unsigned int idx = 0;
        int id = seq.collect1(idx);

#ifndef VTE_DEBUG
        /* Send a dummy reply */
        return reply(seq, VTE_REPLY_DECCKSR, {id}, "0000");
#else

        /* Not in test mode? Send a dummy reply */
        if ((g_test_flags & VTE_TEST_FLAG_DECRQCRA) == 0) {
                return reply(seq, VTE_REPLY_DECCKSR, {id}, "0000");
        }

        idx = seq.next(idx);

        /* We only support 1 'page', so ignore args[1] */
        idx = seq.next(idx);

        int top = seq.collect1(idx, 1, 1, m_row_count);
        idx = seq.next(idx);
        int left = seq.collect1(idx, 1, 1, m_column_count); /* use 1 as default here */
        idx = seq.next(idx);
        int bottom = seq.collect1(idx, m_row_count, 1, m_row_count);
        idx = seq.next(idx);
        int right = seq.collect1(idx, m_column_count, 1, m_column_count);

        if (m_modes_private.DEC_ORIGIN() &&
            m_scrolling_restricted) {
                top += m_scrolling_region.start;

                bottom += m_scrolling_region.start;
                bottom = std::min(bottom, m_scrolling_region.end);

        }

        unsigned int checksum;
        if (bottom < top || right < left)
                checksum = 0; /* empty area */
        else
                checksum = checksum_area(top -1 + m_screen->insert_delta,
                                         left - 1,
                                         bottom - 1 + m_screen->insert_delta,
                                         right);

        reply(seq, VTE_REPLY_DECCKSR, {id}, "%04X", checksum);
#endif /* VTE_DEBUG */
}

void
Terminal::DECRQDE(vte::parser::Sequence const& seq)
{
        /*
         * DECRQDE - request-display-extent
         * Request how much of the curren tpage is shown on screen.
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECRQKT(vte::parser::Sequence const& seq)
{
        /*
         * DECRQKT - request-key-type
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECRQLP(vte::parser::Sequence const& seq)
{
        /*
         * DECRQLP - request-locator-position
         * See DECELR for locator-information.
         *
         * References: VT525
         *
         * TODO: document and implement
         */
}

void
Terminal::DECRQM_ECMA(vte::parser::Sequence const& seq)
{
        /*
         * DECRQM_ECMA - request-mode-ecma
         * The host sends this control function to find out if a particular mode
         * is set or reset. The terminal responds with a report mode function.
         * @args[0] contains the mode to query.
         *
         * Response is DECRPM with the first argument set to the mode that was
         * queried, second argument is 0 if mode is invalid, 1 if mode is set,
         * 2 if mode is not set (reset), 3 if mode is permanently set and 4 if
         * mode is permanently not set (reset):
         *   ECMA: ^[ MODE ; VALUE $ y
         *   DEC:  ^[ ? MODE ; VALUE $ y
         *
         * References: VT525
         */

        auto const param = seq.collect1(0);
        auto const mode = m_modes_ecma.mode_from_param(param);

        int value;
        switch (mode) {
        case vte::terminal::modes::ECMA::eUNKNOWN:      value = 0; break;
        case vte::terminal::modes::ECMA::eALWAYS_SET:   value = 3; break;
        case vte::terminal::modes::ECMA::eALWAYS_RESET: value = 4; break;
        default: assert(mode >= 0); value = m_modes_ecma.get(mode) ? 1 : 2; break;
        }

        _vte_debug_print(VTE_DEBUG_MODES,
                         "Reporting mode %d (%s) is %d\n",
                         param, m_modes_ecma.mode_to_cstring(mode),
                         value);

        reply(seq, VTE_REPLY_DECRPM_ECMA, {param, value});
}

void
Terminal::DECRQM_DEC(vte::parser::Sequence const& seq)
{
        /*
         * DECRQM_DEC - request-mode-dec
         * Same as DECRQM_ECMA but for DEC modes.
         *
         * References: VT525
         */

        auto const param = seq.collect1(0);
        auto const mode = m_modes_private.mode_from_param(param);

        int value;
        switch (mode) {
        case vte::terminal::modes::Private::eUNKNOWN:      value = 0; break;
        case vte::terminal::modes::Private::eALWAYS_SET:   value = 3; break;
        case vte::terminal::modes::Private::eALWAYS_RESET: value = 4; break;
        default: assert(mode >= 0); value = m_modes_private.get(mode) ? 1 : 2; break;
        }

        _vte_debug_print(VTE_DEBUG_MODES,
                         "Reporting private mode %d (%s) is %d\n",
                         param, m_modes_private.mode_to_cstring(mode),
                         value);

        reply(seq, VTE_REPLY_DECRPM_DEC, {param, value});
}

void
Terminal::DECRQPKFM(vte::parser::Sequence const& seq)
{
        /*
         * DECRQPKFM - request-program-key-free-memory
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECRQPSR(vte::parser::Sequence const& seq)
{
        /*
         * DECRQPSR - request-presentation-state-report
         * Requests a report of the terminal state, that can later
         * be restored with DECRSPS.
         *
         * References: VT525
         */

        switch (seq.collect1(0)) {
        case -1:
        case 0:
                /* Error; ignore request */
                break;

        case 1:
                /* Cursor information report. This contains:
                 *   - the cursor position, including character attributes and
                 *     character protection attribute,
                 *   - origin mode (DECOM),
                 *   - the character sets designated to the G0, G1, G2, and G3 sets.
                 *
                 * Reply: DECCIR
                 *   DATA: the report in a unspecified format
                 *         See WY370 for a possible format to use.
                 */
                break;

        case 2:
                /* Tabstop report.
                 *
                 * Reply: DECTABSR
                 */
                break;

        default:
                break;
        }
}

void
Terminal::DECRQSS(vte::parser::Sequence const& seq)
{
        /*
         * DECRQSS - request selection or setting
         * The DATA string contains the intermediate(s) and final
         * character of a CSI sequence that codes for which
         * selection or setting to report.
         *
         * Reply: DECRPSS
         *   @args[0]: 1 if the request was valid, otherwise 0
         *   DATA: the current value of the selection or setting
         *
         * Note that the VT525 documentation is buggy, is says it
         *   sends 0 for a valid and 1 or an invalid request; we
         *   follow the STD 070 and XTERM behaviour.
         *
         * References: VT525
         */

        /* Use a subparser to get the command from the request */
        vte::parser::Parser parser{};
        parser.feed(0x9b); /* CSI */

        int rv = VTE_SEQ_NONE;

        /* If at the end, the parser returns a VTE_SEQ_CSI sequence,
         * we interpret that; otherwise we ignore the request and
         * send only a dummy reply.
         * Note that this makes sure there is only one setting
         * requested; if there were more than one, the parser would
         * parse them as GRAPHIC and thus we reply 'invalid'.
         */
        auto const str = seq.string();
        size_t i;
        for (i = 0; i < str.size(); ++i) {
                auto const c = str[i];
                if (c < 0x20 || c >= 0x7f)
                        break;
                rv = parser.feed(c);
        }

        vte::parser::Sequence request{parser};
        /* If not the whole string was parsed, or the sequence
         * is not a CSI sequence, or it has parameters, reject
         * the request as invalid.
         */
        if (i != str.size() || rv != VTE_SEQ_CSI || request.size() > 0 /* any parameters */)
                return reply(seq, VTE_REPLY_DECRPSS, {0});

        switch (request.command()) {

        case VTE_CMD_DECSCUSR:
                return reply(seq, VTE_REPLY_DECRPSS, {1}, {VTE_REPLY_DECSCUSR, {int(m_cursor_style)}});

        case VTE_CMD_DECSTBM:
                if (m_scrolling_restricted)
                        return reply(seq, VTE_REPLY_DECRPSS, {1},
                                     {VTE_REPLY_DECSTBM,
                                                     {m_scrolling_region.start + 1,
                                                                     m_scrolling_region.end + 1}});
                else
                        return reply(seq, VTE_REPLY_DECRPSS, {1}, {VTE_REPLY_DECSTBM, {}});

        case VTE_CMD_DECAC:
        case VTE_CMD_DECARR:
        case VTE_CMD_DECATC:
        case VTE_CMD_DECCRTST:
        case VTE_CMD_DECDLDA:
        case VTE_CMD_DECSACE:
        case VTE_CMD_DECSASD:
        case VTE_CMD_DECSCA:
        case VTE_CMD_DECSCL:
        case VTE_CMD_DECSCP:
        case VTE_CMD_DECSCPP:
        case VTE_CMD_DECSCS:
        case VTE_CMD_DECSDDT:
        case VTE_CMD_DECSDPT:
        case VTE_CMD_DECSEST:
        case VTE_CMD_DECSFC:
        case VTE_CMD_DECSKCV:
        case VTE_CMD_DECSLCK:
        case VTE_CMD_DECSLPP:
        case VTE_CMD_DECSLRM:
        case VTE_CMD_DECSMBV:
        case VTE_CMD_DECSNLS:
        case VTE_CMD_DECSPMA:
        case VTE_CMD_DECSPP:
        case VTE_CMD_DECSPPCS:
        case VTE_CMD_DECSPRTT:
        case VTE_CMD_DECSSCLS:
        case VTE_CMD_DECSSDT:
        case VTE_CMD_DECSSL:
        case VTE_CMD_DECSTGLT:
        case VTE_CMD_DECSTRL:
        case VTE_CMD_DECSWBV:
        case VTE_CMD_DECSZS:
        case VTE_CMD_DECTME:
        case VTE_CMD_SGR:
        default:
                return reply(seq, VTE_REPLY_DECRPSS, {0});
        }
}

void
Terminal::DECRQTSR(vte::parser::Sequence const& seq)
{
        /*
         * DECRQTSR - request-terminal-state-report
         * Requests a report of the terminal state, that can later
         * be restored by DECRSTS.
         *
         * References: VT525
         */

        switch (seq.collect1(0)) {
        case -1:
        case 0:
                /* Ignore */
                break;

        case 1:
                /* Terminal state report.
                 *
                 * Reply: DECTSR
                 *   DATA: the report in an unspecified format
                 */
                /* return reply(seq, VTE_REPLY_DECTSR, {1}, "FIXME"); */
                break;

        case 2:
                /* Color table report.
                 *
                 * Arguments:
                 *   args[1]: color coordinate system
                 *     0: invalid
                 *     1: HLS (0…360, 0…100, 0…100)
                 *     2: RGB (0…100, 0…100, 0…100) (yes, really!)
                 *
                 * Reply: DECTSR
                 *   DATA: the report
                 */
                /* return reply(seq, VTE_REPLY_DECTSR, {2}, "FIXME"); */
                break;

        default:
                break;
        }
}

void
Terminal::DECRQUPSS(vte::parser::Sequence const& seq)
{
        /*
         * DECRQUPSS - request-user-preferred-supplemental-set
         * Requests the user-preferred supplemental set.
         *
         * Reply: DECAUPSS
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */

        // FIXMEchpe send a dummy reply?
}

void
Terminal::DECRSPS(vte::parser::Sequence const& seq)
{
        /*
         * DECRSPS - restore presentation state
         * Restores terminal state from a DECRQPSR response.
         *
         * References: VT525
         */

        switch (seq.collect1(0)) {
        case -1:
        case 0:
                /* Error; ignore */
                break;

        case 1:
                /* Cursor information report*/
                break;

        case 2:
                /* Tabstop report */
                break;

        default:
                break;
        }
}

void
Terminal::DECRSTS(vte::parser::Sequence const& seq)
{
        /*
         * DECRSTS - restore terminal state
         * Restore terminal state from a DECRQTSR response.
         *
         * References: VT525
         */

        switch (seq.collect1(0)) {
        case -1:
        case 0:
                /* Ignore */
                break;

        case 1:
                /* Terminal state report */
                break;

        case 2:
                /* Color table report */
                break;

        default:
                break;
        }
}

void
Terminal::DECSACE(vte::parser::Sequence const& seq)
{
        /*
         * DECSACE - select-attribute-change-extent
         * Selects which positions a rectangle command (DECCARA, DECCRA,
         * DECERA, DECFRA, DECRARA, DECSERA) affects.
         *
         * Arguments:
         *   args[0]:
         *     0, 1: the stream of positions beginning at the
         *           (top, left) and ending at the (bottom, right)
         *           position
         *     2: the positions in the rectangle with corners
         *        (top, left) and (bottom, right)
         *
         * Defaults;
         *   args[0]: 0
         *
         * References: VT525
         *
         * Not worth implementing unless we implement all the rectangle functions.
         */
}

void
Terminal::DECSASD(vte::parser::Sequence const& seq)
{
        /*
         * DECSASD - select-active-status-display
         * Selects between main screen and status line.
         *
         * Arguments:
         *   args[0]:
         *     0: main screen
         *     1: status line
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSC(vte::parser::Sequence const& seq)
{
        /*
         * DECSC - save-cursor
         * Save cursor and terminal state so it can be restored later on.
         * This stores:
         *   * Cursor position
         *   * SGR attributes
         *   * Charset designations for GL and GR
         *   * Wrap flag
         *   * DECOM state
         *   * Selective erase attribute
         *   * Any SS2 or SS3 sent
         *
         * References: VT525
         */
#if 0
        screen_save_state(screen, &screen->saved);
#endif

        save_cursor();
}

void
Terminal::DECSCA(vte::parser::Sequence const& seq)
{
        /*
         * DECSCA - select character protection attribute
         * Sets whether characters inserted are protected or not.
         * Protected characters will not be erased by DECSED or DECSEL.
         * SGR attributes are unchanged.
         *
         * Arguments:
         *   args[0]:
         *     0, 2: not protected
         *     1: protected
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: VT525
         */
#if 0
        unsigned int mode = 0;

        if (seq->args[0] > 0)
                mode = seq->args[0];

        switch (mode) {
        case 0:
        case 2:
                screen->state.attr.protect = 0;
                break;
        case 1:
                screen->state.attr.protect = 1;
                break;
        }
#endif
}

void
Terminal::DECSCL(vte::parser::Sequence const& seq)
{
        /*
         * DECSCL - select-conformance-level
         * Select the terminal's operating level. The factory default is
         * level 4 (VT Level 4 mode, 7-bit controls).
         * When you change the conformance level, the terminal performs a hard
         * reset (RIS).
         *
         * @args[0] defines the conformance-level, valid values are:
         *   61: Level 1 (VT100)
         *   62: Level 2 (VT200)
         *   63: Level 3 (VT300)
         *   64: Level 4 (VT400)
         * @args[1] defines the 8bit-mode, valid values are:
         *    0: 8-bit controls
         *    1: 7-bit controls
         *    2: 8-bit controls (same as 0)
         *
         * If @args[0] is 61, then @args[1] is ignored and 7bit controls are
         * enforced.
         *
         * Defaults:
         *   args[0]: 64
         *   args[1]: 0
         *
         * When not using private colour registers, this
         * must also clear (assign to black) all SIXEL
         * colour registers. (DEC PPLV2 § 5.8)
         *
         * References: VT525
         */
#if 0
        unsigned int level = 64, bit = 0;

        if (seq->n_args > 0) {
                level = seq->args[0];
                if (seq->n_args > 1)
                        bit = seq->args[1];
        }

        vte_screen_hard_reset(screen);

        switch (level) {
        case 61:
                screen->conformance_level = VTE_CONFORMANCE_LEVEL_VT100;
                screen->flags |= VTE_FLAG_7BIT_MODE;
                break;
        case 62 ... 69:
                screen->conformance_level = VTE_CONFORMANCE_LEVEL_VT400;
                if (bit == 1)
                        screen->flags |= VTE_FLAG_7BIT_MODE;
                else
                        screen->flags &= ~VTE_FLAG_7BIT_MODE;
                break;
        }
#endif
}

void
Terminal::DECSCP(vte::parser::Sequence const& seq)
{
        /*
         * DECSCP - select-communication-port
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSCPP(vte::parser::Sequence const& seq)
{
        /*
         * DECSCPP - select-columns-per-page
         * Select columns per page. The number of rows is unaffected by this.
         * @args[0] selectes the number of columns (width), DEC only defines 80
         * and 132, but we allow any integer here. 0 is equivalent to 80.
         * Page content is *not* cleared and the cursor is left untouched.
         * However, if the page is reduced in width and the cursor would be
         * outside the visible region, it's set to the right border. Newly added
         * cells are cleared. No data is retained outside the visible region.
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: VT525
         *
         * FIXMEchpe: implement this instead of deprecated DECCOLM
         */
}

void
Terminal::DECSCS(vte::parser::Sequence const& seq)
{
        /*
         * DECSCS - select-communication-speed
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSCUSR(vte::parser::Sequence const& seq)
{
        /*
         * DECSCUSR - set-cursor-style
         * This changes the style of the cursor. @args[0] can be one of:
         *   0, 1: blinking block
         *      2: steady block
         *      3: blinking underline
         *      4: steady underline
         *      5: blinking ibeam (XTERM)
         *      6: steady ibeam (XTERM)
         * Changing this setting does _not_ affect the cursor visibility itself.
         * Use DECTCEM for that.
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: VT525 5–126
         *             XTERM
         */

        auto param = seq.collect1(0, 0);
        switch (param) {
        case 0 ... 6:
                set_cursor_style(CursorStyle(param));
                break;
        default:
                break;
        }
}

void
Terminal::DECSDDT(vte::parser::Sequence const& seq)
{
        /*
         * DECSDDT - select-disconnect-delay-time
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSDPT(vte::parser::Sequence const& seq)
{
        /*
         * DECSDPT - select-digital-printed-data-type
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSED(vte::parser::Sequence const& seq)
{
        /*
         * DECSED - selective-erase-in-display
         * This control function erases some or all of the erasable characters
         * in the display. DECSED can only erase characters defined as erasable
         * by the DECSCA control function. DECSED works inside or outside the
         * scrolling margins.
         *
         * @args[0] defines which regions are erased. If it is 0, all cells from
         * the cursor (inclusive) till the end of the display are erase. If it
         * is 1, all cells from the start of the display till the cursor
         * (inclusive) are erased. If it is 2, all cells are erased.
         *
         * Defaults:
         *   args[0]: 0
         */

        erase_in_display(seq);
}

void
Terminal::DECSEL(vte::parser::Sequence const& seq)
{
        /*
         * DECSEL - selective-erase-in-line
         * This control function erases some or all of the erasable characters
         * in a single line of text. DECSEL erases only those characters defined
         * as erasable by the DECSCA control function. DECSEL works inside or
         * outside the scrolling margins.
         *
         * @args[0] defines the region to be erased. If it is 0, all cells from
         * the cursor (inclusive) till the end of the line are erase. If it is
         * 1, all cells from the start of the line till the cursor (inclusive)
         * are erased. If it is 2, the whole line of the cursor is erased.
         *
         * Defaults:
         *   args[0]: 0
         */

        erase_in_line(seq);
}

void
Terminal::DECSERA(vte::parser::Sequence const& seq)
{
        /*
         * DECSERA - selective-erase-rectangular-area
         * Selectively erases characters in the specified rectangle,
         * replacing them with SPACE (2/0). Character attributes,
         * protection attribute (DECSCA) and line attributes (DECDHL,
         * DECDWL) are unchanged.
         *
         * Arguments;
         *   args[0..3]: top, left, bottom, right of the source rectangle (1-based)
         *
         * Defaults:
         *   args[0]: 1
         *   args[1]: 1
         *   args[2]: height of current page
         *   args[3]: width of current page
         *
         * If the top > bottom or left > right the command is ignored.
         *
         * These coordinates are interpreted according to origin mode (DECOM),
         * but unaffected by the page margins (DECSLRM?). Current SGR defaults
         * and cursor position are unchanged.
         *
         * Note: DECSACE selects whether this function operates on the
         * rectangular area or the data stream between the star and end
         * positions.
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSEST(vte::parser::Sequence const& seq)
{
        /*
         * DECSEST - energy saver time
         * Sets the enerty saver timer. When DECCRTSM is set, the
         * screen switches to suspend mode when the time elapsed
         * since the last keystroke or output is greater than the
         * time set here.
         *
         * Arguments:
         *   args[0]: the time in minutes (0…60) (0 = never)
         *
         * Default: 15
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSFC(vte::parser::Sequence const& seq)
{
        /*
         * DECSFC - select-flow-control
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSGR(vte::parser::Sequence const& seq)
{
        /*
         * DECSGR - DEC select graphics rendition
         * Selects the character attributes to use for newly inserted
         * characters.
         *
         * Arguments:
         *   args[0:]: the attributes
         *     0 = reset all attributes (deprecated; same as SGR 0)
         *     4 = set superscript and reset subscript
         *     5 = set subscript and reset superscript
         *     6 = set overline (deprecated; same as SGR 53)
         *     8 = set transparency mode
         *     24 = reset superscript and subscript
         *     26 = reset overline (deprecated; same as SGR 55)
         *     28 = reset transparency mode
         *
         * Defaults:
         *   args[0]: 0 (reset all attributes)
         *
         * References: DEC PPLV2
         *             DEC LJ250
         */
        /* TODO: consider implementing sub/superscript? */
}

void
Terminal::DECSIXEL(vte::parser::Sequence const& seq)
{
        /*
         * DECSIXEL - SIXEL graphics
         * Image data in DECSIXEL format.
         *
         * Arguments:
         *  args[0]: macro parameter (should always use 0 and use DECGRA instead)
         *    See DEC PPLV Table 5–2 in § 5.4.1.1 for more information.
         *  args[1]: background
         *    0: device default (same as 2)
         *    1: pixels with colour 0 retain the colour
         *    2: pixels with colour 0 are set to the current background
         *    5: OR mode (nonstandard NetBSD/x68k extension, see
         *       [https://qiita.com/arakiken/items/26f6c67da5a9f9f907ac])
         *  args[2]: horizontal grid size in the unit set by SSU
         *  args[3]: image ID (range 0..1024) (nonstandard RLogin extension)
         *
         * Defaults:
         *   args[0]: 0
         *   args[1]: 2 (1 for printers)
         *   args[2]: no default
         *   args[3]: no default
         *
         * References: VT330
         *             DEC PPLV2 § 5.4
         */
}

void
Terminal::DECSKCV(vte::parser::Sequence const& seq)
{
        /*
         * DECSKCV - set-key-click-volume
         * Sets the key click volume.
         *
         * Arguments:
         *   args[0]: the volume setting
         *     0, 5…8: high
         *     1: off
         *     2…4: low
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSLCK(vte::parser::Sequence const& seq)
{
        /*
         * DECSLCK - set-lock-key-style
         * Allow host control of the CapsLock key
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSLE(vte::parser::Sequence const& seq)
{
        /*
         * DECSLE - select-locator-events
         *
         * References: VT330
         *
         * TODO: implement
         */
}

void
Terminal::DECSLPP(vte::parser::Sequence const& seq)
{
        /*
         * DECSLPP - set-lines-per-page
         * Set the number of lines per page.
         *
         * Arguments:
         *   args[0]: the number of lines per page
         *
         * Defaults:
         *   args[0]: 0 (meaning 24)
         *
         * Note that VT525 only allows a limited number of choices,
         * (24, 25, 36, 41, 42, 48, 52, 53, 72); VTE is not so limited
         * and supports any value >= 24.
         *
         * Top and bottom scrolling margins are unaffected, unless their
         * current values exceed the new page size, in which case they are
         * reset to the default.
         *
         * References: VT525
         */

        auto param = seq.collect1(0);
        if (param == 0)
                param = 24;
        else if (param < 24)
                return;

        _vte_debug_print(VTE_DEBUG_EMULATION, "Resizing to %d rows.\n", param);

        emit_resize_window(m_column_count, param);
}

void
Terminal::DECSLPP_OR_XTERM_WM(vte::parser::Sequence const& seq)
{
        /*
         * DECSLPP and XTERM_WM use the same sequence, but we can
         * distinguish between them by the parameter value.
         */
        auto const param = seq.collect1(0);
        if (param > 0 && param < 24)
                XTERM_WM(seq);
        else
                DECSLPP(seq);
}

void
Terminal::DECSLRM(vte::parser::Sequence const& seq)
{
        /*
         * DECSLRM - set left and right margins
         * Sets the left and right margins of the scrolling region.
         * This is only applicable if the vertical split-screen mode
         * (DECLRMM) is set.
         *
         * Arguments:
         *   args[0]: left margin
         *   args[1]: right margin
         *
         * Default:
         *   args[0]: 1
         *   args[2]: page width
         *
         * If left > right, the command is ignored.
         * The maximum of right is the page size (set with DECSCPP);
         * the minimum size of the scrolling region is 2 columns.
         *
         * Homes to cursor to (1,1) of the page (scrolling region?).
         *
         * References: VT525
         *
         * FIXMEchpe: Consider implementing this.
         */
}

void
Terminal::DECSLRM_OR_SCOSC(vte::parser::Sequence const& seq)
{
        /*
         * set left and right margins or SCO restore cursor - DECSLRM or SCOSC
         * There is a conflict between SCOSC and DECSLRM that both are
         * CSI s (CSI 7/3). SCOSC has 0 parameters, and DECSLRM has 2
         * parameters which both have default values, and my reading
         * of ECMA-48 § 5.4.2h says that this allows for an empty
         * parameter string to represent them.
         *
         * While the DEC manuals say that SCOSC/SCORC only operates in
         * "SCO Console Mode" (which is entered by DECTME 13), and not in
         * "VT mode" (i.e. native mode), we instead distinguish the cases
         * by private mode DECLRMM: If DECLRMM is set, dispatch DECSLRM;
         * if it's reset, dispatch SCOSC.
         *
         * See issue #48.
         */

#ifdef PARSER_INCLUDE_NOP
        if (m_modes_private.DECLRMM())
                DECSLRM(seq);
        else
#endif
                SCOSC(seq);
}

void
Terminal::DECSMBV(vte::parser::Sequence const& seq)
{
        /*
         * DECSMBV - set-margin-bell-volume
         * Sets the margin bell volume.
         *
         * Arguments:
         *   args[0]: the volume setting
         *     0, 1: off
         *     2…4: low
         *     5…8: high
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSMKR(vte::parser::Sequence const& seq)
{
        /*
         * DECSMKR - select-modifier-key-reporting
         * Make modifier keys send extended keyboard reports (DECEKBD)
         * when pressed or released in key position mode (DECKPM).
         * [...]
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSNLS(vte::parser::Sequence const& seq)
{
        /*
         * DECSNLS - set-lines-per-screen
         * Sets the number of lines per screen.
         * DEC only supports 26, 42, 53 lines here; but VTE has no
         * such restriction.
         *
         * Arguments:
         *   args[0]: the number of lines
         *
         * Defaults:
         *   args[0]: no default
         *
         * References: VT525
         *
         * FIXMEchpe: implement this
         */
}

void
Terminal::DECSPMA(vte::parser::Sequence const& seq)
{
        /*
         * DECSPMA - session page memory allocation
         * Allocate pages of 25 lines to each session.
         *
         * References: VT525
         *
         * VTE does not support sessions.
         */
}

void
Terminal::DECSPP(vte::parser::Sequence const& seq)
{
        /*
         * DECSPP - set-port-parameter
         * Sets parameters for the communications or printer port.
         * [...]
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSPPCS(vte::parser::Sequence const& seq)
{
        /*
         * DECSPPCS - select-pro-printer-character-set
         * [...]
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSPRTT(vte::parser::Sequence const& seq)
{
        /*
         * DECSPRTT - select-printer-type
         * [...]
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSR(vte::parser::Sequence const& seq)
{
        /*
         * DECSR - secure-reset
         * Hard reset, with confirmation.
         * Like RIS, but the terminal replies with the token.
         * [long list of things this resets]
         *
         * Arguments:
         *   args[0]: a token
         *
         * Defaults:
         *   args[0]: no default
         *
         * Reply: DECSRC
         *   args[0]: the token
         *
         * References: VT525
         */

        /* Note: reset() wipes out @seq, so we need to get the
         * param beforehand, and use send() instead of reply().
         */
        auto const token = seq.collect1(0);
	reset(true, true);
        send(VTE_REPLY_DECSRC, {token});
}

void
Terminal::DECSRFR(vte::parser::Sequence const& seq)
{
        /*
         * DECSRFR - select-refresh-rate
         * [...]
         *
         * References: VT510
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSSCLS(vte::parser::Sequence const& seq)
{
        /*
         * DECSSCLS - set-scroll-speed
         * [...]
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSSDT(vte::parser::Sequence const& seq)
{
        /*
         * DECSSDT - select-status-display-line-type
         * Sets the type of status line shown.
         *
         * Arguments:
         *   args[0]: the type
         *     0: no status line
         *     1: indicator status line
         *     2: host-writable status line
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: VT525
         *
         * Not worth implementing.
         */
}

void
Terminal::DECSSL(vte::parser::Sequence const& seq)
{
        /*
         * DECSSL - select-setup-language
         *
         * Selects set-up language
         *
         * References: VT525
         *
         * VTE does not implement a set-up.
         *
         * or:
         *
         * WYDRBX - draw a box
         *
         * References: WY370
         */
}

void
Terminal::DECST8C(vte::parser::Sequence const& seq)
{
        /*
         * DECST8C - set-tab-at-every-8-columns
         * Clear the tab-ruler and reset it to a tab at every 8th column,
         * starting at 9 (though, setting a tab at 1 is fine as it has no
         * effect).
         *
         * References: VT525
         */

        if (seq.collect1(0) != 5)
                return;

        m_tabstops.reset(8);
        m_tabstops.unset(0);
}

void
Terminal::DECSTBM(vte::parser::Sequence const& seq)
{
        /*
         * DECSTBM - set-top-and-bottom-margins
         * Sets the top and bottom scrolling margins.
         * Arguments:
         *   args[0]: the top margin
         *   args[1]: the bottom margin
         *
         * Defaults:
         *   args[0]: 1
         *   args[1]: number of lines
         *
         * If top > bottom, the command is ignored.
         * The maximum size of the scrolling region is the whole page.
         * Homes the cursor to position (1,1) (of the scrolling region?).
         *
         * References: VT525 5–149
         */
#if 0
        unsigned int top, bottom;

        top = 1;
        bottom = screen->page->height;

        if (seq->args[0] > 0)
                top = seq->args[0];
        if (seq->args[1] > 0)
                bottom = seq->args[1];

        if (top > screen->page->height)
                top = screen->page->height;
        if (bottom > screen->page->height)
                bottom = screen->page->height;

        if (top >= bottom ||
            top > screen->page->height ||
            bottom > screen->page->height) {
                top = 1;
                bottom = screen->page->height;
        }

        vte_page_set_scroll_region(screen->page, top - 1, bottom - top + 1);
        screen_cursor_clear_wrap(screen);
        screen_cursor_set(screen, 0, 0);
#endif

        int start, end;
        seq.collect(0, {&start, &end});

        /* Defaults */
        if (start <= 0)
                start = 1;
        if (end == -1)
                end = m_row_count;

        if (start > m_row_count ||
            end <= start) {
                m_scrolling_restricted = FALSE;
                home_cursor();
                return;
        }

        if (end > m_row_count)
                end = m_row_count;

	/* Set the right values. */
        m_scrolling_region.start = start - 1;
        m_scrolling_region.end = end - 1;
        m_scrolling_restricted = TRUE;
        if (m_scrolling_region.start == 0 &&
            m_scrolling_region.end == m_row_count - 1) {
		/* Special case -- run wild, run free. */
                m_scrolling_restricted = FALSE;
	} else {
		/* Maybe extend the ring -- bug 710483 */
                while (_vte_ring_next(m_screen->row_data) < m_screen->insert_delta + m_row_count)
                        _vte_ring_insert(m_screen->row_data, _vte_ring_next(m_screen->row_data), get_bidi_flags());
	}

        home_cursor();
}

void
Terminal::DECSTGLT(vte::parser::Sequence const& seq)
{
        /*
         * DECSTGLT - select color lookup table
         * Selects color mapping.
         *
         * Arguments:
         *   args[0]: mode
         *     0: Text colors are shown in monochrome or grey levels
         *     1: Text attributes (bold, blink, reverse, (single) underline,
         *        and any combinations thereof) are shown with alternate
         *        colors (defined by set-up), plus the attribute
         *     2: Like 1, but attributes are only represented by the color
         *     3: Text color as specified by SGR, and attributes
         *        as specified.
         *
         * Defaults:
         *   args[0]: 3
         *
         * Set-up default: 3
         *
         * References: VT525
         *
         * Maybe worth implementing.
         */
}

void
Terminal::DECSTR(vte::parser::Sequence const& seq)
{
        /*
         * DECSTR - soft-terminal-reset
         * Perform a soft reset to the default values.
         * [list of default values]
         *
         * When not using private colour registers, this
         * must also clear (assign to black) all SIXEL
         * colour registers. (DEC PPLV2 § 5.8)
         *
         * References: VT525
         */

	reset(false, false);
}

void
Terminal::DECSTRL(vte::parser::Sequence const& seq)
{
        /*
         * DECSTRL - set-transmit-rate-limit
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSTUI(vte::parser::Sequence const& seq)
{
        /*
         * DECSTUI - set terminal unit ID
         * Sets the terminal unit ID that DA3 reports.
         *
         * References: VT525
         *
         * VTE does not implement this.
         */
}

void
Terminal::DECSWBV(vte::parser::Sequence const& seq)
{
        /*
         * DECSWBV - set-warning-bell-volume
         * Sets the warning bell volume.
         *
         * Arguments:
         *   args[0]: the volume setting
         *     0, 5…8: high
         *     1: off
         *     2…4: low
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSWL(vte::parser::Sequence const& seq)
{
        /*
         * DECSWL - single-width-single-height-line
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSZS(vte::parser::Sequence const& seq)
{
        /*
         * DECSZS - select zero symbol
         * Selects the zero glyph shape.
         *
         * Aguments:
         *   args[0]: shape
         *     0: oval zero
         *     1: zero with slash
         *     2: zero with dot
         *
         * Default:
         *  args[0]: 0
         *
         * References: VT525
         *
         * Maybe worth implementing; could use the opentype "zero" feature
         * to get the slashed zero.
         */
}

void
Terminal::DECTID(vte::parser::Sequence const& seq)
{
        /*
         * DECTID - select-terminal-id
         * Selects the response to DA1.
         * [...]
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECTME(vte::parser::Sequence const& seq)
{
        /*
         * DECTME - terminal-mode-emulation
         * Selects the terminal emulation mode.
         * Available values are various VTxxx, Wyse, TVI, ADDS, SCO
         * terminals.
         * Changing the emulation mode effects a soft reset.
         *
         * References: VT525
         *
         * Not worth implementing.
         */
}

void
Terminal::DECTST(vte::parser::Sequence const& seq)
{
        /*
         * DECTST - invoke-confidence-test
         * Executes self-tests.
         *
         * Arguments:
         *   args[0]: 4
         *   args[1]: which test to perform
         *
         * References: VT525
         *
         * Not worth implementing.
         */
}

void
Terminal::DECUDK(vte::parser::Sequence const& seq)
{
        /*
         * DECUDK - user define keys
         * Loads key definitions.
         *
         * References: VT525
         *
         * For security reasons, VTE does not implement this.
         */
}

void
Terminal::DECUS(vte::parser::Sequence const& seq)
{
        /*
         * DECUS - update session
         *
         * References: VT525
         *
         * VTE does not support sessions.
         */
}

void
Terminal::DL(vte::parser::Sequence const& seq)
{
        /*
         * DL - delete-line
         * Delete lines starting from the active line (presentation).
         *
         * Depending on DCSM, this function works on the presentation
         * or data position. Terminal-wg/bidi forces DCSM to DATA.
         *
         * Also affected by TSM and VEM modes, and the SLH and SEE
         * functions.
         *
         * Arguments:
         *  args[0]: number of lines to delete
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.32
         *             DEC STD 070 page 5-148
         *             Terminal-wg/bidi
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        vte_page_delete_lines(screen->page,
                                 screen->state.cursor_y,
                                 num,
                                 &screen->state.attr,
                                 screen->age);
#endif

        auto const count = seq.collect1(0, 1);
        delete_lines(count);
}

void
Terminal::DLE(vte::parser::Sequence const& seq)
{
        /*
         * DLE - data link escape
         * Supplementary transmission control functions.
         *
         * References: ECMA-48 § 8.3.33
         *             ECMA-16 § 3.1.7
         *             ECMA-37
         *
         * Not worth implementing.
         */
}

void
Terminal::DMI(vte::parser::Sequence const& seq)
{
        /*
         * DMI - disable manual input
         *
         * References: ECMA-48 § 8.3.34
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DOCS(vte::parser::Sequence const& seq)
{
        /*
         * DOCS - designate other coding systyem
         *
         * References: ECMA-35 § 15.4
         *             ISO 2375 IR
         *
         * TODO: implement (bug #787228)
         */
}

void
Terminal::DSR_ECMA(vte::parser::Sequence const& seq)
{
        /*
         * DSR_ECMA - Device Status Report
         *
         * Reports status, or requests a status report.
         *
         * Arguments:
         *   args[0]: type
         *
         * Defaults:
         *   arg[0]: 0
         *
         * References: ECMA-48 § 8.3.35
         */

        switch (seq.collect1(0)) {
        case -1:
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
                /* This is a status report */
                break;

        case 5:
                /* Request operating status report.
                 * Reply: DSR
                 *   @arg[0]: status
                 *     0 = ok
                 *     3 = malfunction
                 */
                reply(seq, VTE_REPLY_DSR, {0});
                break;

        case 6:
                /* Request cursor position report
                 * Reply: CPR
                 *   @arg[0]: line
                 *   @arg[1]: column
                 */
                vte::grid::row_t rowval, origin, rowmax;
                if (m_modes_private.DEC_ORIGIN() &&
                    m_scrolling_restricted) {
                        origin = m_scrolling_region.start;
                        rowmax = m_scrolling_region.end;
                } else {
                        origin = 0;
                        rowmax = m_row_count - 1;
                }
                // FIXMEchpe this looks wrong. shouldn't this first clamp to origin,rowmax and *then* subtract origin?
                rowval = m_screen->cursor.row - m_screen->insert_delta - origin;
                rowval = CLAMP(rowval, 0, rowmax);

                reply(seq, VTE_REPLY_CPR,
                      {int(rowval + 1), int(CLAMP(m_screen->cursor.col + 1, 1, m_column_count))});
                break;

        default:
                break;
        }
}

void
Terminal::DSR_DEC(vte::parser::Sequence const& seq)
{
        /*
         * DSR_DEC - device-status-report-dec
         *
         * Reports status, or requests a status report.
         *
         * Defaults:
         *   arg[0]: 0
         *
         * References: VT525 5–173
         *             VT330
         *             XTERM
         */

        switch (seq.collect1(0)) {
        case 6:
                /* Request extended cursor position report
                 * Reply: DECXCPR
                 *   @arg[0]: line
                 *   @arg[1]: column
                 *   @arg[2]: page
                 *     Always report page 1 here (per XTERM source code).
                 */
                vte::grid::row_t rowval, origin, rowmax;
                if (m_modes_private.DEC_ORIGIN() &&
                    m_scrolling_restricted) {
                        origin = m_scrolling_region.start;
                        rowmax = m_scrolling_region.end;
                } else {
                        origin = 0;
                        rowmax = m_row_count - 1;
                }
                // FIXMEchpe this looks wrong. shouldn't this first clamp to origin,rowmax and *then* subtract origin?
                rowval = m_screen->cursor.row - m_screen->insert_delta - origin;
                rowval = CLAMP(rowval, 0, rowmax);

                reply(seq, VTE_REPLY_DECXCPR,
                      {int(rowval + 1), int(CLAMP(m_screen->cursor.col + 1, 1, m_column_count)), 1});
                break;

        case 15:
                /* Request printer port report
                 * Reply: DECDSR
                 *   @arg[0]: status
                 *     10 = printer ready
                 *     11 = printer not ready
                 *     13 = no printer
                 *     18 = printer busy
                 *     19 = printer assigned to another session
                 */
                reply(seq, VTE_REPLY_DECDSR, {13});
                break;

        case 25:
                /* Request user-defined keys report
                 * Reply: DECDSR
                 *   @arg[0]: locked status
                 *      20 = UDK unlocked
                 *      21 = UDK locked
                 *
                 * Since we don't do UDK, we report them as locked.
                 */
                reply(seq, VTE_REPLY_DECDSR, {21});
                break;

        case 26:
                /* Request keyboard report
                 * Reply: DECDSR
                 *   @arg[0]: 27
                 *   @arg[1]: Keyboard language
                 *     0 = undetermined
                 *     1..40
                 *
                 *   @arg[2]: Keyboard status
                 *     0 = ready
                 *     3 = no keyboard
                 *     8 = keyboard busy (used by other session)
                 *
                 *   @arg[3]: Keyboard type
                 *     0 = LK201 (XTERM response)
                 *     4 = LK411
                 *     5 = PCXAL
                 */
                reply(seq, VTE_REPLY_DECDSR, {27, 0, 0, 5});
                break;

        case 53:
                /* XTERM alias for 55 */
                [[fallthrough]];
        case 55:
                /* Request locator status report
                 * Reply: DECDSR
                 *   @arg[0]: status
                 *     50 = locator ready
                 *     53 = no locator
                 *
                 * Since we don't implement the DEC locator mode,
                 * we reply with 53.
                 */
                reply(seq, VTE_REPLY_DECDSR, {53});
                break;

        case 56:
                /* Request locator type report
                 * Reply: DECDSR
                 *   @arg[0]: 57
                 *   @arg[1]: status
                 *     0 = unknown
                 *     1 = mouse
                 *
                 * Since we don't implement the DEC locator mode,
                 * we reply with 0.
                 */
                reply(seq, VTE_REPLY_DECDSR, {57, 0});
                break;

        case 62:
                /* Request macro space report
                 * Reply: DECMSR
                 *   @arg[0]: floor((number of bytes available) / 16); we report 0
                 */
                reply(seq, VTE_REPLY_DECMSR, {0});
                break;

        case 63:
                /* Request memory checksum report
                 * Reply: DECCKSR
                 *   @arg[0]: PID
                 *   DATA: the checksum as a 4-digit hex number
                 *
                 * Reply with a dummy checksum.
                 */
                reply(seq, VTE_REPLY_DECCKSR, {seq.collect1(1)}, "0000");
                break;

        case 75:
                /* Request data integrity report
                 * Reply: DECDSR
                 *   @arg[0]: status
                 *     70 = no error, no power loss, no communication errors
                 *     71 = malfunction or communication error
                 *     73 = no data loss since last power-up
                 */
                reply(seq, VTE_REPLY_DECDSR, {70});
                break;

        case 85:
                /* Request multi-session status report
                 * Reply: DECDSR
                 *   @arg[0]: status
                 *     ...
                 *     83 = not configured
                 */
                reply(seq, VTE_REPLY_DECDSR, {83});
                break;

        default:
                break;
        }
}

void
Terminal::DTA(vte::parser::Sequence const& seq)
{
        /*
         * DTA - dimension text area
         * Set the dimension of the text area.
         *
         * Arguments:
         *  args[0]:
         *  args[1]:
         *
         * Defaults:
         *   args[0]: no default
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.36
         */
}

void
Terminal::EA(vte::parser::Sequence const& seq)
{
        /*
         * EA - erase in area
         * Erase some/all character positions in the qualified area.
         *
         * Arguments:
         *  args[0]: type
         *    0 = Erase the active position and all positions to the end
         *        of the qualified area (inclusive).
         *    1 = Erase from the beginning of the qualified area to
         *        the active position (inclusive).
         *    2 = Erase all of the qualified area.
         *
         * Defaults:
         *   args[0]: 0
         *
         * If ERM is set, erases only non-protected areas; if
         * ERM is reset, erases all areas.
         *
         * Depending on DCSM, this function works on the presentation
         * or data position. Terminal-wg/bidi forces DCSM to DATA.
         *
         * References: ECMA-48 § 8.3.37
         *             Terminal-wg/bidi
         */

        switch (seq.collect1(0)) {
        case -1:
        case 0:
                break;
        }
}

void
Terminal::ECH(vte::parser::Sequence const& seq)
{
        /*
         * ECH - erase-character
         * Erase characters from the active position.
         *
         * DSCM mode controls whether this function operates on the
         * presentation or data position.
         * Also affected by ERM mode.
         *
         * Arguments:
         *   args[0]: number of characters to erase
         *
         * Defaults:
         *   args[0]: 1
         *
         * If ERM is set, erases only non-protected characters; if
         * ERM is reset, erases all characters.
         *
         * Depending on DCSM, this function works on the presentation
         * or data position. Terminal-wg/bidi forces DCSM to DATA.
         *
         * References: ECMA-48 § 8.3.38
         *             Terminal-wg/bidi
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        vte_page_erase(screen->page,
                          screen->state.cursor_x, screen->state.cursor_y,
                          screen->state.cursor_x + num, screen->state.cursor_y,
                          &screen->state.attr, screen->age, false);
#endif

        /* Erase characters starting at the cursor position (overwriting N with
         * spaces, but not moving the cursor). */

        // FIXMEchpe limit to column_count - cursor.x ?
        auto const count = seq.collect1(0, 1, 1, int(65535));
        erase_characters(count, false);
}

void
Terminal::ED(vte::parser::Sequence const& seq)
{
        /*
         * ED - erase-in-display
         * Erases characters.
         * Line attributes of completely erased lines are reset to
         * single-width single-height, and all character attributes
         * are reset to default.
         *
         * Arguments:
         *   args[0]: mode
         *     0 = erase from the cursor position to the end of the screen
         *     1 = erase from the beginning of the screen to the cursor
         *         position (inclusive)
         *     2 = erase display
         *     3 = erase scrollback (XTERM extension)
         *
         * Defaults:
         *   args[0]: 0
         *
         * This function does not respect the scrolling margins.
         *
         * If ERM is set, erases only non-protected characters; if
         * ERM is reset, erases all characters.
         *
         * Depending on DCSM, this function works on the presentation
         * or data position. Terminal-wg/bidi forces DCSM to DATA.
         *
         * References: ECMA-48 § 8.3.39
         *             VT525
         *             Terminal-wg/bidi
         */

        erase_in_display(seq);
}

void
Terminal::EF(vte::parser::Sequence const& seq)
{
        /*
         * EF - erase in field
         * Erases characters in the active field.
         *
         * Arguments:
         *   args[0]: mode
         *    0 = Erase the active position and all positions to the end
         *        of the field (inclusive).
         *    1 = Erase from the beginning of the field to
         *        the active position (inclusive).
         *    2 = Erase all of the qualified area.
         *
         * Defaults:
         *   args[0]: 0
         *
         * If ERM is set, erases only non-protected characters; if
         * ERM is reset, erases all characters.
         *
         * Depending on DCSM, this function works on the presentation
         * or data position. Terminal-wg/bidi forces DCSM to DATA.
         *
         * References: ECMA-48 § 8.3.40
         *             Terminal-wg/bidi
         */
}

void
Terminal::EL(vte::parser::Sequence const& seq)
{
        /*
         * EL - erase-in-line
         * Erases characters.
         *
         * Arguments:
         *   args[0]: mode
         *     0 = erase from the cursor position to the end of the line
         *     1 = erase from the beginning of the line to the cursor
         *         position (inclusive)
         *     2 = erase line (FIXME: does this clear line attributes?)
         *
         * Defaults:
         *   args[0]: 0
         *
         * This function does not respect the scrolling margins.
         *
         * If ERM is set, erases only non-protected characters; if
         * ERM is reset, erases all characters.
         *
         * Depending on DCSM, this function works on the presentation
         * or data position. Terminal-wg/bidi forces DCSM to DATA.
         *
         * References: ECMA-48 § 8.3.41
         *             VT525
         *             Terminal-wg/bidi
         */

        erase_in_line(seq);
}

void
Terminal::EM(vte::parser::Sequence const& seq)
{
        /*
         * EM - end of medium
         *
         * References: ECMA-48 § 8.3.42
         */
}

void
Terminal::EMI(vte::parser::Sequence const& seq)
{
        /*
         * DMI - enable manual input
         *
         * References: ECMA-48 § 8.3.43
         *
         * Probably not worth implementing.
         */
}

void
Terminal::ENQ(vte::parser::Sequence const& seq)
{
        /*
         * ENQ - enquiry
         * Transmit the answerback-string. If none is set, do nothing.
         *
         * References: ECMA-48 § 8.3.44
         *             ECMA-16 § 3.1.5
         */

        /* No-op for security reasons */
}

void
Terminal::EOT(vte::parser::Sequence const& seq)
{
        /*
         * EOT - end of transmission
         *
         * References: ECMA-48 § 8.3.45
         *             ECMA-16 § 3.1.4
         *
         * Not worth implementing.
         */
}

void
Terminal::EPA(vte::parser::Sequence const& seq)
{
        /*
         * EPA - end of guarded area
         * Marks the end of an area of positions (presentation)
         * that are protected; the beginning of the area was
         * marked by SPA.
         *
         * The contents of the area will be protected against
         * alteration, transfer (depending on the GATM setting),
         * and erasure (depending on the ERM setting).
         *
         * References: ECMA-48 § 8.3.46
         */
}

void
Terminal::ESA(vte::parser::Sequence const& seq)
{
        /*
         * ESA - end of selected area
         * Marks the end of an area of positions (presentation)
         * that are selected for transfer; the beginning of the area
         * was marked by SSA.
         *
         * References: ECMA-48 § 8.3.47
         */
}

void
Terminal::ETB(vte::parser::Sequence const& seq)
{
        /*
         * ETB - end of transmission block
         *
         * References: ECMA-48 § 8.3.49
         *             ECMA-16 § 3.1.10
         *
         * Not worth implementing.
         */
}

void
Terminal::ETX(vte::parser::Sequence const& seq)
{
        /*
         * ETX - end of text
         *
         * References: ECMA-48 § 8.3.49
         *             ECMA-16 § 3.1.3
         *
         * Not worth implementing.
         */
}

void
Terminal::FF(vte::parser::Sequence const& seq)
{
        /*
         * FF - form-feed
         * This causes the cursor to jump to the next line (presentation).
         *
         * References: ECMA-48 § 8.3.51
         */

        LF(seq);
}

void
Terminal::FNK(vte::parser::Sequence const& seq)
{
        /*
         * FNK - function key
         *
         * Arguments:
         *  args[0]: function key that was operated
         *
         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.52
         *
         * Probably not worth implementing.
         */
}

void
Terminal::FNT(vte::parser::Sequence const& seq)
{
        /*
         * FNT - font selection
         * Select the font to be used by subsequent SGR 10…19.
         *
         * Arguments:
         *  args[0]: the font 0…9
         *  args[1]: font identifier
         *
         * Defaults:
         *   args[0]: 0
         *   args[1]: 0
         *
         * References: ECMA-48 § 8.3.53
         *
         * Probably not worth implementing.
         */
}

void
Terminal::GCC(vte::parser::Sequence const& seq)
{
        /*
         * GCC - graphic character combination
         * Two or more graphic characters that follow should be
         * imaged as one symbol.
         *
         * Arguments:
         *  args[0]: mode
         *    0 = Combine the following two graphic characters
         *    1 = Start of string of characters to be combined
         *    2 = End of string of characters to be combined
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.54
         *             ECMA-43 Annex C
         */
}

void
Terminal::GSM(vte::parser::Sequence const& seq)
{
        /*
         * GSM - graphic size modification
         *
         * Arguments:
         *  args[0]: height as percentage of height set by GSS
         *  args[1]: width as percentage of width set by GSS
         *
         * Defaults:
         *   args[0]: 100
         *   args[0]: 100
         *
         * References: ECMA-48 § 8.3.55
         *
         * Not applicable to VTE.
         */
}

void
Terminal::GSS(vte::parser::Sequence const& seq)
{
        /*
         * GSM - graphic size selection
         *
         * Arguments:
         *  args[0]: size in the unit set by SSU
         *
         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.56
         *
         * Not applicable to VTE.
         */
}

void
Terminal::GnDm(vte::parser::Sequence const& seq)
{
        /*
         * GnDm - Gn-designate 9m-charset
         *
         * Designate character sets to G-sets.
         *
         * References: ECMA-35 § 14.3
         *             ISO 2375 IR
         */

        /* Since we mostly don't implement ECMA-35 anymore, we can mostly ignore this. */

        VteCharacterReplacement replacement;
        switch (seq.charset()) {
        case VTE_CHARSET_DEC_SPECIAL_GRAPHIC:
                /* Some characters replaced by line drawing characters.
                 * This is still used by ncurses :-(
                 */
                replacement = VTE_CHARACTER_REPLACEMENT_LINE_DRAWING;
                break;

        default:
                replacement = VTE_CHARACTER_REPLACEMENT_NONE;
                break;
        }

        unsigned int slot = seq.slot();
        if (slot >= G_N_ELEMENTS(m_character_replacements))
                return;

        m_character_replacements[slot] = replacement;
}

void
Terminal::GnDMm(vte::parser::Sequence const& seq)
{
        /*
         * GnDm - Gn-designate multibyte 9m-charset
         *
         * Designate multibyte character sets to G-sets.
         *
         * References: ECMA-35 § 14.3
         *             ISO 2375 IR
         */

        /* Since we mostly don't implement ECMA-35 anymore, we can ignore this */
}

void
Terminal::HPA(vte::parser::Sequence const& seq)
{
        /*
         * HPA - horizontal position absolute
         * Move the active position (data) to the position specified by @args[0]
         * in the active line.
         *
         * Arguments:
         *   args[0]: position (data)
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.57
         *             VT525
         */

#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_set(screen, num - 1, screen->state.cursor_y);
#endif

        auto value = seq.collect1(0, 1, 1, m_column_count);
        set_cursor_column1(value);
}

void
Terminal::HPB(vte::parser::Sequence const& seq)
{
        /*
         * HPB - horizontal position backward
         * Move the active position (data) to the backward by @args[0] positions
         * in the active line.
         *
         * Arguments:
         *   args[0]: number of positions to move
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.58
         */
}

void
Terminal::HPR(vte::parser::Sequence const& seq)
{
        /*
         * HPR - horizontal-position-relative
         * Move the active position (data) to the foward by @args[0] positions
         * in the active line.
         *
         * Arguments:
         *   args[0]: number of positions to move
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.59
         *             VT525
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_right(screen, num);
#endif
}

void
Terminal::HT(vte::parser::Sequence const& seq)
{
        /*
         * HT - character tabulation
         * Move the active position (presentation) to the next tab stop.
         * If there are no more tab stops, the cursor moves to the right
         * margin. Does not cause text to auto wrap.
         *
         * (If that next tabstop was set by TAC, TALE, TATE or TCC,
         * the properties of that tabstop will determine how subsequently
         * inserted text is positioned.)
         *
         * References: ECMA-48 § 8.3.60
         *             VT525
         */
#if 0
        screen_cursor_clear_wrap(screen);
#endif

        move_cursor_tab_forward();
}

void
Terminal::HTJ(vte::parser::Sequence const& seq)
{
        /*
         * HTJ - character tabulation with justification
         *
         * References: ECMA-48 § 8.3.61
         *             VT525
         */
#if 0
        screen_cursor_clear_wrap(screen);
#endif

        move_cursor_tab_forward();
}

void
Terminal::HTS(vte::parser::Sequence const& seq)
{
        /*
         * HTS - horizontal-tab-set
         * Set a tabstop at the active position (presentation).
         *
         * Affected by TSM mode.
         *
         * References: ECMA-48 § 8.3.62
         *             VT525
         */

        m_tabstops.set(get_cursor_column());
}

void
Terminal::HVP(vte::parser::Sequence const& seq)
{
        /*
         * HVP - horizontal-and-vertical-position
         * Sets the active position (data)
         *
         * Arguments:
         *   args[0]: the line
         *   args[1]: the column
         *
         * Defaults:
         *   args[0]: 1
         *   args[1]: 1
         *
         * If DECOM is set, the position is relative to the top/bottom
         * margins, and may not be outside it.
         *
         * References: ECMA-48 § 8.3.63
         *             VT525
         */

        CUP(seq);
}

void
Terminal::ICH(vte::parser::Sequence const& seq)
{
        /*
         * ICH - insert-character
         * Inserts SPACE (2/0) character(s) at the cursor position.
         *
         * Arguments:
         *   args[0]: the number of characters to insert
         *
         * Defaults:
         *   args[0]: 1
         *
         * Depending on DCSM, this function works on the presentation
         * or data position. Terminal-wg/bidi forces DCSM to DATA.

         * Also affected by HEM mode, and the SLH, and SEE functions.
         *
         * References: ECMA-48 §8.3.64
         *             VT525
         *             Terminal-wg/bidi
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        vte_page_insert_cells(screen->page,
                                 screen->state.cursor_x,
                                 screen->state.cursor_y,
                                 num,
                                 &screen->state.attr,
                                 screen->age);
#endif

        auto const count = seq.collect1(0, 1, 1, int(m_column_count - m_screen->cursor.col));

        /* TODOegmont: Insert them in a single run, so that we call cleanup_fragments only once. */
        for (auto i = 0; i < count; i++)
                insert_blank_character();
}

void
Terminal::IDCS(vte::parser::Sequence const& seq)
{
        /*
         * IDCS - identify device control string
         *
         * Arguments:
         *   args[0]: mode
         *     1 = reserved for use with SRTM mode
         *     2 = reservewd for DRCS according to ECMA-35

         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.65
         */
}

void
Terminal::IGS(vte::parser::Sequence const& seq)
{
        /*
         * IGS - identify graphic subrepertoire
         * Specifies a repertoire of graphic characters to be used
         * in the following text.
         *
         * Arguments:
         *   args[0]: identifier from ISO 7350 registry

         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.66
         *             ISO/IEC 7350
         *             ISO/IEC 10367
         *
         * Not worth implementing.
         */
}

void
Terminal::IL(vte::parser::Sequence const& seq)
{
        /*
         * IL - insert-line
         * Insert (a) blank line(s) at the active position.
         *
         * Arguments:
         *   args[0]: the number of lines
         *
         * Defaults:
         *   args[0]: 1
         *
         * Depending on DCSM, this function works on the presentation
         * or data position. Terminal-wg/bidi forces DCSM to DATA.
         *
         * Also affected by the TSM and VEM modes,
         * and the SLH and SEE functions.
         *
         * References: ECMA-48 § 8.3.67
         *             DEC STD 070 page 5-146
         *             Terminal-wg/bidi
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        vte_page_insert_lines(screen->page,
                                 screen->state.cursor_y,
                                 num,
                                 &screen->state.attr,
                                 screen->age);
#endif

        auto const count = seq.collect1(0, 1);
        insert_lines(count);
}

void
Terminal::IND(vte::parser::Sequence const& seq)
{
        /*
         * IND - index - DEPRECATED
         *
         * References: ECMA-48 § F.8.2
         */

        LF(seq);
}

void
Terminal::INT(vte::parser::Sequence const& seq)
{
        /*
         * INT - interrupt
         *
         * References: ECMA-48 § 8.3.68
         */
}

void
Terminal::IRR(vte::parser::Sequence const& seq)
{
        /*
         * IRR - identify-revised-registration
         *
         * Note that the IRR comes _before_ the GnDm/GnDMm/CnD, see e.g.
         * IR#124 whose C1 designation sequence is ESC 2/6 4/0 ESC 2/2 4/2,
         * i.e. IRR '@', C1D 'B'.
         *
         * References: ECMA-35 § 14.5
         *
         * Probably not worth implementing.
         */

        /* Since we mostly don't implement ECMA-35 anymore, we can ignore this */
}

void
Terminal::IS1(vte::parser::Sequence const& seq)
{
        /*
         * IS1 - information separator 1 / unit separator (US)
         *
         * References: ECMA-48 § 8.3.69, § 8.2.10
         */
}

void
Terminal::IS2(vte::parser::Sequence const& seq)
{
        /*
         * IS2 - information separator 2 / record separator (RS)
         *
         * References: ECMA-48 § 8.3.70, § 8.2.10
         */
}

void
Terminal::IS3(vte::parser::Sequence const& seq)
{
        /*
         * IS3 - information separator 3 / group separator (GS)
         *
         * References: ECMA-48 § 8.3.71, § 8.2.10
         */
}

void
Terminal::IS4(vte::parser::Sequence const& seq)
{
        /*
         * IS4 - information separator 4 / file separator (FS)
         *
         * References: ECMA-48 § 8.3.72, § 8.2.10
         */
}

void
Terminal::JFY(vte::parser::Sequence const& seq)
{
        /*
         * JFY - justify
         *
         * References: ECMA-48 § 8.3.73
         *
         * Probably not worth implementing.
         */
}

void
Terminal::LF(vte::parser::Sequence const& seq)
{
        /*
         * LF - line-feed
         * XXXX
         *
         * References: ECMA-48 § 8.3.74
         */

#if 0
        screen_cursor_down(screen, 1, true);
        if (screen->flags & VTE_FLAG_NEWLINE_MODE)
                screen_cursor_left(screen, screen->state.cursor_x);
#endif

        line_feed();
}

void
Terminal::LS0(vte::parser::Sequence const& seq)
{
        /*
         * LS0 -locking shift 0 (8 bit)
         * SI - shift-in (7 bit)
         *
         * Map G0 into GL.
         *
         * References: ECMA-35 § 9.3.1
         *             ECMA-48 § 8.3.75, 8.3.119
         */
#if 0
        screen->state.gl = &screen->g0;
#endif

        set_character_replacement(0);
}

void
Terminal::LS1(vte::parser::Sequence const& seq)
{
        /*
         * LS1 -locking shift 1 (8 bit)
         * SO - shift-out (7 bit)
         *
         * Map G1 into GL.
         *
         * References: ECMA-35 § 9.3.1
         *             ECMA-48 § 8.3.76, 8.3.126
         */
#if 0
        screen->state.gl = &screen->g1;
#endif

        set_character_replacement(1);
}

void
Terminal::LS1R(vte::parser::Sequence const& seq)
{
        /*
         * LS1R - locking-shift-1-right
         * Map G1 into GR.
         *
         * References: ECMA-35 § 9.3.2
         *             ECMA-48 § 8.3.77
         */
#if 0
        screen->state.gr = &screen->g1;
#endif
}

void
Terminal::LS2(vte::parser::Sequence const& seq)
{
        /*
         * LS2 - locking-shift-2
         * Map G2 into GL.
         *
         * References: ECMA-35 § 9.3.1
         *             ECMA-48 § 8.3.78
         */
#if 0
        screen->state.gl = &screen->g2;
#endif
}

void
Terminal::LS2R(vte::parser::Sequence const& seq)
{
        /*
         * LS2R - locking-shift-2-right
         * Map G2 into GR.
         *
         * References: ECMA-35 § 9.3.2
         *             ECMA-48 § 8.3.79
         */
#if 0
        screen->state.gr = &screen->g2;
#endif
}

void
Terminal::LS3(vte::parser::Sequence const& seq)
{
        /*
         * LS3 - locking-shift-3
         * Map G3 into GL.
         *
         * References: ECMA-35 § 9.3.1
         *             ECMA-48 § 8.3.80
         */

#if 0
        screen->state.gl = &screen->g3;
#endif
}

void
Terminal::LS3R(vte::parser::Sequence const& seq)
{
        /*
         * LS3R - locking-shift-3-right
         * Map G3 into GR.
         *
         * References: ECMA-35 § 9.3.2
         *             ECMA-48 § 8.3.81
         */
#if 0
        screen->state.gr = &screen->g3;
#endif
}

void
Terminal::MC_ECMA(vte::parser::Sequence const& seq)
{
        /*
         * MC_ECMA - media-copy-ecma
         *
         * References: ECMA-48 § 8.3.82
         *             VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::MC_DEC(vte::parser::Sequence const& seq)
{
        /*
         * MC_DEC - media-copy-dec
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::MW(vte::parser::Sequence const& seq)
{
        /*
         * MW - message waiting
         *
         * References: ECMA-48 § 8.3.83
         *
         * Not worth implementing.
         */
}

void
Terminal::NAK(vte::parser::Sequence const& seq)
{
        /*
         * NAK - negative acknowledge
         *
         * References: ECMA-48 § 8.3.84
         *             ECMA-16 § 3.1.8
         *
         * Not worth implementing.
         */
}

void
Terminal::NBH(vte::parser::Sequence const& seq)
{
        /*
         * BPH - no break permitted here
         *
         * References: ECMA-48 § 8.3.85
         *
         * Not worth implementing.
         */
}

void
Terminal::NEL(vte::parser::Sequence const& seq)
{
        /*
         * NEL - next-line
         * Moves the cursor to the first column in the next line.
         * If the cursor is on the bottom margin, this scrolls up.
         *
         * References: ECMA-48 § 8.3.86
         */
#if 0
        screen_cursor_clear_wrap(screen);
        screen_cursor_down(screen, 1, true);
        screen_cursor_set(screen, 0, screen->state.cursor_y);
#endif

        set_cursor_column(0);
        cursor_down(true);
}

void
Terminal::NP(vte::parser::Sequence const& seq)
{
        /*
         * NP - next-page
         * Move cursor to home on the next page (presentation).
         * (Ignored if there is only one page.)
         *
         * Arguments:
         *   args[0]: number of pages to move forward
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.87
         *             VT525
         *
         * Since VTE only has one page, this is ignored.
         */
}

void
Terminal::NUL(vte::parser::Sequence const& seq)
{
        /*
         * NUL - nothing
         *
         * References: ECMA-48 § 8.3.88
         */
}

void
Terminal::OSC(vte::parser::Sequence const& seq)
{
        /*
         * OSC - operating system command
         *
         * References: ECMA-48 § 8.3.89
         *             XTERM
         */

        /* Our OSC have the format
         *   OSC number ; rest of string ST
         * where the rest of the string may or may not contain more semicolons.
         *
         * First, extract the number.
         */

        auto str = seq.string_utf8();
        vte::parser::StringTokeniser tokeniser{str, ';'};
        auto it = tokeniser.cbegin();
        int osc;
        if (!it.number(osc))
                return;

        auto const cend = tokeniser.cend();
        ++it; /* could now be cend */

        switch (osc) {
        case VTE_OSC_VTECWF:
                set_current_file_uri(seq, it, cend);
                break;

        case VTE_OSC_VTECWD:
                set_current_directory_uri(seq, it, cend);
                break;

        case VTE_OSC_VTEHYPER:
                set_current_hyperlink(seq, it, cend);
                break;

        case -1: /* default */
        case VTE_OSC_XTERM_SET_WINDOW_AND_ICON_TITLE:
        case VTE_OSC_XTERM_SET_WINDOW_TITLE: {
                /* Only sets window title; icon title is not supported */
                std::string title;
                if (it != cend &&
                    it.size_remaining() < VTE_WINDOW_TITLE_MAX_LENGTH)
                        title = it.string_remaining();
                m_window_title_pending.swap(title);
                m_pending_changes |= vte::to_integral(PendingChanges::TITLE);
                break;
        }

        case VTE_OSC_XTERM_SET_COLOR:
        case VTE_OSC_XTERM_SET_COLOR_SPECIAL:
                set_color(seq, it, cend, osc);
                break;

        case VTE_OSC_XTERM_SET_COLOR_TEXT_FG:
                set_special_color(seq, it, cend, VTE_DEFAULT_FG, -1, osc);
                break;

        case VTE_OSC_XTERM_SET_COLOR_TEXT_BG:
                set_special_color(seq, it, cend, VTE_DEFAULT_BG, -1, osc);
                break;

        case VTE_OSC_XTERM_SET_COLOR_CURSOR_BG:
                set_special_color(seq, it, cend, VTE_CURSOR_BG, VTE_DEFAULT_FG, osc);
                break;

        case VTE_OSC_XTERM_SET_COLOR_HIGHLIGHT_BG:
                set_special_color(seq, it, cend, VTE_HIGHLIGHT_BG, VTE_DEFAULT_FG, osc);
                break;

        case VTE_OSC_XTERM_SET_COLOR_HIGHLIGHT_FG:
                set_special_color(seq, it, cend, VTE_HIGHLIGHT_FG, VTE_DEFAULT_BG, osc);
                break;

        case VTE_OSC_XTERM_RESET_COLOR:
        case VTE_OSC_XTERM_RESET_COLOR_SPECIAL:
                reset_color(seq, it, cend, osc);
                break;

        case VTE_OSC_XTERM_RESET_COLOR_TEXT_FG:
                reset_color(VTE_DEFAULT_FG, VTE_COLOR_SOURCE_ESCAPE);
                break;

        case VTE_OSC_XTERM_RESET_COLOR_TEXT_BG:
                reset_color(VTE_DEFAULT_BG, VTE_COLOR_SOURCE_ESCAPE);
                break;

        case VTE_OSC_XTERM_RESET_COLOR_CURSOR_BG:
                reset_color(VTE_CURSOR_BG, VTE_COLOR_SOURCE_ESCAPE);
                break;

        case VTE_OSC_XTERM_RESET_COLOR_HIGHLIGHT_BG:
                reset_color(VTE_HIGHLIGHT_BG, VTE_COLOR_SOURCE_ESCAPE);
                break;

        case VTE_OSC_XTERM_RESET_COLOR_HIGHLIGHT_FG:
                reset_color(VTE_HIGHLIGHT_FG, VTE_COLOR_SOURCE_ESCAPE);
                break;

        case VTE_OSC_XTERM_SET_ICON_TITLE:
        case VTE_OSC_XTERM_SET_XPROPERTY:
        case VTE_OSC_XTERM_SET_COLOR_MOUSE_CURSOR_FG:
        case VTE_OSC_XTERM_SET_COLOR_MOUSE_CURSOR_BG:
        case VTE_OSC_XTERM_SET_COLOR_TEK_FG:
        case VTE_OSC_XTERM_SET_COLOR_TEK_BG:
        case VTE_OSC_XTERM_SET_COLOR_TEK_CURSOR:
        case VTE_OSC_XTERM_SET_CURSOR_NAME:
        case VTE_OSC_XTERM_LOGFILE:
        case VTE_OSC_XTERM_SET_FONT:
        case VTE_OSC_XTERM_SET_XSELECTION:
        case VTE_OSC_XTERM_SET_COLOR_MODE:
        case VTE_OSC_XTERM_RESET_COLOR_MOUSE_CURSOR_FG:
        case VTE_OSC_XTERM_RESET_COLOR_MOUSE_CURSOR_BG:
        case VTE_OSC_XTERM_RESET_COLOR_TEK_FG:
        case VTE_OSC_XTERM_RESET_COLOR_TEK_BG:
        case VTE_OSC_XTERM_RESET_COLOR_TEK_CURSOR:
        case VTE_OSC_EMACS_51:
        case VTE_OSC_ITERM2_133:
        case VTE_OSC_ITERM2_1337:
        case VTE_OSC_ITERM2_GROWL:
        case VTE_OSC_KONSOLE_30:
        case VTE_OSC_KONSOLE_31:
        case VTE_OSC_RLOGIN_SET_KANJI_MODE:
        case VTE_OSC_RLOGIN_SPEECH:
        case VTE_OSC_RXVT_SET_BACKGROUND_PIXMAP:
        case VTE_OSC_RXVT_SET_COLOR_FG:
        case VTE_OSC_RXVT_SET_COLOR_BG:
        case VTE_OSC_RXVT_DUMP_SCREEN:
        case VTE_OSC_URXVT_SET_LOCALE:
        case VTE_OSC_URXVT_VERSION:
        case VTE_OSC_URXVT_SET_COLOR_TEXT_ITALIC:
        case VTE_OSC_URXVT_SET_COLOR_TEXT_BOLD:
        case VTE_OSC_URXVT_SET_COLOR_UNDERLINE:
        case VTE_OSC_URXVT_SET_COLOR_BORDER:
        case VTE_OSC_URXVT_SET_FONT:
        case VTE_OSC_URXVT_SET_FONT_BOLD:
        case VTE_OSC_URXVT_SET_FONT_ITALIC:
        case VTE_OSC_URXVT_SET_FONT_BOLD_ITALIC:
        case VTE_OSC_URXVT_VIEW_UP:
        case VTE_OSC_URXVT_VIEW_DOWN:
        case VTE_OSC_URXVT_EXTENSION:
        case VTE_OSC_YF_RQGWR:
        default:
                break;
        }
}

void
Terminal::PEC(vte::parser::Sequence const& seq)
{
        /*
         * PEC - presentation expand or contract
         *
         * References: ECMA-48 § 8.3.90
         *
         * Not applicable in VTE.
         */
}

void
Terminal::PFS(vte::parser::Sequence const& seq)
{
        /*
         * PFS - page format selection
         *
         * References: ECMA-48 § 8.3.91
         *
         * Not applicable in VTE.
         */
}

void
Terminal::PLD(vte::parser::Sequence const& seq)
{
        /*
         * PLD - partial line forward
         *
         * References: ECMA-48 § 8.3.92
         *
         * Could use this to implement subscript text.
         */
}

void
Terminal::PLU(vte::parser::Sequence const& seq)
{
        /*
         * PLU - partial line backward
         *
         * References: ECMA-48 § 8.3.93
         *
         * Could use this to implement superscript text.
         */
}

void
Terminal::PP(vte::parser::Sequence const& seq)
{
        /*
         * PP - preceding page
         * Move cursor to home on the previous page (presentation).
         * (Ignored if there is only one page.)
         *
         * Arguments:
         *   args[0]: number of pages to move backward
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.95
         *             VT525
         *
         * Since VTE only has one page, this is ignored.
         */
}

void
Terminal::PPA(vte::parser::Sequence const& seq)
{
        /*
         * PPA - page position absolute
         * Move the cursor to the current position on the specified page
         * (data).
         * (Ignored if there is only one page.)
         *
         * Arguments:
         *   args[0]: absolute page number
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.96
         *             VT525
         *
         * Since VTE only has one page, this is ignored.
         */
}

void
Terminal::PPB(vte::parser::Sequence const& seq)
{
        /*
         * PPB - page position backward
         * Move the cursor to the current position on a preceding page (data).
         * (Ignored if there is only one page.)
         *
         * Arguments:
         *   args[0]: number of pages to move backward
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.97
         *             VT525
         *
         * Since VTE only has one page, this is ignored.
         */
}

void
Terminal::PPR(vte::parser::Sequence const& seq)
{
        /*
         * PPR - page position foward
         * Move the cursor to the current position on a following page (data).
         * (Ignored if there is only one page.)
         *
         * Arguments:
         *   args[0]: number of pages to move forward
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.98
         *             VT525
         *
         * Since VTE only has one page, this is ignored.
         */
}

void
Terminal::PTX(vte::parser::Sequence const& seq)
{
        /*
         * PTX - parallel texts
         *
         * Arguments:
         *   args[0]: mode
         *     0 = End of parallel texts
         *     1 = Start of a string of principal parallel text
         *     2 = Start of a string of supplementary parallel text
         *     3 = Start of a string of supplementary japanese
         *         phonetic annotations
         *     4 = Start of a string of supplementary chinese
         *         phonetic annotations
         *     5 = Start of a string of supplementary phonetic
         *        annotations
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.99
         *             VT525
         *
         * Since VTE only has one page, this is ignored.
         */
}

void
Terminal::PU1(vte::parser::Sequence const& seq)
{
        /*
         * PU1 - private use 1
         *
         * References: ECMA-48 § 8.3.100
         *
         * Not worth implementing.
         */
}

void
Terminal::PU2(vte::parser::Sequence const& seq)
{
        /*
         * PU1 - private use 2
         *
         * References: ECMA-48 § 8.3.101
         *
         * Not worth implementing.
         */
}

void
Terminal::QUAD(vte::parser::Sequence const& seq)
{
        /*
         * QUAD - quad
         *
         * References: ECMA-48 § 8.3.102
         *
         * Probably not worth implementing.
         */
}

void
Terminal::REP(vte::parser::Sequence const& seq)
{
        /*
         * REP - repeat
         * Repeat the preceding graphics-character the given number of times.
         * @args[0] specifies how often it shall be repeated. 0 is treated as 1.
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.103
         */

        if (m_last_graphic_character == 0)
                return;

        auto const count = seq.collect1(0, 1, 1, int(m_column_count - m_screen->cursor.col));

        // FIXMEchpe insert in one run so we only clean up fragments once
        for (auto i = 0; i < count; i++)
                insert_char(m_last_graphic_character, false, true);
}

void
Terminal::RI(vte::parser::Sequence const& seq)
{
        /*
         * RI - reverse-index
         * Moves the cursor up one line in the same column. If the cursor is at
         * the top margin, the page scrolls down.
         *
         * References: ECMA-48 § 8.3.104
         */
#if 0
        screen_cursor_up(screen, 1, true);
#endif

        ensure_cursor_is_onscreen();

        vte::grid::row_t start, end;
        if (m_scrolling_restricted) {
                start = m_scrolling_region.start + m_screen->insert_delta;
                end = m_scrolling_region.end + m_screen->insert_delta;
	} else {
                start = m_screen->insert_delta;
                end = start + m_row_count - 1;
	}

        if (m_screen->cursor.row == start) {
		/* If we're at the top of the scrolling region, add a
		 * line at the top to scroll the bottom off. */
		ring_remove(end);
		ring_insert(start, true);

                /* Set the boundaries to hard wrapped where we tore apart the contents.
                 * Need to do it after scrolling down, for the end row to be the desired one. */
                set_hard_wrapped(start - 1);
                set_hard_wrapped(end);

                /* Repaint the affected lines. No need to extend, set_hard_wrapped() took care of
                 * invalidating the context lines if necessary. */
                invalidate_rows(start, end);
	} else {
		/* Otherwise, just move the cursor up. */
                m_screen->cursor.row--;
	}
	/* Adjust the scrollbars if necessary. */
        adjust_adjustments();
	/* We modified the display, so make a note of it. */
        m_text_modified_flag = TRUE;
}

void
Terminal::RIS(vte::parser::Sequence const& seq)
{
        /*
         * RIS - reset-to-initial-state
         * Reset to initial state.
         * [list of things reset]
         *
         * When not using private colour registers, this
         * must also clear (assign to black) all SIXEL
         * colour registers. (DEC PPLV2 § 5.8)
         *
         * References: ECMA-48 § 8.3.105
         */

	reset(true, true);
}

void
Terminal::RLOGIN_MML(vte::parser::Sequence const& seq)
{
        /*
         * RLOGIN_MML - RLogin music markup language
         * Music macro language and Midi file support.
         *
         * The music macro language appears to be (a variant of,
         * or based on) the Microsoft and/or Tandy BASIC MML, see
         * [http://www.vgmpf.com/Wiki/index.php?title=Microsoft_BASIC_MML]
         * and
         * [http://www.vgmpf.com/Wiki/index.php?title=Tandy_BASIC_MML].
         * for more information on them, and the RLogin source code; as
         * well as [http://nanno.dip.jp/softlib/man/rlogin/ctrlcode.html#DCS]
         * for this escape sequence's parameters.
         *
         * Probably not worth implementing.
         *
         * References: RLogin
         */
}

void
Terminal::RM_ECMA(vte::parser::Sequence const& seq)
{
        /*
         * RM_ECMA - reset-mode-ecma
         *
         * Defaults: none
         *
         * References: ECMA-48 § 8.3.106
         */

        set_mode_ecma(seq, false);
}

void
Terminal::RM_DEC(vte::parser::Sequence const& seq)
{
        /*
         * RM_DEC - reset-mode-dec
         * This is the same as RM_ECMA but for DEC modes.
         *
         * Defaults: none
         *
         * References: VT525
         */

        set_mode_private(seq, false);
}


void
Terminal::RM_HP(vte::parser::Sequence const& seq)
{
        /*
         * RM_HP - set mode hp
         * This is the same as RM_ECMA but for HP private modes.
         *
         * See SM_HP for information about known modes.
         *
         * Defaults: none
         *
         * References: HP 2397A
         */

        /* Not worth implementing */
}
void
Terminal::SCORC(vte::parser::Sequence const& seq)
{
        /*
         * SCORC - SCO restore cursor
         * Works like DECRC, except in that it does not restore the page.
         * While this is an obsolete sequence from an obsolete terminal,
         * and not used in terminfo, there still are some programmes
         * that use it and break when it's not implemented; see issue#48.
         *
         * References: VT525
         */

        if (m_modes_private.DECLRMM())
                return;

        restore_cursor();
}

void
Terminal::SCOSC(vte::parser::Sequence const& seq)
{
        /*
         * SCORC - SCO save cursor
         * Works like DECSC, except in that it does not save the page.
         * While this is an obsolete sequence from an obsolete terminal,
         * and not used in terminfo, there still are some programmes
         * that use it and break when it's not implemented; see issue#48.
         *
         * References: VT525
         */

        save_cursor();
}

void
Terminal::SACS(vte::parser::Sequence const& seq)
{
        /*
         * SACS - set additional character separation
         *
         * Arguments:
         *   args[0]: spacing (in the unit set by SSU)
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.107
         *
         * Not applicable in VTE.
         */
}

void
Terminal::SAPV(vte::parser::Sequence const& seq)
{
        /*
         * SAPV - select alternative presentation variants
         * Set variants for the presentation of following text.
         *
         * Arguments:
         *   args[0]: type
         *     0 = default presentation; cancels the previous SAPV
         *     ...
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.108
         */
}

void
Terminal::SCO(vte::parser::Sequence const& seq)
{
        /*
         * SCO - select character orientation
         * Set the rotation for the presentation of following text.
         * (positive orientation).
         *
         * Arguments:
         *   args[0]: orientation 0…7 specifying a multiple of 45°
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.110
         */
}

void
Terminal::SCP(vte::parser::Sequence const& seq)
{
        /*
         * SCP - select character path
         * Set the character path relative to the line orientation
         * (presentation).
         *
         * Arguments:
         *   args[0]: path
         *     0 in Terminal-wg/bidi and VTE = terminal's default
         *     1 = LTR or TTB (for horizontal/vertical line orientation)
         *     2 = RTL or BTT (for horizontal/vertical line orientation)
         *   args[1]: effect
         *     0 in ECMA = implementation-defined
         *     0 in Terminal-wg/bidi and VTE = see Terminal-wg/bidi
         *     1 = ...
         *     2 = ...
         *
         * Defaults:
         *   args[0] in ECMA: no default
         *   args[1] in ECMA: no default
         *   args[0] in Terminal-wg/bidi: 0
         *   args[1] in Terminal-wg/bidi: 0
         *
         * References: ECMA-48 § 8.3.111
         *             Terminal-wg/bidi
         */

        auto const param = seq.collect1(0);
        switch (param) {
        case -1:
        case 0:
                /* FIXME switch to the emulator's default, once we have that concept */
                m_bidi_rtl = FALSE;
                _vte_debug_print(VTE_DEBUG_BIDI, "BiDi: default direction restored\n");
                break;
        case 1:
                m_bidi_rtl = FALSE;
                _vte_debug_print(VTE_DEBUG_BIDI, "BiDi: switch to LTR\n");
                break;
        case 2:
                m_bidi_rtl = TRUE;
                _vte_debug_print(VTE_DEBUG_BIDI, "BiDi: switch to RTL\n");
                break;
        default:
                return;
        }

        maybe_apply_bidi_attributes(VTE_BIDI_FLAG_RTL);
}

void
Terminal::SCS(vte::parser::Sequence const& seq)
{
        /*
         * SCS - set character spacing
         *
         * Arguments:
         *   args[0]: spacing (in the unit set by SSU)
         *
         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.112
         */
}

void
Terminal::SD(vte::parser::Sequence const& seq)
{
        /*
         * SD - scroll down / pan up
         * Scrolls down a number of lines (presentation).
         *
         * Arguments:
         *   args[0]: number of lines to scroll
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.113
         *             VT525
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        vte_page_scroll_down(screen->page,
                                num,
                                &screen->state.attr,
                                screen->age,
                                NULL);
#endif

        /* Scroll the text down N lines, but don't move the cursor. */
        auto value = std::max(seq.collect1(0, 1), int(1));
        scroll_text(value);
}

void
Terminal::SD_OR_XTERM_IHMT(vte::parser::Sequence const& seq)
{
        /*
         * There's a conflict between SD and XTERM IHMT that we
         * have to resolve by checking the parameter count.
         * XTERM_IHMT needs exactly 5 arguments, SD takes 0 or 1.
         */
        if (seq.size_final() <= 1)
                SD(seq);
        #ifdef PARSER_INCLUDE_NOP
        else
                XTERM_IHMT(seq);
        #endif
}

void
Terminal::SDS(vte::parser::Sequence const& seq)
{
        /*
         * SDS - start directed string
         *
         * Arguments:
         *   args[0]: direction
         *     0 = End of directed string
         *     1 = Start of LTR string
         *     2 = Start of RTL string
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.114
         */
}

void
Terminal::SEE(vte::parser::Sequence const& seq)
{
        /*
         * SEE - select editing extent
         *
         * Arguments:
         *   args[0]: extent
         *     0 = ...
         *     1 = ...
         *     2 = ...
         *     3 = ...
         *     4 = ...
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.115
         */
}

void
Terminal::SEF(vte::parser::Sequence const& seq)
{
        /*
         * SEF - sheet eject and feed
         *
         * Arguments:
         *   args[0]:
         *   args[1]:
         *
         * Defaults:
         *   args[0]: 0
         *   args[1]: 0
         *
         * References: ECMA-48 § 8.3.116
         *
         * Probably not worth implementing.
         */
}

void
Terminal::SGR(vte::parser::Sequence const& seq)
{
        /*
         * SGR - select-graphics-rendition
         * Selects the character attributes to use for newly inserted
         * characters.
         *
         * Arguments:
         *   args[0:]: the attributes
         *     0 = reset all attributes
         *
         * Defaults:
         *   args[0]: 0 (reset all attributes)
         *
         * References: ECMA-48 § 8.3.117
         *             VT525
         */
        auto const n_params = seq.size();

	/* If we had no parameters, default to the defaults. */
	if (n_params == 0) {
                reset_default_attributes(false);
                return;
	}

        for (unsigned int i = 0; i < n_params; i = seq.next(i)) {
                auto const param = seq.param(i);
                switch (param) {
                case -1:
                case VTE_SGR_RESET_ALL:
                        reset_default_attributes(false);
                        break;
                case VTE_SGR_SET_BOLD:
                        m_defaults.attr.set_bold(true);
                        break;
                case VTE_SGR_SET_DIM:
                        m_defaults.attr.set_dim(true);
                        break;
                case VTE_SGR_SET_ITALIC:
                        m_defaults.attr.set_italic(true);
                        break;
                case VTE_SGR_SET_UNDERLINE: {
                        unsigned int v = 1;
                        /* If we have a subparameter, get it */
                        if (seq.param_nonfinal(i)) {
                                v = seq.param(i + 1, 1, 0, 3);
                        }
                        m_defaults.attr.set_underline(v);
                        break;
                }
                case VTE_SGR_SET_BLINK:
                case VTE_SGR_SET_BLINK_RAPID:
                        m_defaults.attr.set_blink(true);
                        break;
                case VTE_SGR_SET_REVERSE:
                        m_defaults.attr.set_reverse(true);
                        break;
                case VTE_SGR_SET_INVISIBLE:
                        m_defaults.attr.set_invisible(true);
                        break;
                case VTE_SGR_SET_STRIKETHROUGH:
                        m_defaults.attr.set_strikethrough(true);
                        break;
                case VTE_SGR_SET_UNDERLINE_DOUBLE:
                        m_defaults.attr.set_underline(2);
                        break;
                case VTE_SGR_RESET_BOLD_AND_DIM:
                        m_defaults.attr.unset(VTE_ATTR_BOLD_MASK | VTE_ATTR_DIM_MASK);
                        break;
                case VTE_SGR_RESET_ITALIC:
                        m_defaults.attr.set_italic(false);
                        break;
                case VTE_SGR_RESET_UNDERLINE:
                        m_defaults.attr.set_underline(0);
                        break;
                case VTE_SGR_RESET_BLINK:
                        m_defaults.attr.set_blink(false);
                        break;
                case VTE_SGR_RESET_REVERSE:
                        m_defaults.attr.set_reverse(false);
                        break;
                case VTE_SGR_RESET_INVISIBLE:
                        m_defaults.attr.set_invisible(false);
                        break;
                case VTE_SGR_RESET_STRIKETHROUGH:
                        m_defaults.attr.set_strikethrough(false);
                        break;
                case VTE_SGR_SET_FORE_LEGACY_START ... VTE_SGR_SET_FORE_LEGACY_END:
                        m_defaults.attr.set_fore(VTE_LEGACY_COLORS_OFFSET + (param - 30));
                        break;
                case VTE_SGR_SET_FORE_SPEC: {
                        uint32_t fore;
                        if (G_LIKELY((seq_parse_sgr_color<8, 8, 8>(seq, i, fore))))
                                m_defaults.attr.set_fore(fore);
                        break;
                }
                case VTE_SGR_RESET_FORE:
                        /* default foreground */
                        m_defaults.attr.set_fore(VTE_DEFAULT_FG);
                        break;
                case VTE_SGR_SET_BACK_LEGACY_START ... VTE_SGR_SET_BACK_LEGACY_END:
                        m_defaults.attr.set_back(VTE_LEGACY_COLORS_OFFSET + (param - 40));
                        break;
                case VTE_SGR_SET_BACK_SPEC: {
                        uint32_t back;
                        if (G_LIKELY((seq_parse_sgr_color<8, 8, 8>(seq, i, back))))
                                m_defaults.attr.set_back(back);
                        break;
                }
                case VTE_SGR_RESET_BACK:
                        /* default background */
                        m_defaults.attr.set_back(VTE_DEFAULT_BG);
                        break;
                case VTE_SGR_SET_OVERLINE:
                        m_defaults.attr.set_overline(true);
                        break;
                case VTE_SGR_RESET_OVERLINE:
                        m_defaults.attr.set_overline(false);
                        break;
                case VTE_SGR_SET_DECO_SPEC: {
                        uint32_t deco;
                        if (G_LIKELY((seq_parse_sgr_color<4, 5, 4>(seq, i, deco))))
                                m_defaults.attr.set_deco(deco);
                        break;
                }
                case VTE_SGR_RESET_DECO:
                        /* default decoration color, that is, same as the cell's foreground */
                        m_defaults.attr.set_deco(VTE_DEFAULT_FG);
                        break;
                case VTE_SGR_SET_FORE_LEGACY_BRIGHT_START ... VTE_SGR_SET_FORE_LEGACY_BRIGHT_END:
                        m_defaults.attr.set_fore(VTE_LEGACY_COLORS_OFFSET + (param - 90) +
                                                 VTE_COLOR_BRIGHT_OFFSET);
                        break;
                case VTE_SGR_SET_BACK_LEGACY_BRIGHT_START ... VTE_SGR_SET_BACK_LEGACY_BRIGHT_END:
                        m_defaults.attr.set_back(VTE_LEGACY_COLORS_OFFSET + (param - 100) +
                                                 VTE_COLOR_BRIGHT_OFFSET);
                        break;
                }
        }

	/* Save the new colors. */
        m_color_defaults.attr.copy_colors(m_defaults.attr);
}

void
Terminal::SHS(vte::parser::Sequence const& seq)
{
        /*
         * SHS - select character spacing
         *
         * Arguments:
         *   args[0]: spacing (in the unit set by SSU)
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.118
         *
         * Not applicable in VTE.
         */
}

void
Terminal::SIMD(vte::parser::Sequence const& seq)
{
        /*
         * SIMD - select implicit movement direction
         *
         * Arguments:
         *   args[0]: direction
         *     0 = character progression
         *     1 = opposite of character progression
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.120
         */
}

void
Terminal::SL(vte::parser::Sequence const& seq)
{
        /*
         * SL - scroll left
         *
         * Arguments:
         *   args[0]: number of character positions (presentation)
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.121
         *
         * Probably not worth implementing.
         */
}

void
Terminal::SLH(vte::parser::Sequence const& seq)
{
        /*
         * SLH - set line home
         *
         * Arguments:
         *   args[0]: position in the active line
         *
         * Defaults:
         *   args[0]: no default
         *
         * Depending on DCSM, this function works on the presentation
         * or data position. Terminal-wg/bidi forces DCSM to DATA.
         *
         * References: ECMA-48 § 8.3.122
         *             Terminal-wg/bidi
         */
}

void
Terminal::SLL(vte::parser::Sequence const& seq)
{
        /*
         * SLL - set line limit
         *
         * Arguments:
         *   args[0]: position in the active line
         *
         * Defaults:
         *   args[0]: no default
         *
         * Depending on DCSM, this function works on the presentation
         * or data position. Terminal-wg/bidi forces DCSM to DATA.
         *
         * References: ECMA-48 § 8.3.123
         *             Terminal-wg/bidi
         */
}

void
Terminal::SLS(vte::parser::Sequence const& seq)
{
        /*
         * SLS - set line spacing
         *
         * Arguments:
         *   args[0]: spacing (in the unit set by SSU)
         *
         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.124
         *
         * Not applicable in VTE.
         */
}

void
Terminal::SM_ECMA(vte::parser::Sequence const& seq)
{
        /*
         * SM_ECMA - set-mode-ecma
         *
         * Defaults: none
         *
         * References: ECMA-48 § 8.3.125
         */

        set_mode_ecma(seq, true);
}

void
Terminal::SM_DEC(vte::parser::Sequence const& seq)
{
        /*
         * SM_DEC - set-mode-dec
         * This is the same as SM_ECMA but for DEC modes.
         *
         * Defaults: none
         *
         * References: VT525
         */

        set_mode_private(seq, true);
}

void
Terminal::SM_HP(vte::parser::Sequence const& seq)
{
        /*
         * SM_HP - set mode hp
         * This is the same as SM_ECMA but for HP private modes.
         *
         * Known modes:
         *   1: multipage mode
         *      If reset, the terminal only has one page of 24 lines of display memory
         *      Default: reset
         *   2: memory lock
         *      Default: reset
         *
         * Defaults: none
         *
         * References: HP 2397A
         */

        /* Not worth implementing */
}

void
Terminal::SOH(vte::parser::Sequence const& seq)
{
        /*
         * SOH - start of heading
         *
         * References: ECMA-48 § 8.3.127
         *             ECMA-16 § 3.1.1
         */
}

void
Terminal::SPA(vte::parser::Sequence const& seq)
{
        /*
         * SPA - start of protected area
         * Marks the start of an area of positions (presentation)
         * that are protected; the end of the area will be
         * marked by EPA.
         *
         * The contents of the area will be protected against
         * alteration, transfer (depending on the GATM setting),
         * and erasure (depending on the ERM setting).
         *
         * References: ECMA-48 § 8.3.129
         */
}

void
Terminal::SPD(vte::parser::Sequence const& seq)
{
        /*
         * SPD - select presentation directions
         *
         * Arguments:
         *   args[0]: line orientation, progression, character path
         *     0 = horizontal, TTB, LTR
         *     1 = vertical,   RTL, TTB
         *     2 = vertical,   LTR, TTB
         *     3 = horizontal, TTB, RTL
         *     4 = vertical,   LTR, BTT
         *     5 = horizontal, BTT, RTL
         *     6 = horizontal, BTT, LTR
         *     7 = vertical,   RTL, BTT
         *
         *   args[1]: effect
         *     0 = implementation-defined
         *     1 = ...
         *     2 = ...
         *
         * Defaults:
         *   args[0]: 0
         *   args[1]: 0
         *
         * References: ECMA-48 § 8.3.130
         *             Terminal-wg/bidi
         */

        auto const param = seq.collect1(0);
        switch (param) {
        case -1:
        case 0:
                m_bidi_rtl = FALSE;
                _vte_debug_print(VTE_DEBUG_BIDI, "BiDi: switch to LTR\n");
                break;
        case 3:
                m_bidi_rtl = TRUE;
                _vte_debug_print(VTE_DEBUG_BIDI, "BiDi: switch to RTL\n");
                break;
        default:
                return;
        }

        maybe_apply_bidi_attributes(VTE_BIDI_FLAG_RTL);

        /* FIXME maybe apply to all the onscreen lines? */
}

void
Terminal::SPH(vte::parser::Sequence const& seq)
{
        /*
         * SPH - set page home
         *
         * Arguments:
         *   args[0]: position in the active page
         *
         * Defaults:
         *   args[0]: no default
         *
         * Depending on DCSM, this function works on the presentation
         * or data position. Terminal-wg/bidi forces DCSM to DATA.
         *
         * References: ECMA-48 § 8.3.131
         *             Terminal-wg/bidi
         */
}

void
Terminal::SPI(vte::parser::Sequence const& seq)
{
        /*
         * SPI - spacing increment
         * Set line and character spacing for following text.
         *
         * Arguments:
         *   args[0]: line spacing (in the unit set by SSU)
         *   args[0]: character spacing (in the unit set by SSU)
         *
         * Defaults:
         *   args[0]: no default
         *   args[1]: no default
         *
         * References: ECMA-48 § 8.3.132
         */
}

void
Terminal::SPL(vte::parser::Sequence const& seq)
{
        /*
         * SPL - set page limit
         *
         * Arguments:
         *   args[0]: line position in the active page
         *
         * Defaults:
         *   args[0]: no default
         *
         * Depending on DCSM, this function works on the presentation
         * or data position. Terminal-wg/bidi forces DCSM to DATA.
         *
         * References: ECMA-48 § 8.3.133
         *             Terminal-wg/bidi
         */
}

void
Terminal::SPQR(vte::parser::Sequence const& seq)
{
        /*
         * SPQR - select print quality and rapidity
         *
         * Arguments:
         *   args[0]:
         *
         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.134
         */
}

void
Terminal::SR(vte::parser::Sequence const& seq)
{
        /*
         * SL - scroll right
         *
         * Arguments:
         *   args[0]: number of character positions (presentation)
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.135
         *
         * Probably not worth implementing.
         */
}

void
Terminal::SRCS(vte::parser::Sequence const& seq)
{
        /*
         * SRCS - set reduced character separation
         *
         * Arguments:
         *   args[0]: spacing (in the unit set by SSU)
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.136
         *
         * Not applicable in VTE.
         */
}

void
Terminal::SRS(vte::parser::Sequence const& seq)
{
        /*
         * SRS - start reversed string
         *
         * Arguments:
         *   args[0]: direction
         *     0 = End of reversed string
         *     1 = Start of reversed string
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.137
         */
}

void
Terminal::SSA(vte::parser::Sequence const& seq)
{
        /*
         * SSA - start of selected area
         * Marks the start of an area of positions (presentation)
         * that are selected for transfer; the end of the area will
         * be marked by ESA.
         *
         * What will actually be transmitted depends on the setting
         * of the GATM mode, and areas set by the DAQ and SPA/EPA
         * functions.
         *
         * References: ECMA-48 § 8.3.138
         */
}

void
Terminal::SSU(vte::parser::Sequence const& seq)
{
        /*
         * SSU - set size unit
         *
         * Arguments:
         *   args[0]: unit
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.139
         */
}

void
Terminal::SSW(vte::parser::Sequence const& seq)
{
        /*
         * SSW - set space width
         *
         * Arguments:
         *   args[0]: width (in the unit set by SSU)
         *
         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.140
         */
}

void
Terminal::SS2(vte::parser::Sequence const& seq)
{
        /*
         * SS2 - single-shift-2
         * Temporarily map G2 into GL for the next graphics character.
         *
         * References: ECMA-35 § 8.4, 9.4
         *             ECMA-48 § 8.3.141
         *             VT525
         */
#if 0
        screen->state.glt = &screen->g2;
#endif
}

void
Terminal::SS3(vte::parser::Sequence const& seq)
{
        /*
         * SS3 - single-shift-3
         * Temporarily map G3 into GL for the next graphics character
         *
         * References: ECMA-35 § 8.4, 9.4
         *             ECMA-48 § 8.3.142
         *             VT525
         */
#if 0
        screen->state.glt = &screen->g3;
#endif
}

void
Terminal::ST(vte::parser::Sequence const& seq)
{
        /*
         * ST - string-terminator
         * The string-terminator is usually part of control-sequences and
         * handled by the parser. In all other situations it is silently
         * ignored.
         *
         * References: ECMA-48 § 8.3.143
         */
}

void
Terminal::STAB(vte::parser::Sequence const& seq)
{
        /*
         * STAB - selective tabulation
         *
         * Arguments:
         *   args[0]:
         *
         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.144
         *             ITU-T Rec. T.416 (Open Document Architecture)
         */
}

void
Terminal::STS(vte::parser::Sequence const& seq)
{
        /*
         * STS - set transmit state
         *
         * References: ECMA-48 § 8.3.145
         *
         * Not worth implementing.
         */
}

void
Terminal::STX(vte::parser::Sequence const& seq)
{
        /*
         * STX - start of text
         *
         * References: ECMA-48 § 8.3.146
         *             ECMA-16 § 3.1.2
         *
         * Not worth implementing.
         */
}

void
Terminal::SU(vte::parser::Sequence const& seq)
{
        /*
         * SU - scroll-up / pan down
         * Scrolls up a number of lines (presentation).
         *
         * Arguments:
         *   args[0]: number of lines to scroll
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: EMCA-48 § 8.3.147
         *             VT525
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        vte_page_scroll_up(screen->page,
                              num,
                              &screen->state.attr,
                              screen->age,
                              screen->history);
#endif

        auto value = std::max(seq.collect1(0, 1), int(1));
        scroll_text(-value);
}

void
Terminal::SUB(vte::parser::Sequence const& seq)
{
        /*
         * SUB - substitute
         * Cancel the current control-sequence and print a replacement
         * character. Our parser already handles this so all we have to do is
         * print the replacement character.
         *
         * References: ECMA-48 § 8.3.148
         */

        insert_char(0xfffdu, false, true);
}

void
Terminal::SVS(vte::parser::Sequence const& seq)
{
        /*
         * SVS - select line spacing
         *
         * Arguments:
         *   args[0]: spacing
         *     0 = ...
         *     ...
         *     9 = ...
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.149
         */
}

void
Terminal::SYN(vte::parser::Sequence const& seq)
{
        /*
         * SYN - synchronous idle
         *
         * References: ECMA-48 § 8.3.150
         *             ECMA-16 § 3.1.9
         *
         * Not worth implementing.
         */
}

void
Terminal::TAC(vte::parser::Sequence const& seq)
{
        /*
         * TAC - tabulation aligned centre
         *
         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.151
         */
}

void
Terminal::TALE(vte::parser::Sequence const& seq)
{
        /*
         * TALE - tabulation aligned leading edge
         *
         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.152
         */
}

void
Terminal::TATE(vte::parser::Sequence const& seq)
{
        /*
         * TATE - tabulation aligned trailing edge
         *
         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.153
         */
}

void
Terminal::TBC(vte::parser::Sequence const& seq)
{
        /*
         * TBC - tab-clear
         * Clears tab stops.
         *
         * Arguments:
         *   args[0]: mode
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.154
         */

        auto const param = seq.collect1(0);
        switch (param) {
        case -1:
        case 0:
                /* Clear character tabstop at the current presentation position */
                m_tabstops.unset(get_cursor_column());
                break;
        case 1:
                /* Clear line tabstop at the current line */
                break;
        case 2:
                /* Clear all character tabstops in the current line */
                /* NOTE: vttest issues this but claims it's a 'no-op' */
                m_tabstops.clear();
                break;
        case 3:
                /* Clear all character tabstops */
                m_tabstops.clear();
                break;
        case 4:
                /* Clear all line tabstops */
                break;
        case 5:
                /* Clear all (character and line) tabstops */
                m_tabstops.clear();
                break;
        default:
                break;
	}
}

void
Terminal::TCC(vte::parser::Sequence const& seq)
{
        /*
         * TCC - tabulation centred on character
         *
         * Defaults:
         *   args[0]: no default
         *   args[1]: 32 (SPACE)
         *
         * References: ECMA-48 § 8.3.155
         */
}

void
Terminal::TSR(vte::parser::Sequence const& seq)
{
        /*
         * TSR - tabulation stop remove
         * This clears a tab stop at position @arg[0] in the active line (presentation),
         * and on any lines below it.
         *
         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.156
         */

        auto const pos = seq.collect1(0);
        if (pos < 1 || pos > m_column_count)
                return;

        m_tabstops.unset(pos - 1);
}

void
Terminal::TSS(vte::parser::Sequence const& seq)
{
        /*
         * TSS - thin space specification
         *
         * Arguments:
         *   args[0]: width (in the unit set by SSU)
         *
         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.157
         *
         * Not applicable in VTE.
         */
}

void
Terminal::VPA(vte::parser::Sequence const& seq)
{
        /*
         * VPA - vertical line position absolute
         * Moves the cursor to the specified line on the current column (data).
         *
         * Arguments:
         *   args[0]: line number
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.158
         *             VT525
         */
#if 0
        unsigned int pos = 1;

        if (seq->args[0] > 0)
                pos = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_set_rel(screen, screen->state.cursor_x, pos - 1);
#endif

        // FIXMEchpe shouldn't we ensure_cursor_is_onscreen AFTER setting the new cursor row?
        ensure_cursor_is_onscreen();

        auto value = seq.collect1(0, 1, 1, m_row_count);
        set_cursor_row1(value);
}

void
Terminal::VPB(vte::parser::Sequence const& seq)
{
        /*
         * VPB - line position backward
         * Moves the cursor up the specified number of lines on
         * the current column (data).
         *
         * Arguments:
         *   args[0]: line number
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.159
         *             VT525
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_down(screen, num, false);
#endif
}

void
Terminal::VPR(vte::parser::Sequence const& seq)
{
        /*
         * VPR - vertical line position relative
         * Moves the cursor down the specified number of lines
         * on the current column (data).
         *
         * Arguments:
         *   args[0]: line number
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.160
         *             VT525
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_down(screen, num, false);
#endif
}

void
Terminal::VT(vte::parser::Sequence const& seq)
{
        /*
         * VT - vertical-tab
         * This causes a vertical jump by one line. Terminals treat it exactly
         * the same as LF.
         *
         * References: ECMA-48 § 8.3.161
         */

        LF(seq);
}

void
Terminal::VTS(vte::parser::Sequence const& seq)
{
        /*
         * VTS - line tabulation set
         * Sets a tabstop in the active line (presentation).
         *
         * References: ECMA-48 § 8.3.162
         *
         * Not worth implementing.
         */
}

void
Terminal::WYCAA(vte::parser::Sequence const& seq)
{
        /*
         * WYCAA - redefine character display attribute association
         *
         * Arguments:
         *   args[0]: mode
         *
         * Defaults:
         *   args[0]: no defaults
         *
         * Probably not worth implementing.
         *
         * References: WY370
         */

        switch (seq.collect1(0)) {
        case -1:
                break;

        case 0 ... 47:
                /* WYCAA - redefine character attribute association
                 *
                 * Arguments:
                 *   args[0]: character attribute association to be set (0…47)
                 *   args[1]: palette color index for foreground color (0…64)
                 *   args[2]: palette color index for background color (0…64)
                 *   args[3]: new definition for the attribute association @args[0]
                 *
                 * Defaults:
                 *   args[0]: ?
                 *   args[1]: ?
                 *   args[2]: ?
                 *   args[3]: ?
                 */
                break;

        case 48:
                /* WYCOLOR - select foreground color palette
                 *
                 * Arguments:
                 *   args[1]: color palette number 0…7
                 *
                 * Defaults:
                 *   args[1]: ?
                 */
                break;

        case 49:
        case 51 ... 52:
                /* WYCOLOR - select background (49)/screen border(51)/cursor(52) color
                 * Selects the background (and screen border) color.
                 *
                 * Arguments:
                 *   args[1]: palette color index 0…64
                 *
                 * Defaults:
                 *   args[1]: ?
                 */
                break;

        case 50:
                /* WYCOLOR - restore fore- and background colors to set-up default */
                break;

        case 53:
                /* WYSOVR - select overstrike position
                 *
                 * Arguments:
                 *   args[1]: scanline number in the charcell (0=top, …bottom) to
                 *            put the overstrike
                 *
                 * Defaults:
                 *   args[1]:
                 */
                break;

        case 54 ... 57:
                /* WYCOLOR - select attributes and colors
                 * for user status line (54), system status line(55),
                 * replacement character(56), noneraseable character(57).
                 *
                 * Arguments:
                 *   args[1]:
                 *   args[2]:
                 *
                 * Defaults:
                 *   args[1]:
                 *   args[2]:
                 */

        case 58:
                /* WYDTSET - set date and time */
                break;

        case 59:
                /* WYDFPG - define page for session
                 *
                 * Arguments:
                 *   args[1]:
                 *   args[2]:
                 *   args[3]:
                 *   args[4]:
                 *
                 * Defaults:
                 *   args[1]:
                 *   args[2]:
                 *   args[3]:
                 *   args[4]:
                 */
                break;

        case 60:
                /* WYIND - restore default color index values */
                break;

        case 61 ... 62:
        case 64 ... 65:
                /* WYIND - set current fore/background color
                 * Sets the current fore- (61, 64) or background (62, 65)
                 * color for eraseable (61, 62) or noneraseable (64, 65)
                 * characters.
                 *
                 * Also turns on color index mode.
                 *
                 * Arguments:
                 *   args[1]: color index
                 *
                 * Defaults:
                 *   args[1]: ?
                 */
                break;

        case 63:
                /* WYIND - turn color index mode on/off
                 *
                 * Arguments:
                 *   args[1]: setting (0 = off, 1 = on)
                 *
                 * Defaults:
                 *   args[1]: ?
                 */
                break;

        case 66:
                /* WYIND - redefine color index
                 *
                 * Arguments:
                 *   args[1]: index
                 *   args[2]: value
                 *
                 * Defaults:
                 *   args[1]: ?
                 *   args[2]: ?
                 */
                break;
        }
}

void
Terminal::WYCDIR(vte::parser::Sequence const& seq)
{
        /*
         * WYCDIR - set current character color and attributes
         * Sets the foreground and background colours used for SGR attributes.
         *
         * Arguments:
         *   args[0]: foreground colour (0…64)
         *   args[1]: background colour (0…64)
         *   args[2]: SGR attribute (0…15)
         *
         * Defaults:
         *   args[0]: default foreground colour
         *   args[1]: default background colour
         *   args[2]: default attribute (0)
         *
         * Probably not worth implementing.
         *
         * References: WY370
         */
}

void
Terminal::WYDHL_BH(vte::parser::Sequence const& seq)
{
        /*
         * WYDHL_BH - single width double height line: bottom half
         *
         * Probably not worth implementing.
         *
         * References: WY370
         */
}

void
Terminal::WYDHL_TH(vte::parser::Sequence const& seq)
{
        /*
         * WYDHL_TH - single width double height line: top half
         *
         * Probably not worth implementing.
         *
         * References: WY370
         */
}

void
Terminal::WYSCRATE(vte::parser::Sequence const& seq)
{
        /*
         * WYSCRATE - set smooth scroll rate
         * Selects scrolling rate if DECSCLM is set.
         *
         * Probably not worth implementing.
         *
         * References: WY370
         */
}

void
Terminal::WYLSFNT(vte::parser::Sequence const& seq)
{
        /*
         * WYLSFNT - load soft font
         *
         * Probably not worth implementing.
         *
         * References: WY370
         */
}

void
Terminal::XDGSYNC(vte::parser::Sequence const& seq)
{
        /*
         * XDGSYNC - synchronous update
         * Content received between BSU and ESU will be committed
         * atomically on ESU. This is to avoid half-drawn screen
         * content.
         * The terminal may ignore this, or apply a timeout, or
         * terminate the synchronous update prematurely for any
         * reason.
         *
         * Arguments:
         *   args[0]:
         *     1: start (begin synchronous update, BSU)
         *     2: end   (end synchronous update, ESU)
         *
         * Defaults:
         *   args[0]: no defaults
         *
         * References: https://gitlab.com/gnachman/iterm2/wikis/synchronized-updates-spec
         */

        /* TODO: implement this! https://gitlab.gnome.org/GNOME/vte/issues/15 */
}

void
Terminal::XTERM_CHECKSUM_MODE(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_CHECKSUM_MODE - xterm DECRQCRA checksum mode
         * Sets how DECRQCRA calculates the area checksum.
         *
         * Arguments:
         *   args[0]: flag value composed of the following flags:
         *     1: no negation
         *     2: don't report attributes
         *     4: checksum trailing blanks
         *     8: don't checksum empty cells
         *     16: no 8-bit masking or ignoring combining characters
         *     32: no 7-bit masking
         *
         * Defaults:
         *   args[0]: 0, matching the output from VTxxx terminals
         *
         * References: XTERM 335
         *
         * Probably not worth implementing.
         */
}

void
Terminal::XTERM_GETXRES(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_GETXRES - xterm get X resource
         *
         * References: XTERM 350
         *
         * Won't implement.
         */
}

void
Terminal::XTERM_IHMT(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_IHMT - xterm-initiate-highlight-mouse-tracking
         *
         * Probably not worth implementing.
         */
}

void
Terminal::XTERM_MLHP(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_MLHP - xterm-memory-lock-hp-bugfix
         *
         * This seems bogus; SM_HP 2 is the way to set the memory lock on
         * HP terminal.
         *
         * References: XTERM
         *
         * Not worth implementing.
         */
}

void
Terminal::XTERM_MUHP(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_MUHP - xterm-memory-unlock-hp-bugfix
         *
         * This seems bogus; RM_HP 2 is the way to unset the memory lock on
         * HP terminal.
         *
         * References: XTERM
         *
         * Not worth implementing.
         */
}

void
Terminal::XTERM_MODKEYS(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_MODKEYS - xterm set key modifier options
         *
         * Probably not worth implementing.
         */
}

void
Terminal::XTERM_POPCOLORS(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_POPCOLORS: pop color palette stack
         * Restore color palette attributes previously pushed to the stack
         * with XTERM_PUSHCOLORS. If there is nothing on the
         * stack, does nothing.
         *
         * Arguments: none
         *
         * References: XTERM 357
         *
         * See issue vte#23.
         */
}

void
Terminal::XTERM_POPSGR(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_POPSGR: pop SGR stack
         * Restore SGR attributes previously pushed to the stack
         * with XTERM_PUSHSGR. If there is nothing on the
         * stack, does nothing.
         *
         * Arguments: none
         *
         * References: XTERM 334
         *
         * Note: The {PUSH,POP,REPORT}SGR protocol is poorly thought-out, and has
         * no real use case. See the discussion at issue vte#23.
         * Probably won't implement.
         */
}

void
Terminal::XTERM_PUSHCOLORS(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_PUSHSGR: push color palette stack
         * Push current color palette to the stack.
         * If the stack is full, drops the bottommost item before
         * pushing on the stack.
         *
         * If there are any arguments, they are interpreted as in SGR
         * to denote which attributes to save; if there are no arguments,
         * all attributes are saved.
         *
         * Arguments:
         *   args[0:]: the attributes
         *     0 = save all attributes
         *
         * Defaults:
         *   args[0]: 0 (save all attributes)
         *
         * References: XTERM 357
         *
         * See issue vte#23.
         */
}

void
Terminal::XTERM_PUSHSGR(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_PUSHSGR: push SGR stack
         * Push current SGR attributes to the stack.
         * If the stack is full, drops the bottommost item before
         * pushing on the stack.
         *
         * If there are any arguments, they are interpreted as in SGR
         * to denote which attributes to save; if there are no arguments,
         * all attributes are saved.
         *
         * Arguments:
         *   args[0:]: the attributes
         *     0 = save all attributes
         *
         * Defaults:
         *   args[0]: 0 (save all attributes)
         *
         * References: XTERM 334
         *
         * Note: The {PUSH,POP,REPORT}SGR protocol is poorly thought-out, and has
         * no real use case. See the discussion at issue vte#23.
         * Probably won't implement.
         */
}

void
Terminal::XTERM_REPORTCOLORS(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_REPORTCOLORS: report color palette on stack
         *
         * References: XTERM 357
         *
         * See issue vte#23.
         */
}

void
Terminal::XTERM_REPORTSGR(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_REPORTSGR: report SGR attributes in rectangular area
         * Report common character attributes in the specified rectangle.
         *
         * Arguments;
         *   args[0..3]: top, left, bottom, right of the rectangle (1-based)
         *
         * Defaults:
         *   args[0]: 1
         *   args[1]: 1
         *   args[2]: height of current page
         *   args[3]: width of current page
         *
         * Reply: SGR
         *
         * If the top > bottom or left > right, the command is ignored.
         *
         * These coordinates are interpreted according to origin mode (DECOM),
         * but unaffected by the page margins (DECSLRM?).
         *
         * Note: DECSACE selects whether this function operates on the
         * rectangular area or the data stream between the star and end
         * positions.
         *
         * References: XTERM 334
         *
         * Note: The {PUSH,POP,REPORT}SGR protocol is poorly thought-out, and has
         * no real use case. See the discussion at issue vte#23.
         * Probably won't implement.
         */
}

void
Terminal::XTERM_RPM(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_RPM - xterm restore DEC private mode
         *
         * Defaults: none
         *
         * References: XTERM
         */

        save_mode_private(seq, false);
}

void
Terminal::XTERM_RQTCAP(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_TQTCAP - xterm request termcap/terminfo
         *
         * Probably not worth implementing.
         */
}

void
Terminal::XTERM_RRV(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_RRV - xterm-reset-resource-value
         *
         * Probably not worth implementing.
         */
}

void
Terminal::XTERM_RTM(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_RTM - xterm-reset-title-mode
         *
         * Probably not worth implementing.
         */
}

void
Terminal::XTERM_SHIFTESCAPE(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_SHIFTESCAPE - xterm set/reset shift escape
         * Selects whether the SHIFT key acts as a modifier in the mouse
         * protocol, or overrides the mouse protocol.
         *
         * Arguments:
         *   args[0]:
         *     0: overrides mouse protocol
         *     1: conditionally acts as modifier
         *     2: always acts as modifier
         *     3: never acts as modifier
         *
         * Defaults:
         *   args[0]: 0
         *
         * Note that args[0] values 2 and 3 are not actually executed
         * from an escape sequence, they correspond to the value of the
         * xterm resource controlling this setting.
         *
         * References: XTERM 362
         */
        /* Not worth implementing this */
}


void
Terminal::XTERM_SMGRAPHICS(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_SMGRAPHICS - xterm set or request graphics attributes
         * Set or request graphics attributes for SIXEL and REGIS.
         *
         * Reply: XTERM_SMGRAPHICS_REPORT
         *
         * Arguments:
         *   args[0]: select function
         *     0: number of colour registers
         *     1: SIXEL geometry
         *     2: REGIS geometry
         *   args[1]: select subfunction
         *     1: read attribute
         *     2: reset attribute
         *     3: set attribute
         *     4: read maximum value of attribute
         *   args[2:]: values, used only for subfuncion 3
         *
         * Defaults:
         *   args[0]: no default
         *   args[1]: no default
         *   args[2:]: no default
         *
         * The reply is XTERM_SMGRAPHICS_REPORT, with arguments:
         *   args[0]: function
         *   args[1]: status
         *     0: success
         *     1: error in function parameter
         *     2: error in subfunction parameter
         *     3: failure
         *
         * References: XTERM
         */

        auto const attr = seq.collect1(0);
        auto status = 1, rv0 = -2, rv1 = -2;

        reply(seq, VTE_REPLY_XTERM_SMGRAPHICS_REPORT, {attr, status, rv0, rv1});
}

void
Terminal::XTERM_SPM(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_SPM - xterm save DEC private mode
         *
         * Defaults: none
         *
         * References: XTERM
         */

        save_mode_private(seq, true);
}

void
Terminal::XTERM_PTRMODE(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_PTRMODE - xterm set pointer mode
         *
         * Defaults: none
         *
         * References: XTERM
         *
         * Probably not worth implementing.
         */
}

void
Terminal::XTERM_STM(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_STM - xterm-set-title-mode
         *
         * Probably not worth implementing.
         */
}

void
Terminal::XTERM_STCAP(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_STCAP - xterm set termcap/terminfo
         *
         * Probably not worth implementing.
         */
}

void
Terminal::XTERM_WM(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_WM - xterm-window-management
         *
         * Window manipulation control sequences.  Most of these are considered
         * bad ideas, but they're implemented as signals which the application
         * is free to ignore, so they're harmless.  Handle at most one action,
         * see bug 741402.
         *
         * No parameter default values.
         *
         * References: XTERM
         *             VT525
         */

        #if 0
	char buf[128];
        #endif

        int param = seq.collect1(0);
        switch (param) {
        case -1:
        case 0:
                break;

        case VTE_XTERM_WM_RESTORE_WINDOW:
                m_xterm_wm_iconified = false;
                break;

        case VTE_XTERM_WM_MINIMIZE_WINDOW:
                m_xterm_wm_iconified = true;
                break;

        case VTE_XTERM_WM_SET_WINDOW_POSITION:
                /* No-op */
                break;

        case VTE_XTERM_WM_SET_WINDOW_SIZE_PIXELS: {
                int width, height;
                seq.collect(1, {&height, &width});

                if (width != -1 && height != -1) {
                        _vte_debug_print(VTE_DEBUG_EMULATION,
                                         "Resizing window to %dx%d pixels, grid size %dx%d.\n",
                                         width, height,
                                         width / int(m_cell_height), height / int(m_cell_width));
                        emit_resize_window(width / int(m_cell_height), height / int(m_cell_width));
                }
                break;
        }

        case VTE_XTERM_WM_RAISE_WINDOW:
                break;

        case VTE_XTERM_WM_LOWER_WINDOW:
                break;

        case VTE_XTERM_WM_REFRESH_WINDOW:
                break;

        case VTE_XTERM_WM_SET_WINDOW_SIZE_CELLS: {
                int width, height;
                seq.collect(1, {&height, &width});

                if (width != -1 && height != -1) {
                        _vte_debug_print(VTE_DEBUG_EMULATION,
                                         "Resizing window to %d columns, %d rows.\n",
                                         width, height);
                        emit_resize_window(width, height);
                }
                break;
        }

        case VTE_XTERM_WM_MAXIMIZE_WINDOW:
                switch (seq.collect1(1)) {
                case -1: /* default */
                case 0:
                        /* Restore */
                        break;
                case 1:
                        /* Maximise */
                        break;
                case 2:
                        /* Maximise Vertically */
                        break;
                case 3:
                        /* Maximise Horizontally */
                        break;
                default:
                        break;
                }
                break;

        case VTE_XTERM_WM_FULLSCREEN_WINDOW:
                break;

        case VTE_XTERM_WM_GET_WINDOW_STATE:
                reply(seq, VTE_REPLY_XTERM_WM, {m_xterm_wm_iconified ? 2 : 1});
                break;

        case VTE_XTERM_WM_GET_WINDOW_POSITION:
                /* Reply with fixed origin. */
                reply(seq, VTE_REPLY_XTERM_WM, {3, 0, 0});
                break;

        case VTE_XTERM_WM_GET_WINDOW_SIZE_PIXELS: {
                auto const height = int(m_row_count * m_cell_height_unscaled);
                auto const width = int(m_column_count * m_cell_width_unscaled);
                reply(seq, VTE_REPLY_XTERM_WM, {4, height, width});
                break;
        }

        case VTE_XTERM_WM_GET_WINDOW_SIZE_CELLS:
                reply(seq, VTE_REPLY_XTERM_WM,
                      {8, (int)m_row_count, (int)m_column_count});
                break;

        case VTE_XTERM_WM_GET_SCREEN_SIZE_CELLS: {
                /* FIMXE: this should really report the monitor's workarea,
                 * or even just a fixed value.
                 */
#if VTE_GTK == 3
                auto gdkscreen = gtk_widget_get_screen(m_widget);
                int height = gdk_screen_get_height(gdkscreen);
                int width = gdk_screen_get_width(gdkscreen);
                _vte_debug_print(VTE_DEBUG_EMULATION,
                                 "Reporting screen size as %dx%d cells.\n",
                                 height / int(m_cell_height), width / int(m_cell_width));
#elif VTE_GTK == 4
                auto height = int(m_row_count * m_cell_height);
                auto width = int(m_column_count * m_cell_width);
#endif

                reply(seq, VTE_REPLY_XTERM_WM,
                      {9, height / int(m_cell_height), width / int(m_cell_width)});
                break;
        }

        case VTE_XTERM_WM_GET_ICON_TITLE:
                /* Report a static icon title, since the real
                 * icon title should NEVER be reported, as it
                 * creates a security vulnerability.  See
                 * http://marc.info/?l=bugtraq&m=104612710031920&w=2
                 * and CVE-2003-0070.
                 */
                _vte_debug_print(VTE_DEBUG_EMULATION,
                                 "Reporting empty icon title.\n");

                send(seq, vte::parser::u8SequenceBuilder{VTE_SEQ_OSC, "L"s});
                break;

        case VTE_XTERM_WM_GET_WINDOW_TITLE:
                /* Report a static window title, since the real
                 * window title should NEVER be reported, as it
                 * creates a security vulnerability.  See
                 * http://marc.info/?l=bugtraq&m=104612710031920&w=2
                 * and CVE-2003-0070.
                 */
                _vte_debug_print(VTE_DEBUG_EMULATION,
                                 "Reporting empty window title.\n");

                send(seq, vte::parser::u8SequenceBuilder{VTE_SEQ_OSC, "l"s});
                break;

        case VTE_XTERM_WM_TITLE_STACK_PUSH:
                switch (seq.collect1(1)) {
                case -1:
                case VTE_OSC_XTERM_SET_WINDOW_AND_ICON_TITLE:
                case VTE_OSC_XTERM_SET_WINDOW_TITLE:
                        if (m_window_title_stack.size() >= VTE_WINDOW_TITLE_STACK_MAX_DEPTH) {
                                /* Drop the bottommost item */
                                m_window_title_stack.erase(m_window_title_stack.cbegin());
                        }

                        if (m_pending_changes & vte::to_integral(PendingChanges::TITLE))
                                m_window_title_stack.emplace(m_window_title_stack.cend(),
                                                             m_window_title_pending);
                        else
                                m_window_title_stack.emplace(m_window_title_stack.cend(),
                                                             m_window_title);

                        g_assert_cmpuint(m_window_title_stack.size(), <=, VTE_WINDOW_TITLE_STACK_MAX_DEPTH);
                        break;

                case VTE_OSC_XTERM_SET_ICON_TITLE:
                default:
                        break;
                }
                break;

        case VTE_XTERM_WM_TITLE_STACK_POP:
                switch (seq.collect1(1)) {
                case -1:
                case VTE_OSC_XTERM_SET_WINDOW_AND_ICON_TITLE:
                case VTE_OSC_XTERM_SET_WINDOW_TITLE:
                        if (m_window_title_stack.empty())
                                break;

                        m_pending_changes |= vte::to_integral(PendingChanges::TITLE);
                        m_window_title_pending.swap(m_window_title_stack.back());
                        m_window_title_stack.pop_back();
                        break;

                case VTE_OSC_XTERM_SET_ICON_TITLE:
                default:
                        break;
                }
                break;

        default:
                /* DECSLPP, handled elsewhere */
                break;
        }
}

} // namespace terminal
} // namespace vte
