/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   gradient.c for the Openbox window manager
   Copyright (c) 2006        Mikael Magnusson
   Copyright (c) 2003-2008   Dana Jansens
   Copyright (c) 2003        Derek Foreman

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   See the COPYING file for a copy of the GNU General Public License.
*/

#include "render.h"
#include "gradient.h"
#include "color.h"
#include <glib.h>
#include <string.h>

static void highlight(RrSurface *s, RrPixel32 *x, RrPixel32 *y,
                      gboolean raised);
static void gradient_parentrelative(RrAppearance *a, gint w, gint h);
static void gradient_solid(RrAppearance *l, gint w, gint h);
static void gradient_splitvertical(RrAppearance *a, gint w, gint h);
static void gradient_vertical(RrSurface *sf, gint w, gint h);
static void gradient_horizontal(RrSurface *sf, gint w, gint h);
static void gradient_mirrorhorizontal(RrSurface *sf, gint w, gint h);
static void gradient_diagonal(RrSurface *sf, gint w, gint h);
static void gradient_crossdiagonal(RrSurface *sf, gint w, gint h);
static void gradient_pyramid(RrSurface *sf, gint inw, gint inh);

void RrRender(RrAppearance *a, gint w, gint h)
{
    RrPixel32 *data = a->surface.pixel_data;
    RrPixel32 current;
    guint r,g,b;
    register gint off, x;

    switch (a->surface.grad) {
    case RR_SURFACE_PARENTREL:
        gradient_parentrelative(a, w, h);
        break;
    case RR_SURFACE_SOLID:
        gradient_solid(a, w, h);
        break;
    case RR_SURFACE_SPLIT_VERTICAL:
        gradient_splitvertical(a, w, h);
        break;
    case RR_SURFACE_VERTICAL:
        gradient_vertical(&a->surface, w, h);
        break;
    case RR_SURFACE_HORIZONTAL:
        gradient_horizontal(&a->surface, w, h);
        break;
    case RR_SURFACE_MIRROR_HORIZONTAL:
        gradient_mirrorhorizontal(&a->surface, w, h);
        break;
    case RR_SURFACE_DIAGONAL:
        gradient_diagonal(&a->surface, w, h);
        break;
    case RR_SURFACE_CROSS_DIAGONAL:
        gradient_crossdiagonal(&a->surface, w, h);
        break;
    case RR_SURFACE_PYRAMID:
        gradient_pyramid(&a->surface, w, h);
        break;
    default:
        g_assert_not_reached(); /* unhandled gradient */
        return;
    }

    if (a->surface.interlaced) {
        gint i;
        RrPixel32 *p;

        r = a->surface.interlace_color->r;
        g = a->surface.interlace_color->g;
        b = a->surface.interlace_color->b;
        current = (r << RrDefaultRedOffset)
            + (g << RrDefaultGreenOffset)
            + (b << RrDefaultBlueOffset);
        p = data;
        for (i = 0; i < h; i += 2, p += w)
            for (x = 0; x < w; ++x, ++p)
                *p = current;
    }

    if (a->surface.relief == RR_RELIEF_FLAT && a->surface.border) {
        r = a->surface.border_color->r;
        g = a->surface.border_color->g;
        b = a->surface.border_color->b;
        current = (r << RrDefaultRedOffset)
            + (g << RrDefaultGreenOffset)
            + (b << RrDefaultBlueOffset);
        for (off = 0, x = 0; x < w; ++x, off++) {
            *(data + off) = current;
            *(data + off + ((h-1) * w)) = current;
        }
        for (off = 0, x = 0; x < h; ++x, off++) {
            *(data + (off * w)) = current;
            *(data + (off * w) + w - 1) = current;
        }
    }

    if (a->surface.relief != RR_RELIEF_FLAT) {
        if (a->surface.bevel == RR_BEVEL_1) {
            for (off = 1, x = 1; x < w - 1; ++x, off++)
                highlight(&a->surface, data + off,
                          data + off + (h-1) * w,
                          a->surface.relief==RR_RELIEF_RAISED);
            for (off = 0, x = 0; x < h; ++x, off++)
                highlight(&a->surface, data + off * w,
                          data + off * w + w - 1,
                          a->surface.relief==RR_RELIEF_RAISED);
        }

        if (a->surface.bevel == RR_BEVEL_2) {
            for (off = 2, x = 2; x < w - 2; ++x, off++)
                highlight(&a->surface, data + off + w,
                          data + off + (h-2) * w,
                          a->surface.relief==RR_RELIEF_RAISED);
            for (off = 1, x = 1; x < h-1; ++x, off++)
                highlight(&a->surface, data + off * w + 1,
                          data + off * w + w - 2,
                          a->surface.relief==RR_RELIEF_RAISED);
        }
    }
}

