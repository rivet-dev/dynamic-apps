/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   focus_cycle.c for the Openbox window manager
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

#include "focus_cycle.h"
#include "focus_cycle_indicator.h"
#include "client.h"
#include "frame.h"
#include "focus.h"
#include "screen.h"
#include "openbox.h"
#include "debug.h"

#include <X11/Xlib.h>
#include <glib.h>

typedef enum {
    OB_CYCLE_NONE = 0,
    OB_CYCLE_NORMAL,
    OB_CYCLE_DIRECTIONAL
} ObCycleType;

ObClient       *focus_cycle_target = NULL;
static ObCycleType focus_cycle_type = OB_CYCLE_NONE;
static gboolean focus_cycle_linear;
static gboolean focus_cycle_iconic_windows;
static gboolean focus_cycle_all_desktops;
static gboolean focus_cycle_nonhilite_windows;
static gboolean focus_cycle_dock_windows;
static gboolean focus_cycle_desktop_windows;

static ObClient *focus_find_directional(ObClient *c,
                                        ObDirection dir,
                                        gboolean dock_windows,
                                        gboolean desktop_windows);

void focus_cycle_startup(gboolean reconfig)
{
    if (reconfig) return;
}

void focus_cycle_shutdown(gboolean reconfig)
{
    if (reconfig) return;
}

void focus_cycle_addremove(ObClient *c, gboolean redraw)
{
    if (!focus_cycle_type)
        return;

    if (focus_cycle_type == OB_CYCLE_DIRECTIONAL) {
        if (c && focus_cycle_target == c) {
            focus_directional_cycle(0, TRUE, TRUE, TRUE, TRUE,
                                    TRUE, TRUE, TRUE);
        }
    }
    else if (c && redraw) {
        gboolean v, s;

        v = focus_cycle_valid(c);
        s = focus_cycle_popup_is_showing(c) || c == focus_cycle_target;

        if (v != s)
            focus_cycle_reorder();
    }
    else if (redraw) {
        focus_cycle_reorder();
    }
}

void focus_cycle_reorder()
{
    if (focus_cycle_type == OB_CYCLE_NORMAL) {
        focus_cycle_target = focus_cycle_popup_refresh(focus_cycle_target,
                                                       TRUE,
                                                       focus_cycle_linear);
        focus_cycle_update_indicator(focus_cycle_target);
        if (!focus_cycle_target)
            focus_cycle(TRUE, TRUE, TRUE, TRUE, TRUE, TRUE,
                        TRUE, OB_FOCUS_CYCLE_POPUP_MODE_NONE,
                        TRUE, TRUE);
    }
}

ObClient* focus_cycle(gboolean forward, gboolean all_desktops,
                      gboolean nonhilite_windows,
                      gboolean dock_windows, gboolean desktop_windows,
                      gboolean linear, gboolean showbar,
                      ObFocusCyclePopupMode mode,
                      gboolean done, gboolean cancel)
{
    static GList *order = NULL;
    GList *it, *start, *list;
    ObClient *ft = NULL;
    ObClient *ret = NULL;

    if (cancel) {
        focus_cycle_target = NULL;
        goto done_cycle;
    } else if (done)
        goto done_cycle;

    if (!focus_order)
        goto done_cycle;

    if (linear) list = client_list;
    else        list = focus_order;

    if (focus_cycle_target == NULL) {
        focus_cycle_linear = linear;
        focus_cycle_iconic_windows = TRUE;
        focus_cycle_all_desktops = all_desktops;
        focus_cycle_nonhilite_windows = nonhilite_windows;
        focus_cycle_dock_windows = dock_windows;
        focus_cycle_desktop_windows = desktop_windows;
        start = it = g_list_find(list, focus_client);
    } else
        start = it = g_list_find(list, focus_cycle_target);

    if (!start) /* switched desktops or something? */
        start = it = forward ? g_list_last(list) : g_list_first(list);
    if (!start) goto done_cycle;

    do {
        if (forward) {
            it = it->next;
            if (it == NULL) it = g_list_first(list);
        } else {
            it = it->prev;
            if (it == NULL) it = g_list_last(list);
        }
        ft = it->data;
        if (focus_cycle_valid(ft)) {
            if (ft != focus_cycle_target) { /* prevents flicker */
                focus_cycle_target = ft;
                focus_cycle_type = OB_CYCLE_NORMAL;
                focus_cycle_draw_indicator(showbar ? ft : NULL);
            }
            /* same arguments as focus_target_valid */
            focus_cycle_popup_show(ft, mode, focus_cycle_linear);
            return focus_cycle_target;
        }
    } while (it != start);

done_cycle:
    if (done && !cancel) ret = focus_cycle_target;

    focus_cycle_target = NULL;
    focus_cycle_type = OB_CYCLE_NONE;
    g_list_free(order);
    order = NULL;

    focus_cycle_draw_indicator(NULL);
    focus_cycle_popup_hide();

    return ret;
}

