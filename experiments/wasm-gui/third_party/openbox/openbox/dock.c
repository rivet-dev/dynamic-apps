/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   dock.c for the Openbox window manager
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
#include "dock.h"
#include "screen.h"
#include "config.h"
#include "grab.h"
#include "openbox.h"
#include "obrender/theme.h"
#include "obt/prop.h"

#define DOCK_EVENT_MASK (ButtonPressMask | ButtonReleaseMask | \
                         EnterWindowMask | LeaveWindowMask)
#define DOCKAPP_EVENT_MASK (StructureNotifyMask)
#define DOCK_NOPROPAGATEMASK (ButtonPressMask | ButtonReleaseMask | \
                              ButtonMotionMask)

static ObDock *dock;
static guint show_timeout_id;
static guint hide_timeout_id;

StrutPartial dock_strut;

static void dock_app_grab_button(ObDockApp *app, gboolean grab)
{
    if (grab) {
        grab_button_full(config_dock_app_move_button,
                         config_dock_app_move_modifiers, app->icon_win,
                         ButtonPressMask | ButtonReleaseMask |
                         ButtonMotionMask,
                         GrabModeAsync, OB_CURSOR_MOVE);
    } else {
        ungrab_button(config_dock_app_move_button,
                      config_dock_app_move_modifiers, app->icon_win);
    }
}

static guint window_hash(Window *w) { return *w; }
static gboolean window_comp(Window *w1, Window *w2) { return *w1 == *w2; }

void dock_startup(gboolean reconfig)
{
    XSetWindowAttributes attrib;

    if (reconfig) {
        GList *it;

        XSetWindowBorder(obt_display, dock->frame,
                         RrColorPixel(ob_rr_theme->osd_border_color));
        XSetWindowBorderWidth(obt_display, dock->frame, ob_rr_theme->obwidth);

        RrAppearanceFree(dock->a_frame);
        dock->a_frame = RrAppearanceCopy(ob_rr_theme->osd_bg);

        stacking_add(DOCK_AS_WINDOW(dock));

        dock_configure();
        dock_hide(TRUE);

        for (it = dock->dock_apps; it; it = g_list_next(it))
            dock_app_grab_button(it->data, TRUE);
        return;
    }

    STRUT_PARTIAL_SET(dock_strut, 0, 0, 0, 0,
                      0, 0, 0, 0, 0, 0, 0, 0);

    dock = g_slice_new0(ObDock);
    dock->obwin.type = OB_WINDOW_CLASS_DOCK;

    dock->hidden = TRUE;

    dock->dock_map = g_hash_table_new((GHashFunc)window_hash,
                                      (GEqualFunc)window_comp);

    attrib.event_mask = DOCK_EVENT_MASK;
    attrib.override_redirect = True;
    attrib.do_not_propagate_mask = DOCK_NOPROPAGATEMASK;
    dock->frame = XCreateWindow(obt_display, obt_root(ob_screen),
                                0, 0, 1, 1, 0,
                                RrDepth(ob_rr_inst), InputOutput,
                                RrVisual(ob_rr_inst),
                                CWOverrideRedirect | CWEventMask |
                                CWDontPropagate,
                                &attrib);
    dock->a_frame = RrAppearanceCopy(ob_rr_theme->osd_bg);
    XSetWindowBorder(obt_display, dock->frame,
                     RrColorPixel(ob_rr_theme->osd_border_color));
    XSetWindowBorderWidth(obt_display, dock->frame, ob_rr_theme->obwidth);

    /* Setting the window type so xcompmgr can tell what it is */
    OBT_PROP_SET32(dock->frame, NET_WM_WINDOW_TYPE, ATOM,
                   OBT_PROP_ATOM(NET_WM_WINDOW_TYPE_DOCK));

    window_add(&dock->frame, DOCK_AS_WINDOW(dock));
    stacking_add(DOCK_AS_WINDOW(dock));
}

void dock_shutdown(gboolean reconfig)
{
    if (reconfig) {
        GList *it;

        stacking_remove(DOCK_AS_WINDOW(dock));

        for (it = dock->dock_apps; it; it = g_list_next(it))
            dock_app_grab_button(it->data, FALSE);
        return;
    }

    g_hash_table_destroy(dock->dock_map);

    XDestroyWindow(obt_display, dock->frame);
    RrAppearanceFree(dock->a_frame);
    window_remove(dock->frame);
    stacking_remove(dock);
    g_slice_free(ObDock, dock);
    dock = NULL;
}

