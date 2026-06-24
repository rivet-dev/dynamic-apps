/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   place.c for the Openbox window manager
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

#include "client.h"
#include "group.h"
#include "screen.h"
#include "frame.h"
#include "focus.h"
#include "config.h"
#include "dock.h"
#include "debug.h"
#include "place_overlap.h"

static Rect *choose_pointer_monitor(ObClient *c)
{
    return screen_area(c->desktop, screen_monitor_pointer(), NULL);
}

/* use the following priority lists for choose_monitor()

   When a window is being placed in the FOREGROUND, use a monitor chosen in
   the following order:
   1. per-app settings
   2. same monitor as parent
   3. primary monitor if placement=PRIMARY
      active monitor if placement=ACTIVE
      pointer monitor if placement=MOUSE
   4. primary monitor
   5. other monitors where the window has group members on the same desktop
   6. other monitors where the window has group members on other desktops
   7. other monitors

   When a window is being placed in the BACKGROUND, use a monitor chosen in the
   following order:
   1. per-app settings
   2. same monitor as parent
   3. other monitors where the window has group members on the same desktop
    3a. primary monitor in this set
    3b. other monitors in this set
   4. other monitors where the window has group members on other desktops
    4a. primary monitor in this set
    4b. other monitors in this set
   5. other monitors
    5a. primary monitor in this set
    5b. other monitors in this set
*/

/*! One for each possible head, used to sort them in order of precedence. */
typedef struct {
    guint monitor;
    guint flags;
} ObPlaceHead;

/*! Flags for ObPlaceHead */
enum {
    HEAD_PARENT = 1 << 0, /* parent's monitor */
    HEAD_PLACED = 1 << 1, /* chosen monitor by placement */
    HEAD_PRIMARY = 1 << 2, /* primary monitor */
    HEAD_GROUP_DESK = 1 << 3, /* has a group member on the same desktop */
    HEAD_GROUP = 1 << 4, /* has a group member on another desktop */
    HEAD_PERAPP = 1 << 5, /* chosen by per-app settings */
};

gint cmp_foreground(const void *a, const void *b)
{
    const ObPlaceHead *h1 = a;
    const ObPlaceHead *h2 = b;
    gint i = 0;

    if (h1->monitor == h2->monitor) return 0;

    if (h1->flags & HEAD_PERAPP) --i;
    if (h2->flags & HEAD_PERAPP) ++i;
    if (i) return i;

    if (h1->flags & HEAD_PARENT) --i;
    if (h2->flags & HEAD_PARENT) ++i;
    if (i) return i;

    if (h1->flags & HEAD_PLACED) --i;
    if (h2->flags & HEAD_PLACED) ++i;
    if (i) return i;

    if (h1->flags & HEAD_PRIMARY) --i;
    if (h2->flags & HEAD_PRIMARY) ++i;
    if (i) return i;

    if (h1->flags & HEAD_GROUP_DESK) --i;
    if (h2->flags & HEAD_GROUP_DESK) ++i;
    if (i) return i;

    if (h1->flags & HEAD_GROUP) --i;
    if (h2->flags & HEAD_GROUP) ++i;
    if (i) return i;

    return h1->monitor - h2->monitor;
}

gint cmp_background(const void *a, const void *b)
{
    const ObPlaceHead *h1 = a;
    const ObPlaceHead *h2 = b;
    gint i = 0;

    if (h1->monitor == h2->monitor) return 0;

    if (h1->flags & HEAD_PERAPP) --i;
    if (h2->flags & HEAD_PERAPP) ++i;
    if (i) return i;

    if (h1->flags & HEAD_PARENT) --i;
    if (h2->flags & HEAD_PARENT) ++i;
    if (i) return i;

    if (h1->flags & HEAD_GROUP_DESK || h2->flags & HEAD_GROUP_DESK) {
        if (h1->flags & HEAD_GROUP_DESK) --i;
        if (h2->flags & HEAD_GROUP_DESK) ++i;
        if (i) return i;
        if (h1->flags & HEAD_PRIMARY) --i;
        if (h2->flags & HEAD_PRIMARY) ++i;
        if (i) return i;
    }

    if (h1->flags & HEAD_GROUP || h2->flags & HEAD_GROUP) {
        if (h1->flags & HEAD_GROUP) --i;
        if (h2->flags & HEAD_GROUP) ++i;
        if (i) return i;
        if (h1->flags & HEAD_PRIMARY) --i;
        if (h2->flags & HEAD_PRIMARY) ++i;
        if (i) return i;
    }

    if (h1->flags & HEAD_PRIMARY) --i;
    if (h2->flags & HEAD_PRIMARY) ++i;
    if (i) return i;

    return h1->monitor - h2->monitor;
}

