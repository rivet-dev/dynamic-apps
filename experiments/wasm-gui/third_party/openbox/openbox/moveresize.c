/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   moveresize.c for the Openbox window manager
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

#include "grab.h"
#include "framerender.h"
#include "screen.h"
#include "client.h"
#include "focus.h"
#include "frame.h"
#include "openbox.h"
#include "resist.h"
#include "popup.h"
#include "moveresize.h"
#include "config.h"
#include "event.h"
#include "debug.h"
#include "obrender/render.h"
#include "obrender/theme.h"
#include "obt/display.h"
#include "obt/xqueue.h"
#include "obt/prop.h"
#include "obt/keyboard.h"

#include <X11/Xlib.h>
#include <glib.h>

/* how far windows move and resize with the keyboard arrows */
#define KEY_DIST 8
#define SYNC_TIMEOUTS 4

gboolean moveresize_in_progress = FALSE;
ObClient *moveresize_client = NULL;
#ifdef SYNC
XSyncAlarm moveresize_alarm = None;
#endif

static gboolean moving = FALSE; /* TRUE - moving, FALSE - resizing */

/* starting geometry for the window being moved/resized, so it can be
   restored */
static gint start_x, start_y, start_cx, start_cy, start_cw, start_ch;
static gboolean was_max_horz, was_max_vert;
static Rect pre_max_area;
static gint cur_x, cur_y, cur_w, cur_h;
static guint button;
static guint32 corner;
static ObDirection edge_warp_dir = -1;
static gboolean edge_warp_odd = FALSE;
static guint edge_warp_timer = 0;
static ObDirection key_resize_edge = -1;
static guint waiting_for_sync;
#ifdef SYNC
static guint sync_timer = 0;
#endif

static ObPopup *popup = NULL;

static void do_move(gboolean keyboard, gint keydist);
static void do_resize(void);
static void do_edge_warp(gint x, gint y);
static void cancel_edge_warp();
#ifdef SYNC
static gboolean sync_timeout_func(gpointer data);
#endif

static void client_dest(ObClient *client, gpointer data)
{
    if (moveresize_client == client)
        moveresize_end(TRUE);
    if (popup && client == popup->client)
        popup->client = NULL;
}

void moveresize_startup(gboolean reconfig)
{
    popup = popup_new();
    popup_set_text_align(popup, RR_JUSTIFY_CENTER);

    if (!reconfig)
        client_add_destroy_notify(client_dest, NULL);
}

void moveresize_shutdown(gboolean reconfig)
{
    if (!reconfig) {
        if (moveresize_in_progress)
            moveresize_end(FALSE);
        client_remove_destroy_notify(client_dest);
    }

    popup_free(popup);
    popup = NULL;
}

static void popup_coords(ObClient *c, const gchar *format, gint a, gint b)
{
    gchar *text;

    text = g_strdup_printf(format, a, b);
    if (config_resize_popup_pos == OB_RESIZE_POS_TOP)
        popup_position(popup, SouthGravity,
                       c->frame->area.x
                     + c->frame->area.width/2,
                       c->frame->area.y - ob_rr_theme->fbwidth);
    else if (config_resize_popup_pos == OB_RESIZE_POS_CENTER)
        popup_position(popup, CenterGravity,
                       c->frame->area.x + c->frame->area.width / 2,
                       c->frame->area.y + c->frame->area.height / 2);
    else /* Fixed */ {
        const Rect *area = screen_physical_area_active();
        gint gravity, x, y;

        x = config_resize_popup_fixed.x.pos;
        if (config_resize_popup_fixed.x.center)
            x = area->x + area->width/2;
        else if (config_resize_popup_fixed.x.opposite)
            x = RECT_RIGHT(*area) - x;
        else
            x = area->x + x;

        y = config_resize_popup_fixed.y.pos;
        if (config_resize_popup_fixed.y.center)
            y = area->y + area->height/2;
        else if (config_resize_popup_fixed.y.opposite)
            y = RECT_RIGHT(*area) - y;
        else
            y = area->y + y;

        if (config_resize_popup_fixed.x.center) {
            if (config_resize_popup_fixed.y.center)
                gravity = CenterGravity;
            else if (config_resize_popup_fixed.y.opposite)
                gravity = SouthGravity;
            else
                gravity = NorthGravity;
        }
        else if (config_resize_popup_fixed.x.opposite) {
            if (config_resize_popup_fixed.y.center)
                gravity = EastGravity;
            else if (config_resize_popup_fixed.y.opposite)
                gravity = SouthEastGravity;
            else
                gravity = NorthEastGravity;
        }
        else {
            if (config_resize_popup_fixed.y.center)
                gravity = WestGravity;
            else if (config_resize_popup_fixed.y.opposite)
                gravity = SouthWestGravity;
            else
                gravity = NorthWestGravity;
        }

        popup_position(popup, gravity, x, y);
    }
    popup->client = c;
    popup_show(popup, text);
    g_free(text);
}

