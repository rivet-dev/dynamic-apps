/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   instance.h for the Openbox window manager
   Copyright (c) 2003-2007   Dana Jansens

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

#ifndef __render_instance_h
#define __render_instance_h

#include <X11/Xlib.h>
#include <glib.h>
#include <pango/pangoxft.h>

struct _RrInstance {
    Display *display;
    gint screen;

    Visual *visual;
    gint depth;
    Colormap colormap;
    PangoContext *pango;

    gint red_offset;
    gint green_offset;
    gint blue_offset;

    gint red_shift;
    gint green_shift;
    gint blue_shift;

    gint red_mask;
    gint green_mask;
    gint blue_mask;

    gint pseudo_bpc;
    XColor *pseudo_colors;

    GHashTable *color_hash;
};

guint       RrPseudoBPC    (const RrInstance *inst);
XColor*     RrPseudoColors (const RrInstance *inst);
GHashTable* RrColorHash    (const RrInstance *inst);

#endif
