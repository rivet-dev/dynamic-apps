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

#ifndef __geom_h
#define __geom_h

#include <glib.h>

typedef struct _GravityCoord {
    gint pos;
    gint denom;
    gboolean center;
    gboolean opposite;
} GravityCoord;

typedef struct _GravityPoint {
    GravityCoord x;
    GravityCoord y;
} GravityPoint;

#define GRAVITY_COORD_SET(c, p, cen, opp) \
    (c).pos = (p), (c).center = (cen), (c).opposite = (opp)
  

typedef struct _Point {
    int x;
    int y;
} Point;

#define POINT_SET(pt, nx, ny) (pt).x = (nx), (pt).y = (ny)
#define POINT_EQUAL(p1, p2) ((p1).x == (p2).x && (p1).y == (p2).y)

typedef struct _Size {
    int width;
    int height;
} Size;

#define SIZE_SET(sz, w, h) (sz).width = (w), (sz).height = (h)

typedef struct _Rect {
    int x;
    int y;
    int width;
    int height;
} Rect;

#define RECT_LEFT(r) ((r).x)
#define RECT_TOP(r) ((r).y)
#define RECT_RIGHT(r) ((r).x + (r).width - 1)
#define RECT_BOTTOM(r) ((r).y + (r).height - 1)

#define RECT_AREA(r) ((r).width * (r).height)

#define RECT_SET_POINT(r, nx, ny) \
    (r).x = (nx), (r).y = (ny)
#define RECT_SET_SIZE(r, w, h) \
    (r).width = (w), (r).height = (h)
#define RECT_SET(r, nx, ny, w, h) \
    (r).x = (nx), (r).y = (ny), (r).width = (w), (r).height = (h)

#define RECT_EQUAL(r1, r2) ((r1).x == (r2).x && (r1).y == (r2).y && \
                            (r1).width == (r2).width && \
                            (r1).height == (r2).height)
#define RECT_EQUAL_DIMS(r, x, y, w, h) \
    ((r).x == (x) && (r).y == (y) && (r).width == (w) && (r).height == (h))

#define RECT_TO_DIMS(r, x, y, w, h) \
    (x) = (r).x, (y) = (r).y, (w) = (r).width, (h) = (r).height

#define RECT_CONTAINS(r, px, py) \
    ((px) >= (r).x && (px) < (r).x + (r).width && \
     (py) >= (r).y && (py) < (r).y + (r).height)
#define RECT_CONTAINS_RECT(r, o) \
    ((o).x >= (r).x && (o).x + (o).width <= (r).x + (r).width && \
     (o).y >= (r).y && (o).y + (o).height <= (r).y + (r).height)

/* Returns true if Rect r and o intersect */
#define RECT_INTERSECTS_RECT(r, o) \
    ((o).x < (r).x + (r).width && (o).x + (o).width > (r).x && \
     (o).y < (r).y + (r).height && (o).y + (o).height > (r).y)

/* Sets Rect r to be the intersection of Rect a and b. */
#define RECT_SET_INTERSECTION(r, a, b) \
    ((r).x = MAX((a).x, (b).x), \
     (r).y = MAX((a).y, (b).y), \
     (r).width = MIN((a).x + (a).width - 1, \
                     (b).x + (b).width - 1) - (r).x + 1, \
     (r).height = MIN((a).y + (a).height - 1, \
                      (b).y + (b).height - 1) - (r).y + 1)

/* Returns the shortest manhatten distance between two rects, or 0 if they
   intersect. */
static inline gint rect_manhatten_distance(Rect r, Rect o)
{
    if (RECT_INTERSECTS_RECT(r, o))
        return 0;

    gint min_distance = G_MAXINT;
    if (RECT_RIGHT(o) < RECT_LEFT(r))
        min_distance = MIN(min_distance, RECT_LEFT(r) - RECT_RIGHT(o));
    if (RECT_LEFT(o) > RECT_RIGHT(r))
        min_distance = MIN(min_distance, RECT_LEFT(o) - RECT_RIGHT(r));
    if (RECT_BOTTOM(o) < RECT_TOP(r))
        min_distance = MIN(min_distance, RECT_TOP(r) - RECT_BOTTOM(o));
    if (RECT_TOP(o) > RECT_BOTTOM(r))
        min_distance = MIN(min_distance, RECT_TOP(o) - RECT_BOTTOM(r));
    return min_distance;
}

typedef struct _Strut {
    int left;
    int top;
    int right;
    int bottom;
} Strut;

typedef struct _StrutPartial {
    int left;
    int top;
    int right;
    int bottom;

    int left_start,   left_end;
    int top_start,    top_end;
    int right_start,  right_end;
    int bottom_start, bottom_end;
} StrutPartial;

#define STRUT_SET(s, l, t, r, b) \
    (s).left = (l), (s).top = (t), (s).right = (r), (s).bottom = (b)

#define STRUT_PARTIAL_SET(s, l, t, r, b, ls, le, ts, te, rs, re, bs, be) \
    (s).left = (l), (s).top = (t), (s).right = (r), (s).bottom = (b), \
    (s).left_start = (ls), (s).left_end = (le), \
    (s).top_start = (ts), (s).top_end = (te), \
    (s).right_start = (rs), (s).right_end = (re), \
    (s).bottom_start = (bs), (s).bottom_end = (be)

#define STRUT_ADD(s1, s2) \
    (s1).left = MAX((s1).left, (s2).left), \
    (s1).right = MAX((s1).right, (s2).right), \
    (s1).top = MAX((s1).top, (s2).top), \
    (s1).bottom = MAX((s1).bottom, (s2).bottom)

#define STRUT_EXISTS(s1) \
    ((s1).left || (s1).top || (s1).right || (s1).bottom)

#define STRUT_EQUAL(s1, s2) \
    ((s1).left == (s2).left && \
     (s1).top == (s2).top && \
     (s1).right == (s2).right && \
     (s1).bottom == (s2).bottom)

#define PARTIAL_STRUT_EQUAL(s1, s2) \
    ((s1).left == (s2).left && \
     (s1).top == (s2).top && \
     (s1).right == (s2).right && \
     (s1).bottom == (s2).bottom && \
     (s1).left_start == (s2).left_start && \
     (s1).left_end == (s2).left_end && \
     (s1).top_start == (s2).top_start && \
     (s1).top_end == (s2).top_end && \
     (s1).right_start == (s2).right_start && \
     (s1).right_end == (s2).right_end && \
     (s1).bottom_start == (s2).bottom_start && \
     (s1).bottom_end == (s2).bottom_end)

#define RANGES_INTERSECT(r1x, r1w, r2x, r2w) \
    (r1w && r2w && r1x < r2x + r2w && r1x + r1w > r2x)

#endif