void moveresize_start(ObClient *c, gint x, gint y, guint b, guint32 cnr)
{
    ObCursor cur;
    gboolean mv = (cnr == OBT_PROP_ATOM(NET_WM_MOVERESIZE_MOVE) ||
                   cnr == OBT_PROP_ATOM(NET_WM_MOVERESIZE_MOVE_KEYBOARD));
    gint up = 1;
    gint left = 1;

    if (moveresize_in_progress || !c->frame->visible ||
        !(mv ?
          (c->functions & OB_CLIENT_FUNC_MOVE) :
          (c->functions & OB_CLIENT_FUNC_RESIZE)))
        return;

    if (cnr == OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_TOPLEFT)) {
        cur = OB_CURSOR_NORTHWEST;
        up = left = -1;
    }
    else if (cnr == OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_TOP)) {
        cur = OB_CURSOR_NORTH;
        up = -1;
    }
    else if (cnr == OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_TOPRIGHT)) {
        cur = OB_CURSOR_NORTHEAST;
        up = -1;
    }
    else if (cnr == OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_RIGHT))
        cur = OB_CURSOR_EAST;
    else if (cnr == OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT))
        cur = OB_CURSOR_SOUTHEAST;
    else if (cnr == OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_BOTTOM))
        cur = OB_CURSOR_SOUTH;
    else if (cnr == OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT)) {
        cur = OB_CURSOR_SOUTHWEST;
        left = -1;
    }
    else if (cnr == OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_LEFT)) {
        cur = OB_CURSOR_WEST;
        left = -1;
    }
    else if (cnr == OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_KEYBOARD))
        cur = OB_CURSOR_SOUTHEAST;
    else if (cnr == OBT_PROP_ATOM(NET_WM_MOVERESIZE_MOVE))
        cur = OB_CURSOR_MOVE;
    else if (cnr == OBT_PROP_ATOM(NET_WM_MOVERESIZE_MOVE_KEYBOARD))
        cur = OB_CURSOR_MOVE;
    else
        g_assert_not_reached();

    /* keep the pointer bounded to the screen for move/resize */
    if (!grab_pointer(FALSE, TRUE, cur))
        return;
    if (!grab_keyboard()) {
        ungrab_pointer();
        return;
    }

    frame_end_iconify_animation(c->frame);

    moving = mv;
    moveresize_client = c;
    start_cx = c->area.x;
    start_cy = c->area.y;
    start_cw = c->area.width;
    start_ch = c->area.height;
    /* these adjustments for the size_inc make resizing a terminal more
       friendly. you essentially start the resize in the middle of the
       increment instead of at 0, so you have to move half an increment
       either way instead of a full increment one and 1 px the other. */
    start_x = x - (mv ? 0 : left * c->size_inc.width / 2);
    start_y = y - (mv ? 0 : up * c->size_inc.height / 2);
    corner = cnr;
    button = b;
    key_resize_edge = -1;

    /* default to not putting max back on cancel */
    was_max_horz = was_max_vert = FALSE;

    /*
      have to change start_cx and start_cy if going to do this..
    if (corner == prop_atoms.net_wm_moveresize_move_keyboard ||
        corner == prop_atoms.net_wm_moveresize_size_keyboard)
        XWarpPointer(ob_display, None, c->window, 0, 0, 0, 0,
                     c->area.width / 2, c->area.height / 2);
    */

    cur_x = start_cx;
    cur_y = start_cy;
    cur_w = start_cw;
    cur_h = start_ch;

    moveresize_in_progress = TRUE;
    waiting_for_sync = 0;

#ifdef SYNC
    if (config_resize_redraw && !moving && obt_display_extension_sync &&
        moveresize_client->sync_request && moveresize_client->sync_counter &&
        !moveresize_client->not_responding)
    {
        /* Initialize values for the resize syncing, and create an alarm for
           the client's xsync counter */

        XSyncValue val;
        XSyncAlarmAttributes aa;

        /* set the counter to an initial value */
        XSyncIntToValue(&val, 0);
        XSyncSetCounter(obt_display, moveresize_client->sync_counter, val);

        /* this will be incremented when we tell the client what we're
           looking for */
        moveresize_client->sync_counter_value = 0;

        /* the next sequence we're waiting for with the alarm */
        XSyncIntToValue(&val, 1);

        /* set an alarm on the counter */
        aa.trigger.counter = moveresize_client->sync_counter;
        aa.trigger.wait_value = val;
        aa.trigger.value_type = XSyncAbsolute;
        aa.trigger.test_type = XSyncPositiveTransition;
        aa.events = True;
        XSyncIntToValue(&aa.delta, 1);
        moveresize_alarm = XSyncCreateAlarm(obt_display,
                                            XSyncCACounter |
                                            XSyncCAValue |
                                            XSyncCAValueType |
                                            XSyncCATestType |
                                            XSyncCADelta |
                                            XSyncCAEvents,
                                            &aa);
    }
