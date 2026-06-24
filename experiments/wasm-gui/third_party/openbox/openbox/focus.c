/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   focus.c for the Openbox window manager
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

#include "debug.h"
#include "event.h"
#include "openbox.h"
#include "grab.h"
#include "client.h"
#include "config.h"
#include "group.h"
#include "focus_cycle.h"
#include "screen.h"
#include "keyboard.h"
#include "focus.h"
#include "stacking.h"
#include "obt/prop.h"

#include <X11/Xlib.h>
#include <glib.h>

#define FOCUS_INDICATOR_WIDTH 6

ObClient *focus_client = NULL;
GList *focus_order = NULL;

void focus_startup(gboolean reconfig)
{
    if (reconfig) return;

    /* start with nothing focused */
    focus_nothing();
}

void focus_shutdown(gboolean reconfig)
{
    if (reconfig) return;

    /* reset focus to root */
    XSetInputFocus(obt_display, PointerRoot, RevertToNone, CurrentTime);
}

static void push_to_top(ObClient *client)
{
    ObClient *p;

    /* if it is modal for a single window, then put that window at the top
       of the focus order first, so it will be right after ours. the same is
       done with stacking */
    if (client->modal && (p = client_direct_parent(client)))
        push_to_top(p);

    focus_order = g_list_remove(focus_order, client);
    focus_order = g_list_prepend(focus_order, client);
}

void focus_set_client(ObClient *client)
{
    Window active;

    ob_debug_type(OB_DEBUG_FOCUS,
                  "focus_set_client 0x%lx", client ? client->window : 0);

    if (focus_client == client)
        return;

    /* uninstall the old colormap, and install the new one */
    screen_install_colormap(focus_client, FALSE);
    screen_install_colormap(client, TRUE);

    focus_client = client;

    if (client != NULL) {
        /* move to the top of the list */
        push_to_top(client);
        /* remove hiliting from the window when it gets focused */
        client_hilite(client, FALSE);

        /* make sure the focus cycle popup shows things in the right order */
        focus_cycle_reorder();
    }

    /* set the NET_ACTIVE_WINDOW hint, but preserve it on shutdown */
    if (ob_state() != OB_STATE_EXITING) {
        active = client ? client->window : None;
        OBT_PROP_SET32(obt_root(ob_screen), NET_ACTIVE_WINDOW, WINDOW, active);
    }

    /* when focus is moved to a new window, the last_user_time timestamp would
       no longer be valid, as it applies for the focused window */
    event_reset_user_time();
}

static ObClient* focus_fallback_target(gboolean allow_refocus,
                                       gboolean allow_pointer,
                                       gboolean allow_omnipresent,
                                       ObClient *old)
{
    GList *it;
    ObClient *c;

    ob_debug_type(OB_DEBUG_FOCUS, "trying pointer stuff");
    if (allow_pointer && config_focus_follow)
        if ((c = client_under_pointer()) &&
            (allow_refocus || client_focus_target(c) != old) &&
            (client_normal(c) &&
             client_focus(c)))
        {
            ob_debug_type(OB_DEBUG_FOCUS, "found in pointer stuff");
            return c;
        }

    ob_debug_type(OB_DEBUG_FOCUS, "trying the focus order");
    for (it = focus_order; it; it = g_list_next(it)) {
        c = it->data;
        /* fallback focus to a window if:
           1. it is on the current desktop. this ignores omnipresent
           windows, which are problematic in their own rite, unless they are
           specifically allowed
           2. it is a valid auto-focus target
           3. it is not shaded
        */
        if ((allow_omnipresent || c->desktop == screen_desktop) &&
            focus_valid_target(c, screen_desktop,
                               TRUE, FALSE, FALSE, TRUE, FALSE, FALSE,
                               FALSE) &&
            !c->shaded &&
            (allow_refocus || client_focus_target(c) != old) &&
            client_focus(c))
        {
            ob_debug_type(OB_DEBUG_FOCUS, "found in focus order");
            return c;
        }
    }

    ob_debug_type(OB_DEBUG_FOCUS, "trying a desktop window");
    for (it = focus_order; it; it = g_list_next(it)) {
        c = it->data;
        /* fallback focus to a window if:
           1. it is on the current desktop. this ignores omnipresent
           windows, which are problematic in their own rite.
           2. it is a normal type window, don't fall back onto a dock or
           a splashscreen or a desktop window (save the desktop as a
           backup fallback though)
        */
        if (focus_valid_target(c, screen_desktop,
                               TRUE, FALSE, FALSE, TRUE, FALSE, TRUE, FALSE) &&
            (allow_refocus || client_focus_target(c) != old) &&
            client_focus(c))
        {
            ob_debug_type(OB_DEBUG_FOCUS, "found a desktop window");
            return c;
        }
    }

    return NULL;
}

