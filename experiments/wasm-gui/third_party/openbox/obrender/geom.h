/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   geom.h for the Openbox window manager
   Copyright (c) 2006        Mikael Magnusson
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

#ifndef __render_geom_h
#define __render_geom_h

typedef struct {
    int width;
    int height;
} RrSize;

typedef struct {
    int x;
    int y;
    int width;
    int height;
} RrRect;

#define RECT_SET(r, nx, ny, w, h) \
    (r).x = (nx), (r).y = (ny), (r).width = (w), (r).height = (h)

#endif