#endif
}

void moveresize_end(gboolean cancel)
{
    ungrab_keyboard();
    ungrab_pointer();

    popup_hide(popup);
    popup->client = NULL;

    if (!moving) {
#ifdef SYNC
        /* turn off the alarm */
        if (moveresize_alarm != None) {
            XSyncDestroyAlarm(obt_display, moveresize_alarm);
            moveresize_alarm = None;
        }

        if (sync_timer) g_source_remove(sync_timer);
        sync_timer = 0;
#endif
    }

    /* don't use client_move() here, use the same width/height as
       we've been using during the move, otherwise we get different results
       when moving maximized windows between monitors of different sizes !
    */
    client_configure(moveresize_client,
                     (cancel ? start_cx : cur_x),
                     (cancel ? start_cy : cur_y),
                     (cancel ? start_cw : cur_w),
                     (cancel ? start_ch : cur_h),
                     TRUE, TRUE, FALSE);

    /* restore the client's maximized state. do this after putting the window
       back in its original spot to minimize visible flicker */
    if (cancel && (was_max_horz || was_max_vert)) {
        const gboolean h = moveresize_client->max_horz;
        const gboolean v = moveresize_client->max_vert;

        client_maximize(moveresize_client, TRUE,
                        was_max_horz && was_max_vert ? 0 :
                        (was_max_horz ? 1 : 2));

        /* replace the premax values with the ones we had saved if
           the client doesn't have any already set */
        if (was_max_horz && !h) {
            moveresize_client->pre_max_area.x = pre_max_area.x;
            moveresize_client->pre_max_area.width = pre_max_area.width;
        }
        if (was_max_vert && !v) {
            moveresize_client->pre_max_area.y = pre_max_area.y;
            moveresize_client->pre_max_area.height = pre_max_area.height;
        }
    }

    /* dont edge warp after its ended */
    cancel_edge_warp();

    moveresize_in_progress = FALSE;
    moveresize_client = NULL;
}

static void do_move(gboolean keyboard, gint keydist)
{
    gint resist;

    if (keyboard) resist = keydist - 1; /* resist for one key press */
    else resist = config_resist_win;
    resist_move_windows(moveresize_client, resist, &cur_x, &cur_y);
    if (!keyboard) resist = config_resist_edge;
    resist_move_monitors(moveresize_client, resist, &cur_x, &cur_y);

    client_configure(moveresize_client, cur_x, cur_y, cur_w, cur_h,
                     TRUE, FALSE, FALSE);
    if (config_resize_popup_show == 2) /* == "Always" */
        popup_coords(moveresize_client, "%d x %d",
                     moveresize_client->frame->area.x,
                     moveresize_client->frame->area.y);
}

static void do_resize(void)
{
    gint x, y, w, h, lw, lh;

    /* see if it is actually going to resize
       USE cur_x AND cur_y HERE !  Otherwise the try_configure won't know
       what struts to use !!
     */
    x = cur_x;
    y = cur_y;
    w = cur_w;
    h = cur_h;
    client_try_configure(moveresize_client, &x, &y, &w, &h,
                         &lw, &lh, TRUE);
    if (!(w == moveresize_client->area.width &&
          h == moveresize_client->area.height) &&
        /* if waiting_for_sync == 0, then we aren't waiting.
           if it is > SYNC_TIMEOUTS, then we have timed out
           that many times already, so forget about waiting more */
        (waiting_for_sync == 0 || waiting_for_sync > SYNC_TIMEOUTS))
    {
#ifdef SYNC
        if (config_resize_redraw && obt_display_extension_sync &&
            /* don't send another sync when one is pending */
            waiting_for_sync == 0 &&
            moveresize_client->sync_request &&
            moveresize_client->sync_counter &&
            !moveresize_client->not_responding)
        {
            XEvent ce;
            XSyncValue val;

            /* increment the value we're waiting for */
            ++moveresize_client->sync_counter_value;
            XSyncIntToValue(&val, moveresize_client->sync_counter_value);

            /* tell the client what we're waiting for */
            ce.xclient.type = ClientMessage;
            ce.xclient.message_type = OBT_PROP_ATOM(WM_PROTOCOLS);
            ce.xclient.display = obt_display;
            ce.xclient.window = moveresize_client->window;
            ce.xclient.format = 32;
            ce.xclient.data.l[0] = OBT_PROP_ATOM(NET_WM_SYNC_REQUEST);
            ce.xclient.data.l[1] = event_time();
            ce.xclient.data.l[2] = XSyncValueLow32(val);
            ce.xclient.data.l[3] = XSyncValueHigh32(val);
            ce.xclient.data.l[4] = 0l;
            XSendEvent(obt_display, moveresize_client->window, FALSE,
                       NoEventMask, &ce);

            waiting_for_sync = 1;

            if (sync_timer) g_source_remove(sync_timer);
            sync_timer = g_timeout_add(2000, sync_timeout_func, NULL);
        }
#endif

        /* force a ConfigureNotify, it is part of the spec for SYNC resizing
           and MUST follow the sync counter notification */
        client_configure(moveresize_client, cur_x, cur_y, cur_w, cur_h,
                         TRUE, FALSE, TRUE);
    }

    /* this would be better with a fixed width font ... XXX can do it better
       if there are 2 text boxes */
    if (config_resize_popup_show == 2 || /* == "Always" */
            (config_resize_popup_show == 1 && /* == "Nonpixel" */
             moveresize_client->size_inc.width > 1 &&
             moveresize_client->size_inc.height > 1))
        popup_coords(moveresize_client, "%d x %d", lw, lh);
}