ObClient* focus_fallback(gboolean allow_refocus, gboolean allow_pointer,
                         gboolean allow_omnipresent, gboolean focus_lost)
{
    ObClient *new;
    ObClient *old = focus_client;

    /* unfocus any focused clients.. they can be focused by Pointer events
       and such, and then when we try focus them, we won't get a FocusIn
       event at all for them. */
    if (focus_lost)
        focus_nothing();

    new = focus_fallback_target(allow_refocus, allow_pointer,
                                allow_omnipresent, old);
    /* get what was really focused */
    if (new) new = client_focus_target(new);

    return new;
}

void focus_nothing(void)
{
    /* nothing is focused, update the colormap and _the root property_ */
    focus_set_client(NULL);

    /* when nothing will be focused, send focus to the backup target */
    XSetInputFocus(obt_display, screen_support_win, RevertToPointerRoot,
                   event_time());
}

void focus_order_add_new(ObClient *c)
{
    if (c->iconic)
        focus_order_to_top(c);
    else {
        g_assert(!g_list_find(focus_order, c));
        /* if there are only iconic windows, put this above them in the order,
           but if there are not, then put it under the currently focused one */
        if (focus_order && ((ObClient*)focus_order->data)->iconic)
            focus_order = g_list_insert(focus_order, c, 0);
        else
            focus_order = g_list_insert(focus_order, c, 1);
    }

    focus_cycle_addremove(c, TRUE);
}

void focus_order_remove(ObClient *c)
{
    focus_order = g_list_remove(focus_order, c);

    focus_cycle_addremove(c, TRUE);
}

void focus_order_like_new(struct _ObClient *c)
{
    focus_order = g_list_remove(focus_order, c);
    focus_order_add_new(c);
}

void focus_order_to_top(ObClient *c)
{
    focus_order = g_list_remove(focus_order, c);
    if (!c->iconic) {
        focus_order = g_list_prepend(focus_order, c);
    } else {
        GList *it;

        /* insert before first iconic window */
        for (it = focus_order;
             it && !((ObClient*)it->data)->iconic; it = g_list_next(it));
        focus_order = g_list_insert_before(focus_order, it, c);
    }

    focus_cycle_reorder();
}

void focus_order_to_bottom(ObClient *c)
{
    focus_order = g_list_remove(focus_order, c);
    if (c->iconic) {
        focus_order = g_list_append(focus_order, c);
    } else {
        GList *it;

        /* insert before first iconic window */
        for (it = focus_order;
             it && !((ObClient*)it->data)->iconic; it = g_list_next(it));
        focus_order = g_list_insert_before(focus_order, it, c);
    }

    focus_cycle_reorder();
}

ObClient *focus_order_find_first(guint desktop)
{
    GList *it;
    for (it = focus_order; it; it = g_list_next(it)) {
        ObClient *c = it->data;
        if (c->desktop == desktop || c->desktop == DESKTOP_ALL)
            return c;
    }
    return NULL;
}