/*! Pick a monitor to place a window on. */
static Rect* choose_monitor(ObClient *c, gboolean client_to_be_foregrounded,
                            ObAppSettings *settings)
{
    Rect *area;
    ObPlaceHead *choice;
    guint i;
    ObClient *p;
    GSList *it;

    choice = g_new(ObPlaceHead, screen_num_monitors);
    for (i = 0; i < screen_num_monitors; ++i) {
        choice[i].monitor = i;
        choice[i].flags = 0;
    }

    /* find monitors with group members */
    if (c->group) {
        for (it = c->group->members; it; it = g_slist_next(it)) {
            ObClient *itc = it->data;
            if (itc != c) {
                guint m = client_monitor(itc);

                if (m < screen_num_monitors) {
                    if (screen_compare_desktops(itc->desktop, c->desktop))
                        choice[m].flags |= HEAD_GROUP_DESK;
                    else
                        choice[m].flags |= HEAD_GROUP;
                }
            }
        }
    }

    i = screen_monitor_primary(FALSE);
    if (i < screen_num_monitors) {
        choice[i].flags |= HEAD_PRIMARY;
        if (config_place_monitor == OB_PLACE_MONITOR_PRIMARY)
            choice[i].flags |= HEAD_PLACED;
        if (settings &&
            settings->monitor_type == OB_PLACE_MONITOR_PRIMARY)
            choice[i].flags |= HEAD_PERAPP;
    }

    i = screen_monitor_active();
    if (i < screen_num_monitors) {
        if (config_place_monitor == OB_PLACE_MONITOR_ACTIVE)
            choice[i].flags |= HEAD_PLACED;
        if (settings &&
            settings->monitor_type == OB_PLACE_MONITOR_ACTIVE)
            choice[i].flags |= HEAD_PERAPP;
    }

    i = screen_monitor_pointer();
    if (i < screen_num_monitors) {
        if (config_place_monitor == OB_PLACE_MONITOR_MOUSE)
            choice[i].flags |= HEAD_PLACED;
        if (settings &&
            settings->monitor_type == OB_PLACE_MONITOR_MOUSE)
            choice[i].flags |= HEAD_PERAPP;
    }

    if (settings) {
        i = settings->monitor - 1;
        if (i < screen_num_monitors)
            choice[i].flags |= HEAD_PERAPP;
    }

    /* direct parent takes highest precedence */
    if ((p = client_direct_parent(c))) {
        i = client_monitor(p);
        if (i < screen_num_monitors)
            choice[i].flags |= HEAD_PARENT;
    }

    qsort(choice, screen_num_monitors, sizeof(ObPlaceHead),
          client_to_be_foregrounded ? cmp_foreground : cmp_background);

    /* save the areas of the monitors in order of their being chosen */
    for (i = 0; i < screen_num_monitors; ++i)
    {
        ob_debug("placement choice %d is monitor %d", i, choice[i].monitor);
        if (choice[i].flags & HEAD_PARENT)
            ob_debug("  - parent on monitor");
        if (choice[i].flags & HEAD_PLACED)
            ob_debug("  - placement choice");
        if (choice[i].flags & HEAD_PRIMARY)
            ob_debug("  - primary monitor");
        if (choice[i].flags & HEAD_GROUP_DESK)
            ob_debug("  - group on same desktop");
        if (choice[i].flags & HEAD_GROUP)
            ob_debug("  - group on other desktop");
    }

    area = screen_area(c->desktop, choice[0].monitor, NULL);

    g_free(choice);

    /* return the area for the chosen monitor */
    return area;
}