static void highlight(RrSurface *s, RrPixel32 *x, RrPixel32 *y, gboolean raised)
{
    register gint r, g, b;

    RrPixel32 *up, *down;
    if (raised) {
        up = x;
        down = y;
    } else {
        up = y;
        down = x;
    }

    r = (*up >> RrDefaultRedOffset) & 0xFF;
    r += (r * s->bevel_light_adjust) >> 8;
    g = (*up >> RrDefaultGreenOffset) & 0xFF;
    g += (g * s->bevel_light_adjust) >> 8;
    b = (*up >> RrDefaultBlueOffset) & 0xFF;
    b += (b * s->bevel_light_adjust) >> 8;
    if (r > 0xFF) r = 0xFF;
    if (g > 0xFF) g = 0xFF;
    if (b > 0xFF) b = 0xFF;
    *up = (r << RrDefaultRedOffset) + (g << RrDefaultGreenOffset)
        + (b << RrDefaultBlueOffset);

    r = (*down >> RrDefaultRedOffset) & 0xFF;
    r -= (r * s->bevel_dark_adjust) >> 8;
    g = (*down >> RrDefaultGreenOffset) & 0xFF;
    g -= (g * s->bevel_dark_adjust) >> 8;
    b = (*down >> RrDefaultBlueOffset) & 0xFF;
    b -= (b * s->bevel_dark_adjust) >> 8;
    *down = (r << RrDefaultRedOffset) + (g << RrDefaultGreenOffset)
        + (b << RrDefaultBlueOffset);
}

static void create_bevel_colors(RrAppearance *l)
{
    register gint r, g, b;

    /* light color */
    r = l->surface.primary->r;
    r += (r * l->surface.bevel_light_adjust) >> 8;
    g = l->surface.primary->g;
    g += (g * l->surface.bevel_light_adjust) >> 8;
    b = l->surface.primary->b;
    b += (b * l->surface.bevel_light_adjust) >> 8;
    if (r > 0xFF) r = 0xFF;
    if (g > 0xFF) g = 0xFF;
    if (b > 0xFF) b = 0xFF;
    g_assert(!l->surface.bevel_light);
    l->surface.bevel_light = RrColorNew(l->inst, r, g, b);

    /* dark color */
    r = l->surface.primary->r;
    r -= (r * l->surface.bevel_dark_adjust) >> 8;
    g = l->surface.primary->g;
    g -= (g * l->surface.bevel_dark_adjust) >> 8;
    b = l->surface.primary->b;
    b -= (b * l->surface.bevel_dark_adjust) >> 8;
    g_assert(!l->surface.bevel_dark);
    l->surface.bevel_dark = RrColorNew(l->inst, r, g, b);
}

