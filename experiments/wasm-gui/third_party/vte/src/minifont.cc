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

#include "config.h"

#include <cmath>

#include <cairo.h>

#include "drawing-cairo.hh"
#include "minifont.hh"

/* pixman data must have stride 0 mod 4 */
static unsigned char const hatching_pattern_lr_data[16] = {
        0xff, 0x00, 0x00, 0x00,
        0x00, 0xff, 0x00, 0x00,
        0x00, 0x00, 0xff, 0x00,
        0x00, 0x00, 0x00, 0xff,
};
static unsigned char const hatching_pattern_rl_data[16] = {
        0x00, 0x00, 0x00, 0xff,
        0x00, 0x00, 0xff, 0x00,
        0x00, 0xff, 0x00, 0x00,
        0xff, 0x00, 0x00, 0x00,
};
static unsigned char const checkerboard_pattern_data[16] = {
        0xff, 0xff, 0x00, 0x00,
        0xff, 0xff, 0x00, 0x00,
        0x00, 0x00, 0xff, 0xff,
        0x00, 0x00, 0xff, 0xff,
};
static unsigned char const checkerboard_reverse_pattern_data[16] = {
        0x00, 0x00, 0xff, 0xff,
        0x00, 0x00, 0xff, 0xff,
        0xff, 0xff, 0x00, 0x00,
        0xff, 0xff, 0x00, 0x00,
};

#define DEFINE_STATIC_PATTERN_FUNC(name,data,width,height,stride) \
static cairo_pattern_t* \
name(void) \
{ \
        static cairo_pattern_t* pattern = nullptr; \
\
        if (pattern == nullptr) { \
                auto surface = cairo_image_surface_create_for_data(const_cast<unsigned char*>(data), \
                                                                   CAIRO_FORMAT_A8, \
                                                                   width, \
                                                                   height, \
                                                                   stride); \
                pattern = cairo_pattern_create_for_surface(surface); \
                cairo_surface_destroy(surface); \
\
                cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT); \
                cairo_pattern_set_filter (pattern, CAIRO_FILTER_NEAREST); \
       } \
\
       return pattern; \
}

DEFINE_STATIC_PATTERN_FUNC(create_hatching_pattern_lr, hatching_pattern_lr_data, 4, 4, 4)
DEFINE_STATIC_PATTERN_FUNC(create_hatching_pattern_rl, hatching_pattern_rl_data, 4, 4, 4)
DEFINE_STATIC_PATTERN_FUNC(create_checkerboard_pattern, checkerboard_pattern_data, 4, 4, 4)
DEFINE_STATIC_PATTERN_FUNC(create_checkerboard_reverse_pattern, checkerboard_reverse_pattern_data, 4, 4, 4)

#undef DEFINE_STATIC_PATTERN_FUNC

static void
rectangle(cairo_t* cr,
          double x,
          double y,
          double w,
          double h,
          int xdenom,
          int ydenom,
          int xb1,
          int yb1,
          int xb2,
          int yb2)
{
        int const x1 = (w) * (xb1) / (xdenom);
        int const y1 = (h) * (yb1) / (ydenom);
        int const x2 = (w) * (xb2) / (xdenom);
        int const y2 = (h) * (yb2) / (ydenom);
        cairo_rectangle ((cr), (x) + x1, (y) + y1, MAX(x2 - x1, 1), MAX(y2 - y1, 1));
        cairo_fill (cr);
}

static void
polygon(cairo_t* cr,
        double x,
        double y,
        double w,
        double h,
        int xdenom,
        int ydenom,
        int8_t const* cc)
{
        int x1 = (w) * (cc[0]) / (xdenom);
        int y1 = (h) * (cc[1]) / (ydenom);
        cairo_move_to ((cr), (x) + x1, (y) + y1);
        int i = 2;
        while (cc[i] != -1) {
                x1 = (w) * (cc[i]) / (xdenom);
                y1 = (h) * (cc[i + 1]) / (ydenom);
                cairo_line_to ((cr), (x) + x1, (y) + y1);
                i += 2;
        }
        cairo_fill (cr);
}

static void
pattern(cairo_t* cr,
        cairo_pattern_t* pattern,
        double x,
        double y,
        double width,
        double height)
{
        cairo_push_group(cr);
        cairo_rectangle(cr, x, y, width, height);
        cairo_fill(cr);
        cairo_pop_group_to_source(cr);
        cairo_mask(cr, pattern);
}

#include "box_drawing.h"

