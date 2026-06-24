/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   resist.c for the Openbox window manager
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

#include "resist.h"
#include "client.h"
#include "frame.h"
#include "stacking.h"
#include "screen.h"
#include "dock.h"
#include "config.h"

#include <glib.h>

static gboolean resist_move_window(Rect window,
                                   Rect target, gint resist,
                                   gint *x, gint *y)
{
    gint l, t, r, b; /* requested edges */
    gint cl, ct, cr, cb; /* current edges */
    gint w, h; /* current size */
    gint tl, tt, tr, tb; /* 1 past the target's edges on each side */
    gboolean snapx = 0, snapy = 0;

    w = window.width;
    h = window.height;

    l = *x;
    t = *y;
    r = l + w - 1;
    b = t + h - 1;

    cl = RECT_LEFT(window);
    ct = RECT_TOP(window);
    cr = RECT_RIGHT(window);
    cb = RECT_BOTTOM(window);

    tl = RECT_LEFT(target) - 1;
    tt = RECT_TOP(target) - 1;
    tr = RECT_RIGHT(target) + 1;
    tb = RECT_BOTTOM(target) + 1;

    /* snapx and snapy ensure that the window snaps to the top-most
       window edge available, without going all the way from
       bottom-to-top in the stacking list
    */
    if (!snapx) {
        if (ct < tb && cb > tt) {
            if (cl >= tr && l < tr && l >= tr - resist)
                *x = tr, snapx = TRUE;
            else if (cr <= tl && r > tl &&
                     r <= tl + resist)
                *x = tl - w + 1, snapx = TRUE;
            if (snapx) {
                /* try to corner snap to the window */
                if (ct > tt && t <= tt &&
                    t > tt - resist)
                    *y = tt + 1, snapy = TRUE;
                else if (cb < tb && b >= tb &&
                         b < tb + resist)
                    *y = tb - h, snapy = TRUE;
            }
        }
    }
    if (!snapy) {
        if (cl < tr && cr > tl) {
            if (ct >= tb && t < tb && t >= tb - resist)
                *y = tb, snapy = TRUE;
            else if (cb <= tt && b > tt &&
                     b <= tt + resist)
                *y = tt - h + 1, snapy = TRUE;
            if (snapy) {
                /* try to corner snap to the window */
                if (cl > tl && l <= tl &&
                    l > tl - resist)
                    *x = tl + 1, snapx = TRUE;
                else if (cr < tr && r >= tr &&
                         r < tr + resist)
                    *x = tr - w, snapx = TRUE;
            }
        }
    }

    return snapx && snapy;
}

void resist_move_windows(ObClient *c, gint resist, gint *x, gint *y)
{
    GList *it;
    Rect dock_area;

    if (!resist) return;

    frame_client_gravity(c->frame, x, y);


    for (it = stacking_list; it; it = g_list_next(it)) {
        ObClient *target;

        if (!WINDOW_IS_CLIENT(it->data))
            continue;
        target = it->data;

        /* don't snap to self or non-visibles */
        if (!target->frame->visible || target == c)
            continue;
        /* don't snap to windows set to below and skip_taskbar (desklets) */
        if (target->below && !c->below && target->skip_taskbar)
            continue;

        if (resist_move_window(c->frame->area, target->frame->area,
                               resist, x, y))
            break;
    }
    dock_get_area(&dock_area);
    resist_move_window(c->frame->area, dock_area, resist, x, y);

    frame_frame_gravity(c->frame, x, y);
}

