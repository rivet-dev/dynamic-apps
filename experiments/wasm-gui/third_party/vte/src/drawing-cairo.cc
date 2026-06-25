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

#include "bidi.hh"
#include "debug.h"
#include "drawing-cairo.hh"
#include "fonts-pangocairo.hh"

/* cairo_show_glyphs accepts runs up to 102 glyphs before it allocates a
 * temporary array.
 *
 * Setting this to a large value can cause dramatic slow-downs for some
 * xservers (notably fglrx), see bug #410534.
 */
#define MAX_RUN_LENGTH 100

#define VTE_DRAW_NORMAL      0
#define VTE_DRAW_BOLD        1
#define VTE_DRAW_ITALIC      2
#define VTE_DRAW_BOLD_ITALIC 3

static unsigned
attr_to_style(uint32_t attr)
{
	auto style = unsigned{0};
	if (attr & VTE_ATTR_BOLD)
		style |= VTE_DRAW_BOLD;
	if (attr & VTE_ATTR_ITALIC)
		style |= VTE_DRAW_ITALIC;
	return style;
}

static inline constexpr double
_vte_draw_get_undercurl_rad(gint width)
{
        return width / 2. / M_SQRT2;
}

static inline constexpr double
_vte_draw_get_undercurl_arc_height(gint width)
{
        return _vte_draw_get_undercurl_rad(width) * (1. - M_SQRT2 / 2.);
}

double
_vte_draw_get_undercurl_height(gint width, double line_width)
{
        return 2. * _vte_draw_get_undercurl_arc_height(width) + line_width;
}