#ifdef SYNC
static gboolean sync_timeout_func(gpointer data)
{
    ++waiting_for_sync; /* we timed out waiting for our sync... */
    do_resize(); /* ...so let any pending resizes through */

    if (waiting_for_sync > SYNC_TIMEOUTS) {
        sync_timer = 0;
        return FALSE; /* don't repeat */
    }
    else
        return TRUE; /* keep waiting */
}
#endif

static void calc_resize(gboolean keyboard, gint keydist, gint *dw, gint *dh,
                        ObDirection dir)
{
    gint resist, x = 0, y = 0, lw, lh, ow, oh, nw, nh;
    gint trydw, trydh;

    ow = cur_w;
    oh = cur_h;
    nw = ow + *dw;
    nh = oh + *dh;

    if (!keyboard &&
        (moveresize_client->max_ratio || moveresize_client->min_ratio))
    {
        switch (dir) {
        case OB_DIRECTION_NORTH:
        case OB_DIRECTION_SOUTH:
            /* resize the width based on the height */
            if (moveresize_client->min_ratio) {
                if (nh * moveresize_client->min_ratio > nw)
                    nw = (gint)(nh * moveresize_client->min_ratio);
            }
            if (moveresize_client->max_ratio) {
                if (nh * moveresize_client->max_ratio < nw)
                    nw = (gint)(nh * moveresize_client->max_ratio);
            }
            break;
        default:
            /* resize the height based on the width */
            if (moveresize_client->min_ratio) {
                if (nh * moveresize_client->min_ratio > nw)
                    nh = (gint)(nw / moveresize_client->min_ratio);
            }
            if (moveresize_client->max_ratio) {
                if (nh * moveresize_client->max_ratio < nw)
                    nh = (gint)(nw / moveresize_client->max_ratio);
            }
            break;
        }

        /* see its actual size (apply aspect ratios) */
        client_try_configure(moveresize_client, &x, &y, &nw, &nh, &lw, &lh,
                             TRUE);
        trydw = nw - ow;
        trydh = nh - oh;
    }

    /* resist_size_* needs the frame size */
    nw += moveresize_client->frame->size.left +
        moveresize_client->frame->size.right;
    nh += moveresize_client->frame->size.top +
        moveresize_client->frame->size.bottom;

    if (keyboard) resist = keydist - 1; /* resist for one key press */
    else resist = config_resist_win;
    resist_size_windows(moveresize_client, resist, &nw, &nh, dir);
    if (!keyboard) resist = config_resist_edge;
    resist_size_monitors(moveresize_client, resist, &nw, &nh, dir);

    nw -= moveresize_client->frame->size.left +
        moveresize_client->frame->size.right;
    nh -= moveresize_client->frame->size.top +
        moveresize_client->frame->size.bottom;

    *dw = nw - ow;
    *dh = nh - oh;

    /* take aspect ratios into account for resistance */
    if (!keyboard &&
        (moveresize_client->max_ratio || moveresize_client->min_ratio))
    {
        if (*dh != trydh) { /* got resisted */
            /* resize the width based on the height */
            if (moveresize_client->min_ratio) {
                if (nh * moveresize_client->min_ratio > nw)
                    nw = (gint)(nh * moveresize_client->min_ratio);
            }
            if (moveresize_client->max_ratio) {
                if (nh * moveresize_client->max_ratio < nw)
                    nw = (gint)(nh * moveresize_client->max_ratio);
            }
        }
        if (*dw != trydw) { /* got resisted */
            /* resize the height based on the width */
            if (moveresize_client->min_ratio) {
                if (nh * moveresize_client->min_ratio > nw)
                    nh = (gint)(nw / moveresize_client->min_ratio);
            }
            if (moveresize_client->max_ratio) {
                if (nh * moveresize_client->max_ratio < nw)
                    nh = (gint)(nw / moveresize_client->max_ratio);
            }
        }
    }

    /* make sure it's all valid */
    client_try_configure(moveresize_client, &x, &y, &nw, &nh, &lw, &lh, TRUE);

    *dw = nw - ow;
    *dh = nh - oh;
}