/* this be mostly ripped from fvwm */
static ObClient *focus_find_directional(ObClient *c, ObDirection dir,
                                        gboolean dock_windows,
                                        gboolean desktop_windows)
{
    gint my_cx, my_cy, his_cx, his_cy;
    gint offset = 0;
    gint distance = 0;
    gint score, best_score;
    ObClient *best_client, *cur;
    GList *it;

    if (!client_list)
        return NULL;

    /* first, find the centre coords of the currently focused window */
    my_cx = c->frame->area.x + c->frame->area.width / 2;
    my_cy = c->frame->area.y + c->frame->area.height / 2;

    best_score = -1;
    best_client = c;

    for (it = g_list_first(client_list); it; it = g_list_next(it)) {
        cur = it->data;

        /* the currently selected window isn't interesting */
        if (cur == c)
            continue;
        if (!focus_cycle_valid(it->data))
            continue;

        /* find the centre coords of this window, from the
         * currently focused window's point of view */
        his_cx = (cur->frame->area.x - my_cx)
            + cur->frame->area.width / 2;
        his_cy = (cur->frame->area.y - my_cy)
            + cur->frame->area.height / 2;

        if (dir == OB_DIRECTION_NORTHEAST || dir == OB_DIRECTION_SOUTHEAST ||
            dir == OB_DIRECTION_SOUTHWEST || dir == OB_DIRECTION_NORTHWEST)
        {
            gint tx;
            /* Rotate the diagonals 45 degrees counterclockwise.
             * To do this, multiply the matrix /+h +h\ with the
             * vector (x y).                   \-h +h/
             * h = sqrt(0.5). We can set h := 1 since absolute
             * distance doesn't matter here. */
            tx = his_cx + his_cy;
            his_cy = -his_cx + his_cy;
            his_cx = tx;
        }

        switch (dir) {
        case OB_DIRECTION_NORTH:
        case OB_DIRECTION_SOUTH:
        case OB_DIRECTION_NORTHEAST:
        case OB_DIRECTION_SOUTHWEST:
            offset = (his_cx < 0) ? -his_cx : his_cx;
            distance = ((dir == OB_DIRECTION_NORTH ||
                         dir == OB_DIRECTION_NORTHEAST) ?
                        -his_cy : his_cy);
            break;
        case OB_DIRECTION_EAST:
        case OB_DIRECTION_WEST:
        case OB_DIRECTION_SOUTHEAST:
        case OB_DIRECTION_NORTHWEST:
            offset = (his_cy < 0) ? -his_cy : his_cy;
            distance = ((dir == OB_DIRECTION_WEST ||
                         dir == OB_DIRECTION_NORTHWEST) ?
                        -his_cx : his_cx);
            break;
        }

        /* the target must be in the requested direction */
        if (distance <= 0)
            continue;

        /* Calculate score for this window.  The smaller the better. */
        score = distance + offset;

        /* windows more than 45 degrees off the direction are
         * heavily penalized and will only be chosen if nothing
         * else within a million pixels */
        if (offset > distance)
            score += 1000000;

        if (best_score == -1 || score < best_score) {
            best_client = cur;
            best_score = score;
        }
    }

    return best_client;
}

ObClient* focus_directional_cycle(ObDirection dir, gboolean dock_windows,
                                  gboolean desktop_windows,
                                  gboolean interactive,
                                  gboolean showbar, gboolean dialog,
                                  gboolean done, gboolean cancel)
{
    static ObClient *first = NULL;
    ObClient *ft = NULL;
    ObClient *ret = NULL;

    if (cancel) {
        focus_cycle_target = NULL;
        goto done_cycle;
    } else if (done && interactive)
        goto done_cycle;

    if (!focus_order)
        goto done_cycle;

    if (focus_cycle_target == NULL) {
        focus_cycle_linear = FALSE;
        focus_cycle_iconic_windows = FALSE;
        focus_cycle_all_desktops = FALSE;
        focus_cycle_nonhilite_windows = TRUE;
        focus_cycle_dock_windows = dock_windows;
        focus_cycle_desktop_windows = desktop_windows;
    }

    if (!first) first = focus_client;

    if (focus_cycle_target)
        ft = focus_find_directional(focus_cycle_target, dir, dock_windows,
                                    desktop_windows);
    else if (first)
        ft = focus_find_directional(first, dir, dock_windows, desktop_windows);
    else {
        GList *it;

        for (it = focus_order; it; it = g_list_next(it))
            if (focus_cycle_valid(it->data)) {
                ft = it->data;
                break;
            }
    }

    if (ft && ft != focus_cycle_target) {/* prevents flicker */
        focus_cycle_target = ft;
        focus_cycle_type = OB_CYCLE_DIRECTIONAL;
        if (!interactive)
            goto done_cycle;
        focus_cycle_draw_indicator(showbar ? ft : NULL);
    }
    if (focus_cycle_target && dialog)
        /* same arguments as focus_target_valid */
        focus_cycle_popup_single_show(focus_cycle_target);
    return focus_cycle_target;

done_cycle:
    if (done && !cancel) ret = focus_cycle_target;

    first = NULL;
    focus_cycle_target = NULL;
    focus_cycle_type = OB_CYCLE_NONE;

    focus_cycle_draw_indicator(NULL);
    focus_cycle_popup_single_hide();

    return ret;
}

gboolean focus_cycle_valid(struct _ObClient *client)
{
    return focus_valid_target(client, screen_desktop, TRUE,
                              focus_cycle_iconic_windows,
                              focus_cycle_all_desktops,
                              focus_cycle_nonhilite_windows,
                              focus_cycle_dock_windows,
                              focus_cycle_desktop_windows,
                              FALSE);
}
