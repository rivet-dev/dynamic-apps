/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   render.c for the Openbox window manager
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
#include "gradient.h"
#include "font.h"
#include "mask.h"
#include "color.h"
#include "image.h"
#include "theme.h"

#include <glib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>

#ifdef HAVE_STDLIB_H
#  include <stdlib.h>
#endif

static void pixel_data_to_pixmap(RrAppearance *l,
                                 gint x, gint y, gint w, gint h);

Pixmap RrPaintPixmap(RrAppearance *a, gint w, gint h)
{
    gint i, transferred = 0, force_transfer = 0;
    Pixmap oldp = None;
    RrRect tarea; /* area in which to draw textures */
    gboolean resized;

    if (w <= 0 || h <= 0) return None;

    if (a->surface.parentx < 0 || a->surface.parenty < 0) {
        /* ob_debug("Invalid parent co-ordinates\n"); */
        return None;
    }

    if (a->surface.grad == RR_SURFACE_PARENTREL &&
        (a->surface.parentx >= a->surface.parent->w ||
         a->surface.parenty >= a->surface.parent->h))
    {
        return None;
    }

    resized = (a->w != w || a->h != h);

    oldp = a->pixmap; /* save to free after changing the visible pixmap */
    a->pixmap = XCreatePixmap(RrDisplay(a->inst),
                              RrRootWindow(a->inst),
                              w, h, RrDepth(a->inst));

    g_assert(a->pixmap != None);
    a->w = w;
    a->h = h;

    if (a->xftdraw != NULL)
        XftDrawDestroy(a->xftdraw);
    a->xftdraw = XftDrawCreate(RrDisplay(a->inst), a->pixmap,
                               RrVisual(a->inst), RrColormap(a->inst));
    g_assert(a->xftdraw != NULL);

    if (resized) {
        g_free(a->surface.pixel_data);
        a->surface.pixel_data = g_new(RrPixel32, w * h);
    }

    RrRender(a, w, h);

    {
        gint l, t, r, b;
        RrMargins(a, &l, &t, &r, &b);
        RECT_SET(tarea, l, t, w - l - r, h - t - b);
    }

    for (i = 0; i < a->textures; i++) {
        switch (a->texture[i].type) {
        case RR_TEXTURE_NONE:
            break;
        case RR_TEXTURE_TEXT:
            if (!transferred) {
                transferred = 1;
                if ((a->surface.grad != RR_SURFACE_SOLID)
                    || (a->surface.interlaced))
                    pixel_data_to_pixmap(a, 0, 0, w, h);
            }
            if (a->xftdraw == NULL) {
                a->xftdraw = XftDrawCreate(RrDisplay(a->inst), a->pixmap,
                                           RrVisual(a->inst),
                                           RrColormap(a->inst));
            }
            RrFontDraw(a->xftdraw, &a->texture[i].data.text, &tarea);
            break;
        case RR_TEXTURE_LINE_ART:
            if (!transferred) {
                transferred = 1;
                if ((a->surface.grad != RR_SURFACE_SOLID)
                    || (a->surface.interlaced))
                    pixel_data_to_pixmap(a, 0, 0, w, h);
            }
            XDrawLine(RrDisplay(a->inst), a->pixmap,
                      RrColorGC(a->texture[i].data.lineart.color),
                      a->texture[i].data.lineart.x1,
                      a->texture[i].data.lineart.y1,
                      a->texture[i].data.lineart.x2,
                      a->texture[i].data.lineart.y2);
            break;
        case RR_TEXTURE_MASK:
            if (!transferred) {
                transferred = 1;
                if ((a->surface.grad != RR_SURFACE_SOLID)
                    || (a->surface.interlaced))
                    pixel_data_to_pixmap(a, 0, 0, w, h);
            }
            RrPixmapMaskDraw(a->pixmap, &a->texture[i].data.mask, &tarea);
            break;
        case RR_TEXTURE_IMAGE:
            g_assert(!transferred);
            {
                RrRect narea = tarea;
                RrTextureImage *img = &a->texture[i].data.image;
                narea.x += img->tx;
                narea.width -= img->tx;
                narea.y += img->ty;
                narea.height -= img->ty;
                if (img->twidth)
                    narea.width = MIN(narea.width, img->twidth);
                if (img->theight)
                    narea.height = MIN(narea.height, img->theight);
                RrImageDrawImage(a->surface.pixel_data,
                                 &a->texture[i].data.image,
                                 a->w, a->h,
                                 &narea);
            }
            force_transfer = 1;
            break;
        case RR_TEXTURE_RGBA:
            g_assert(!transferred);
            {
                RrRect narea = tarea;
                RrTextureRGBA *rgb = &a->texture[i].data.rgba;
                narea.x += rgb->tx;
                narea.width -= rgb->tx;
                narea.y += rgb->ty;
                narea.height -= rgb->ty;
                if (rgb->twidth)
                    narea.width = MIN(narea.width, rgb->twidth);
                if (rgb->theight)
                    narea.height = MIN(narea.height, rgb->theight);
                RrImageDrawRGBA(a->surface.pixel_data,
                                &a->texture[i].data.rgba,
                                a->w, a->h,
                                &narea);
            }
            force_transfer = 1;
        break;
        case RR_TEXTURE_NUM_TYPES:
            g_assert_not_reached();
        }
    }

    if (!transferred) {
        transferred = 1;
        if ((a->surface.grad != RR_SURFACE_SOLID) || (a->surface.interlaced) ||
            force_transfer)
        {
            pixel_data_to_pixmap(a, 0, 0, w, h);
        }
    }

    return oldp;
}

