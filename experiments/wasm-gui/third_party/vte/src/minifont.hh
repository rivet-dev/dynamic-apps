/*
 * Copyright (C) 2003,2008 Red Hat, Inc.
 * Copyright © 2019, 2020 Christian Persch
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

#include <cstdint>

#include "fwd.hh"
#include "vtetypes.hh"
#include "vteunistr.h"

namespace vte {
namespace view {

class Minifont {
public:

        /* Check if a unicode character is actually a graphic character we draw
         * ourselves to handle cases where fonts don't have glyphs for them.
         */
        static inline constexpr bool
        unistr_is_local_graphic(vteunistr const c) noexcept
        {
                /* Box Drawing & Block Elements */
                return ((c >=  0x2500 && c <=  0x259f) ||
                        (c >=  0x25e2 && c <=  0x25e5) ||
                        (c >= 0x1fb00 && c <= 0x1fbaf));
        }

        /* Draw the graphic representation of a line-drawing or special graphics
         * character.
         */
        void draw_graphic(DrawingContext const& context,
                          vteunistr c,
                          uint32_t const attr,
                          vte::color::rgb const* fg,
                          int x,
                          int y,
                          int font_width,
                          int columns,
                          int font_height);

}; // class Minifont

} // namespace view
} // namespace vte