/*! Repeat the first pixel over the entire block of memory
  @param start The block of memory. start[0] will be copied
         to the rest of the block.
  @param w The width of the block of memory (including the already-set first
           element
*/
static inline void repeat_pixel(RrPixel32 *start, gint w)
{
    register gint x;
    RrPixel32 *dest;

    dest = start + 1;

    /* for really small things, just copy ourselves */
    if (w < 8) {
        for (x = w-1; x > 0; --x)
            *(dest++) = *start;
    }

    /* for >= 8, then use O(log n) memcpy's... */
    else {
        gchar *cdest;
        gint lenbytes;

        /* copy the first 3 * 32 bits (3 words) ourselves - then we have
           3 + the original 1 = 4 words to make copies of at a time

           this is faster than doing memcpy for 1 or 2 words at a time
        */
        for (x = 3; x > 0; --x)
            *(dest++) = *start;

        /* cdest is a pointer to the pixel data that is typed char* so that
           adding 1 to its position moves it only one byte

           lenbytes is the amount of bytes that we will be copying each
           iteration.  this doubles each time through the loop.

           x is the number of bytes left to copy into.  lenbytes will alwaysa
           be bounded by x

           this loop will run O(log n) times (n is the number of bytes we
           need to copy into), since the size of the copy is doubled each
           iteration.  it seems that gcc does some nice optimizations to make
           this memcpy very fast on hardware with support for vector operations
           such as mmx or see.  here is an idea of the kind of speed up we are
           getting by doing this (splitvertical3 switches from doing
           "*(data++) = color" n times to doing this memcpy thing log n times:

           %   cumulative   self              self     total           
           time   seconds   seconds    calls  ms/call  ms/call  name    
           49.44      0.88     0.88     1063     0.83     0.83  splitvertical1
           47.19      1.72     0.84     1063     0.79     0.79  splitvertical2
            2.81      1.77     0.05     1063     0.05     0.05  splitvertical3
        */
        cdest = (gchar*)dest;
        lenbytes = 4 * sizeof(RrPixel32);
        for (x = (w - 4) * sizeof(RrPixel32); x > 0;) {
            memcpy(cdest, start, lenbytes);
            x -= lenbytes;
            cdest += lenbytes;
            lenbytes <<= 1;
            if (lenbytes > x)
                lenbytes = x;
        }
    }
}

static void gradient_parentrelative(RrAppearance *a, gint w, gint h)
{
    RrPixel32 *source, *dest;
    gint sw, sh, partial_w, partial_h;
    register gint i;

    g_assert (a->surface.parent);
    g_assert (a->surface.parent->w);

    sw = a->surface.parent->w;
    sh = a->surface.parent->h;

    /* This is a little hack. When a texture is parentrelative, and the same
       area as the parent, and has a bevel, it will draw its bevel on top
       of the parent's, amplifying it. So instead, rerender the child with
       the parent's settings, but the child's bevel and interlace */
    if (a->surface.relief != RR_RELIEF_FLAT &&
        (a->surface.parent->surface.relief != RR_RELIEF_FLAT ||
         a->surface.parent->surface.border) &&
        !a->surface.parentx && !a->surface.parenty &&
        sw == w && sh == h)
    {
        RrSurface old = a->surface;
        a->surface = a->surface.parent->surface;

        /* turn these off for the parent */
        a->surface.relief = RR_RELIEF_FLAT;
        a->surface.border = FALSE;

        a->surface.pixel_data = old.pixel_data;

        RrRender(a, w, h);
        a->surface = old;
    } else {
        source = (a->surface.parent->surface.pixel_data +
                  a->surface.parentx + sw * a->surface.parenty);
        dest = a->surface.pixel_data;

        if (a->surface.parentx + w > sw) {
            partial_w = sw - a->surface.parentx;
        } else partial_w = w;

        if (a->surface.parenty + h > sh) {
            partial_h = sh - a->surface.parenty;
        } else partial_h = h;

        for (i = 0; i < partial_h; i++, source += sw, dest += w) {
            memcpy(dest, source, partial_w * sizeof(RrPixel32));
        }
    }
}