void RrPaint(RrAppearance *a, Window win, gint w, gint h)
{
    Pixmap oldp;

    oldp = RrPaintPixmap(a, w, h);
    XSetWindowBackgroundPixmap(RrDisplay(a->inst), win, a->pixmap);
    XClearWindow(RrDisplay(a->inst), win);
    /* free this after changing the visible pixmap */
    if (oldp) XFreePixmap(RrDisplay(a->inst), oldp);
}

RrAppearance *RrAppearanceNew(const RrInstance *inst, gint numtex)
{
  RrAppearance *out;

  out = g_slice_new0(RrAppearance);
  out->inst = inst;
  out->textures = numtex;
  out->surface.bevel_light_adjust = 128;
  out->surface.bevel_dark_adjust = 64;
  if (numtex) out->texture = g_new0(RrTexture, numtex);

  return out;
}

void RrAppearanceRemoveTextures(RrAppearance *a)
{
    g_free(a->texture);
    a->textures = 0;
}

void RrAppearanceAddTextures(RrAppearance *a, gint numtex)
{
    g_assert(a->textures == 0);

    a->textures = numtex;
    if (numtex) a->texture = g_new0(RrTexture, numtex);
}

void RrAppearanceClearTextures(RrAppearance *a)
{
    memset(a->texture, 0, a->textures * sizeof(RrTexture));
}

/* deep copy of orig, means reset ref to 1 on copy
 * and copy each thing memwise. */
RrAppearance *RrAppearanceCopy(RrAppearance *orig)
{
    RrSurface *spo, *spc;
    RrAppearance *copy = g_slice_new(RrAppearance);

    copy->inst = orig->inst;

    spo = &(orig->surface);
    spc = &(copy->surface);
    spc->grad = spo->grad;
    spc->relief = spo->relief;
    spc->bevel = spo->bevel;
    if (spo->primary != NULL)
        spc->primary = RrColorNew(copy->inst,
                                  spo->primary->r,
                                  spo->primary->g,
                                  spo->primary->b);
    else spc->primary = NULL;

    if (spo->secondary != NULL)
        spc->secondary = RrColorNew(copy->inst,
                                    spo->secondary->r,
                                    spo->secondary->g,
                                    spo->secondary->b);
    else spc->secondary = NULL;

    if (spo->border_color != NULL)
        spc->border_color = RrColorNew(copy->inst,
                                       spo->border_color->r,
                                       spo->border_color->g,
                                       spo->border_color->b);
    else spc->border_color = NULL;

    if (spo->interlace_color != NULL)
        spc->interlace_color = RrColorNew(copy->inst,
                                       spo->interlace_color->r,
                                       spo->interlace_color->g,
                                       spo->interlace_color->b);
    else spc->interlace_color = NULL;

    if (spo->bevel_dark != NULL)
        spc->bevel_dark = RrColorNew(copy->inst,
                                     spo->bevel_dark->r,
                                     spo->bevel_dark->g,
                                     spo->bevel_dark->b);
    else spc->bevel_dark = NULL;

    if (spo->bevel_light != NULL)
        spc->bevel_light = RrColorNew(copy->inst,
                                      spo->bevel_light->r,
                                      spo->bevel_light->g,
                                      spo->bevel_light->b);
    else spc->bevel_light = NULL;

    if (spo->split_primary != NULL)
        spc->split_primary = RrColorNew(copy->inst,
                                        spo->split_primary->r,
                                        spo->split_primary->g,
                                        spo->split_primary->b);
    else spc->split_primary = NULL;

    if (spo->split_secondary != NULL)
        spc->split_secondary = RrColorNew(copy->inst,
                                        spo->split_secondary->r,
                                        spo->split_secondary->g,
                                        spo->split_secondary->b);
    else spc->split_secondary = NULL;

    spc->interlaced = spo->interlaced;
    spc->bevel_light_adjust = spo->bevel_light_adjust;
    spc->bevel_dark_adjust = spo->bevel_dark_adjust;
    spc->border = spo->border;
    spc->parent = NULL;
    spc->parentx = spc->parenty = 0;
    spc->pixel_data = NULL;

    copy->textures = orig->textures;
    copy->texture = g_memdup(orig->texture,
                             orig->textures * sizeof(RrTexture));
    copy->pixmap = None;
    copy->xftdraw = NULL;
    copy->w = copy->h = 0;
    return copy;
}