void resist_move_monitors(ObClient *c, gint resist, gint *x, gint *y)
{
    Rect *area;
    const Rect *parea;
    guint i;
    gint l, t, r, b; /* requested edges */
    gint al, at, ar, ab; /* screen area edges */
    gint pl, pt, pr, pb; /* physical screen area edges */
    gint cl, ct, cr, cb; /* current edges */
    gint w, h; /* current size */
    Rect desired_area;

    if (!resist) return;

    frame_client_gravity(c->frame, x, y);

    w = c->frame->area.width;
    h = c->frame->area.height;

    l = *x;
    t = *y;
    r = l + w - 1;
    b = t + h - 1;

    cl = RECT_LEFT(c->frame->area);
    ct = RECT_TOP(c->frame->area);
    cr = RECT_RIGHT(c->frame->area);
    cb = RECT_BOTTOM(c->frame->area);

    RECT_SET(desired_area, c->frame->area.x, c->frame->area.y,
             c->frame->area.width, c->frame->area.height);

    for (i = 0; i < screen_num_monitors; ++i) {
        parea = screen_physical_area_monitor(i);

        if (!RECT_INTERSECTS_RECT(*parea, c->frame->area))
            continue;

        area = screen_area(c->desktop, SCREEN_AREA_ALL_MONITORS,
                           &desired_area);

        al = RECT_LEFT(*area);
        at = RECT_TOP(*area);
        ar = RECT_RIGHT(*area);
        ab = RECT_BOTTOM(*area);
        pl = RECT_LEFT(*parea);
        pt = RECT_TOP(*parea);
        pr = RECT_RIGHT(*parea);
        pb = RECT_BOTTOM(*parea);

        if (cl >= al && l < al && l >= al - resist)
            *x = al;
        else if (cr <= ar && r > ar && r <= ar + resist)
            *x = ar - w + 1;
        else if (cl >= pl && l < pl && l >= pl - resist)
            *x = pl;
        else if (cr <= pr && r > pr && r <= pr + resist)
            *x = pr - w + 1;

        if (ct >= at && t < at && t >= at - resist)
            *y = at;
        else if (cb <= ab && b > ab && b < ab + resist)
            *y = ab - h + 1;
        else if (ct >= pt && t < pt && t >= pt - resist)
            *y = pt;
        else if (cb <= pb && b > pb && b < pb + resist)
            *y = pb - h + 1;

        g_slice_free(Rect, area);
    }

    frame_frame_gravity(c->frame, x, y);
}

static gboolean resist_size_window(Rect window, Rect target, gint resist,
                                   gint *w, gint *h, ObDirection dir)
{
    gint l, t, r, b; /* my left, top, right and bottom sides */
    gint tl, tt, tr, tb; /* target's left, top, right and bottom bottom sides*/
    gint dlt, drb; /* my destination left/top and right/bottom sides */
    gboolean snapx = 0, snapy = 0;
    gint orgw, orgh;

    l = RECT_LEFT(window);
    t = RECT_TOP(window);
    r = RECT_RIGHT(window);
    b = RECT_BOTTOM(window);

    orgw = window.width;
    orgh = window.height;

    tl = RECT_LEFT(target);
    tt = RECT_TOP(target);
    tr = RECT_RIGHT(target);
    tb = RECT_BOTTOM(target);

    if (!snapx) {
        /* horizontal snapping */
        if (t < tb && b > tt) {
            switch (dir) {
            case OB_DIRECTION_EAST:
            case OB_DIRECTION_NORTHEAST:
            case OB_DIRECTION_SOUTHEAST:
            case OB_DIRECTION_NORTH:
            case OB_DIRECTION_SOUTH:
                dlt = l;
                drb = r + *w - orgw;
                if (r < tl && drb >= tl &&
                    drb < tl + resist)
                    *w = tl - l, snapx = TRUE;
                break;
            case OB_DIRECTION_WEST:
            case OB_DIRECTION_NORTHWEST:
            case OB_DIRECTION_SOUTHWEST:
                dlt = l - *w + orgw;
                drb = r;
                if (l > tr && dlt <= tr &&
                    dlt > tr - resist)
                    *w = r - tr, snapx = TRUE;
                break;
            }
        }
    }

    if (!snapy) {
        /* vertical snapping */
        if (l < tr && r > tl) {
            switch (dir) {
            case OB_DIRECTION_SOUTH:
            case OB_DIRECTION_SOUTHWEST:
            case OB_DIRECTION_SOUTHEAST:
            case OB_DIRECTION_EAST:
            case OB_DIRECTION_WEST:
                dlt = t;
                drb = b + *h - orgh;
                if (b < tt && drb >= tt &&
                    drb < tt + resist)
                    *h = tt - t, snapy = TRUE;
                break;
            case OB_DIRECTION_NORTH:
            case OB_DIRECTION_NORTHWEST:
            case OB_DIRECTION_NORTHEAST:
                dlt = t - *h + orgh;
                drb = b;
                if (t > tb && dlt <= tb &&
                    dlt > tb - resist)
                    *h = b - tb, snapy = TRUE;
                break;
            }
        }
    }

    /* snapped both ways */
    return snapx && snapy;
}