static void edge_warp_move_ptr(void)
{
    gint x, y;
    const Rect* a;

    screen_pointer_pos(&x, &y);
    a = screen_physical_area_all_monitors();

    switch (edge_warp_dir) {
    case OB_DIRECTION_NORTH:
        y = a->height - 1;
        break;
    case OB_DIRECTION_EAST:
        x = a->x;
        break;
    case OB_DIRECTION_SOUTH:
        y = a->y;
        break;
    case OB_DIRECTION_WEST:
        x = a->width - 1;
        break;
    default:
        g_assert_not_reached();
    }

    XWarpPointer(obt_display, 0, obt_root(ob_screen), 0, 0, 0, 0, x, y);
}

static gboolean edge_warp_delay_func(gpointer data)
{
    guint d;

    /* only fire every second time. so it's fast the first time, but slower
       after that */
    if (edge_warp_odd) {
        d = screen_find_desktop(screen_desktop, edge_warp_dir, TRUE, FALSE);
        if (d != screen_desktop) {
            if (config_mouse_screenedgewarp) edge_warp_move_ptr();
            screen_set_desktop(d, TRUE);
        }
    }
    edge_warp_odd = !edge_warp_odd;

    return TRUE; /* do repeat ! */
}

static void do_edge_warp(gint x, gint y)
{
    guint i;
    ObDirection dir;

    if (!config_mouse_screenedgetime) return;

    dir = -1;

    for (i = 0; i < screen_num_monitors; ++i) {
        const Rect *a = screen_physical_area_monitor(i);

        if (!RECT_CONTAINS(*a, x, y))
            continue;

        if (x == RECT_LEFT(*a)) dir = OB_DIRECTION_WEST;
        if (x == RECT_RIGHT(*a)) dir = OB_DIRECTION_EAST;
        if (y == RECT_TOP(*a)) dir = OB_DIRECTION_NORTH;
        if (y == RECT_BOTTOM(*a)) dir = OB_DIRECTION_SOUTH;

        /* try check for xinerama boundaries */
        if ((x + 1 == RECT_LEFT(*a) || x - 1 == RECT_RIGHT(*a)) &&
            (dir == OB_DIRECTION_WEST || dir == OB_DIRECTION_EAST))
        {
            dir = -1;
        }
        if ((y + 1 == RECT_TOP(*a) || y - 1 == RECT_BOTTOM(*a)) &&
            (dir == OB_DIRECTION_NORTH || dir == OB_DIRECTION_SOUTH))
        {
            dir = -1;
        }
    }

    if (dir != edge_warp_dir) {
        cancel_edge_warp();
        if (dir != (ObDirection)-1) {
            edge_warp_odd = TRUE; /* switch on the first timeout */
            edge_warp_timer = g_timeout_add(config_mouse_screenedgetime,
                                            edge_warp_delay_func, NULL);
        }
        edge_warp_dir = dir;
    }
}

static void cancel_edge_warp(void)
{
    if (edge_warp_timer) g_source_remove(edge_warp_timer);
    edge_warp_timer = 0;
}

