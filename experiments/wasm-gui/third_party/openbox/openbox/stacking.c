/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   stacking.c for the Openbox window manager
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

#include "openbox.h"
#include "screen.h"
#include "focus.h"
#include "client.h"
#include "group.h"
#include "frame.h"
#include "window.h"
#include "event.h"
#include "debug.h"
#include "dock.h"
#include "config.h"
#include "obt/prop.h"

GList  *stacking_list = NULL;
GList  *stacking_list_tail = NULL;
/*! When true, stacking changes will not be reflected on the screen.  This is
  to freeze the on-screen stacking order while a window is being temporarily
  raised during focus cycling */
static gboolean pause_changes = FALSE;

void stacking_set_list(void)
{
    Window *windows = NULL;
    GList *it;
    guint i = 0;

    /* on shutdown, don't update the properties, so that we can read it back
       in on startup and re-stack the windows as they were before we shut down
    */
    if (ob_state() == OB_STATE_EXITING) return;

    /* create an array of the window ids (from bottom to top,
       reverse order!) */
    if (stacking_list) {
        windows = g_new(Window, g_list_length(stacking_list));
        for (it = g_list_last(stacking_list); it; it = g_list_previous(it)) {
            if (WINDOW_IS_CLIENT(it->data))
                windows[i++] = WINDOW_AS_CLIENT(it->data)->window;
        }
    }

    OBT_PROP_SETA32(obt_root(ob_screen), NET_CLIENT_LIST_STACKING, WINDOW,
                    (gulong*)windows, i);

    g_free(windows);
}

static void do_restack(GList *wins, GList *before)
{
    GList *it;
    Window *win;
    gint i;

#ifdef DEBUG
    GList *next;

    g_assert(wins);
    /* pls only restack stuff in the same layer at a time */
    for (it = wins; it; it = next) {
        next = g_list_next(it);
        if (!next) break;
        g_assert (window_layer(it->data) == window_layer(next->data));
    }
    if (before)
        g_assert(window_layer(it->data) >= window_layer(before->data));
#endif

    win = g_new(Window, g_list_length(wins) + 1);

    if (before == stacking_list)
        win[0] = screen_support_win;
    else if (!before)
        win[0] = window_top(g_list_last(stacking_list)->data);
    else
        win[0] = window_top(g_list_previous(before)->data);

    for (i = 1, it = wins; it; ++i, it = g_list_next(it)) {
        win[i] = window_top(it->data);
        g_assert(win[i] != None); /* better not call stacking shit before
                                     setting your top level window value */
        stacking_list = g_list_insert_before(stacking_list, before, it->data);
    }

#ifdef DEBUG
    /* some debug checking of the stacking list's order */
    for (it = stacking_list; ; it = next) {
        next = g_list_next(it);
        if (!next) break;
        g_assert(window_layer(it->data) >= window_layer(next->data));
    }
#endif

    if (!pause_changes)
        XRestackWindows(obt_display, win, i);
    g_free(win);

    stacking_set_list();
}

void stacking_temp_raise(ObWindow *window)
{
    Window win[2];
    GList *it;
    gulong start;

    /* don't use this for internal windows..! it would lower them.. */
    g_assert(window_layer(window) < OB_STACKING_LAYER_INTERNAL);

    /* find the window to drop it underneath */
    win[0] = screen_support_win;
    for (it = stacking_list; it; it = g_list_next(it)) {
        ObWindow *w = it->data;
        if (window_layer(w) >= OB_STACKING_LAYER_INTERNAL)
            win[0] = window_top(w);
        else
            break;
    }

    win[1] = window_top(window);
    start = event_start_ignore_all_enters();
    XRestackWindows(obt_display, win, 2);
    event_end_ignore_all_enters(start);

    pause_changes = TRUE;
}

void stacking_restore(void)
{
    Window *win;
    GList *it;
    gint i;
    gulong start;

    win = g_new(Window, g_list_length(stacking_list) + 1);
    win[0] = screen_support_win;
    for (i = 1, it = stacking_list; it; ++i, it = g_list_next(it))
        win[i] = window_top(it->data);
    start = event_start_ignore_all_enters();
    XRestackWindows(obt_display, win, i);
    event_end_ignore_all_enters(start);
    g_free(win);

    pause_changes = FALSE;
}