namespace vte {
namespace view {

DrawingContext::~DrawingContext()
{
        clear_font_cache();
}

void
DrawingContext::clear_font_cache()
{
        // m_fonts = {};

	for (auto style = int{0}; style < 4; ++style) {
		if (m_fonts[style] != nullptr)
                        m_fonts[style]->unref();
                m_fonts[style] = nullptr;
        }
}

void
DrawingContext::set_cairo(cairo_t* cr) noexcept
{
        m_cr = cr;
}

void
DrawingContext::clip(cairo_rectangle_int_t const* rect)
{
        cairo_save(m_cr);
        cairo_rectangle(m_cr,
                        rect->x, rect->y, rect->width, rect->height);
        cairo_clip(m_cr);
}

void
DrawingContext::unclip()
{
        cairo_restore(m_cr);
}

void
DrawingContext::set_source_color_alpha(vte::color::rgb const* color,
                                       double alpha)
{
        g_assert(m_cr);
	cairo_set_source_rgba(m_cr,
			      color->red / 65535.,
			      color->green / 65535.,
			      color->blue / 65535.,
			      alpha);
}

void
DrawingContext::clear(int x,
                      int y,
                      int width,
                      int height,
                      vte::color::rgb const* color,
                      double alpha)
{
        g_assert(m_cr);
	cairo_rectangle(m_cr, x, y, width, height);
	cairo_set_operator(m_cr, CAIRO_OPERATOR_SOURCE);
	set_source_color_alpha(color, alpha);
	cairo_fill(m_cr);
}

void
DrawingContext::set_text_font(GtkWidget* widget,
                              PangoFontDescription const* fontdesc,
                              double cell_width_scale,
                              double cell_height_scale)
{
	PangoFontDescription *bolddesc   = nullptr;
	PangoFontDescription *italicdesc = nullptr;
	PangoFontDescription *bolditalicdesc = nullptr;
	gint normal, bold, ratio;

	_vte_debug_print (VTE_DEBUG_DRAW, "draw_set_text_font\n");

        clear_font_cache();

	/* calculate bold font desc */
	bolddesc = pango_font_description_copy (fontdesc);
        if (pango_font_description_get_set_fields(bolddesc) & PANGO_FONT_MASK_WEIGHT) {
                auto const weight = pango_font_description_get_weight(bolddesc);
                auto const bold_weight = std::min(1000, weight + VTE_FONT_WEIGHT_BOLDENING);
                pango_font_description_set_weight(bolddesc, PangoWeight(bold_weight));
        } else {
                pango_font_description_set_weight (bolddesc, PANGO_WEIGHT_BOLD);
        }

	/* calculate italic font desc */
	italicdesc = pango_font_description_copy (fontdesc);
	pango_font_description_set_style (italicdesc, PANGO_STYLE_ITALIC);

	/* calculate bold italic font desc */
	bolditalicdesc = pango_font_description_copy (bolddesc);
	pango_font_description_set_style (bolditalicdesc, PANGO_STYLE_ITALIC);

	m_fonts[VTE_DRAW_NORMAL]  = FontInfo::create_for_widget(widget, fontdesc);
	m_fonts[VTE_DRAW_BOLD]    = FontInfo::create_for_widget(widget, bolddesc);
	m_fonts[VTE_DRAW_ITALIC]  = FontInfo::create_for_widget(widget, italicdesc);
	m_fonts[VTE_DRAW_ITALIC | VTE_DRAW_BOLD] =
                FontInfo::create_for_widget(widget, bolditalicdesc);
	pango_font_description_free (bolddesc);
	pango_font_description_free (italicdesc);
	pango_font_description_free (bolditalicdesc);

	/* Decide if we should keep this bold font face, per bug 54926:
	 *  - reject bold font if it is not within 10% of normal font width
	 */
	normal = VTE_DRAW_NORMAL;
	bold   = normal | VTE_DRAW_BOLD;
	ratio = m_fonts[bold]->width() * 100 / m_fonts[normal]->width();
	if (abs(ratio - 100) > 10) {
		_vte_debug_print (VTE_DEBUG_DRAW,
			"Rejecting bold font (%i%%).\n", ratio);
                m_fonts[bold]->unref();
                m_fonts[bold] = m_fonts[normal]->ref();
	}
	normal = VTE_DRAW_ITALIC;
	bold   = normal | VTE_DRAW_BOLD;
	ratio = m_fonts[bold]->width() * 100 / m_fonts[normal]->width();
	if (abs(ratio - 100) > 10) {
		_vte_debug_print (VTE_DEBUG_DRAW,
			"Rejecting italic bold font (%i%%).\n", ratio);
                m_fonts[bold]->unref();
                m_fonts[bold] = m_fonts[normal]->ref();
	}

        /* Apply letter spacing and line spacing. */
        m_cell_width = m_fonts[VTE_DRAW_NORMAL]->width() * cell_width_scale;
        m_char_spacing.left = (m_cell_width - m_fonts[VTE_DRAW_NORMAL]->width()) / 2;
        m_char_spacing.right = (m_cell_width - m_fonts[VTE_DRAW_NORMAL]->width() + 1) / 2;
        m_cell_height = m_fonts[VTE_DRAW_NORMAL]->height() * cell_height_scale;
        m_char_spacing.top = (m_cell_height - m_fonts[VTE_DRAW_NORMAL]->height() + 1) / 2;
        m_char_spacing.bottom = (m_cell_height - m_fonts[VTE_DRAW_NORMAL]->height()) / 2;

        m_undercurl_surface.reset();
}

void
DrawingContext::get_text_metrics(int* cell_width,
                                 int* cell_height,
                                 int* char_ascent,
                                 int* char_descent,
                                 GtkBorder* char_spacing)
{
	g_return_if_fail (m_fonts[VTE_DRAW_NORMAL] != nullptr);

        if (cell_width)
                *cell_width = m_cell_width;
        if (cell_height)
                *cell_height = m_cell_height;
        if (char_ascent)
                *char_ascent = m_fonts[VTE_DRAW_NORMAL]->ascent();
        if (char_descent)
                *char_descent = m_fonts[VTE_DRAW_NORMAL]->height() - m_fonts[VTE_DRAW_NORMAL]->ascent();
        if (char_spacing)
                *char_spacing = m_char_spacing;
}

/* Stores the left and right edges of the given glyph, relative to the cell's left edge. */
void
DrawingContext::get_char_edges(vteunistr c,
                               int columns,
                               uint32_t attr,
                               int& left,
                               int& right)
{
        if (G_UNLIKELY(m_minifont.unistr_is_local_graphic (c))) {
                left = 0;
                right = m_cell_width * columns;
                return;
        }

        int l, w, normal_width, fits_width;

        if (G_UNLIKELY (m_fonts[VTE_DRAW_NORMAL] == nullptr)) {
                left = 0;
                right = 0;
                return;
        }

        w = m_fonts[attr_to_style(attr)]->get_unistr_info(c)->width;
        normal_width = m_fonts[VTE_DRAW_NORMAL]->width() * columns;
        fits_width = m_cell_width * columns;

        if (G_LIKELY (w <= normal_width)) {
                /* The regular case: The glyph is not wider than one (CJK: two) regular character(s).
                 * Align to the left, after applying half (CJK: one) letter spacing. */
                l = m_char_spacing.left + (columns == 2 ? m_char_spacing.right : 0);
        } else if (G_UNLIKELY (w <= fits_width)) {
                /* Slightly wider glyph, but still fits in the cell (spacing included). This case can
                 * only happen with nonzero letter spacing. Center the glyph in the cell(s). */
                l = (fits_width - w) / 2;
        } else {
                /* Even wider glyph: doesn't fit in the cell. Align at left and overflow on the right. */
                l = 0;
        }

        left = l;
        right = l + w;
}

void
DrawingContext::draw_text_internal(TextRequest* requests,
                                   gsize n_requests,
                                   uint32_t attr,
                                   vte::color::rgb const* color,
                                   double alpha)
{
	gsize i;
	cairo_scaled_font_t *last_scaled_font = nullptr;
	int n_cr_glyphs = 0;
	cairo_glyph_t cr_glyphs[MAX_RUN_LENGTH];
	auto font = m_fonts[attr_to_style(attr)];

	g_return_if_fail (font != nullptr);

        g_assert(m_cr);
	set_source_color_alpha(color, alpha);
	cairo_set_operator(m_cr, CAIRO_OPERATOR_OVER);

	for (i = 0; i < n_requests; i++) {
		vteunistr c = requests[i].c;

                if (G_UNLIKELY (requests[i].mirror)) {
                        vte_bidi_get_mirror_char (c, requests[i].box_mirror, &c);
                }

                if (m_minifont.unistr_is_local_graphic(c)) {
                        m_minifont.draw_graphic(*this,
                                                c,
                                                        attr,
                                                        color,
                                                        requests[i].x, requests[i].y,
                                     font->width(), requests[i].columns, font->height());
                        continue;
                }

		auto uinfo = font->get_unistr_info(c);
		auto ufi = &uinfo->m_ufi;
                int x, y, ye;

                get_char_edges(c, requests[i].columns, attr, x, ye /* unused */);
                x += requests[i].x;
                /* Bold/italic versions might have different ascents. In order to align their
                 * baselines, we offset by the normal font's ascent here. (Bug 137.) */
                y = requests[i].y + m_char_spacing.top + m_fonts[VTE_DRAW_NORMAL]->ascent();

		switch (uinfo->coverage()) {
		default:
		case FontInfo::UnistrInfo::Coverage::UNKNOWN:
			g_assert_not_reached ();
			break;
		case FontInfo::UnistrInfo::Coverage::USE_PANGO_LAYOUT_LINE:
			cairo_move_to(m_cr, x, y);
			pango_cairo_show_layout_line(m_cr,
						      ufi->using_pango_layout_line.line);
			break;
		case FontInfo::UnistrInfo::Coverage::USE_PANGO_GLYPH_STRING:
			cairo_move_to(m_cr, x, y);
			pango_cairo_show_glyph_string(m_cr,
						       ufi->using_pango_glyph_string.font,
						       ufi->using_pango_glyph_string.glyph_string);
			break;
		case FontInfo::UnistrInfo::Coverage::USE_CAIRO_GLYPH:
			if (last_scaled_font != ufi->using_cairo_glyph.scaled_font || n_cr_glyphs == MAX_RUN_LENGTH) {
				if (n_cr_glyphs) {
					cairo_set_scaled_font(m_cr, last_scaled_font);
					cairo_show_glyphs(m_cr,
							   cr_glyphs,
							   n_cr_glyphs);
					n_cr_glyphs = 0;
				}
				last_scaled_font = ufi->using_cairo_glyph.scaled_font;
			}
			cr_glyphs[n_cr_glyphs].index = ufi->using_cairo_glyph.glyph_index;
			cr_glyphs[n_cr_glyphs].x = x;
			cr_glyphs[n_cr_glyphs].y = y;
			n_cr_glyphs++;
			break;
		}
	}
	if (n_cr_glyphs) {
		cairo_set_scaled_font(m_cr, last_scaled_font);
		cairo_show_glyphs(m_cr,
				   cr_glyphs,
				   n_cr_glyphs);
		n_cr_glyphs = 0;
	}
}

void
DrawingContext::draw_text(TextRequest* requests,
                          gsize n_requests,
                          uint32_t attr,
                          vte::color::rgb const* color,
                          double alpha)
{
        g_assert(m_cr);

	if (_vte_debug_on (VTE_DEBUG_DRAW)) {
		GString *string = g_string_new ("");
		gchar *str;
		gsize n;
		for (n = 0; n < n_requests; n++) {
			g_string_append_unichar (string, requests[n].c);
		}
		str = g_string_free (string, FALSE);
		g_printerr ("draw_text (\"%s\", len=%" G_GSIZE_FORMAT ", color=(%d,%d,%d,%.3f), %s - %s)\n",
				str, n_requests, color->red, color->green, color->blue, alpha,
				(attr & VTE_ATTR_BOLD)   ? "bold"   : "normal",
				(attr & VTE_ATTR_ITALIC) ? "italic" : "regular");
		g_free (str);
	}

	draw_text_internal(requests, n_requests, attr, color, alpha);
}

/* The following two functions are unused since commit 154abade902850afb44115cccf8fcac51fc082f0,
 * but let's keep them for now since they may become used again.
 */
bool
DrawingContext::has_char(vteunistr c,
                         uint32_t attr)
{
	_vte_debug_print (VTE_DEBUG_DRAW, "draw_has_char ('0x%04X', %s - %s)\n", c,
				(attr & VTE_ATTR_BOLD)   ? "bold"   : "normal",
				(attr & VTE_ATTR_ITALIC) ? "italic" : "regular");

        auto const style = attr_to_style(attr);
	g_return_val_if_fail(m_fonts[style], false);

	auto uinfo = m_fonts[style]->get_unistr_info(c);
	return !uinfo->has_unknown_chars;
}

bool
DrawingContext::draw_char(TextRequest* request,
                          uint32_t attr,
                          vte::color::rgb const* color,
                          double alpha)
{
	_vte_debug_print (VTE_DEBUG_DRAW,
			"draw_char ('%c', color=(%d,%d,%d,%.3f), %s, %s)\n",
			request->c,
			color->red, color->green, color->blue,
			alpha,
			(attr & VTE_ATTR_BOLD)   ? "bold"   : "normal",
			(attr & VTE_ATTR_ITALIC) ? "italic" : "regular");

	auto const have_char = has_char(request->c, attr);
	if (have_char)
		draw_text(request, 1, attr, color, alpha);

	return have_char;
}

void
DrawingContext::draw_rectangle(int x,
                               int y,
                               int width,
                               int height,
                               vte::color::rgb const* color,
                               double alpha)
{
        g_assert(m_cr);

	_vte_debug_print (VTE_DEBUG_DRAW,
			"draw_rectangle (%d, %d, %d, %d, color=(%d,%d,%d,%.3f))\n",
			x,y,width,height,
			color->red, color->green, color->blue,
			alpha);

	cairo_set_operator(m_cr, CAIRO_OPERATOR_OVER);
	cairo_rectangle(m_cr, x+VTE_LINE_WIDTH/2., y+VTE_LINE_WIDTH/2., width-VTE_LINE_WIDTH, height-VTE_LINE_WIDTH);
	set_source_color_alpha(color, alpha);
	cairo_set_line_width(m_cr, VTE_LINE_WIDTH);
	cairo_stroke (m_cr);
}

void
DrawingContext::fill_rectangle(int x,
                               int y,
                               int width,
                               int height,
                               vte::color::rgb const* color,
                               double alpha)
{
        g_assert(m_cr);

	_vte_debug_print (VTE_DEBUG_DRAW,
			"draw_fill_rectangle (%d, %d, %d, %d, color=(%d,%d,%d,%.3f))\n",
			x,y,width,height,
			color->red, color->green, color->blue,
			alpha);

	cairo_set_operator(m_cr, CAIRO_OPERATOR_OVER);
	cairo_rectangle(m_cr, x, y, width, height);
	set_source_color_alpha(color, alpha);
	cairo_fill (m_cr);
}

void
DrawingContext::draw_line(int x,
                          int y,
                          int xp,
                          int yp,
                          int line_width,
                          vte::color::rgb const *color,
                          double alpha)
{
	fill_rectangle(
                                 x, y,
                                 MAX(line_width, xp - x + 1), MAX(line_width, yp - y + 1),
                                 color, alpha);
}

void
DrawingContext::draw_undercurl(int x,
                               double y,
                               double line_width,
                               int count,
                               vte::color::rgb const *color,
                               double alpha)
{
        /* The end of the curly line slightly overflows to the next cell, so the canvas
         * caching the rendered look has to be wider not to chop this off. */
        gint x_padding = line_width + 1;  /* ceil, kind of */

        gint surface_top = y;  /* floor */

        g_assert(m_cr);

        _vte_debug_print (VTE_DEBUG_DRAW,
                        "draw_undercurl (x=%d, y=%f, count=%d, color=(%d,%d,%d,%.3f))\n",
                        x, y, count,
                        color->red, color->green, color->blue,
                        alpha);

        if (G_UNLIKELY (!m_undercurl_surface)) {
                /* Cache the undercurl's look. The design assumes that until the cached look is
                 * invalidated (the font is changed), this method is always called with the "y"
                 * parameter having the same fractional part, and the same "line_width" parameter.
                 * For caching, only the fractional part of "y" is used.
                 */
                double rad = _vte_draw_get_undercurl_rad(m_cell_width);
                double y_bottom = y + _vte_draw_get_undercurl_height(m_cell_width, line_width);
                double y_center = (y + y_bottom) / 2.;
                gint surface_bottom = y_bottom + 1;  /* ceil, kind of */

                _vte_debug_print (VTE_DEBUG_DRAW,
                                  "caching undercurl shape\n");

                /* Add a line_width of margin horizontally on both sides, for nice antialias overflowing. */
                m_undercurl_surface = vte::take_freeable
                        (cairo_surface_create_similar(cairo_get_target(m_cr),
                                                      CAIRO_CONTENT_ALPHA,
                                                      m_cell_width + 2 * x_padding,
                                                      surface_bottom - surface_top));
                auto undercurl_cr = vte::take_freeable(cairo_create(m_undercurl_surface.get()));
                cairo_set_operator(undercurl_cr.get(), CAIRO_OPERATOR_OVER);
                /* First quarter circle, similar to the left half of the tilde symbol. */
                cairo_arc(undercurl_cr.get(), x_padding + m_cell_width / 4., y_center - surface_top + m_cell_width / 4., rad, M_PI * 5 / 4, M_PI * 7 / 4);
                /* Second quarter circle, similar to the right half of the tilde symbol. */
                cairo_arc_negative(undercurl_cr.get(), x_padding + m_cell_width * 3 / 4., y_center - surface_top - m_cell_width / 4., rad, M_PI * 3 / 4, M_PI / 4);
                cairo_set_line_width (undercurl_cr.get(), line_width);
                cairo_stroke(undercurl_cr.get());
        }

        /* Paint the cached look of the undercurl using the desired look.
         * The cached look takes the fractional part of "y" into account,
         * here we only offset by its integer part. */
        cairo_save (m_cr);
        cairo_set_operator(m_cr, CAIRO_OPERATOR_OVER);
        set_source_color_alpha(color, alpha);
        for (int i = 0; i < count; i++) {
                cairo_mask_surface(m_cr, m_undercurl_surface.get(), x - x_padding + i * m_cell_width, surface_top);
        }
        cairo_restore (m_cr);
}

} // namespace view
} // namespace vte
