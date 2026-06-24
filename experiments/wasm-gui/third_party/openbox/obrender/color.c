/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   color.c for the Openbox window manager
   Copyright (c) 2006        Mikael Magnusson
   Copyright (c) 2003-2007   Dana Jansens
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
#include "color.h"
#include "instance.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <string.h>

void RrColorAllocateGC(RrColor *in)
{
    XGCValues gcv;

    gcv.foreground = in->pixel;
    gcv.cap_style = CapProjecting;
    in->gc = XCreateGC(RrDisplay(in->inst),
                       RrRootWindow(in->inst),
                       GCForeground | GCCapStyle, &gcv);
}

RrColor *RrColorParse(const RrInstance *inst, gchar *colorname)
{
    XColor xcol;

    g_assert(colorname != NULL);
    /* get rgb values from colorname */

    xcol.red = 0;
    xcol.green = 0;
    xcol.blue = 0;
    xcol.pixel = 0;
    if (!XParseColor(RrDisplay(inst), RrColormap(inst), colorname, &xcol)) {
        g_message("Unable to parse color '%s'", colorname);
        return NULL;
    }
    return RrColorNew(inst, xcol.red >> 8, xcol.green >> 8, xcol.blue >> 8);
}

/*#define NO_COLOR_CACHE*/
#ifdef DEBUG
gint id;
#endif

RrColor *RrColorNew(const RrInstance *inst, gint r, gint g, gint b)
{
    /* this should be replaced with something far cooler */
    RrColor *out = NULL;
    XColor xcol;
    gint key;

    g_assert(r >= 0 && r < 256);
    g_assert(g >= 0 && g < 256);
    g_assert(b >= 0 && b < 256);

    key = (r << 24) + (g << 16) + (b << 8);
#ifndef NO_COLOR_CACHE
    if ((out = g_hash_table_lookup(RrColorHash(inst), &key))) {
        out->refcount++;
    } else {
#endif
        xcol.red = (r << 8) | r;
        xcol.green = (g << 8) | g;
        xcol.blue = (b << 8) | b;
        if (XAllocColor(RrDisplay(inst), RrColormap(inst), &xcol)) {
            out = g_slice_new(RrColor);
            out->inst = inst;
            out->r = xcol.red >> 8;
            out->g = xcol.green >> 8;
            out->b = xcol.blue >> 8;
            out->gc = None;
            out->pixel = xcol.pixel;
            out->key = key;
            out->refcount = 1;
#ifdef DEBUG
            out->id = id++;
#endif
#ifndef NO_COLOR_CACHE
            g_hash_table_insert(RrColorHash(inst), &out->key, out);
        }
#endif
    }
    return out;
}

RrColor *RrColorCopy(RrColor* c)
{
    return RrColorNew(c->inst, c->r, c->g, c->b);
}

void RrColorFree(RrColor *c)
{
    if (c) {
        if (--c->refcount < 1) {
#ifndef NO_COLOR_CACHE
            g_assert(g_hash_table_lookup(RrColorHash(c->inst), &c->key));
            g_hash_table_remove(RrColorHash(c->inst), &c->key);
#endif
            if (c->pixel) XFreeColors(RrDisplay(c->inst), RrColormap(c->inst),
                                      &c->pixel, 1, 0);
            if (c->gc) XFreeGC(RrDisplay(c->inst), c->gc);
            g_slice_free(RrColor, c);
        }
    }
}