/*! Returns if a focus target has valid group siblings that can be cycled
  to in its place */
static gboolean focus_target_has_siblings(ObClient *ft,
                                          gboolean iconic_windows,
                                          gboolean all_desktops)

{
    GSList *it;

    if (!ft->group) return FALSE;

    for (it = ft->group->members; it; it = g_slist_next(it)) {
        ObClient *c = it->data;
        /* check that it's not a helper window to avoid infinite recursion */
        if (c != ft && c->type == OB_CLIENT_TYPE_NORMAL &&
            focus_valid_target(c, screen_desktop,
                               TRUE, iconic_windows, all_desktops,
                               TRUE, FALSE, FALSE, FALSE))
        {
            return TRUE;
        }
    }
    return FALSE;
}

gboolean focus_valid_target(ObClient *ft,
                            guint    desktop,
                            gboolean helper_windows,
                            gboolean iconic_windows,
                            gboolean all_desktops,
                            gboolean nonhilite_windows,
                            gboolean dock_windows,
                            gboolean desktop_windows,
                            gboolean user_request)
{
    /* NOTE: if any of these things change on a client, then they should call
       focus_cycle_addremove() to make sure the client is not shown/hidden
       when it should not be */

    gboolean ok = FALSE;

    /* see if the window is still managed or is going away */
    if (!ft->managed) return FALSE;

    /* it's on this desktop unless you want all desktops.

       do this check first because it will usually filter out the most
       windows */
    ok = (all_desktops || ft->desktop == desktop ||
          ft->desktop == DESKTOP_ALL);

    /* if we only include hilited windows, check if the window is */
    ok = ok && (nonhilite_windows || ft->demands_attention);

    /* the window can receive focus somehow */
    ok = ok && (ft->can_focus || ft->focus_notify);

    /* the window is not iconic, or we're allowed to go to iconic ones */
    ok = ok && (iconic_windows || !ft->iconic);

    /* it's the right type of window */
    if (dock_windows || desktop_windows)
        ok = ok && ((dock_windows && ft->type == OB_CLIENT_TYPE_DOCK) ||
                    (desktop_windows && ft->type == OB_CLIENT_TYPE_DESKTOP));
    /* modal windows are important and can always get focus if they are
       visible and stuff, so don't change 'ok' based on their type */
    else if (!ft->modal)
        /* normal non-helper windows are valid targets */
        ok = ok &&
            ((client_normal(ft) && !client_helper(ft))
             ||
             /* helper windows are valid targets if... */
             (client_helper(ft) &&
              /* ...a window in its group already has focus and we want to
                 include helper windows ... */
              ((focus_client && ft->group == focus_client->group &&
                helper_windows) ||
               /* ... or if there are no other windows in its group
                  that can be focused instead */
               !focus_target_has_siblings(ft, iconic_windows, all_desktops))));

    /* it's not set to skip the taskbar (but this is overridden if the
       window is modal or if the user asked for this window to be focused,
       or if the window is iconified (and does not have any parents with
       which to uniconify it), and it is not used for windows which are
       hilited, or dialog windows as these need user interaction and should
       not be long-lasting windows */
    ok = ok && (!ft->skip_taskbar ||
                (ft->modal || user_request ||
                 (ft->iconic && !ft->parents) ||
                 ft->demands_attention ||
                 ft->type == OB_CLIENT_TYPE_DIALOG));

    /* it's not going to just send focus off somewhere else (modal window),
       unless that modal window is not one of our valid targets, then let
       you choose this window and bring the modal one here */
    {
        ObClient *cft = client_focus_target(ft);
        ok = ok && (ft == cft || !focus_valid_target(cft,
                                                     screen_desktop,
                                                     TRUE,
                                                     iconic_windows,
                                                     all_desktops,
                                                     nonhilite_windows,
                                                     dock_windows,
                                                     desktop_windows,
                                                     FALSE));
    }

    return ok;
}