static void do_raise(GList *wins)
{
    GList *it;
    GList *layer[OB_NUM_STACKING_LAYERS] = {NULL};
    gint i;

    for (it = wins; it; it = g_list_next(it)) {
        ObStackingLayer l;

        l = window_layer(it->data);
        layer[l] = g_list_append(layer[l], it->data);
    }

    it = stacking_list;
    for (i = OB_NUM_STACKING_LAYERS - 1; i >= 0; --i) {
        if (layer[i]) {
            for (; it; it = g_list_next(it)) {
                /* look for the top of the layer */
                if (window_layer(it->data) <= (ObStackingLayer) i)
                    break;
            }
            do_restack(layer[i], it);
            g_list_free(layer[i]);
        }
    }
}

static void do_lower(GList *wins)
{
    GList *it;
    GList *layer[OB_NUM_STACKING_LAYERS] = {NULL};
    gint i;

    for (it = wins; it; it = g_list_next(it)) {
        ObStackingLayer l;

        l = window_layer(it->data);
        layer[l] = g_list_append(layer[l], it->data);
    }

    it = stacking_list;
    for (i = OB_NUM_STACKING_LAYERS - 1; i >= 0; --i) {
        if (layer[i]) {
            for (; it; it = g_list_next(it)) {
                /* look for the top of the next layer down */
                if (window_layer(it->data) < (ObStackingLayer) i)
                    break;
            }
            do_restack(layer[i], it);
            g_list_free(layer[i]);
        }
    }
}

static void restack_windows(ObClient *selected, gboolean raise)
{
    GList *it, *last, *below, *above, *next;
    GList *wins = NULL;

    GList *group_helpers = NULL;
    GList *group_modals = NULL;
    GList *group_trans = NULL;
    GList *modals = NULL;
    GList *trans = NULL;

    if (raise) {
        ObClient *p;

        /* if a window is modal for another single window, then raise it to the
           top too, the same is done with the focus order */
        while (selected->modal && (p = client_direct_parent(selected)))
            selected = p;
    }

    /* remove first so we can't run into ourself */
    it = g_list_find(stacking_list, selected);
    g_assert(it);
    stacking_list = g_list_delete_link(stacking_list, it);

    /* go from the bottom of the stacking list up. don't move any other windows
       when lowering, we call this for each window independently */
    if (raise) {
        for (it = g_list_last(stacking_list); it; it = next) {
            next = g_list_previous(it);

            if (WINDOW_IS_CLIENT(it->data)) {
                ObClient *ch = it->data;

                /* only move windows in the same stacking layer */
                if (ch->layer == selected->layer &&
                    /* looking for windows that are transients, and so would
                       remain above the selected window */
                    client_search_transient(selected, ch))
                {
                    if (client_is_direct_child(selected, ch)) {
                        if (ch->modal)
                            modals = g_list_prepend(modals, ch);
                        else
                            trans = g_list_prepend(trans, ch);
                    }
                    else if (client_helper(ch)) {
                        if (selected->transient) {
                            /* helpers do not stay above transient windows */
                            continue;
                        }
                        group_helpers = g_list_prepend(group_helpers, ch);
                    }
                    else {
                        if (ch->modal)
                            group_modals = g_list_prepend(group_modals, ch);
                        else
                            group_trans = g_list_prepend(group_trans, ch);
                    }
                    stacking_list = g_list_delete_link(stacking_list, it);
                }
            }
        }
    }

    /* put modals above other direct transients */
    wins = g_list_concat(modals, trans);

    /* put helpers below direct transients */
    wins = g_list_concat(wins, group_helpers);

    /* put the selected window right below these children */
    wins = g_list_append(wins, selected);

    /* if selected window is transient for group then raise it above others */
    if (selected->transient_for_group) {
        /* if it's modal, raise it above those also */
        if (selected->modal) {
            wins = g_list_concat(wins, group_modals);
            group_modals = NULL;
        }
        wins = g_list_concat(wins, group_trans);
        group_trans = NULL;
    }

    /* find where to put the selected window, start from bottom of list,
       this is the window below everything we are re-adding to the list */
    last = NULL;
    for (it = g_list_last(stacking_list); it; it = g_list_previous(it))
    {
        if (window_layer(it->data) < selected->layer) {
            last = it;
            continue;
        }
        /* if lowering, stop at the beginning of the layer */
        if (!raise)
            break;
        /* if raising, stop at the end of the layer */
        if (window_layer(it->data) > selected->layer)
            break;

        last = it;
    }

    /* save this position in the stacking list */
    below = last;

    /* find where to put the group transients, start from the top of list */
    for (it = stacking_list; it; it = g_list_next(it)) {
        /* skip past higher layers */
        if (window_layer(it->data) > selected->layer)
            continue;
        /* if we reach the end of the layer (how?) then don't go further */
        if (window_layer(it->data) < selected->layer)
            break;
        /* stop when we reach the first window in the group */
        if (WINDOW_IS_CLIENT(it->data)) {
            ObClient *c = it->data;
            if (c->group == selected->group)
                break;
        }
        /* if we don't hit any other group members, stop here because this
           is where we are putting the selected window (and its children) */
        if (it == below)
            break;
    }

    /* save this position, this is the top of the group of windows between the
       group transient ones we're restacking and the others up above that we're
       restacking

       we actually want to save 1 position _above_ that, for for loops to work
       nicely, so move back one position in the list while saving it
    */
    above = it ? g_list_previous(it) : g_list_last(stacking_list);

    /* put the windows inside the gap to the other windows we're stacking
       into the restacking list, go from the bottom up so that we can use
       g_list_prepend */
    if (below) it = g_list_previous(below);
    else       it = g_list_last(stacking_list);
    for (; it != above; it = next) {
        next = g_list_previous(it);
        wins = g_list_prepend(wins, it->data);
        stacking_list = g_list_delete_link(stacking_list, it);
    }

    /* group transients go above the rest of the stuff acquired to now */
    wins = g_list_concat(group_trans, wins);
    /* group modals go on the very top */
    wins = g_list_concat(group_modals, wins);

    do_restack(wins, below);
    g_list_free(wins);

    /* lower our parents after us, so they go below us */
    if (!raise && selected->parents) {
        GSList *parents_copy, *sit;
        GSList *reorder = NULL;

        parents_copy = g_slist_copy(selected->parents);

        /* go thru stacking list backwards so we can use g_slist_prepend */
        for (it = g_list_last(stacking_list); it && parents_copy;
             it = g_list_previous(it))
            if ((sit = g_slist_find(parents_copy, it->data))) {
                reorder = g_slist_prepend(reorder, sit->data);
                parents_copy = g_slist_delete_link(parents_copy, sit);
            }
        g_assert(parents_copy == NULL);

        /* call restack for each of these to lower them */
        for (sit = reorder; sit; sit = g_slist_next(sit))
            restack_windows(sit->data, raise);
    }
}