void dock_manage(Window icon_win, Window name_win)
{
    ObDockApp *app;
    XWindowAttributes attrib;
    gchar **data;

    app = g_slice_new0(ObDockApp);
    app->name_win = name_win;
    app->icon_win = icon_win;

    if (OBT_PROP_GETSS_TYPE(app->name_win, WM_CLASS, STRING_NO_CC, &data)) {
        if (data[0]) {
            app->name = g_strdup(data[0]);
            if (data[1])
                app->class = g_strdup(data[1]);
        }
        g_strfreev(data);
    }

    if (app->name == NULL) app->name = g_strdup("");
    if (app->class == NULL) app->class = g_strdup("");

    if (XGetWindowAttributes(obt_display, app->icon_win, &attrib)) {
        app->w = attrib.width;
        app->h = attrib.height;
    } else {
        app->w = app->h = 64;
    }

    dock->dock_apps = g_list_append(dock->dock_apps, app);
    g_hash_table_insert(dock->dock_map, &app->icon_win, app);
    dock_configure();

    XReparentWindow(obt_display, app->icon_win, dock->frame, app->x, app->y);
    /*
      This is the same case as in frame.c for client windows. When Openbox is
      starting, the window is already mapped so we see unmap events occur for
      it. There are 2 unmap events generated that we see, one with the 'event'
      member set the root window, and one set to the client, but both get
      handled and need to be ignored.
    */
    if (ob_state() == OB_STATE_STARTING)
        app->ignore_unmaps += 2;
    XChangeSaveSet(obt_display, app->icon_win, SetModeInsert);
    XMapWindow(obt_display, app->icon_win);

    if (app->name_win != app->icon_win) {
        XReparentWindow(obt_display, app->name_win, dock->frame, -1000, -1000);
        XChangeSaveSet(obt_display, app->name_win, SetModeInsert);
        XMapWindow(obt_display, app->name_win);
    }

    XSync(obt_display, False);

    XSelectInput(obt_display, app->icon_win, DOCKAPP_EVENT_MASK);

    dock_app_grab_button(app, TRUE);

    ob_debug("Managed Dock App: 0x%lx 0x%lx (%s)",
             app->icon_win, app->name_win, app->class);

    grab_server(FALSE);
}

void dock_unmanage_all(void)
{
    while (dock->dock_apps)
        dock_unmanage(dock->dock_apps->data, TRUE);
}

void dock_unmanage(ObDockApp *app, gboolean reparent)
{
    dock_app_grab_button(app, FALSE);
    XSelectInput(obt_display, app->icon_win, NoEventMask);
    /* remove the window from our save set */
    XChangeSaveSet(obt_display, app->icon_win, SetModeDelete);
    XSync(obt_display, False);

    if (reparent) {
        XReparentWindow(obt_display, app->icon_win, obt_root(ob_screen), 0, 0);
        if (app->name_win != app->icon_win)
            XReparentWindow(obt_display, app->name_win,
                            obt_root(ob_screen), 0, 0);
    }

    dock->dock_apps = g_list_remove(dock->dock_apps, app);
    g_hash_table_remove(dock->dock_map, &app->icon_win);
    dock_configure();

    ob_debug("Unmanaged Dock App: 0x%lx (%s)", app->icon_win, app->class);

    g_free(app->name);
    g_free(app->class);
    g_slice_free(ObDockApp, app);
}