static void gradient_solid(RrAppearance *l, gint w, gint h)
{
    register gint i;
    RrPixel32 pix;
    RrPixel32 *data = l->surface.pixel_data;
    RrSurface *sp = &l->surface;
    gint left = 0, top = 0, right = w - 1, bottom = h - 1;

    pix = (sp->primary->r << RrDefaultRedOffset)
        + (sp->primary->g << RrDefaultGreenOffset)
        + (sp->primary->b << RrDefaultBlueOffset);

    for (i = 0; i < w * h; i++)
        *data++ = pix;

    if (sp->interlaced)
        return;

    XFillRectangle(RrDisplay(l->inst), l->pixmap, RrColorGC(sp->primary),
                   0, 0, w, h);

    switch (sp->relief) {
    case RR_RELIEF_RAISED:
        if (!sp->bevel_dark)
            create_bevel_colors(l);

        switch (sp->bevel) {
        case RR_BEVEL_1:
            XDrawLine(RrDisplay(l->inst), l->pixmap, RrColorGC(sp->bevel_dark),
                      left, bottom, right, bottom);
            XDrawLine(RrDisplay(l->inst), l->pixmap, RrColorGC(sp->bevel_dark),
                      right, bottom, right, top);

            XDrawLine(RrDisplay(l->inst), l->pixmap,RrColorGC(sp->bevel_light),
                      left, top, right, top);
            XDrawLine(RrDisplay(l->inst), l->pixmap,RrColorGC(sp->bevel_light),
                      left, bottom, left, top);
            break;
        case RR_BEVEL_2:
            XDrawLine(RrDisplay(l->inst), l->pixmap, RrColorGC(sp->bevel_dark),
                      left + 2, bottom - 1, right - 2, bottom - 1);
            XDrawLine(RrDisplay(l->inst), l->pixmap, RrColorGC(sp->bevel_dark),
                      right - 1, bottom - 1, right - 1, top + 1);

            XDrawLine(RrDisplay(l->inst), l->pixmap,RrColorGC(sp->bevel_light),
                      left + 2, top + 1, right - 2, top + 1);
            XDrawLine(RrDisplay(l->inst), l->pixmap,RrColorGC(sp->bevel_light),
                      left + 1, bottom - 1, left + 1, top + 1);
            break;
        default:
            g_assert_not_reached(); /* unhandled BevelType */
        }
        break;
    case RR_RELIEF_SUNKEN:
        if (!sp->bevel_dark)
            create_bevel_colors(l);

        switch (sp->bevel) {
        case RR_BEVEL_1:
            XDrawLine(RrDisplay(l->inst), l->pixmap,RrColorGC(sp->bevel_light),
                      left, bottom, right, bottom);
            XDrawLine(RrDisplay(l->inst), l->pixmap,RrColorGC(sp->bevel_light),
                      right, bottom, right, top);

            XDrawLine(RrDisplay(l->inst), l->pixmap, RrColorGC(sp->bevel_dark),
                      left, top, right, top);
            XDrawLine(RrDisplay(l->inst), l->pixmap, RrColorGC(sp->bevel_dark),
                      left, bottom, left, top);
            break;
        case RR_BEVEL_2:
            XDrawLine(RrDisplay(l->inst), l->pixmap,RrColorGC(sp->bevel_light),
                      left + 2, bottom - 1, right - 2, bottom - 1);
            XDrawLine(RrDisplay(l->inst), l->pixmap,RrColorGC(sp->bevel_light),
                      right - 1, bottom - 1, right - 1, top + 1);

            XDrawLine(RrDisplay(l->inst), l->pixmap, RrColorGC(sp->bevel_dark),
                      left + 2, top + 1, right - 2, top + 1);
            XDrawLine(RrDisplay(l->inst), l->pixmap, RrColorGC(sp->bevel_dark),
                      left + 1, bottom - 1, left + 1, top + 1);
            break;
        default:
            g_assert_not_reached(); /* unhandled BevelType */
        }
        break;
    case RR_RELIEF_FLAT:
        if (sp->border) {
            XDrawRectangle(RrDisplay(l->inst), l->pixmap,
                           RrColorGC(sp->border_color),
                           left, top, right, bottom);
        }
        break;
    default:
        g_assert_not_reached(); /* unhandled ReliefType */
    }
}

/* * * * * * * * * * * * * * GRADIENT MAGIC WOOT * * * * * * * * * * * * * * */

#define VARS(x)                                                \
    register gint len##x;                                      \
    guint color##x[3];                                         \
    gint cdelta##x[3], error##x[3] = { 0, 0, 0 }, inc##x[3];   \
    gboolean bigslope##x[3] /* color slope > 1 */