void stacking_raise(ObWindow *window)
{
    if (WINDOW_IS_CLIENT(window)) {
        ObClient *selected;
        selected = WINDOW_AS_CLIENT(window);
        restack_windows(selected, TRUE);
    } else {
        GList *wins;
        wins = g_list_append(NULL, window);
        stacking_list = g_list_remove(stacking_list, window);
        do_raise(wins);
        g_list_free(wins);
    }
    stacking_list_tail = g_list_last(stacking_list);
}

void stacking_lower(ObWindow *window)
{
    if (WINDOW_IS_CLIENT(window)) {
        ObClient *selected;
        selected = WINDOW_AS_CLIENT(window);
        restack_windows(selected, FALSE);
    } else {
        GList *wins;
        wins = g_list_append(NULL, window);
        stacking_list = g_list_remove(stacking_list, window);
        do_lower(wins);
        g_list_free(wins);
    }
    stacking_list_tail = g_list_last(stacking_list);
}

void stacking_below(ObWindow *window, ObWindow *below)
{
    GList *wins, *before;

    if (window_layer(window) != window_layer(below))
        return;

    wins = g_list_append(NULL, window);
    stacking_list = g_list_remove(stacking_list, window);
    before = g_list_next(g_list_find(stacking_list, below));
    do_restack(wins, before);
    g_list_free(wins);
    stacking_list_tail = g_list_last(stacking_list);
}

void stacking_add(ObWindow *win)
{
    g_assert(screen_support_win != None); /* make sure I dont break this in the
                                             future */
    /* don't add windows that are being unmanaged ! */
    if (WINDOW_IS_CLIENT(win)) g_assert(WINDOW_AS_CLIENT(win)->managed);

    stacking_list = g_list_append(stacking_list, win);

    stacking_raise(win);
    /* stacking_list_tail set by stacking_raise() */
}