namespace vte::view {

/* Draw the graphic representation of a line-drawing or special graphics
 * character. */
void
Minifont::draw_graphic(DrawingContext const& context,
                       vteunistr c,
                       uint32_t const attr,
                       vte::color::rgb const* fg,
                       int x,
                       int y,
                       int font_width,
                       int columns,
                       int font_height)
{
        gint width, height, xcenter, xright, ycenter, ybottom;
        int upper_half, left_half;
        int light_line_width, heavy_line_width;
        double adjust;
        auto cr = context.cairo();

        cairo_save (cr);

        width = context.cell_width() * columns;
        height = context.cell_height();
        upper_half = height / 2;
        left_half = width / 2;

        /* Exclude the spacing for line width computation. */
        light_line_width = font_width / 5;
        light_line_width = MAX (light_line_width, 1);

        if (c >= 0x2550 && c <= 0x256c) {
                heavy_line_width = 3 * light_line_width;
        } else {
                heavy_line_width = light_line_width + 2;
        }

        xcenter = x + left_half;
        ycenter = y + upper_half;
        xright = x + width;
        ybottom = y + height;

        switch (c) {

        /* Box Drawing */
        case 0x1fbaf: /* box drawings light horizontal with vertical stroke */
                rectangle(cr, x + left_half - light_line_width / 2, y,
                          light_line_width, height, 1, 3, 0, 1, 1, 2);
                c = 0x2500;
                [[fallthrough]];
        case 0x2500: /* box drawings light horizontal */
        case 0x2501: /* box drawings heavy horizontal */
        case 0x2502: /* box drawings light vertical */
        case 0x2503: /* box drawings heavy vertical */
        case 0x250c: /* box drawings light down and right */
        case 0x250d: /* box drawings down light and right heavy */
        case 0x250e: /* box drawings down heavy and right light */
        case 0x250f: /* box drawings heavy down and right */
        case 0x2510: /* box drawings light down and left */
        case 0x2511: /* box drawings down light and left heavy */
        case 0x2512: /* box drawings down heavy and left light */
        case 0x2513: /* box drawings heavy down and left */
        case 0x2514: /* box drawings light up and right */
        case 0x2515: /* box drawings up light and right heavy */
        case 0x2516: /* box drawings up heavy and right light */
        case 0x2517: /* box drawings heavy up and right */
        case 0x2518: /* box drawings light up and left */
        case 0x2519: /* box drawings up light and left heavy */
        case 0x251a: /* box drawings up heavy and left light */
        case 0x251b: /* box drawings heavy up and left */
        case 0x251c: /* box drawings light vertical and right */
        case 0x251d: /* box drawings vertical light and right heavy */
        case 0x251e: /* box drawings up heavy and right down light */
        case 0x251f: /* box drawings down heavy and right up light */
        case 0x2520: /* box drawings vertical heavy and right light */
        case 0x2521: /* box drawings down light and right up heavy */
        case 0x2522: /* box drawings up light and right down heavy */
        case 0x2523: /* box drawings heavy vertical and right */
        case 0x2524: /* box drawings light vertical and left */
        case 0x2525: /* box drawings vertical light and left heavy */
        case 0x2526: /* box drawings up heavy and left down light */
        case 0x2527: /* box drawings down heavy and left up light */
        case 0x2528: /* box drawings vertical heavy and left light */
        case 0x2529: /* box drawings down light and left up heavy */
        case 0x252a: /* box drawings up light and left down heavy */
        case 0x252b: /* box drawings heavy vertical and left */
        case 0x252c: /* box drawings light down and horizontal */
        case 0x252d: /* box drawings left heavy and right down light */
        case 0x252e: /* box drawings right heavy and left down light */
        case 0x252f: /* box drawings down light and horizontal heavy */
        case 0x2530: /* box drawings down heavy and horizontal light */
        case 0x2531: /* box drawings right light and left down heavy */
        case 0x2532: /* box drawings left light and right down heavy */
        case 0x2533: /* box drawings heavy down and horizontal */
        case 0x2534: /* box drawings light up and horizontal */
        case 0x2535: /* box drawings left heavy and right up light */
        case 0x2536: /* box drawings right heavy and left up light */
        case 0x2537: /* box drawings up light and horizontal heavy */
        case 0x2538: /* box drawings up heavy and horizontal light */
        case 0x2539: /* box drawings right light and left up heavy */
        case 0x253a: /* box drawings left light and right up heavy */
        case 0x253b: /* box drawings heavy up and horizontal */
        case 0x253c: /* box drawings light vertical and horizontal */
        case 0x253d: /* box drawings left heavy and right vertical light */
        case 0x253e: /* box drawings right heavy and left vertical light */
        case 0x253f: /* box drawings vertical light and horizontal heavy */
        case 0x2540: /* box drawings up heavy and down horizontal light */
        case 0x2541: /* box drawings down heavy and up horizontal light */
        case 0x2542: /* box drawings vertical heavy and horizontal light */
        case 0x2543: /* box drawings left up heavy and right down light */
        case 0x2544: /* box drawings right up heavy and left down light */
        case 0x2545: /* box drawings left down heavy and right up light */
        case 0x2546: /* box drawings right down heavy and left up light */
        case 0x2547: /* box drawings down light and up horizontal heavy */
        case 0x2548: /* box drawings up light and down horizontal heavy */
        case 0x2549: /* box drawings right light and left vertical heavy */
        case 0x254a: /* box drawings left light and right vertical heavy */
        case 0x254b: /* box drawings heavy vertical and horizontal */
        case 0x2550: /* box drawings double horizontal */
        case 0x2551: /* box drawings double vertical */
        case 0x2552: /* box drawings down single and right double */
        case 0x2553: /* box drawings down double and right single */
        case 0x2554: /* box drawings double down and right */
        case 0x2555: /* box drawings down single and left double */
        case 0x2556: /* box drawings down double and left single */
        case 0x2557: /* box drawings double down and left */
        case 0x2558: /* box drawings up single and right double */
        case 0x2559: /* box drawings up double and right single */
        case 0x255a: /* box drawings double up and right */
        case 0x255b: /* box drawings up single and left double */
        case 0x255c: /* box drawings up double and left single */
        case 0x255d: /* box drawings double up and left */
        case 0x255e: /* box drawings vertical single and right double */
        case 0x255f: /* box drawings vertical double and right single */
        case 0x2560: /* box drawings double vertical and right */
        case 0x2561: /* box drawings vertical single and left double */
        case 0x2562: /* box drawings vertical double and left single */
        case 0x2563: /* box drawings double vertical and left */
        case 0x2564: /* box drawings down single and horizontal double */
        case 0x2565: /* box drawings down double and horizontal single */
        case 0x2566: /* box drawings double down and horizontal */
        case 0x2567: /* box drawings up single and horizontal double */
        case 0x2568: /* box drawings up double and horizontal single */
        case 0x2569: /* box drawings double up and horizontal */
        case 0x256a: /* box drawings vertical single and horizontal double */
        case 0x256b: /* box drawings vertical double and horizontal single */
        case 0x256c: /* box drawings double vertical and horizontal */
        case 0x2574: /* box drawings light left */
        case 0x2575: /* box drawings light up */
        case 0x2576: /* box drawings light right */
        case 0x2577: /* box drawings light down */
        case 0x2578: /* box drawings heavy left */
        case 0x2579: /* box drawings heavy up */
        case 0x257a: /* box drawings heavy right */
        case 0x257b: /* box drawings heavy down */
        case 0x257c: /* box drawings light left and heavy right */
        case 0x257d: /* box drawings light up and heavy down */
        case 0x257e: /* box drawings heavy left and light right */
        case 0x257f: /* box drawings heavy up and light down */
        {
                guint32 bitmap = _vte_draw_box_drawing_bitmaps[c - 0x2500];
                int xboundaries[6] = { 0,
                                       left_half - heavy_line_width / 2,
                                       left_half - light_line_width / 2,
                                       left_half - light_line_width / 2 + light_line_width,
                                       left_half - heavy_line_width / 2 + heavy_line_width,
                                       width};
                int yboundaries[6] = { 0,
                                       upper_half - heavy_line_width / 2,
                                       upper_half - light_line_width / 2,
                                       upper_half - light_line_width / 2 + light_line_width,
                                       upper_half - heavy_line_width / 2 + heavy_line_width,
                                       height};
                int xi, yi;
                cairo_set_line_width(cr, 0);
                for (yi = 4; yi >= 0; yi--) {
                        for (xi = 4; xi >= 0; xi--) {
                                if (bitmap & 1) {
                                        cairo_rectangle(cr,
                                                        x + xboundaries[xi],
                                                        y + yboundaries[yi],
                                                        xboundaries[xi + 1] - xboundaries[xi],
                                                        yboundaries[yi + 1] - yboundaries[yi]);
                                        cairo_fill(cr);
                                }
                                bitmap >>= 1;
                        }
                }
                break;
        }

        case 0x2504: /* box drawings light triple dash horizontal */
        case 0x2505: /* box drawings heavy triple dash horizontal */
        case 0x2506: /* box drawings light triple dash vertical */
        case 0x2507: /* box drawings heavy triple dash vertical */
        case 0x2508: /* box drawings light quadruple dash horizontal */
        case 0x2509: /* box drawings heavy quadruple dash horizontal */
        case 0x250a: /* box drawings light quadruple dash vertical */
        case 0x250b: /* box drawings heavy quadruple dash vertical */
        case 0x254c: /* box drawings light double dash horizontal */
        case 0x254d: /* box drawings heavy double dash horizontal */
        case 0x254e: /* box drawings light double dash vertical */
        case 0x254f: /* box drawings heavy double dash vertical */
        {
                const guint v = c - 0x2500;
                int size, line_width;

                size = (v & 2) ? height : width;

                switch (v >> 2) {
                case 1: /* triple dash */
                {
                        double segment = size / 8.;
                        double dashes[2] = { segment * 2., segment };
                        cairo_set_dash(cr, dashes, G_N_ELEMENTS(dashes), 0.);
                        break;
                }
                case 2: /* quadruple dash */
                {
                        double segment = size / 11.;
                        double dashes[2] = { segment * 2., segment };
                        cairo_set_dash(cr, dashes, G_N_ELEMENTS(dashes), 0.);
                        break;
                }
                case 19: /* double dash */
                {
                        double segment = size / 5.;
                        double dashes[2] = { segment * 2., segment };
                        cairo_set_dash(cr, dashes, G_N_ELEMENTS(dashes), 0.);
                        break;
                }
                }

                line_width = (v & 1) ? heavy_line_width : light_line_width;
                adjust = (line_width & 1) ? .5 : 0.;

                cairo_set_line_width(cr, line_width);
                cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
                if (v & 2) {
                        cairo_move_to(cr, xcenter + adjust, y);
                        cairo_line_to(cr, xcenter + adjust, y + height);
                } else {
                        cairo_move_to(cr, x, ycenter + adjust);
                        cairo_line_to(cr, x + width, ycenter + adjust);
                }
                cairo_stroke(cr);
                break;
        }

        case 0x256d: /* box drawings light arc down and right */
        case 0x256e: /* box drawings light arc down and left */
        case 0x256f: /* box drawings light arc up and left */
        case 0x2570: /* box drawings light arc up and right */
        {
                const guint v = c - 0x256d;
                int line_width;
                int radius;

                cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);

                line_width = light_line_width;
                adjust = (line_width & 1) ? .5 : 0.;
                cairo_set_line_width(cr, line_width);

                radius = (font_width + 2) / 3;
                radius = MAX(radius, heavy_line_width);

                if (v & 2) {
                        cairo_move_to(cr, xcenter + adjust, y);
                        cairo_line_to(cr, xcenter + adjust, ycenter - radius + 2 * adjust);
                } else {
                        cairo_move_to(cr, xcenter + adjust, ybottom);
                        cairo_line_to(cr, xcenter + adjust, ycenter + radius);
                }
                cairo_stroke(cr);

                cairo_arc(cr,
                          (v == 1 || v == 2) ? xcenter - radius + 2 * adjust
                                             : xcenter + radius,
                          (v & 2) ? ycenter - radius + 2 * adjust
                                  : ycenter + radius,
                          radius - adjust,
                          (v + 2) * M_PI / 2.0, (v + 3) * M_PI / 2.0);
                cairo_stroke(cr);

                if (v == 1 || v == 2) {
                        cairo_move_to(cr, xcenter - radius + 2 * adjust, ycenter + adjust);
                        cairo_line_to(cr, x, ycenter + adjust);
                } else {
                        cairo_move_to(cr, xcenter + radius, ycenter + adjust);
                        cairo_line_to(cr, xright, ycenter + adjust);
                }

                cairo_stroke(cr);
                break;
        }

        case 0x2571: /* box drawings light diagonal upper right to lower left */
        case 0x2572: /* box drawings light diagonal upper left to lower right */
        case 0x2573: /* box drawings light diagonal cross */
        {
                auto const dx = (light_line_width + 1) / 2;
                cairo_rectangle(cr, x - dx, y, width + 2 * dx, height);
                cairo_clip(cr);
                cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE);
                cairo_set_line_width(cr, light_line_width);
                if (c != 0x2571) {
                        cairo_move_to(cr, x, y);
                        cairo_line_to(cr, xright, ybottom);
                        cairo_stroke(cr);
                }
                if (c != 0x2572) {
                        cairo_move_to(cr, xright, y);
                        cairo_line_to(cr, x, ybottom);
                        cairo_stroke(cr);
                }
                break;
        }