void resist_size_windows(ObClient *c, gint resist, gint *w, gint *h,
                         ObDirection dir)
{
    GList *it;
    ObClient *target; /* target */
    Rect dock_area;

    if (!resist) return;

    for (it = stacking_list; it; it = g_list_next(it)) {
        if (!WINDOW_IS_CLIENT(it->data))
            continue;
        target = it->data;

        /* don't snap to invisibles or ourself */
        if (!target->frame->visible || target == c)
            continue;
        /* don't snap to windows set to below and skip_taskbar (desklets) */
        if (target->below && !c->below && target->skip_taskbar)
            continue;

        if (resist_size_window(c->frame->area, target->frame->area,
                               resist, w, h, dir))
            break;
    }
    dock_get_area(&dock_area);
    resist_size_window(c->frame->area, dock_area,
                       resist, w, h, dir);
}

void resist_size_monitors(ObClient *c, gint resist, gint *w, gint *h,
                          ObDirection dir)
{
    gint l, t, r, b; /* my left, top, right and bottom sides */
    gint dlt, drb; /* my destination left/top and right/bottom sides */
    Rect *area;
    const Rect *parea;
    gint al, at, ar, ab; /* screen boundaries */
    gint pl, pt, pr, pb; /* physical screen boundaries */
    guint i;
    Rect desired_area;

    if (!resist) return;

    l = RECT_LEFT(c->frame->area);
    r = RECT_RIGHT(c->frame->area);
    t = RECT_TOP(c->frame->area);
    b = RECT_BOTTOM(c->frame->area);

    RECT_SET(desired_area, c->area.x, c->area.y, *w, *h);

    for (i = 0; i < screen_num_monitors; ++i) {
        parea = screen_physical_area_monitor(i);

        if (!RECT_INTERSECTS_RECT(*parea, c->frame->area))
            continue;

        area = screen_area(c->desktop, SCREEN_AREA_ALL_MONITORS,
                           &desired_area);

        /* get the screen boundaries */
        al = RECT_LEFT(*area);
        at = RECT_TOP(*area);
        ar = RECT_RIGHT(*area);
        ab = RECT_BOTTOM(*area);
        pl = RECT_LEFT(*parea);
        pt = RECT_TOP(*parea);
        pr = RECT_RIGHT(*parea);
        pb = RECT_BOTTOM(*parea);

        /* horizontal snapping */
        switch (dir) {
        case OB_DIRECTION_EAST:
        case OB_DIRECTION_NORTHEAST:
        case OB_DIRECTION_SOUTHEAST:
        case OB_DIRECTION_NORTH:
        case OB_DIRECTION_SOUTH:
            dlt = l;
            drb = r + *w - c->frame->area.width;
            if (r <= ar && drb > ar && drb <= ar + resist)
                *w = ar - l + 1;
            else if (r <= pr && drb > pr && drb <= pr + resist)
                *w = pr - l + 1;
            break;
        case OB_DIRECTION_WEST:
        case OB_DIRECTION_NORTHWEST:
        case OB_DIRECTION_SOUTHWEST:
            dlt = l - *w + c->frame->area.width;
            drb = r;
            if (l >= al && dlt < al && dlt >= al - resist)
                *w = r - al + 1;
            else if (l >= pl && dlt < pl && dlt >= pl - resist)
                *w = r - pl + 1;
            break;
        }

        /* vertical snapping */
        switch (dir) {
        case OB_DIRECTION_SOUTH:
        case OB_DIRECTION_SOUTHWEST:
        case OB_DIRECTION_SOUTHEAST:
        case OB_DIRECTION_WEST:
        case OB_DIRECTION_EAST:
            dlt = t;
            drb = b + *h - c->frame->area.height;
            if (b <= ab && drb > ab && drb <= ab + resist)
                *h = ab - t + 1;
            else if (b <= pb && drb > pb && drb <= pb + resist)
                *h = pb - t + 1;
            break;
        case OB_DIRECTION_NORTH:
        case OB_DIRECTION_NORTHWEST:
        case OB_DIRECTION_NORTHEAST:
            dlt = t - *h + c->frame->area.height;
            drb = b;
            if (t >= at && dlt < at && dlt >= at - resist)
                *h = b - at + 1;
            else if (t >= pt && dlt < pt && dlt >= pt - resist)
                *h = b - pt + 1;
            break;
        }

        g_slice_free(Rect, area);
    }
}