static GList *find_highest_relative(ObClient *client)
{
    GList *ret = NULL;

    if (client->parents) {
        GList *it;
        GSList *top;

        /* get all top level relatives of this client */
        top = client_search_all_top_parents_layer(client);

        /* go from the top of the stacking order down */
        for (it = stacking_list; !ret && it; it = g_list_next(it)) {
            if (WINDOW_IS_CLIENT(it->data)) {
                ObClient *c = it->data;
                /* only look at windows in the same layer and that are
                   visible */
                if (c->layer == client->layer &&
                    !c->iconic &&
                    (c->desktop == client->desktop ||
                     c->desktop == DESKTOP_ALL ||
                     client->desktop == DESKTOP_ALL))
                {
                    GSList *sit;

                    /* go through each top level parent and see it this window
                       is related to them */
                    for (sit = top; !ret && sit; sit = g_slist_next(sit)) {
                        ObClient *topc = sit->data;

                        /* are they related ? */
                        if (topc == c || client_search_transient(topc, c))
                            ret = it;
                    }
                }
            }
        }
    }
    return ret;
}

void stacking_add_nonintrusive(ObWindow *win)
{
    ObClient *client;
    GList *it_below = NULL; /* this client will be below us */
    GList *it_above;
    GList *wins;

    if (!WINDOW_IS_CLIENT(win)) {
        stacking_add(win); /* no special rules for others */
        return;
    }

    client = WINDOW_AS_CLIENT(win);

    /* don't add windows that are being unmanaged ! */
    g_assert(client->managed);

    /* insert above its highest parent (or its highest child !) */
    it_below = find_highest_relative(client);

    if (!it_below) {
        /* nothing to put it directly above, so try find the focused client
           to put it underneath it */
        if (focus_client && client != focus_client &&
            focus_client->layer == client->layer)
        {
            it_below = g_list_find(stacking_list, focus_client);
            /* this can give NULL, but it means the focused window is on the
               bottom of the stacking order, so go to the bottom in that case,
               below it */
            it_below = g_list_next(it_below);
        }
        else {
            /* There is no window to put this directly above, so put it at the
               top, so you know it is there.

               It used to do this only if the window was focused and lower
               it otherwise.

               We also put it at the top not the bottom to fix a bug with
               fullscreen windows. When focusLast is off and followsMouse is
               on, when you switch desktops, the fullscreen window loses
               focus and goes into its lower layer. If this puts it at the
               bottom then when you come back to the desktop, the window is
               at the bottom and won't get focus back.
            */
            it_below = stacking_list;
        }
    }

    /* make sure it's not in the wrong layer though ! */
    for (; it_below; it_below = g_list_next(it_below)) {
        /* stop when the window is not in a higher layer than the window
           it is going above (it_below) */
        if (client->layer >= window_layer(it_below->data))
            break;
    }
    for (; it_below != stacking_list; it_below = it_above) {
        /* stop when the window is not in a lower layer than the
           window it is going under (it_above) */
        it_above = it_below ?
            g_list_previous(it_below) : g_list_last(stacking_list);
        if (client->layer <= window_layer(it_above->data))
            break;
    }

    wins = g_list_append(NULL, win);
    do_restack(wins, it_below);
    g_list_free(wins);
    stacking_list_tail = g_list_last(stacking_list);
}

/*! Returns TRUE if client is occluded by the sibling. If sibling is NULL it
  tries against all other clients.
*/
static gboolean stacking_occluded(ObClient *client, ObWindow *sibling_win)
{
    GList *it;
    gboolean occluded = FALSE;
    ObClient *sibling = NULL;

    if (sibling_win && WINDOW_IS_CLIENT(sibling_win))
        sibling = WINDOW_AS_CLIENT(sibling_win);

    /* no need for any looping in this case */
    if (sibling && client->layer != sibling->layer)
        return FALSE;

    for (it = g_list_previous(g_list_find(stacking_list, client)); it;
         it = g_list_previous(it))
        if (WINDOW_IS_CLIENT(it->data)) {
            ObClient *c = it->data;
            if (!c->iconic &&
                (c->desktop == DESKTOP_ALL || client->desktop == DESKTOP_ALL ||
                 c->desktop == client->desktop) &&
                !client_search_transient(client, c))
            {
                if (RECT_INTERSECTS_RECT(c->frame->area, client->frame->area))
                {
                    if (sibling != NULL) {
                        if (c == sibling) {
                            occluded = TRUE;
                            break;
                        }
                    }
                    else if (c->layer == client->layer) {
                        occluded = TRUE;
                        break;
                    }
                    else if (c->layer > client->layer)
                        break; /* we past its layer */
                }
            }
        } else if (WINDOW_IS_DOCK(it->data)) {
            ObDock *dock = it->data;
            if (RECT_INTERSECTS_RECT(dock->area, client->frame->area))
            {
                if (sibling_win != NULL) {
                    if (DOCK_AS_WINDOW(dock) == sibling_win) {
                        occluded = TRUE;
                        break;
                    }
                }
                else if (config_dock_layer == client->layer) {
                    occluded = TRUE;
                    break;
                }
            }
        }
    return occluded;
}