static gboolean place_under_mouse(ObClient *client, gint *x, gint *y,
                                  Size frame_size)
{
    gint l, r, t, b;
    gint px, py;
    Rect *area;

    if (config_place_policy != OB_PLACE_POLICY_MOUSE)
        return FALSE;

    ob_debug("placing under mouse");

    if (!screen_pointer_pos(&px, &py))
        return FALSE;
    area = choose_pointer_monitor(client);

    l = area->x;
    t = area->y;
    r = area->x + area->width - frame_size.width;
    b = area->y + area->height - frame_size.height;

    *x = px - frame_size.width / 2;
    *x = MIN(MAX(*x, l), r);
    *y = py - frame_size.height / 2;
    *y = MIN(MAX(*y, t), b);

    g_slice_free(Rect, area);

    return TRUE;
}

static gboolean place_per_app_setting_position(ObClient *client, Rect *screen,
                                               gint *x, gint *y,
                                               ObAppSettings *settings,
                                               Size frame_size)
{
    if (!settings || !settings->pos_given)
        return FALSE;

    ob_debug("placing by per-app settings");

    screen_apply_gravity_point(x, y, frame_size.width, frame_size.height,
                               &settings->position, screen);

    return TRUE;
}

static void place_per_app_setting_size(ObClient *client, Rect *screen,
                                       gint *w, gint *h,
                                       ObAppSettings *settings)
{
    if (!settings)
        return;

    g_assert(settings->width_num >= 0);
    g_assert(settings->width_denom >= 0);
    g_assert(settings->height_num >= 0);
    g_assert(settings->height_denom >= 0);

    if (settings->width_num) {
        ob_debug("setting width by per-app settings");
        if (!settings->width_denom)
            *w = settings->width_num;
        else {
            *w = screen->width * settings->width_num / settings->width_denom;
            *w = MIN(*w, screen->width);
        }
    }

    if (settings->height_num) {
        ob_debug("setting height by per-app settings");
        if (!settings->height_denom)
            *h = settings->height_num;
        else {
            *h = screen->height * settings->height_num / settings->height_denom;
            *h = MIN(*h, screen->height);
        }
    }
}

static gboolean place_transient_splash(ObClient *client, Rect *area,
                                       gint *x, gint *y, Size frame_size)
{
    if (client->type == OB_CLIENT_TYPE_DIALOG) {
        GSList *it;
        gboolean first = TRUE;
        gint l, r, t, b;

        ob_debug("placing dialog");

        for (it = client->parents; it; it = g_slist_next(it)) {
            ObClient *m = it->data;
            if (!m->iconic) {
                if (first) {
                    l = RECT_LEFT(m->frame->area);
                    t = RECT_TOP(m->frame->area);
                    r = RECT_RIGHT(m->frame->area);
                    b = RECT_BOTTOM(m->frame->area);
                    first = FALSE;
                } else {
                    l = MIN(l, RECT_LEFT(m->frame->area));
                    t = MIN(t, RECT_TOP(m->frame->area));
                    r = MAX(r, RECT_RIGHT(m->frame->area));
                    b = MAX(b, RECT_BOTTOM(m->frame->area));
                }
            }
        }
        if (!first) {
            *x = ((r + 1 - l) - frame_size.width) / 2 + l;
            *y = ((b + 1 - t) - frame_size.height) / 2 + t;
            return TRUE;
        }
    }

    if (client->type == OB_CLIENT_TYPE_DIALOG ||
        client->type == OB_CLIENT_TYPE_SPLASH)
    {
        ob_debug("placing dialog or splash");

        *x = (area->width - frame_size.width) / 2 + area->x;
        *y = (area->height - frame_size.height) / 2 + area->y;
        return TRUE;
    }

    return FALSE;
}