void dock_configure(void)
{
    GList *it;
    gint hspot, vspot;
    gint gravity;
    gint l, r, t, b;
    gint strw, strh;
    const Rect *a;
    gint hidesize;

    RrMargins(dock->a_frame, &l, &t, &r, &b);
    hidesize = MAX(1, ob_rr_theme->obwidth);

    dock->area.width = dock->area.height = 0;

    /* get the size */
    for (it = dock->dock_apps; it; it = g_list_next(it)) {
        ObDockApp *app = it->data;
        switch (config_dock_orient) {
        case OB_ORIENTATION_HORZ:
            dock->area.width += app->w;
            dock->area.height = MAX(dock->area.height, app->h);
            break;
        case OB_ORIENTATION_VERT:
            dock->area.width = MAX(dock->area.width, app->w);
            dock->area.height += app->h;
            break;
        }
    }

    if (dock->dock_apps) {
        dock->area.width += l + r;
        dock->area.height += t + b;
    }

    hspot = l;
    vspot = t;

    /* position the apps */
    for (it = dock->dock_apps; it; it = g_list_next(it)) {
        ObDockApp *app = it->data;
        switch (config_dock_orient) {
        case OB_ORIENTATION_HORZ:
            app->x = hspot;
            app->y = (dock->area.height - app->h) / 2;
            hspot += app->w;
            break;
        case OB_ORIENTATION_VERT:
            app->x = (dock->area.width - app->w) / 2;
            app->y = vspot;
            vspot += app->h;
            break;
        }

        XMoveWindow(obt_display, app->icon_win, app->x, app->y);
    }

    /* used for calculating offsets */
    dock->area.width += ob_rr_theme->obwidth * 2;
    dock->area.height += ob_rr_theme->obwidth * 2;

    a = screen_physical_area_all_monitors();

    /* calculate position */
    if (config_dock_floating) {
        dock->area.x = config_dock_x;
        dock->area.y = config_dock_y;
        gravity = NorthWestGravity;
    } else {
        switch (config_dock_pos) {
        case OB_DIRECTION_NORTHWEST:
            dock->area.x = 0;
            dock->area.y = 0;
            gravity = NorthWestGravity;
            break;
        case OB_DIRECTION_NORTH:
            dock->area.x = a->width / 2;
            dock->area.y = 0;
            gravity = NorthGravity;
            break;
        case OB_DIRECTION_NORTHEAST:
            dock->area.x = a->width;
            dock->area.y = 0;
            gravity = NorthEastGravity;
            break;
        case OB_DIRECTION_WEST:
            dock->area.x = 0;
            dock->area.y = a->height / 2;
            gravity = WestGravity;
            break;
        case OB_DIRECTION_EAST:
            dock->area.x = a->width;
            dock->area.y = a->height / 2;
            gravity = EastGravity;
            break;
        case OB_DIRECTION_SOUTHWEST:
            dock->area.x = 0;
            dock->area.y = a->height;
            gravity = SouthWestGravity;
            break;
        case OB_DIRECTION_SOUTH:
            dock->area.x = a->width / 2;
            dock->area.y = a->height;
            gravity = SouthGravity;
            break;
        case OB_DIRECTION_SOUTHEAST:
            dock->area.x = a->width;
            dock->area.y = a->height;
            gravity = SouthEastGravity;
            break;
        default:
            g_assert_not_reached();
        }
    }

    switch(gravity) {
    case NorthGravity:
    case CenterGravity:
    case SouthGravity:
        dock->area.x -= dock->area.width / 2;
        break;
    case NorthEastGravity:
    case EastGravity:
    case SouthEastGravity:
        dock->area.x -= dock->area.width;
        break;
    }
    switch(gravity) {
    case WestGravity:
    case CenterGravity:
    case EastGravity:
        dock->area.y -= dock->area.height / 2;
        break;
    case SouthWestGravity:
    case SouthGravity:
    case SouthEastGravity:
        dock->area.y -= dock->area.height;
        break;
    }

    if (config_dock_hide && dock->hidden) {
        if (!config_dock_floating) {
            switch (config_dock_pos) {
            case OB_DIRECTION_NORTHWEST:
                switch (config_dock_orient) {
                case OB_ORIENTATION_HORZ:
                    dock->area.y -= dock->area.height - hidesize;
                    break;
                case OB_ORIENTATION_VERT:
                    dock->area.x -= dock->area.width - hidesize;
                    break;
                }
                break;
            case OB_DIRECTION_NORTH:
                dock->area.y -= dock->area.height - hidesize;
                break;
            case OB_DIRECTION_NORTHEAST:
                switch (config_dock_orient) {
                case OB_ORIENTATION_HORZ:
                    dock->area.y -= dock->area.height - hidesize;
                    break;
                case OB_ORIENTATION_VERT:
                    dock->area.x += dock->area.width - hidesize;
                    break;
                }
                break;
            case OB_DIRECTION_WEST:
                dock->area.x -= dock->area.width - hidesize;
                break;
            case OB_DIRECTION_EAST:
                dock->area.x += dock->area.width - hidesize;
                break;
            case OB_DIRECTION_SOUTHWEST:
                switch (config_dock_orient) {
                case OB_ORIENTATION_HORZ:
                    dock->area.y += dock->area.height - hidesize;
                    break;
                case OB_ORIENTATION_VERT:
                    dock->area.x -= dock->area.width - hidesize;
                    break;
                } break;
            case OB_DIRECTION_SOUTH:
                dock->area.y += dock->area.height - hidesize;
                break;
            case OB_DIRECTION_SOUTHEAST:
                switch (config_dock_orient) {
                case OB_ORIENTATION_HORZ:
                    dock->area.y += dock->area.height - hidesize;
                    break;
                case OB_ORIENTATION_VERT:
                    dock->area.x += dock->area.width - hidesize;
                    break;
                }
                break;
            }
        }
    }

    if (!config_dock_floating && config_dock_hide) {
        strw = hidesize;
        strh = hidesize;
    } else {
        strw = dock->area.width;
        strh = dock->area.height;
    }

    /* set the strut */
    if (!dock->dock_apps) {
        STRUT_PARTIAL_SET(dock_strut, 0, 0, 0, 0,
                          0, 0, 0, 0, 0, 0, 0, 0);
    }
    else if (config_dock_floating || config_dock_nostrut) {
        STRUT_PARTIAL_SET(dock_strut, 0, 0, 0, 0,
                          0, 0, 0, 0, 0, 0, 0, 0);
    }
    else {
        switch (config_dock_pos) {
        case OB_DIRECTION_NORTHWEST:
            switch (config_dock_orient) {
            case OB_ORIENTATION_HORZ:
                STRUT_PARTIAL_SET(dock_strut, 0, strh, 0, 0,
                                  0, 0, dock->area.x, dock->area.x
                                  + dock->area.width - 1, 0, 0, 0, 0);
                break;
            case OB_ORIENTATION_VERT:
                STRUT_PARTIAL_SET(dock_strut, strw, 0, 0, 0,
                                  dock->area.y, dock->area.y
                                  + dock->area.height - 1, 0, 0, 0, 0, 0, 0);
                break;
            }
            break;
        case OB_DIRECTION_NORTH:
            STRUT_PARTIAL_SET(dock_strut, 0, strh, 0, 0,
                              0, 0, dock->area.x, dock->area.x
                              + dock->area.width - 1, 0, 0, 0, 0);
            break;
        case OB_DIRECTION_NORTHEAST:
            switch (config_dock_orient) {
            case OB_ORIENTATION_HORZ:
                STRUT_PARTIAL_SET(dock_strut, 0, strh, 0, 0,
                                  0, 0, dock->area.x, dock->area.x
                                  + dock->area.width -1, 0, 0, 0, 0);
                break;
            case OB_ORIENTATION_VERT:
                STRUT_PARTIAL_SET(dock_strut, 0, 0, strw, 0,
                                  0, 0, 0, 0, dock->area.y, dock->area.y
                                  + dock->area.height - 1, 0, 0);
                break;
            }
            break;
        case OB_DIRECTION_WEST:
            STRUT_PARTIAL_SET(dock_strut, strw, 0, 0, 0,
                              dock->area.y, dock->area.y
                              + dock->area.height - 1, 0, 0, 0, 0, 0, 0);
            break;
        case OB_DIRECTION_EAST:
            STRUT_PARTIAL_SET(dock_strut, 0, 0, strw, 0,
                              0, 0, 0, 0, dock->area.y, dock->area.y
                              + dock->area.height - 1, 0, 0);
            break;
        case OB_DIRECTION_SOUTHWEST:
            switch (config_dock_orient) {
            case OB_ORIENTATION_HORZ:
                STRUT_PARTIAL_SET(dock_strut, 0, 0, 0, strh,
                                  0, 0, 0, 0, 0, 0, dock->area.x, dock->area.x
                                  + dock->area.width - 1);
                break;
            case OB_ORIENTATION_VERT:
                STRUT_PARTIAL_SET(dock_strut, strw, 0, 0, 0,
                                  dock->area.y, dock->area.y
                                  + dock->area.height - 1, 0, 0, 0, 0, 0, 0);
                break;
            }
            break;
        case OB_DIRECTION_SOUTH:
            STRUT_PARTIAL_SET(dock_strut, 0, 0, 0, strh,
                              0, 0, 0, 0, 0, 0, dock->area.x, dock->area.x
                              + dock->area.width - 1);
            break;
        case OB_DIRECTION_SOUTHEAST:
            switch (config_dock_orient) {
            case OB_ORIENTATION_HORZ:
                STRUT_PARTIAL_SET(dock_strut, 0, 0, 0, strh,
                                  0, 0, 0, 0, 0, 0, dock->area.x,
                                  dock->area.x + dock->area.width - 1);
                break;
            case OB_ORIENTATION_VERT:
                STRUT_PARTIAL_SET(dock_strut, 0, 0, strw, 0,
                                  0, 0, 0, 0, dock->area.y, dock->area.y
                                  + dock->area.height - 1, 0, 0);
                break;
            }
            break;
        }
    }

    /* not used for actually sizing shit */
    dock->area.width -= ob_rr_theme->obwidth * 2;
    dock->area.height -= ob_rr_theme->obwidth * 2;

    if (dock->dock_apps) {
        g_assert(dock->area.width > 0);
        g_assert(dock->area.height > 0);

        XMoveResizeWindow(obt_display, dock->frame, dock->area.x, dock->area.y,
                          dock->area.width, dock->area.height);

        RrPaint(dock->a_frame, dock->frame, dock->area.width,
                dock->area.height);
        XMapWindow(obt_display, dock->frame);
    } else
        XUnmapWindow(obt_display, dock->frame);

    /* but they are useful outside of this function! but don't add it if the
       dock is actually not visible */
    if (dock->dock_apps) {
        dock->area.width += ob_rr_theme->obwidth * 2;
        dock->area.height += ob_rr_theme->obwidth * 2;
    }

    /* screen_resize() depends on this function to call screen_update_areas(),
       so if this changes, also update screen_resize(). */
    screen_update_areas();
}