static void move_with_keys(KeySym sym, guint state)
{
    gint dx = 0, dy = 0, ox = cur_x, oy = cur_y;
    gint opx, px, opy, py;
    gint dist = 0;

    /* shift means jump to edge */
    if (state & obt_keyboard_modkey_to_modmask(OBT_KEYBOARD_MODKEY_SHIFT))
    {
        gint x, y;
        ObDirection dir;

        if (sym == XK_Right)
            dir = OB_DIRECTION_EAST;
        else if (sym == XK_Left)
            dir = OB_DIRECTION_WEST;
        else if (sym == XK_Down)
            dir = OB_DIRECTION_SOUTH;
        else /* sym == XK_Up */
            dir = OB_DIRECTION_NORTH;

        client_find_move_directional(moveresize_client, dir, &x, &y);
        dx = x - moveresize_client->area.x;
        dy = y - moveresize_client->area.y;
    } else {
        /* control means fine grained */
        if (state &
            obt_keyboard_modkey_to_modmask(OBT_KEYBOARD_MODKEY_CONTROL))
        {
            dist = 1;
        }
        else
            dist = KEY_DIST;

        if (sym == XK_Right)
            dx = dist;
        else if (sym == XK_Left)
            dx = -dist;
        else if (sym == XK_Down)
            dy = dist;
        else /* if (sym == XK_Up) */
            dy = -dist;
    }

    screen_pointer_pos(&opx, &opy);
    XWarpPointer(obt_display, None, None, 0, 0, 0, 0, dx, dy);
    /* steal the motion events this causes */
    XSync(obt_display, FALSE);
    {
        XEvent ce;
        while (xqueue_remove_local(&ce, xqueue_match_type,
                                   GINT_TO_POINTER(MotionNotify)));
    }
    screen_pointer_pos(&px, &py);

    cur_x += dx;
    cur_y += dy;
    do_move(TRUE, dist);

    /* because the cursor moves even though the window does
       not nessesarily (resistance), this adjusts where the curor
       thinks it started so that it keeps up with where the window
       actually is */
    start_x += (px - opx) - (cur_x - ox);
    start_y += (py - opy) - (cur_y - oy);
}

static void resize_with_keys(KeySym sym, guint state)
{
    gint dw = 0, dh = 0, pdx = 0, pdy = 0, opx, opy, px, py;
    gint resist = 0;
    ObDirection dir;

    /* pick the edge if it needs to move */
    if (sym == XK_Right) {
        dir = OB_DIRECTION_EAST;
        if (key_resize_edge != OB_DIRECTION_WEST &&
            key_resize_edge != OB_DIRECTION_EAST)
        {
            key_resize_edge = OB_DIRECTION_EAST;
            return;
        }
    } else if (sym == XK_Left) {
        dir = OB_DIRECTION_WEST;
        if (key_resize_edge != OB_DIRECTION_WEST &&
            key_resize_edge != OB_DIRECTION_EAST)
        {
            key_resize_edge = OB_DIRECTION_WEST;
            return;
        }
    } else if (sym == XK_Up) {
        dir = OB_DIRECTION_NORTH;
        if (key_resize_edge != OB_DIRECTION_NORTH &&
            key_resize_edge != OB_DIRECTION_SOUTH)
        {
            key_resize_edge = OB_DIRECTION_NORTH;
            return;
        }
    } else /* if (sym == XK_Down) */ {
        dir = OB_DIRECTION_SOUTH;
        if (key_resize_edge != OB_DIRECTION_NORTH &&
            key_resize_edge != OB_DIRECTION_SOUTH)
        {
            key_resize_edge = OB_DIRECTION_SOUTH;
            return;
        }
    }

    /* shift means jump to edge */
    if (state & obt_keyboard_modkey_to_modmask(OBT_KEYBOARD_MODKEY_SHIFT))
    {
        gint x, y, w, h;

        if (sym == XK_Right)
            dir = OB_DIRECTION_EAST;
        else if (sym == XK_Left)
            dir = OB_DIRECTION_WEST;
        else if (sym == XK_Down)
            dir = OB_DIRECTION_SOUTH;
        else /* if (sym == XK_Up)) */
            dir = OB_DIRECTION_NORTH;

        ObClientDirectionalResizeType resize_type =
            key_resize_edge == dir ? CLIENT_RESIZE_GROW
                                   : CLIENT_RESIZE_SHRINK;

        client_find_resize_directional(moveresize_client,
                                       key_resize_edge,
                                       resize_type,
                                       &x, &y, &w, &h);
        dw = w - moveresize_client->area.width;
        dh = h - moveresize_client->area.height;
    } else {
        gint distw, disth;

        /* control means fine grained */
        if (moveresize_client->size_inc.width > 1) {
            distw = moveresize_client->size_inc.width;
            resist = 1;
        }
        else if (state &
                 obt_keyboard_modkey_to_modmask(OBT_KEYBOARD_MODKEY_CONTROL))
        {
            distw = 1;
            resist = 1;
        }
        else {
            distw = KEY_DIST;
            resist = KEY_DIST;
        }
        if (moveresize_client->size_inc.height > 1) {
            disth = moveresize_client->size_inc.height;
            resist = 1;
        }
        else if (state &
                 obt_keyboard_modkey_to_modmask(OBT_KEYBOARD_MODKEY_CONTROL))
        {
            disth = 1;
            resist = 1;
        }
        else {
            disth = KEY_DIST;
            resist = KEY_DIST;
        }

        if (key_resize_edge == OB_DIRECTION_WEST) {
            if (dir == OB_DIRECTION_WEST)
                dw = distw;
            else
                dw = -distw;
        }
        else if (key_resize_edge == OB_DIRECTION_EAST) {
            if (dir == OB_DIRECTION_EAST)
                dw = distw;
            else
                dw = -distw;
        }
        else if (key_resize_edge == OB_DIRECTION_NORTH) {
            if (dir == OB_DIRECTION_NORTH)
                dh = disth;
            else
                dh = -disth;
        }
        else /*if (key_resize_edge == OB_DIRECTION_SOUTH)*/ {
            if (dir == OB_DIRECTION_SOUTH)
                dh = disth;
            else
                dh = -disth;
        }
    }

    if (moveresize_client->max_horz &&
        (key_resize_edge == OB_DIRECTION_WEST ||
         key_resize_edge == OB_DIRECTION_EAST))
    {
        /* unmax horz */
        was_max_horz = TRUE;
        pre_max_area.x = moveresize_client->pre_max_area.x;
        pre_max_area.width = moveresize_client->pre_max_area.width;

        moveresize_client->pre_max_area.x = cur_x;
        moveresize_client->pre_max_area.width = cur_w;
        client_maximize(moveresize_client, FALSE, 1);
    }
    else if (moveresize_client->max_vert &&
             (key_resize_edge == OB_DIRECTION_NORTH ||
              key_resize_edge == OB_DIRECTION_SOUTH))
    {
        /* unmax vert */
        was_max_vert = TRUE;
        pre_max_area.y = moveresize_client->pre_max_area.y;
        pre_max_area.height = moveresize_client->pre_max_area.height;

        moveresize_client->pre_max_area.y = cur_y;
        moveresize_client->pre_max_area.height = cur_h;
        client_maximize(moveresize_client, FALSE, 2);
    }

    calc_resize(TRUE, resist, &dw, &dh, dir);
    if (key_resize_edge == OB_DIRECTION_WEST)
        cur_x -= dw;
    else if (key_resize_edge == OB_DIRECTION_NORTH)
        cur_y -= dh;
    cur_w += dw;
    cur_h += dh;

    /* how to move the pointer to keep up with the change */
    if (key_resize_edge == OB_DIRECTION_WEST)
        pdx = -dw;
    else if (key_resize_edge == OB_DIRECTION_EAST)
        pdx = dw;
    else if (key_resize_edge == OB_DIRECTION_NORTH)
        pdy = -dh;
    else if (key_resize_edge == OB_DIRECTION_SOUTH)
        pdy = dh;

    screen_pointer_pos(&opx, &opy);
    XWarpPointer(obt_display, None, None, 0, 0, 0, 0, pdx, pdy);
    /* steal the motion events this causes */
    XSync(obt_display, FALSE);
    {
        XEvent ce;
        while (xqueue_remove_local(&ce, xqueue_match_type,
                                   GINT_TO_POINTER(MotionNotify)));
    }
    screen_pointer_pos(&px, &py);

    do_resize();

    /* because the cursor moves even though the window does
       not nessesarily (resistance), this adjusts where the cursor
       thinks it started so that it keeps up with where the window
       actually is */
    start_x += (px - opx) - dw;
    start_y += (py - opy) - dh;

}