static gboolean place_least_overlap(ObClient *c, Rect *head, int *x, int *y,
                                    Size frame_size)
{
    /* Assemble the list of windows that could overlap with @c in the user's
       current view. */
    GSList* potential_overlap_clients = NULL;
    gint n_client_rects = config_dock_hide ? 0 : 1;

    /* If we're "showing desktop", and going to allow this window to
       be shown now, then ignore all existing windows */
    gboolean ignore_windows = FALSE;
    switch (screen_show_desktop_mode) {
    case SCREEN_SHOW_DESKTOP_NO:
    case SCREEN_SHOW_DESKTOP_UNTIL_WINDOW:
        break;
    case SCREEN_SHOW_DESKTOP_UNTIL_TOGGLE:
        ignore_windows = TRUE;
        break;
    }

    if (!ignore_windows) {
        GList* it;
        for (it = client_list; it != NULL; it = g_list_next(it)) {
            ObClient* maybe_client = (ObClient*)it->data;
            if (maybe_client == c)
                continue;
            if (maybe_client->iconic)
                continue;
            if (!client_occupies_space(maybe_client))
                continue;
            if (c->desktop != DESKTOP_ALL) {
                if (maybe_client->desktop != c->desktop &&
                    maybe_client->desktop != DESKTOP_ALL)
                    continue;
            } else {
                if (maybe_client->desktop != screen_desktop &&
                    maybe_client->desktop != DESKTOP_ALL)
                    continue;
            }

            potential_overlap_clients = g_slist_prepend(
                potential_overlap_clients, maybe_client);
            n_client_rects += 1;
        }
    }

    if (n_client_rects) {
        Rect client_rects[n_client_rects];
        GSList* it;
        Point result;
        guint i = 0;

        if (!config_dock_hide)
            dock_get_area(&client_rects[i++]);
        for (it = potential_overlap_clients; it != NULL; it = g_slist_next(it)) {
            ObClient* potential_overlap_client = (ObClient*)it->data;
            client_rects[i] = potential_overlap_client->frame->area;
            i += 1;
        }
        g_slist_free(potential_overlap_clients);

        place_overlap_find_least_placement(client_rects, n_client_rects, head,
                                           &frame_size, &result);
        *x = result.x;
        *y = result.y;
    }

    return TRUE;
}

static gboolean should_set_client_position(ObClient *client,
                                           ObAppSettings *settings)
{
    gboolean has_position = settings && settings->pos_given;
    gboolean has_forced_position = has_position && settings->pos_force;

    gboolean user_positioned = client->positioned & USPosition;
    if (user_positioned && !has_forced_position)
        return FALSE;

    gboolean program_positioned = client->positioned & PPosition;
    if (program_positioned && !has_position)
        return FALSE;

    return TRUE;
}

gboolean place_client(ObClient *client, gboolean client_to_be_foregrounded,
                      Rect* client_area, ObAppSettings *settings)
{
    gboolean ret;
    Rect *monitor_area;
    int *x, *y, *w, *h;
    Size frame_size;

    monitor_area = choose_monitor(client, client_to_be_foregrounded, settings);

    w = &client_area->width;
    h = &client_area->height;
    place_per_app_setting_size(client, monitor_area, w, h, settings);

    if (!should_set_client_position(client, settings))
        return FALSE;

    x = &client_area->x;
    y = &client_area->y;

    SIZE_SET(frame_size,
             *w + client->frame->size.left + client->frame->size.right,
             *h + client->frame->size.top + client->frame->size.bottom);

    ret =
        place_per_app_setting_position(client, monitor_area, x, y, settings,
                                       frame_size) ||
        place_transient_splash(client, monitor_area, x, y, frame_size) ||
        place_under_mouse(client, x, y, frame_size) ||
        place_least_overlap(client, monitor_area, x, y, frame_size);
    g_assert(ret);

    g_slice_free(Rect, monitor_area);

    /* get where the client should be */
    frame_frame_gravity(client->frame, x, y);
    return TRUE;
}