void RrReduceDepth(const RrInstance *inst, RrPixel32 *data, XImage *im)
{
    gint r, g, b;
    gint x,y;
    gint ro = RrRedOffset(inst);
    gint bo = RrBlueOffset(inst);
    gint go = RrGreenOffset(inst);
    gint rs = RrRedShift(inst);
    gint bs = RrBlueShift(inst);
    gint gs = RrGreenShift(inst);
    RrPixel32 *p32 = (RrPixel32 *) im->data;
    RrPixel16 *p16 = (RrPixel16 *) im->data;
    RrPixel8  *p8  = (RrPixel8 *)  im->data;
    switch (im->bits_per_pixel) {
    case 32:
        if ((ro != RrDefaultRedOffset) ||
            (bo != RrDefaultBlueOffset) ||
            (go != RrDefaultGreenOffset)) {
            for (y = 0; y < im->height; y++) {
                for (x = 0; x < im->width; x++) {
                    r = (data[x] >> RrDefaultRedOffset) & 0xFF;
                    g = (data[x] >> RrDefaultGreenOffset) & 0xFF;
                    b = (data[x] >> RrDefaultBlueOffset) & 0xFF;
                    p32[x] = (r << ro) + (g << go) + (b << bo);
                }
                data += im->width;
                p32 += im->width;
            }
        } else im->data = (gchar*) data;
        break;
    case 24:
    {
        /* reverse the ordering, shifting left 16bit should be the first byte
           out of three, etc */
        const guint roff = (16 - ro) / 8;
        const guint goff = (16 - go) / 8;
        const guint boff = (16 - bo) / 8;
        gint outx;
        for (y = 0; y < im->height; y++) {
            for (x = 0, outx = 0; x < im->width; x++, outx += 3) {
                r = (data[x] >> RrDefaultRedOffset) & 0xFF;
                g = (data[x] >> RrDefaultGreenOffset) & 0xFF;
                b = (data[x] >> RrDefaultBlueOffset) & 0xFF;
                p8[outx+roff] = r;
                p8[outx+goff] = g;
                p8[outx+boff] = b;
            }
            data += im->width;
            p8 += im->bytes_per_line;
        }
        break;
    }
    case 16:
        for (y = 0; y < im->height; y++) {
            for (x = 0; x < im->width; x++) {
                r = (data[x] >> RrDefaultRedOffset) & 0xFF;
                r = r >> rs;
                g = (data[x] >> RrDefaultGreenOffset) & 0xFF;
                g = g >> gs;
                b = (data[x] >> RrDefaultBlueOffset) & 0xFF;
                b = b >> bs;
                p16[x] = (r << ro) + (g << go) + (b << bo);
            }
            data += im->width;
            p16 += im->bytes_per_line/2;
        }
        break;
    case 8:
        if (RrVisual(inst)->class == TrueColor) {
            for (y = 0; y < im->height; y++) {
                for (x = 0; x < im->width; x++) {
                    r = (data[x] >> RrDefaultRedOffset) & 0xFF;
                    r = r >> rs;
                    g = (data[x] >> RrDefaultGreenOffset) & 0xFF;
                    g = g >> gs;
                    b = (data[x] >> RrDefaultBlueOffset) & 0xFF;
                    b = b >> bs;
                    p8[x] = (r << ro) + (g << go) + (b << bo);
                }
                data += im->width;
                p8 += im->bytes_per_line;
            }
        } else {
            for (y = 0; y < im->height; y++) {
                for (x = 0; x < im->width; x++) {
                    p8[x] = RrPickColor(inst,
                                        data[x] >> RrDefaultRedOffset,
                                        data[x] >> RrDefaultGreenOffset,
                                        data[x] >> RrDefaultBlueOffset)->pixel;
                }
                data += im->width;
                p8 += im->bytes_per_line;
            }
        }
        break;
    default:
        g_error("This image bit depth (%i) is currently unhandled", im->bits_per_pixel);

    }
}

XColor *RrPickColor(const RrInstance *inst, gint r, gint g, gint b)
{
  r = (r & 0xff) >> (8-RrPseudoBPC(inst));
  g = (g & 0xff) >> (8-RrPseudoBPC(inst));
  b = (b & 0xff) >> (8-RrPseudoBPC(inst));
  return &RrPseudoColors(inst)[(r << (2*RrPseudoBPC(inst))) +
                               (g << (1*RrPseudoBPC(inst))) +
                               b];
}