gboolean moveresize_event(XEvent *e)
{
    gboolean used = FALSE;

    if (!moveresize_in_progress) return FALSE;

    if (e->type == ButtonPress) {
        if (!button) {
            start_x = e->xbutton.x_root;
            start_y = e->xbutton.y_root;
            button = e->xbutton.button; /* this will end it now */
        }
        used = e->xbutton.button == button;
    } else if (e->type == ButtonRelease) {
        if (!button || e->xbutton.button == button) {
            moveresize_end(FALSE);
            used = TRUE;
        }
    } else if (e->type == MotionNotify) {
        if (moving) {
            cur_x = start_cx + e->xmotion.x_root - start_x;
            cur_y = start_cy + e->xmotion.y_root - start_y;
            do_move(FALSE, 0);
            do_edge_warp(e->xmotion.x_root, e->xmotion.y_root);
        } else {
            gint dw, dh;
            ObDirection dir;

            if (corner == OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_TOPLEFT)) {
                dw = -(e->xmotion.x_root - start_x);
                dh = -(e->xmotion.y_root - start_y);
                dir = OB_DIRECTION_NORTHWEST;
            } else if (corner == OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_TOP)) {
                dw = 0;
                dh = -(e->xmotion.y_root - start_y);
                dir = OB_DIRECTION_NORTH;
            } else if (corner ==
                       OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_TOPRIGHT)) {
                dw = (e->xmotion.x_root - start_x);
                dh = -(e->xmotion.y_root - start_y);
                dir = OB_DIRECTION_NORTHEAST;
            } else if (corner == OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_RIGHT)) {
                dw = (e->xmotion.x_root - start_x);
                dh = 0;
                dir = OB_DIRECTION_EAST;
            } else if (corner ==
                       OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT)) {
                dw = (e->xmotion.x_root - start_x);
                dh = (e->xmotion.y_root - start_y);
                dir = OB_DIRECTION_SOUTHEAST;
            } else if (corner == OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_BOTTOM))
            {
                dw = 0;
                dh = (e->xmotion.y_root - start_y);
                dir = OB_DIRECTION_SOUTH;
            } else if (corner ==
                       OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT)) {
                dw = -(e->xmotion.x_root - start_x);
                dh = (e->xmotion.y_root - start_y);
                dir = OB_DIRECTION_SOUTHWEST;
            } else if (corner == OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_LEFT)) {
                dw = -(e->xmotion.x_root - start_x);
                dh = 0;
                dir = OB_DIRECTION_WEST;
            } else if (corner ==
                       OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_KEYBOARD)) {
                dw = (e->xmotion.x_root - start_x);
                dh = (e->xmotion.y_root - start_y);
                dir = OB_DIRECTION_SOUTHEAST;
            } else
                g_assert_not_reached();

            /* override the client's max state if desired */
            if (ABS(dw) >= config_resist_edge) {
                if (moveresize_client->max_horz) {
                    /* unmax horz */
                    was_max_horz = TRUE;
                    pre_max_area.x = moveresize_client->pre_max_area.x;
                    pre_max_area.width = moveresize_client->pre_max_area.width;

                    moveresize_client->pre_max_area.x = cur_x;
                    moveresize_client->pre_max_area.width = cur_w;
                    client_maximize(moveresize_client, FALSE, 1);
                }
            }
            else if (was_max_horz && !moveresize_client->max_horz) {
                /* remax horz and put the premax back */
                client_maximize(moveresize_client, TRUE, 1);
                moveresize_client->pre_max_area.x = pre_max_area.x;
                moveresize_client->pre_max_area.width = pre_max_area.width;
            }

            if (ABS(dh) >= config_resist_edge) {
                if (moveresize_client->max_vert) {
                    /* unmax vert */
                    was_max_vert = TRUE;
                    pre_max_area.y = moveresize_client->pre_max_area.y;
                    pre_max_area.height =
                        moveresize_client->pre_max_area.height;

                    moveresize_client->pre_max_area.y = cur_y;
                    moveresize_client->pre_max_area.height = cur_h;
                    client_maximize(moveresize_client, FALSE, 2);
                }
            }
            else if (was_max_vert && !moveresize_client->max_vert) {
                /* remax vert and put the premax back */
                client_maximize(moveresize_client, TRUE, 2);
                moveresize_client->pre_max_area.y = pre_max_area.y;
                moveresize_client->pre_max_area.height = pre_max_area.height;
            }

            dw -= cur_w - start_cw;
            dh -= cur_h - start_ch;

            calc_resize(FALSE, 0, &dw, &dh, dir);
            cur_w += dw;
            cur_h += dh;

            if (corner == OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_TOPLEFT) ||
                corner == OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_LEFT) ||
                corner == OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT))
            {
                cur_x -= dw;
            }
            if (corner == OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_TOPLEFT) ||
                corner == OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_TOP) ||
                corner == OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_TOPRIGHT))
            {
                cur_y -= dh;
            }

            do_resize();
        }
        used = TRUE;
    } else if (e->type == KeyPress) {
        KeySym sym = obt_keyboard_keypress_to_keysym(e);

        if (sym == XK_Escape) {
            moveresize_end(TRUE);
            used = TRUE;
        } else if (sym == XK_Return || sym == XK_KP_Enter) {
            moveresize_end(FALSE);
            used = TRUE;
        } else if (sym == XK_Right || sym == XK_Left ||
                   sym == XK_Up || sym == XK_Down)
        {
            if (corner == OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_KEYBOARD)) {
                resize_with_keys(sym, e->xkey.state);
                used = TRUE;
            } else if (corner ==
                       OBT_PROP_ATOM(NET_WM_MOVERESIZE_MOVE_KEYBOARD))
            {
                move_with_keys(sym, e->xkey.state);
                used = TRUE;
            }
        }
    }
#ifdef SYNC
    else if (e->type == obt_display_extension_sync_basep + XSyncAlarmNotify)
    {
        waiting_for_sync = 0; /* we got our sync... */
        do_resize(); /* ...so try resize if there is more change pending */
        used = TRUE;
    }
#endif

    if (used && moveresize_client == focus_client)
        event_update_user_time();

    return used;
}