void dock_app_configure(ObDockApp *app, gint w, gint h)
{
    app->w = w;
    app->h = h;
    dock_configure();
}

void dock_app_drag(ObDockApp *app, XMotionEvent *e)
{
    ObDockApp *over = NULL;
    GList *it;
    gint x, y;
    gboolean after;
    gboolean stop;

    x = e->x_root;
    y = e->y_root;

    /* are we on top of the dock? */
    if (!(x >= dock->area.x &&
          y >= dock->area.y &&
          x < dock->area.x + dock->area.width &&
          y < dock->area.y + dock->area.height))
        return;

    x -= dock->area.x;
    y -= dock->area.y;

    /* which dock app are we on top of? */
    stop = FALSE;
    for (it = dock->dock_apps; it; it = g_list_next(it)) {
        over = it->data;
        switch (config_dock_orient) {
        case OB_ORIENTATION_HORZ:
            if (x >= over->x && x < over->x + over->w)
                stop = TRUE;
            break;
        case OB_ORIENTATION_VERT:
            if (y >= over->y && y < over->y + over->h)
                stop = TRUE;
            break;
        }
        /* dont go to it->next! */
        if (stop) break;
    }
    if (!it || app == over) return;

    x -= over->x;
    y -= over->y;

    switch (config_dock_orient) {
    case OB_ORIENTATION_HORZ:
        after = (x > over->w / 2);
        break;
    case OB_ORIENTATION_VERT:
        after = (y > over->h / 2);
        break;
    default:
        g_assert_not_reached();
    }

    /* remove before doing the it->next! */
    dock->dock_apps = g_list_remove(dock->dock_apps, app);

    if (after) it = it->next;

    dock->dock_apps = g_list_insert_before(dock->dock_apps, it, app);
    dock_configure();
}

