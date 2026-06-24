/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   overlap.h for the Openbox window manager
   Copyright (c) 2011        Ian Zimmerman

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

#include "geom.h"

void place_overlap_find_least_placement(const Rect* client_rects,
                                        int n_client_rects,
                                        const Rect* bounds,
                                        const Size* req_size,
                                        Point* result);