#define SETUP(x, from, to, w)         \
    len##x = w;                       \
                                      \
    color##x[0] = from->r;            \
    color##x[1] = from->g;            \
    color##x[2] = from->b;            \
                                      \
    cdelta##x[0] = to->r - from->r;   \
    cdelta##x[1] = to->g - from->g;   \
    cdelta##x[2] = to->b - from->b;   \
                                      \
    if (cdelta##x[0] < 0) {           \
        cdelta##x[0] = -cdelta##x[0]; \
        inc##x[0] = -1;               \
    } else                            \
        inc##x[0] = 1;                \
    if (cdelta##x[1] < 0) {           \
        cdelta##x[1] = -cdelta##x[1]; \
        inc##x[1] = -1;               \
    } else                            \
        inc##x[1] = 1;                \
    if (cdelta##x[2] < 0) {           \
        cdelta##x[2] = -cdelta##x[2]; \
        inc##x[2] = -1;               \
    } else                            \
        inc##x[2] = 1;                \
    bigslope##x[0] = cdelta##x[0] > w;\
    bigslope##x[1] = cdelta##x[1] > w;\
    bigslope##x[2] = cdelta##x[2] > w

#define COLOR_RR(x, c)                       \
    c->r = color##x[0];                      \
    c->g = color##x[1];                      \
    c->b = color##x[2]