static void swap_byte_order(XImage *im)
{
    gint x, y, di;

    di = 0;
    for (y = 0; y < im->height; ++y) {
        for (x = 0; x < im->width; ++x) {
            gchar *c = &im->data[di + x * im->bits_per_pixel / 8];
            gchar t;

            switch (im->bits_per_pixel) {
            case 32:
                t = c[2];
                c[2] = c[3];
                c[3] = t;
            case 16:
                t = c[0];
                c[0] = c[1];
                c[1] = t;
            case 8:
            case 1:
                break;
            default:
                g_error("Your bit depth (%i) is currently unhandled",
                        im->bits_per_pixel);
            }
        }
        di += im->bytes_per_line;
    }

    if (im->byte_order == LSBFirst)
        im->byte_order = MSBFirst;
    else
        im->byte_order = LSBFirst;
}

void RrIncreaseDepth(const RrInstance *inst, RrPixel32 *data, XImage *im)
{
    gint r, g, b;
    gint x,y;
    RrPixel32 *p32 = (RrPixel32 *) im->data;
    RrPixel16 *p16 = (RrPixel16 *) im->data;
    guchar *p8 = (guchar *)im->data;

    if (im->byte_order != LSBFirst)
        swap_byte_order(im);

    switch (im->bits_per_pixel) {
    case 32:
        for (y = 0; y < im->height; y++) {
            for (x = 0; x < im->width; x++) {
                r = (p32[x] >> RrRedOffset(inst)) & 0xff;
                g = (p32[x] >> RrGreenOffset(inst)) & 0xff;
                b = (p32[x] >> RrBlueOffset(inst)) & 0xff;
                data[x] = (r << RrDefaultRedOffset)
                    + (g << RrDefaultGreenOffset)
                    + (b << RrDefaultBlueOffset)
                    + (0xff << RrDefaultAlphaOffset);
            }
            data += im->width;
            p32 += im->bytes_per_line/4;
        }
        break;
    case 16:
        for (y = 0; y < im->height; y++) {
            for (x = 0; x < im->width; x++) {
                r = (p16[x] & RrRedMask(inst)) >>
                    RrRedOffset(inst) <<
                    RrRedShift(inst);
                g = (p16[x] & RrGreenMask(inst)) >>
                    RrGreenOffset(inst) <<
                    RrGreenShift(inst);
                b = (p16[x] & RrBlueMask(inst)) >>
                    RrBlueOffset(inst) <<
                    RrBlueShift(inst);
                data[x] = (r << RrDefaultRedOffset)
                    + (g << RrDefaultGreenOffset)
                    + (b << RrDefaultBlueOffset)
                    + (0xff << RrDefaultAlphaOffset);
            }
            data += im->width;
            p16 += im->bytes_per_line/2;
        }
        break;
    case 8:
        g_error("This image bit depth (%i) is currently unhandled", 8);
        break;
    case 1:
        for (y = 0; y < im->height; y++) {
            for (x = 0; x < im->width; x++) {
                if (!(((p8[x / 8]) >> (x % 8)) & 0x1))
                    data[x] = 0xff << RrDefaultAlphaOffset; /* black */
                else
                    data[x] = 0xffffffff; /* white */
            }
            data += im->width;
            p8 += im->bytes_per_line;
        }
        break;
    default:
        g_error("This image bit depth (%i) is currently unhandled",
                im->bits_per_pixel);
    }
}

gint RrColorRed(const RrColor *c)
{
    return c->r;
}

gint RrColorGreen(const RrColor *c)
{
    return c->g;
}

gint RrColorBlue(const RrColor *c)
{
    return c->b;
}

gulong RrColorPixel(const RrColor *c)
{
    return c->pixel;
}

GC RrColorGC(RrColor *c)
{
    if (!c->gc)
        RrColorAllocateGC(c);
    return c->gc;
}