/* now decrements ref counter, and frees only if ref <= 0 */
void RrAppearanceFree(RrAppearance *a)
{
    if (a) {
        RrSurface *p;
        if (a->pixmap != None) XFreePixmap(RrDisplay(a->inst), a->pixmap);
        if (a->xftdraw != NULL) XftDrawDestroy(a->xftdraw);
        if (a->textures)
            g_free(a->texture);
        p = &a->surface;
        RrColorFree(p->primary);
        RrColorFree(p->secondary);
        RrColorFree(p->border_color);
        RrColorFree(p->interlace_color);
        RrColorFree(p->bevel_dark);
        RrColorFree(p->bevel_light);
        RrColorFree(p->split_primary);
        RrColorFree(p->split_secondary);
        g_free(p->pixel_data);
        p->pixel_data = NULL;
        g_slice_free(RrAppearance, a);
    }
}

static void pixel_data_to_pixmap(RrAppearance *l,
                                 gint x, gint y, gint w, gint h)
{
    RrPixel32 *in, *scratch;
    Pixmap out;
    XImage *im = NULL;
    im = XCreateImage(RrDisplay(l->inst), RrVisual(l->inst), RrDepth(l->inst),
                      ZPixmap, 0, NULL, w, h, 32, 0);
    g_assert(im != NULL);

    in = l->surface.pixel_data;
    out = l->pixmap;

/* this malloc is a complete waste of time on normal 32bpp
   as reduce_depth just sets im->data = data and returns
*/
    scratch = g_new(RrPixel32, im->width * im->height);
    im->data = (gchar*) scratch;
    RrReduceDepth(l->inst, in, im);
    XPutImage(RrDisplay(l->inst), out,
              DefaultGC(RrDisplay(l->inst), RrScreen(l->inst)),
              im, 0, 0, x, y, w, h);
    im->data = NULL;
    XDestroyImage(im);
    g_free(scratch);
}

void RrMargins (RrAppearance *a, gint *l, gint *t, gint *r, gint *b)
{
    *l = *t = *r = *b = 0;

    if (a->surface.grad != RR_SURFACE_PARENTREL) {
        if (a->surface.relief != RR_RELIEF_FLAT) {
            switch (a->surface.bevel) {
            case RR_BEVEL_1:
                *l = *t = *r = *b = 1;
                break;
            case RR_BEVEL_2:
                *l = *t = *r = *b = 2;
                break;
            case RR_BEVEL_NUM_TYPES:
                g_assert_not_reached();
            }
        } else if (a->surface.border) {
            *l = *t = *r = *b = 1;
        }
    }
}

void RrMinSize(RrAppearance *a, gint *w, gint *h)
{
    *w = RrMinWidth(a);
    *h = RrMinHeight(a);
}

gint RrMinWidth(RrAppearance *a)
{
    gint i;
    RrSize *m;
    gint l, t, r, b;
    gint w = 0;

    RrMargins(a, &l, &t, &r, &b);

    for (i = 0; i < a->textures; ++i) {
        switch (a->texture[i].type) {
        case RR_TEXTURE_NONE:
            break;
        case RR_TEXTURE_MASK:
            w = MAX(w, a->texture[i].data.mask.mask->width);
            break;
        case RR_TEXTURE_TEXT:
            m = RrFontMeasureString(a->texture[i].data.text.font,
                                    a->texture[i].data.text.string,
                                    a->texture[i].data.text.shadow_offset_x,
                                    a->texture[i].data.text.shadow_offset_y,
                                    a->texture[i].data.text.flow,
                                    a->texture[i].data.text.maxwidth);
            w = MAX(w, m->width);
            g_slice_free(RrSize, m);
            break;
        case RR_TEXTURE_RGBA:
            w += MAX(w, a->texture[i].data.rgba.width);
            break;
        case RR_TEXTURE_IMAGE:
            /* images resize so they don't contribute anything to the min */
            break;
        case RR_TEXTURE_LINE_ART:
            w = MAX(w, MAX(a->texture[i].data.lineart.x1 - l - r,
                           a->texture[i].data.lineart.x2 - l - r));
            break;
        case RR_TEXTURE_NUM_TYPES:
            g_assert_not_reached();
        }
    }

    w += l + r;

    if (w < 1) w = 1;
    return w;
}