#define COLOR(x)                             \
    ((color##x[0] << RrDefaultRedOffset) +   \
     (color##x[1] << RrDefaultGreenOffset) + \
     (color##x[2] << RrDefaultBlueOffset))

#define INCREMENT(x, i) \
    (inc##x[i])

#define NEXT(x)                                           \
{                                                         \
    register gint i;                                      \
    for (i = 2; i >= 0; --i) {                            \
        if (!cdelta##x[i]) continue;                      \
                                                          \
        if (!bigslope##x[i]) {                            \
            /* Y (color) is dependant on X */             \
            error##x[i] += cdelta##x[i];                  \
            if ((error##x[i] << 1) >= len##x) {           \
                color##x[i] += INCREMENT(x, i);           \
                error##x[i] -= len##x;                    \
            }                                             \
        } else {                                          \
            /* X is dependant on Y (color) */             \
            while (1) {                                   \
                color##x[i] += INCREMENT(x, i);           \
                error##x[i] += len##x;                    \
                if ((error##x[i] << 1) >= cdelta##x[i]) { \
                    error##x[i] -= cdelta##x[i];          \
                    break;                                \
                }                                         \
            }                                             \
        }                                                 \
    }                                                     \
}

static void gradient_splitvertical(RrAppearance *a, gint w, gint h)
{
    register gint y1, y2, y3;
    RrSurface *sf = &a->surface;
    RrPixel32 *data;
    register gint y1sz, y2sz, y3sz;

    VARS(y1);
    VARS(y2);
    VARS(y3);

    /* if h <= 5, then a 0 or 1px middle gradient.
       if h > 5, then always a 1px middle gradient.
    */
    if (h <= 5) {
        y1sz = MAX(h/2, 0);
        y2sz = (h < 3) ? 0 : (h & 1);
        y3sz = MAX(h/2, 1);
    }
    else {
        y1sz = h/2 - (1 - (h & 1));
        y2sz = 1;
        y3sz = h/2;
    }

    SETUP(y1, sf->split_primary, sf->primary, y1sz);
    if (y2sz) {
        /* setup to get the colors _in between_ these other 2 */
        SETUP(y2, sf->primary, sf->secondary, y2sz + 2);
        NEXT(y2); /* skip the first one, its the same as the last of y1 */
    }
    SETUP(y3, sf->secondary, sf->split_secondary,  y3sz);

    /* find the color for the first pixel of each row first */
    data = sf->pixel_data;

    if (y1sz) {
        for (y1 = y1sz-1; y1 > 0; --y1) {
            *data = COLOR(y1);
            data += w;
            NEXT(y1);
        }
        *data = COLOR(y1);
        data += w;
    }
    if (y2sz) {
        for (y2 = y2sz-1; y2 > 0; --y2) {
            *data = COLOR(y2);
            data += w;
            NEXT(y2);
        }
        *data = COLOR(y2);
        data += w;
    }
    for (y3 = y3sz-1; y3 > 0; --y3) {
        *data = COLOR(y3);
        data += w;
        NEXT(y3);
    }
    *data = COLOR(y3);

    /* copy the first pixels into the whole rows */
    data = sf->pixel_data;
    for (y1 = h; y1 > 0; --y1) {
        repeat_pixel(data, w);
        data += w;
    }
}

static void gradient_horizontal(RrSurface *sf, gint w, gint h)
{
    register gint x, y, cpbytes;
    RrPixel32 *data = sf->pixel_data, *datav;
    gchar *datac;

    VARS(x);
    SETUP(x, sf->primary, sf->secondary, w);

    /* set the color values for the first row */
    datav = data;
    for (x = w - 1; x > 0; --x) {  /* 0 -> w - 1 */
        *datav = COLOR(x);
        ++datav;
        NEXT(x);
    }
    *datav = COLOR(x);
    ++datav;

    /* copy the first row to the rest in O(logn) copies */
    datac = (gchar*)datav;
    cpbytes = 1 * w * sizeof(RrPixel32);
    for (y = (h - 1) * w * sizeof(RrPixel32); y > 0;) {
        memcpy(datac, data, cpbytes);
        y -= cpbytes;
        datac += cpbytes;
        cpbytes <<= 1;
        if (cpbytes > y)
            cpbytes = y;
    }
}

static void gradient_mirrorhorizontal(RrSurface *sf, gint w, gint h)
{
    register gint x, y, half1, half2, cpbytes;
    RrPixel32 *data = sf->pixel_data, *datav;
    gchar *datac;

    VARS(x);

    half1 = (w + 1) / 2;
    half2 = w / 2;

    /* set the color values for the first row */

    SETUP(x, sf->primary, sf->secondary, half1);
    datav = data;
    for (x = half1 - 1; x > 0; --x) {  /* 0 -> half1 - 1 */
        *datav = COLOR(x);
        ++datav;
        NEXT(x);
    }
    *datav = COLOR(x);
    ++datav;

    if (half2 > 0) {
        SETUP(x, sf->secondary, sf->primary, half2);
        for (x = half2 - 1; x > 0; --x) {  /* 0 -> half2 - 1 */
            *datav = COLOR(x);
            ++datav;
            NEXT(x);
        }
        *datav = COLOR(x);
        ++datav;
    }

    /* copy the first row to the rest in O(logn) copies */
    datac = (gchar*)datav;
    cpbytes = 1 * w * sizeof(RrPixel32);
    for (y = (h - 1) * w * sizeof(RrPixel32); y > 0;) {
        memcpy(datac, data, cpbytes);
        y -= cpbytes;
        datac += cpbytes;
        cpbytes <<= 1;
        if (cpbytes > y)
            cpbytes = y;
    }
}

static void gradient_vertical(RrSurface *sf, gint w, gint h)
{
    register gint y;
    RrPixel32 *data;

    VARS(y);
    SETUP(y, sf->primary, sf->secondary, h);

    /* find the color for the first pixel of each row first */
    data = sf->pixel_data;

    for (y = h - 1; y > 0; --y) {  /* 0 -> h-1 */
        *data = COLOR(y);
        data += w;
        NEXT(y);
    }
    *data = COLOR(y);

    /* copy the first pixels into the whole rows */
    data = sf->pixel_data;
    for (y = h; y > 0; --y) {
        repeat_pixel(data, w);
        data += w;
    }
}

static void gradient_diagonal(RrSurface *sf, gint w, gint h)
{
    register gint x, y;
    RrPixel32 *data = sf->pixel_data;
    RrColor left, right;
    RrColor extracorner;

    VARS(lefty);
    VARS(righty);
    VARS(x);

    extracorner.r = (sf->primary->r + sf->secondary->r) / 2;
    extracorner.g = (sf->primary->g + sf->secondary->g) / 2;
    extracorner.b = (sf->primary->b + sf->secondary->b) / 2;

    SETUP(lefty, sf->primary, (&extracorner), h);
    SETUP(righty, (&extracorner), sf->secondary, h);

    for (y = h - 1; y > 0; --y) {  /* 0 -> h-1 */
        COLOR_RR(lefty, (&left));
        COLOR_RR(righty, (&right));

        SETUP(x, (&left), (&right), w);

        for (x = w - 1; x > 0; --x) {  /* 0 -> w-1 */
            *(data++) = COLOR(x);

            NEXT(x);
        }
        *(data++) = COLOR(x);

        NEXT(lefty);
        NEXT(righty);
    }
    COLOR_RR(lefty, (&left));
    COLOR_RR(righty, (&right));

    SETUP(x, (&left), (&right), w);

    for (x = w - 1; x > 0; --x) {  /* 0 -> w-1 */
        *(data++) = COLOR(x);

        NEXT(x);
    }
    *data = COLOR(x);
}

static void gradient_crossdiagonal(RrSurface *sf, gint w, gint h)
{
    register gint x, y;
    RrPixel32 *data = sf->pixel_data;
    RrColor left, right;
    RrColor extracorner;

    VARS(lefty);
    VARS(righty);
    VARS(x);

    extracorner.r = (sf->primary->r + sf->secondary->r) / 2;
    extracorner.g = (sf->primary->g + sf->secondary->g) / 2;
    extracorner.b = (sf->primary->b + sf->secondary->b) / 2;

    SETUP(lefty, (&extracorner), sf->secondary, h);
    SETUP(righty, sf->primary, (&extracorner), h);

    for (y = h - 1; y > 0; --y) {  /* 0 -> h-1 */
        COLOR_RR(lefty, (&left));
        COLOR_RR(righty, (&right));

        SETUP(x, (&left), (&right), w);

        for (x = w - 1; x > 0; --x) {  /* 0 -> w-1 */
            *(data++) = COLOR(x);

            NEXT(x);
        }
        *(data++) = COLOR(x);

        NEXT(lefty);
        NEXT(righty);
    }
    COLOR_RR(lefty, (&left));
    COLOR_RR(righty, (&right));

    SETUP(x, (&left), (&right), w);

    for (x = w - 1; x > 0; --x) {  /* 0 -> w-1 */
        *(data++) = COLOR(x);

        NEXT(x);
    }
    *data = COLOR(x);
}

static void gradient_pyramid(RrSurface *sf, gint w, gint h)
{
    RrPixel32 *ldata, *rdata;
    RrPixel32 *cp;
    RrColor left, right;
    RrColor extracorner;
    register gint x, y, halfw, halfh, midx, midy;

    VARS(lefty);
    VARS(righty);
    VARS(x);

    extracorner.r = (sf->primary->r + sf->secondary->r) / 2;
    extracorner.g = (sf->primary->g + sf->secondary->g) / 2;
    extracorner.b = (sf->primary->b + sf->secondary->b) / 2;

    halfw = w >> 1;
    halfh = h >> 1;
    midx = w - halfw - halfw; /* 0 or 1, depending if w is even or odd */
    midy = h - halfh - halfh;   /* 0 or 1, depending if h is even or odd */

    SETUP(lefty, sf->primary, (&extracorner), halfh + midy);
    SETUP(righty, (&extracorner), sf->secondary, halfh + midy);

    /* draw the top half

       it is faster to draw both top quarters together than to draw one and
       then copy it over to the other side.
    */

    ldata = sf->pixel_data;
    rdata = ldata + w - 1;
    for (y = halfh + midy; y > 0; --y) {  /* 0 -> (h+1)/2 */
        RrPixel32 c;

        COLOR_RR(lefty, (&left));
        COLOR_RR(righty, (&right));

        SETUP(x, (&left), (&right), halfw + midx);

        for (x = halfw + midx - 1; x > 0; --x) {  /* 0 -> (w+1)/2 */
            c = COLOR(x);
            *(ldata++) = *(rdata--) = c;

            NEXT(x);
        }
        c = COLOR(x);
        *ldata = *rdata = c;
        ldata += halfw + 1;
        rdata += halfw - 1 + midx + w;

        NEXT(lefty);
        NEXT(righty);
    }

    /* copy the top half into the bottom half, mirroring it, so we can only
       copy one row at a time

       it is faster, to move the writing pointer forward, and the reading
       pointer backward

       this is the current code, moving the write pointer forward and read
       pointer backward
       41.78      4.26     1.78      504     3.53     3.53  gradient_pyramid2
       this is the opposite, moving the read pointer forward and the write
       pointer backward
       42.27      4.40     1.86      504     3.69     3.69  gradient_pyramid2
       
    */
    ldata = sf->pixel_data + (halfh - 1) * w;
    cp = ldata + (midy + 1) * w;
    for (y = halfh; y > 0; --y) {
        memcpy(cp, ldata, w * sizeof(RrPixel32));
        ldata -= w;
        cp += w;
    }
}