        /* Block Elements */
        case 0x2580: /* upper half block */
                rectangle(cr, x, y, width, height, 1, 2,  0, 0,  1, 1);
                break;

        case 0x2581: /* lower one eighth block */
        case 0x2582: /* lower one quarter block */
        case 0x2583: /* lower three eighths block */
        case 0x2584: /* lower half block */
        case 0x2585: /* lower five eighths block */
        case 0x2586: /* lower three quarters block */
        case 0x2587: /* lower seven eighths block */
        {
                const guint v = 0x2588 - c;
                rectangle(cr, x, y, width, height, 1, 8,  0, v,  1, 8);
                break;
        }

        case 0x2588: /* full block */
        case 0x2589: /* left seven eighths block */
        case 0x258a: /* left three quarters block */
        case 0x258b: /* left five eighths block */
        case 0x258c: /* left half block */
        case 0x258d: /* left three eighths block */
        case 0x258e: /* left one quarter block */
        case 0x258f: /* left one eighth block */
        {
                const guint v = 0x2590 - c;
                rectangle(cr, x, y, width, height, 8, 1,  0, 0,  v, 1);
                break;
        }

        case 0x2590: /* right half block */
                rectangle(cr, x, y, width, height, 2, 1,  1, 0,  2, 1);
                break;

