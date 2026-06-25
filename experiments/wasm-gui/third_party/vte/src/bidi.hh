/*
 * Copyright © 2018–2019 Egmont Koblinger
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

#pragma once

#include <glib.h>

#include "ring.hh"
#include "ringview.hh"
#include "vterowdata.hh"
#include "vtetypes.hh"
#include "vteunistr.h"

namespace vte {

namespace base {

class RingView;

/* BidiRow contains the BiDi transformation of a single row. */
class BidiRow {
        friend class BidiRunner;

public:
        BidiRow() { }
        ~BidiRow();

        // prevent accidents
        BidiRow(BidiRow& o) = delete;
        BidiRow(BidiRow const& o) = delete;
        BidiRow(BidiRow&& o) = delete;
        BidiRow& operator= (BidiRow& o) = delete;
        BidiRow& operator= (BidiRow const& o) = delete;
        BidiRow& operator= (BidiRow&& o) = delete;

        vte::grid::column_t log2vis(vte::grid::column_t col) const;
        vte::grid::column_t vis2log(vte::grid::column_t col) const;
        bool log_is_rtl(vte::grid::column_t col) const;
        bool vis_is_rtl(vte::grid::column_t col) const;
        vteunistr vis_get_shaped_char(vte::grid::column_t col, vteunistr s) const;

        /* Whether the line's base direction is RTL. */
        inline constexpr bool base_is_rtl() const noexcept { return m_base_rtl; }

        /* Whether the implicit paragraph contains a foreign directionality character.
         * This is used in the cursor, showing the character's directionality. */
        inline constexpr bool has_foreign() const noexcept { return m_has_foreign; }

private:
        void set_width(vte::grid::column_t width);

        /* The value of m_width == 0 is a valid representation of the trivial LTR mapping. */
        uint16_t m_width{0};
        uint16_t m_width_alloc{0};

        /* These will be initialized / allocated on demand, when some shuffling or shaping is needed. */
        uint16_t *m_log2vis{nullptr};
        uint16_t *m_vis2log{nullptr};
        uint8_t  *m_vis_rtl{nullptr};  /* FIXME use a bitset */
        gunichar *m_vis_shaped_base_char{nullptr};  /* without combining accents */

        bool m_base_rtl{false};
        bool m_has_foreign{false};
};


/* BidiRunner is not a "real" class, rather the collection of methods that run the BiDi algorithm. */
class BidiRunner {
public:
        constexpr BidiRunner(RingView *ringview) : m_ringview{ringview} { }
        ~BidiRunner() { }

        // prevent accidents
        BidiRunner(BidiRunner& o) = delete;
        BidiRunner(BidiRunner const& o) = delete;
        BidiRunner(BidiRunner&& o) = delete;
        BidiRunner& operator= (BidiRunner& o) = delete;
        BidiRunner& operator= (BidiRunner const& o) = delete;
        BidiRunner& operator= (BidiRunner&& o) = delete;

        void paragraph(vte::grid::row_t start, vte::grid::row_t end,
                       bool do_bidi, bool do_shaping);

private:
        RingView *m_ringview;

#ifdef WITH_FRIBIDI
        void explicit_line_shape(vte::grid::row_t row);
#endif

        void explicit_line(vte::grid::row_t row, bool rtl, bool do_shaping);
        void explicit_paragraph(vte::grid::row_t start, vte::grid::row_t end, bool rtl, bool do_shaping);
#ifdef WITH_FRIBIDI
        bool implicit_paragraph(vte::grid::row_t start, vte::grid::row_t end, bool do_shaping);
#endif
};

}; /* namespace base */

}; /* namespace vte */


gboolean vte_bidi_get_mirror_char (vteunistr unistr, gboolean mirror_box_drawing, vteunistr *unistr_mirrored);