static gboolean hide_timeout(gpointer data)
{
    /* hide */
    dock->hidden = TRUE;
    dock_configure();

    return FALSE; /* don't repeat */
}

static gboolean show_timeout(gpointer data)
{
    /* show */
    dock->hidden = FALSE;
    dock_configure();

    return FALSE; /* don't repeat */
}

static void destroy_timeout(gpointer data)
{
    gint *id = data;
    *id = 0;
}

void dock_hide(gboolean hide)
{
    if (!hide) {
        if (dock->hidden && config_dock_hide) {
            show_timeout_id = g_timeout_add_full(G_PRIORITY_DEFAULT,
                                                 config_dock_show_delay,
                                                 show_timeout, &show_timeout_id, destroy_timeout);
        } else if (!dock->hidden && config_dock_hide && hide_timeout_id) {
            if (hide_timeout_id) g_source_remove(hide_timeout_id);
        }
    } else {
        if (!dock->hidden && config_dock_hide) {
            hide_timeout_id = g_timeout_add_full(G_PRIORITY_DEFAULT,
                                                 config_dock_hide_delay,
                                                 hide_timeout, &hide_timeout_id, destroy_timeout);
        } else if (dock->hidden && config_dock_hide && show_timeout_id) {
            if (show_timeout_id) g_source_remove(show_timeout_id);
        }
    }
}

void dock_get_area(Rect *a)
{
    RECT_SET(*a, dock->area.x, dock->area.y,
             dock->area.width, dock->area.height);
}

void dock_raise_dock(void)
{
    stacking_raise(DOCK_AS_WINDOW(dock));
}

void dock_lower_dock(void)
{
    stacking_lower(DOCK_AS_WINDOW(dock));
}

ObDockApp* dock_find_dockapp(Window xwin)
{
    return g_hash_table_lookup(dock->dock_map, &xwin);
}
