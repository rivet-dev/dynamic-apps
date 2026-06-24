/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   mask.h for the Openbox window manager
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

#ifndef __mask_h
#define __mask_h

#include "render.h"
#include "geom.h"

RrPixmapMask *RrPixmapMaskNew(const RrInstance *inst,
                              gint w, gint h, const gchar *data);
void RrPixmapMaskFree(RrPixmapMask *m);
RrPixmapMask *RrPixmapMaskCopy(const RrPixmapMask *src);
void RrPixmapMaskDraw(Pixmap p, const RrTextureMask *m, const RrRect *area);

#endif