        case 0x2591: /* light shade */
        case 0x2592: /* medium shade */
        case 0x2593: /* dark shade */
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       (c - 0x2590) / 4.);
                cairo_rectangle(cr, x, y, width, height);
                cairo_fill (cr);
                break;

        case 0x2594: /* upper one eighth block */
        {
                rectangle(cr, x, y, width, height, 1, 8,  0, 0,  1, 1);
                break;
        }

        case 0x2595: /* right one eighth block */
        {
                rectangle(cr, x, y, width, height, 8, 1,  7, 0,  8, 1);
                break;
        }

        case 0x2596: /* quadrant lower left */
                rectangle(cr, x, y, width, height, 2, 2,  0, 1,  1, 2);
                break;

        case 0x2597: /* quadrant lower right */
                rectangle(cr, x, y, width, height, 2, 2,  1, 1,  2, 2);
                break;

        case 0x2598: /* quadrant upper left */
                rectangle(cr, x, y, width, height, 2, 2,  0, 0,  1, 1);
                break;

        case 0x2599: /* quadrant upper left and lower left and lower right */
                rectangle(cr, x, y, width, height, 2, 2,  0, 0,  1, 1);
                rectangle(cr, x, y, width, height, 2, 2,  0, 1,  2, 2);
                break;

        case 0x259a: /* quadrant upper left and lower right */
                rectangle(cr, x, y, width, height, 2, 2,  0, 0,  1, 1);
                rectangle(cr, x, y, width, height, 2, 2,  1, 1,  2, 2);
                break;

        case 0x259b: /* quadrant upper left and upper right and lower left */
                rectangle(cr, x, y, width, height, 2, 2,  0, 0,  2, 1);
                rectangle(cr, x, y, width, height, 2, 2,  0, 1,  1, 2);
                break;

        case 0x259c: /* quadrant upper left and upper right and lower right */
                rectangle(cr, x, y, width, height, 2, 2,  0, 0,  2, 1);
                rectangle(cr, x, y, width, height, 2, 2,  1, 1,  2, 2);
                break;

        case 0x259d: /* quadrant upper right */
                rectangle(cr, x, y, width, height, 2, 2,  1, 0,  2, 1);
                break;

        case 0x259e: /* quadrant upper right and lower left */
                rectangle(cr, x, y, width, height, 2, 2,  1, 0,  2, 1);
                rectangle(cr, x, y, width, height, 2, 2,  0, 1,  1, 2);
                break;

        case 0x259f: /* quadrant upper right and lower left and lower right */
                rectangle(cr, x, y, width, height, 2, 2,  1, 0,  2, 1);
                rectangle(cr, x, y, width, height, 2, 2,  0, 1,  2, 2);
                break;

        case 0x25e2: /* black lower right triangle */
        {
                static int8_t const coords[] = { 0, 1,  1, 0,  1, 1,  -1 };
                polygon(cr, x, y, width, height, 1, 1, coords);
                break;
        }

        case 0x25e3: /* black lower left triangle */
        {
                static int8_t const coords[] = { 0, 0,  1, 1,  0, 1,  -1 };
                polygon(cr, x, y, width, height, 1, 1, coords);
                break;
        }

        case 0x25e4: /* black upper left triangle */
        {
                static int8_t const coords[] = { 0, 0,  1, 0,  0, 1,  -1 };
                polygon(cr, x, y, width, height, 1, 1, coords);
                break;
        }

        case 0x25e5: /* black upper right triangle */
        {
                static int8_t const coords[] = { 0, 0,  1, 0,  1, 1,  -1 };
                polygon(cr, x, y, width, height, 1, 1, coords);
                break;
        }

        case 0x1fb00:
        case 0x1fb01:
        case 0x1fb02:
        case 0x1fb03:
        case 0x1fb04:
        case 0x1fb05:
        case 0x1fb06:
        case 0x1fb07:
        case 0x1fb08:
        case 0x1fb09:
        case 0x1fb0a:
        case 0x1fb0b:
        case 0x1fb0c:
        case 0x1fb0d:
        case 0x1fb0e:
        case 0x1fb0f:
        case 0x1fb10:
        case 0x1fb11:
        case 0x1fb12:
        case 0x1fb13:
        case 0x1fb14:
        case 0x1fb15:
        case 0x1fb16:
        case 0x1fb17:
        case 0x1fb18:
        case 0x1fb19:
        case 0x1fb1a:
        case 0x1fb1b:
        case 0x1fb1c:
        case 0x1fb1d:
        case 0x1fb1e:
        case 0x1fb1f:
        case 0x1fb20:
        case 0x1fb21:
        case 0x1fb22:
        case 0x1fb23:
        case 0x1fb24:
        case 0x1fb25:
        case 0x1fb26:
        case 0x1fb27:
        case 0x1fb28:
        case 0x1fb29:
        case 0x1fb2a:
        case 0x1fb2b:
        case 0x1fb2c:
        case 0x1fb2d:
        case 0x1fb2e:
        case 0x1fb2f:
        case 0x1fb30:
        case 0x1fb31:
        case 0x1fb32:
        case 0x1fb33:
        case 0x1fb34:
        case 0x1fb35:
        case 0x1fb36:
        case 0x1fb37:
        case 0x1fb38:
        case 0x1fb39:
        case 0x1fb3a:
        case 0x1fb3b:
        {
                guint32 bitmap = c - 0x1fb00 + 1;
                if (bitmap >= 0x15) bitmap++;
                if (bitmap >= 0x2a) bitmap++;
                int xi, yi;
                cairo_set_line_width(cr, 0);
                for (yi = 0; yi <= 2; yi++) {
                        for (xi = 0; xi <= 1; xi++) {
                                if (bitmap & 1) {
                                        rectangle(cr, x, y, width, height, 2, 3,  xi, yi, xi + 1,  yi + 1);
                                }
                                bitmap >>= 1;
                        }
                }
                break;
        }

        case 0x1fb3c:
        case 0x1fb3d:
        case 0x1fb3e:
        case 0x1fb3f:
        case 0x1fb40:
        case 0x1fb41:
        case 0x1fb42:
        case 0x1fb43:
        case 0x1fb44:
        case 0x1fb45:
        case 0x1fb46:
        case 0x1fb47:
        case 0x1fb48:
        case 0x1fb49:
        case 0x1fb4a:
        case 0x1fb4b:
        case 0x1fb4c:
        case 0x1fb4d:
        case 0x1fb4e:
        case 0x1fb4f:
        case 0x1fb50:
        case 0x1fb51:
        case 0x1fb52:
        case 0x1fb53:
        case 0x1fb54:
        case 0x1fb55:
        case 0x1fb56:
        case 0x1fb57:
        case 0x1fb58:
        case 0x1fb59:
        case 0x1fb5a:
        case 0x1fb5b:
        case 0x1fb5c:
        case 0x1fb5d:
        case 0x1fb5e:
        case 0x1fb5f:
        case 0x1fb60:
        case 0x1fb61:
        case 0x1fb62:
        case 0x1fb63:
        case 0x1fb64:
        case 0x1fb65:
        case 0x1fb66:
        case 0x1fb67:
        {
                auto const v = c - 0x1fb3c;
                static int8_t const coords[46][11] = {
                        { 0, 2,  1, 3,  0, 3,  -1 },                /* 3c */
                        { 0, 2,  2, 3,  0, 3,  -1 },                /* 3d */
                        { 0, 1,  1, 3,  0, 3,  -1 },                /* 3e */
                        { 0, 1,  2, 3,  0, 3,  -1 },                /* 3f */
                        { 0, 0,  1, 3,  0, 3,  -1 },                /* 40 */
                        { 0, 1,  1, 0,  2, 0,  2, 3,  0, 3,  -1 },  /* 41 */
                        { 0, 1,  2, 0,  2, 3,  0, 3,  -1 },         /* 42 */
                        { 0, 2,  1, 0,  2, 0,  2, 3,  0, 3,  -1 },  /* 43 */
                        { 0, 2,  2, 0,  2, 3,  0, 3,  -1 },         /* 44 */
                        { 0, 3,  1, 0,  2, 0,  2, 3,  -1 },         /* 45 */
                        { 0, 2,  2, 1,  2, 3,  0, 3,  -1 },         /* 46 */
                        { 1, 3,  2, 2,  2, 3,  -1 },                /* 47 */
                        { 0, 3,  2, 2,  2, 3,  -1 },                /* 48 */
                        { 1, 3,  2, 1,  2, 3,  -1 },                /* 49 */
                        { 0, 3,  2, 1,  2, 3,  -1 },                /* 4a */
                        { 1, 3,  2, 0,  2, 3,  -1 },                /* 4b */
                        { 0, 0,  1, 0,  2, 1,  2, 3,  0, 3,  -1 },  /* 4c */
                        { 0, 0,  2, 1,  2, 3,  0, 3,  -1 },         /* 4d */
                        { 0, 0,  1, 0,  2, 2,  2, 3,  0, 3,  -1 },  /* 4e */
                        { 0, 0,  2, 2,  2, 3,  0, 3,  -1 },         /* 4f */
                        { 0, 0,  1, 0,  2, 3,  0, 3,  -1 },         /* 50 */
                        { 0, 1,  2, 2,  2, 3,  0, 3,  -1 },         /* 51 */
                        { 0, 0,  2, 0,  2, 3,  1, 3,  0, 2,  -1 },  /* 52 */
                        { 0, 0,  2, 0,  2, 3,  0, 2,  -1 },         /* 53 */
                        { 0, 0,  2, 0,  2, 3,  1, 3,  0, 1,  -1 },  /* 54 */
                        { 0, 0,  2, 0,  2, 3,  0, 1,  -1 },         /* 55 */
                        { 0, 0,  2, 0,  2, 3,  1, 3,  -1 },         /* 56 */
                        { 0, 0,  1, 0,  0, 1,  -1 },                /* 57 */
                        { 0, 0,  2, 0,  0, 1,  -1 },                /* 58 */
                        { 0, 0,  1, 0,  0, 2,  -1 },                /* 59 */
                        { 0, 0,  2, 0,  0, 2,  -1 },                /* 5a */
                        { 0, 0,  1, 0,  0, 3,  -1 },                /* 5b */
                        { 0, 0,  2, 0,  2, 1,  0, 2,  -1 },         /* 5c */
                        { 0, 0,  2, 0,  2, 2,  1, 3,  0, 3,  -1 },  /* 5d */
                        { 0, 0,  2, 0,  2, 2,  0, 3,  -1 },         /* 5e */
                        { 0, 0,  2, 0,  2, 1,  1, 3,  0, 3,  -1 },  /* 5f */
                        { 0, 0,  2, 0,  2, 1,  0, 3,  -1 },         /* 60 */
                        { 0, 0,  2, 0,  1, 3,  0, 3,  -1 },         /* 61 */
                        { 1, 0,  2, 0,  2, 1,  -1 },                /* 62 */
                        { 0, 0,  2, 0,  2, 1,  -1 },                /* 63 */
                        { 1, 0,  2, 0,  2, 2,  -1 },                /* 64 */
                        { 0, 0,  2, 0,  2, 2,  -1 },                /* 65 */
                        { 1, 0,  2, 0,  2, 3,  -1 },                /* 66 */
                        { 0, 0,  2, 0,  2, 2,  0, 1,  -1 },         /* 67 */
                };
                polygon(cr, x, y, width, height, 2, 3, coords[v]);
                break;
        }

        case 0x1fb68:
        case 0x1fb69:
        case 0x1fb6a:
        case 0x1fb6b:
        case 0x1fb6c:
        case 0x1fb6d:
        case 0x1fb6e:
        case 0x1fb6f:
        {
                auto const v = c - 0x1fb68;
                static int8_t const coords[8][11] = {
                        { 0, 0,  2, 0,  2, 2,  0, 2,  1, 1,  -1 },  /* 68 */
                        { 0, 0,  1, 1,  2, 0,  2, 2,  0, 2,  -1 },  /* 69 */
                        { 0, 0,  2, 0,  1, 1,  2, 2,  0, 2,  -1 },  /* 6a */
                        { 0, 0,  2, 0,  2, 2,  1, 1,  0, 2,  -1 },  /* 6b */
                        { 0, 0,  1, 1,  0, 2,  -1 },                /* 6c */
                        { 0, 0,  2, 0,  1, 1,  -1 },                /* 6d */
                        { 1, 1,  2, 0,  2, 2,  -1 },                /* 6e */
                        { 1, 1,  2, 2,  0, 2,  -1 },                /* 6f */
                };
                polygon(cr, x, y, width, height, 2, 2, coords[v]);
                break;
        }

        case 0x1fb70:
        case 0x1fb71:
        case 0x1fb72:
        case 0x1fb73:
        case 0x1fb74:
        case 0x1fb75:
        {
                auto const v = c - 0x1fb70 + 1;
                rectangle(cr, x, y, width, height, 8, 1,  v, 0,  v + 1, 1);
                break;
        }

        case 0x1fb76:
        case 0x1fb77:
        case 0x1fb78:
        case 0x1fb79:
        case 0x1fb7a:
        case 0x1fb7b:
        {
                auto const v = c - 0x1fb76 + 1;
                rectangle(cr, x, y, width, height, 1, 8,  0, v,  1, v + 1);
                break;
        }

        case 0x1fb7c:
                rectangle(cr, x, y, width, height, 1, 8,  0, 7,  1, 8);
                rectangle(cr, x, y, width, height, 8, 1,  0, 0,  1, 1);
                break;

        case 0x1fb7d:
                rectangle(cr, x, y, width, height, 1, 8,  0, 0,  1, 1);
                rectangle(cr, x, y, width, height, 8, 1,  0, 0,  1, 1);
                break;

        case 0x1fb7e:
                rectangle(cr, x, y, width, height, 1, 8,  0, 0,  1, 1);
                rectangle(cr, x, y, width, height, 8, 1,  7, 0,  8, 1);
                break;

        case 0x1fb7f:
                rectangle(cr, x, y, width, height, 1, 8,  0, 7,  1, 8);
                rectangle(cr, x, y, width, height, 8, 1,  7, 0,  8, 1);
                break;

        case 0x1fb80:
                rectangle(cr, x, y, width, height, 1, 8,  0, 0,  1, 1);
                rectangle(cr, x, y, width, height, 1, 8,  0, 7,  1, 8);
                break;

        case 0x1fb81:
                rectangle(cr, x, y, width, height, 1, 8,  0, 0,  1, 1);
                rectangle(cr, x, y, width, height, 1, 8,  0, 2,  1, 3);
                rectangle(cr, x, y, width, height, 1, 8,  0, 4,  1, 5);
                rectangle(cr, x, y, width, height, 1, 8,  0, 7,  1, 8);
                break;

        case 0x1fb82:
        case 0x1fb83:
        case 0x1fb84:
        case 0x1fb85:
        case 0x1fb86:
        {
                auto v = c - 0x1fb82 + 2;
                if (v >= 4) v++;
                rectangle(cr, x, y, width, height, 1, 8,  0, 0,  1, v);
                break;
        }

        case 0x1fb87:
        case 0x1fb88:
        case 0x1fb89:
        case 0x1fb8a:
        case 0x1fb8b:
        {
                auto v = c - 0x1fb87 + 2;
                if (v >= 4) v++;
                rectangle(cr, x, y, width, height, 8, 1,  8 - v, 0,  8, 1);
                break;
        }

        case 0x1fb8c:
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       0.5);
                rectangle(cr, x, y, width, height, 2, 1,  0, 0,  1, 1);
                break;

        case 0x1fb8d:
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       0.5);
                rectangle(cr, x, y, width, height, 2, 1,  1, 0,  2, 1);
                break;

        case 0x1fb8e:
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       0.5);
                rectangle(cr, x, y, width, height, 1, 2,  0, 0,  1, 1);
                break;

        case 0x1fb8f:
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       0.5);
                rectangle(cr, x, y, width, height, 1, 2,  0, 1,  1, 2);
                break;

        case 0x1fb90:
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       0.5);
                rectangle(cr, x, y, width, height, 1, 1,  0, 0,  1, 1);
                break;

        case 0x1fb91:
                rectangle(cr, x, y, width, height, 1, 2,  0, 0,  1, 1);
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       0.5);
                rectangle(cr, x, y, width, height, 1, 2,  0, 1,  1, 2);
                break;

        case 0x1fb92:
                rectangle(cr, x, y, width, height, 1, 2,  0, 1,  1, 2);
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       0.5);
                rectangle(cr, x, y, width, height, 1, 2,  0, 0,  1, 1);
                break;

        case 0x1fb93:
#if 0
                /* codepoint not assigned */
                rectangle(cr, x, y, width, height, 2, 1,  0, 0,  1, 1);
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       0.5);
                rectangle(cr, x, y, width, height, 2, 1,  1, 0,  2, 1);
#endif
                break;

        case 0x1fb94:
                rectangle(cr, x, y, width, height, 2, 1,  1, 0,  2, 1);
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       0.5);
                rectangle(cr, x, y, width, height, 2, 1,  0, 0,  1, 1);
                break;

        case 0x1fb95:
                pattern(cr, create_checkerboard_pattern(), x, y, width, height);
                break;

        case 0x1fb96:
                pattern(cr, create_checkerboard_reverse_pattern(), x, y, width, height);
                break;

        case 0x1fb97:
                rectangle(cr, x, y, width, height, 1, 4,  0, 1,  1, 2);
                rectangle(cr, x, y, width, height, 1, 4,  0, 3,  1, 4);
                break;

        case 0x1fb98:
                pattern(cr, create_hatching_pattern_lr(), x, y, width, height);
                break;

        case 0x1fb99:
                pattern(cr, create_hatching_pattern_rl(), x, y, width, height);
                break;

        case 0x1fb9a:
        {
                static int8_t const coords[] = { 0, 0,  1, 0,  0, 1,  1, 1,  -1 };
                polygon(cr, x, y, width, height, 1, 1, coords);
                break;
        }

        case 0x1fb9b:
        {
                static int8_t coords[] = { 0, 0,  1, 1,  1, 0,  0, 1,  -1 };
                polygon(cr, x, y, width, height, 1, 1, coords);
                break;
        }

        case 0x1fb9c:
        {
                static int8_t const coords[] = { 0, 0,  1, 0,  0, 1,  -1 };
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       0.5);
                polygon(cr, x, y, width, height, 1, 1, coords);
                break;
        }

        case 0x1fb9d:
        {
                static int8_t const coords[] = { 0, 0,  1, 0,  1, 1,  -1 };
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       0.5);
                polygon(cr, x, y, width, height, 1, 1, coords);
                break;
        }

        case 0x1fb9e:
        {
                static int8_t const coords[] = { 0, 1,  1, 0,  1, 1,  -1 };
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       0.5);
                polygon(cr, x, y, width, height, 1, 1, coords);
                break;
        }

        case 0x1fb9f:
        {
                static int8_t const coords[] = { 0, 0,  1, 1,  0, 1,  -1 };
                cairo_set_source_rgba (cr,
                                       fg->red / 65535.,
                                       fg->green / 65535.,
                                       fg->blue / 65535.,
                                       0.5);
                polygon(cr, x, y, width, height, 1, 1, coords);
                break;
        }

        case 0x1fba0:
        case 0x1fba1:
        case 0x1fba2:
        case 0x1fba3:
        case 0x1fba4:
        case 0x1fba5:
        case 0x1fba6:
        case 0x1fba7:
        case 0x1fba8:
        case 0x1fba9:
        case 0x1fbaa:
        case 0x1fbab:
        case 0x1fbac:
        case 0x1fbad:
        case 0x1fbae:
        {
                auto const v = c - 0x1fba0;
                static uint8_t const map[15] = { 0b0001, 0b0010, 0b0100, 0b1000, 0b0101, 0b1010, 0b1100, 0b0011,
                                                 0b1001, 0b0110, 0b1110, 0b1101, 0b1011, 0b0111, 0b1111 };
                cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
                cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
                cairo_set_line_width(cr, light_line_width);
                adjust = (light_line_width & 1) ? .5 : 0.;
                double const dx = light_line_width / 2.;
                double const dy = light_line_width / 2.;
                if (map[v] & 1) {
                        /* upper left */
                        cairo_move_to(cr, x, ycenter + adjust);
                        cairo_line_to(cr, x + dx, ycenter + adjust);
                        cairo_line_to(cr, xcenter + adjust, y + dy);
                        cairo_line_to(cr, xcenter + adjust, y);
                        cairo_stroke(cr);
                }
                if (map[v] & 2) {
                        /* upper right */
                        cairo_move_to(cr, xright, ycenter + adjust);
                        cairo_line_to(cr, xright - dx, ycenter + adjust);
                        cairo_line_to(cr, xcenter + adjust, y + dy);
                        cairo_line_to(cr, xcenter + adjust, y);
                        cairo_stroke(cr);
                }
                if (map[v] & 4) {
                        /* lower left */
                        cairo_move_to(cr, x, ycenter + adjust);
                        cairo_line_to(cr, x + dx, ycenter + adjust);
                        cairo_line_to(cr, xcenter + adjust, ybottom - dy);
                        cairo_line_to(cr, xcenter + adjust, ybottom);
                        cairo_stroke(cr);
                }
                if (map[v] & 8) {
                        /* lower right */
                        cairo_move_to(cr, xright, ycenter + adjust);
                        cairo_line_to(cr, xright - dx, ycenter + adjust);
                        cairo_line_to(cr, xcenter + adjust, ybottom - dy);
                        cairo_line_to(cr, xcenter + adjust, ybottom);
                        cairo_stroke(cr);
                }
                break;
        }

        default:
                g_assert_not_reached();
        }

        cairo_restore(cr);
}

} // namespace vte::view