/*! Returns TRUE if client occludes the sibling. If sibling is NULL it tries
  against all other clients.
*/
static gboolean stacking_occludes(ObClient *client, ObWindow *sibling_win)
{
    GList *it;
    gboolean occludes = FALSE;
    ObClient *sibling = NULL;

    if (sibling_win && WINDOW_IS_CLIENT(sibling_win))
        sibling = WINDOW_AS_CLIENT(sibling_win);

    /* no need for any looping in this case */
    if (sibling && client->layer != sibling->layer)
        return FALSE;

    for (it = g_list_next(g_list_find(stacking_list, client));
         it; it = g_list_next(it))
        if (WINDOW_IS_CLIENT(it->data)) {
            ObClient *c = it->data;
            if (!c->iconic &&
                (c->desktop == DESKTOP_ALL || client->desktop == DESKTOP_ALL ||
                 c->desktop == client->desktop) &&
                !client_search_transient(c, client))
            {
                if (RECT_INTERSECTS_RECT(c->frame->area, client->frame->area))
                {
                    if (sibling != NULL) {
                        if (c == sibling) {
                            occludes = TRUE;
                            break;
                        }
                    }
                    else if (c->layer == client->layer) {
                        occludes = TRUE;
                        break;
                    }
                    else if (c->layer < client->layer)
                        break; /* we past its layer */
                }
            }
        }
        else if (WINDOW_IS_DOCK(it->data)) {
            ObDock *dock = it->data;
            if (RECT_INTERSECTS_RECT(dock->area, client->frame->area))
            {
                if (sibling_win != NULL) {
                    if (DOCK_AS_WINDOW(dock) == sibling_win) {
                        occludes = TRUE;
                        break;
                    }
                }
                else if (config_dock_layer == client->layer) {
                    occludes = TRUE;
                    break;
                }
            }
        }
    return occludes;
}

gboolean stacking_restack_request(ObClient *client, ObWindow *sibling_win,
                                  gint detail)
{
    gboolean ret = FALSE;

    ObClient *sibling = NULL;

    if (sibling_win && WINDOW_IS_CLIENT(sibling_win))
        sibling = WINDOW_AS_CLIENT(sibling_win);

    if (sibling && ((client->desktop != sibling->desktop &&
                     client->desktop != DESKTOP_ALL &&
                     sibling->desktop != DESKTOP_ALL) ||
                    sibling->iconic))
    {
        ob_debug("Setting restack sibling to NULL, they are not on the same "
                 "desktop or it is iconified");
        sibling = NULL;
    }

    switch (detail) {
    case Below:
        ob_debug("Restack request Below for client %s sibling %s",
                 client->title, sibling ? sibling->title : "(all)");
        /* just lower it */
        stacking_lower(CLIENT_AS_WINDOW(client));
        ret = TRUE;
        break;
    case BottomIf:
        ob_debug("Restack request BottomIf for client %s sibling %s",
                 client->title, sibling ? sibling->title : "(all)");
        /* if this client occludes sibling (or anything if NULL), then
           lower it to the bottom */
        if (stacking_occludes(client, sibling_win)) {
            stacking_lower(CLIENT_AS_WINDOW(client));
            ret = TRUE;
        }
        break;
    case Above:
        ob_debug("Restack request Above for client %s sibling %s",
                 client->title, sibling ? sibling->title : "(all)");
        stacking_raise(CLIENT_AS_WINDOW(client));
        ret = TRUE;
        break;
    case TopIf:
        ob_debug("Restack request TopIf for client %s sibling %s",
                 client->title, sibling ? sibling->title : "(all)");
        if (stacking_occluded(client, sibling_win)) {
            stacking_raise(CLIENT_AS_WINDOW(client));
            ret = TRUE;
        }
        break;
    case Opposite:
        ob_debug("Restack request Opposite for client %s sibling %s",
                 client->title, sibling ? sibling->title : "(all)");
        if (stacking_occluded(client, sibling_win)) {
            stacking_raise(CLIENT_AS_WINDOW(client));
            ret = TRUE;
        }
        else if (stacking_occludes(client, sibling_win)) {
            stacking_lower(CLIENT_AS_WINDOW(client));
            ret = TRUE;
        }
        break;
    }
    return ret;
}
