/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   color.h for the Openbox window manager
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

#ifndef __color_h
#define __color_h

#include "render.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <glib.h>

struct _RrColor {
    const RrInstance *inst;

    gint r;
    gint g;
    gint b;
    gulong pixel;
    GC gc;

    gint key;
    gint refcount;

#ifdef DEBUG
    gint id;
#endif
};

void RrColorAllocateGC(RrColor *in);
XColor *RrPickColor(const RrInstance *inst, gint r, gint g, gint b);
void RrReduceDepth(const RrInstance *inst, RrPixel32 *data, XImage *im);
void RrIncreaseDepth(const RrInstance *inst, RrPixel32 *data, XImage *im);

#endif /* __color_h */