gint RrMinHeight(RrAppearance *a)
{
    gint i;
    gint l, t, r, b;
    RrSize *m;
    gint h = 0;

    RrMargins(a, &l, &t, &r, &b);

    for (i = 0; i < a->textures; ++i) {
        switch (a->texture[i].type) {
        case RR_TEXTURE_NONE:
            break;
        case RR_TEXTURE_MASK:
            h = MAX(h, a->texture[i].data.mask.mask->height);
            break;
        case RR_TEXTURE_TEXT:
            if (a->texture[i].data.text.flow) {
                g_assert(a->texture[i].data.text.string != NULL);

                m = RrFontMeasureString
                    (a->texture[i].data.text.font,
                     a->texture[i].data.text.string,
                     a->texture[i].data.text.shadow_offset_x,
                     a->texture[i].data.text.shadow_offset_y,
                     a->texture[i].data.text.flow,
                     a->texture[i].data.text.maxwidth);
                h += MAX(h, m->height);
                g_slice_free(RrSize, m);
            }
            else
                h += MAX(h,
                         RrFontHeight
                         (a->texture[i].data.text.font,
                          a->texture[i].data.text.shadow_offset_y));
            break;
        case RR_TEXTURE_RGBA:
            h += MAX(h, a->texture[i].data.rgba.height);
            break;
        case RR_TEXTURE_IMAGE:
            /* images resize so they don't contribute anything to the min */
            break;
        case RR_TEXTURE_LINE_ART:
            h = MAX(h, MAX(a->texture[i].data.lineart.y1 - t - b,
                           a->texture[i].data.lineart.y2 - t - b));
            break;
        case RR_TEXTURE_NUM_TYPES:
            g_assert_not_reached();
        }
    }

    h += t + b;

    if (h < 1) h = 1;
    return h;
}

static void reverse_bits(gchar *c, gint n)
{
    gint i;
    for (i = 0; i < n; i++, c++)
        *c = (((*c * 0x0802UL & 0x22110UL) |
               (*c * 0x8020UL & 0x88440UL)) * 0x10101UL) >> 16;
}

gboolean RrPixmapToRGBA(const RrInstance *inst,
                        Pixmap pmap, Pixmap mask,
                        gint *w, gint *h, RrPixel32 **data)
{
    Window xr;
    gint xx, xy;
    guint pw, ph, mw, mh, xb, xd, i, x, y, di;
    XImage *xi, *xm = NULL;

    if (!XGetGeometry(RrDisplay(inst), pmap,
                      &xr, &xx, &xy, &pw, &ph, &xb, &xd))
        return FALSE;

    if (mask) {
        if (!XGetGeometry(RrDisplay(inst), mask,
                          &xr, &xx, &xy, &mw, &mh, &xb, &xd))
            return FALSE;
        if (pw != mw || ph != mh || xd != 1)
            return FALSE;
    }

    xi = XGetImage(RrDisplay(inst), pmap,
                   0, 0, pw, ph, 0xffffffff, ZPixmap);
    if (!xi)
        return FALSE;

    if (mask) {
        xm = XGetImage(RrDisplay(inst), mask,
                       0, 0, mw, mh, 0xffffffff, ZPixmap);
        if (!xm) {
            XDestroyImage(xi);
            return FALSE;
        }
        if ((xm->bits_per_pixel == 1) && (xm->bitmap_bit_order != LSBFirst))
            reverse_bits(xm->data, xm->bytes_per_line * xm->height);
    }

    if ((xi->bits_per_pixel == 1) && (xi->bitmap_bit_order != LSBFirst))
        reverse_bits(xi->data, xi->bytes_per_line * xi->height);

    *data = g_new(RrPixel32, pw * ph);
    RrIncreaseDepth(inst, *data, xi);

    if (mask) {
        /* apply transparency from the mask */
        di = 0;
        for (i = 0, y = 0; y < ph; ++y) {
            for (x = 0; x < pw; ++x, ++i) {
                if (!((((unsigned)xm->data[di + x / 8]) >> (x % 8)) & 0x1))
                    (*data)[i] &= ~(0xff << RrDefaultAlphaOffset);
            }
            di += xm->bytes_per_line;
        }
    }

    *w = pw;
    *h = ph;

    XDestroyImage(xi);
    if (mask)
        XDestroyImage(xm);

    return TRUE;
}
