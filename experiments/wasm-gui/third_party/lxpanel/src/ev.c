/*
 * fb-background-monitor.c:
 *
 * Copyright (C) 2001, 2002 Ian McKellar <yakk@yakk.net>
 *                     2002 Sun Microsystems, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Authors:
 *      Ian McKellar <yakk@yakk.net>
 *      Mark McLoughlin <mark@skynet.ie>
 */

#include <glib.h>
#include <glib-object.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "ev.h"
#include "misc.h"

//#define DEBUG
#include "dbg.h"


struct _FbEvClass {
    GObjectClass   parent_class;
    void *dummy;
    void (*current_desktop)(FbEv *ev, gpointer p);
    void (*active_window)(FbEv *ev, gpointer p);
    void (*number_of_desktops)(FbEv *ev, gpointer p);
    void (*desktop_names)(FbEv *ev, gpointer p);
    void (*client_list)(FbEv *ev, gpointer p);
    void (*client_list_stacking)(FbEv *ev, gpointer p);
};

struct _FbEv {
    GObject    parent_instance;

    int current_desktop;
    int number_of_desktops;
    char **desktop_names;
    Window active_window;
    Window *client_list;
    Window *client_list_stacking;

    Window   xroot;
    Atom     id;
    GC       gc;
    Display *dpy;
    Pixmap   pixmap;
};

/* it is created in main.c */
FbEv *fbev = NULL;

static void fb_ev_class_init (FbEvClass *klass);
static void fb_ev_init (FbEv *monitor);
static void fb_ev_finalize (GObject *object);

static void ev_current_desktop(FbEv *ev, gpointer p);
static void ev_active_window(FbEv *ev, gpointer p);
static void ev_number_of_desktops(FbEv *ev, gpointer p);
static void ev_desktop_names(FbEv *ev, gpointer p);
static void ev_client_list(FbEv *ev, gpointer p);
static void ev_client_list_stacking(FbEv *ev, gpointer p);

static guint signals [LAST_SIGNAL] = { 0 };


GType
fb_ev_get_type (void)
{
    static GType object_type = 0;

    if (!object_type) {
        static const GTypeInfo object_info = {
            sizeof (FbEvClass),
            (GBaseInitFunc) NULL,
            (GBaseFinalizeFunc) NULL,
            (GClassInitFunc) fb_ev_class_init,
            NULL,           /* class_finalize */
            NULL,           /* class_data */
            sizeof (FbEv),
            0,              /* n_preallocs */
            (GInstanceInitFunc) fb_ev_init,
        };

        object_type = g_type_register_static (
            G_TYPE_OBJECT, "FbEv", &object_info, 0);
    }

    return object_type;
}



static void
fb_ev_class_init (FbEvClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    signals [EV_CURRENT_DESKTOP] =
        g_signal_new ("current_desktop",
              G_OBJECT_CLASS_TYPE (object_class),
              G_SIGNAL_RUN_FIRST,
              G_STRUCT_OFFSET (FbEvClass, current_desktop),
              NULL, NULL,
              g_cclosure_marshal_VOID__VOID,
              G_TYPE_NONE, 0);
    signals [EV_NUMBER_OF_DESKTOPS] =
        g_signal_new ("number_of_desktops",
              G_OBJECT_CLASS_TYPE (object_class),
              G_SIGNAL_RUN_FIRST,
              G_STRUCT_OFFSET (FbEvClass, number_of_desktops),
              NULL, NULL,
              g_cclosure_marshal_VOID__VOID,
              G_TYPE_NONE, 0);
    signals [EV_DESKTOP_NAMES] =
        g_signal_new ("desktop_names",
              G_OBJECT_CLASS_TYPE (object_class),
              G_SIGNAL_RUN_FIRST,
              G_STRUCT_OFFSET (FbEvClass, desktop_names),
              NULL, NULL,
              g_cclosure_marshal_VOID__VOID,
              G_TYPE_NONE, 0);
    signals [EV_ACTIVE_WINDOW] =
        g_signal_new ("active_window",
              G_OBJECT_CLASS_TYPE (object_class),
              G_SIGNAL_RUN_FIRST,
              G_STRUCT_OFFSET (FbEvClass, active_window),
              NULL, NULL,
              g_cclosure_marshal_VOID__VOID,
              G_TYPE_NONE, 0);
    signals [EV_DESTROY_WINDOW] =
        g_signal_new ("destroy_window",
              G_OBJECT_CLASS_TYPE (object_class),
              G_SIGNAL_RUN_FIRST,
              G_STRUCT_OFFSET (FbEvClass, active_window),
              NULL, NULL,
              g_cclosure_marshal_VOID__POINTER,
              G_TYPE_NONE, 1, G_TYPE_POINTER);
    signals [EV_CLIENT_LIST_STACKING] =
        g_signal_new ("client_list_stacking",
              G_OBJECT_CLASS_TYPE (object_class),
              G_SIGNAL_RUN_FIRST,
              G_STRUCT_OFFSET (FbEvClass, client_list_stacking),
              NULL, NULL,
              g_cclosure_marshal_VOID__VOID,
              G_TYPE_NONE, 0);
    signals [EV_CLIENT_LIST] =
        g_signal_new ("client_list",
              G_OBJECT_CLASS_TYPE (object_class),
              G_SIGNAL_RUN_FIRST,
              G_STRUCT_OFFSET (FbEvClass, client_list),
              NULL, NULL,
              g_cclosure_marshal_VOID__VOID,
              G_TYPE_NONE, 0);
    object_class->finalize = fb_ev_finalize;

    klass->current_desktop = ev_current_desktop;
    klass->active_window = ev_active_window;
    klass->number_of_desktops = ev_number_of_desktops;
    klass->desktop_names = ev_desktop_names;
    klass->client_list = ev_client_list;
    klass->client_list_stacking = ev_client_list_stacking;
}

static void
fb_ev_init (FbEv *ev)
{
    ev->number_of_desktops = -1;
    ev->current_desktop = -1;
    ev->active_window = None;
    ev->client_list_stacking = NULL;
    ev->client_list = NULL;
}


FbEv *
fb_ev_new()
{
    return  g_object_new (FB_TYPE_EV, NULL);
}

static void
fb_ev_finalize (GObject *object)
{
    //FbEv *ev;

    //ev = FB_EV (object);
    //XFreeGC(ev->dpy, ev->gc);
}

void
fb_ev_emit(FbEv *ev, int signal)
{
    DBG("signal=%d\n", signal);
    g_assert(signal >=0 && signal < LAST_SIGNAL);
    DBG("\n");
    if( signal == EV_ACTIVE_WINDOW )
    {
        Window* win = None;
        ev->active_window = None;
        win = (Window*)get_xaproperty (GDK_ROOT_WINDOW(), a_NET_ACTIVE_WINDOW, XA_WINDOW, 0);
        if (win) {
            ev->active_window = *win;
            /* g_debug( "WIN: %p", *win ); */
            XFree (win);
        }
    }
    g_signal_emit(ev, signals [signal], 0);
}

void fb_ev_emit_destroy(FbEv *ev, Window win)
{
    g_signal_emit(ev, signals [EV_DESTROY_WINDOW], 0, win );
}

static void
ev_current_desktop(FbEv *ev, gpointer p)
{
    ENTER;
    ev->current_desktop = -1;
    RET();
}

static void
ev_active_window(FbEv *ev, gpointer p)
{
    ENTER;
//    ev->active_window = None;		// Wrecks fb_ev_active window
    RET();
}

static void
ev_number_of_desktops(FbEv *ev, gpointer p)
{
    ENTER;
    ev->number_of_desktops = -1;
    RET();
}

static void
ev_desktop_names(FbEv *ev, gpointer p)
{
    ENTER;
    if (ev->desktop_names) {
        g_strfreev (ev->desktop_names);
        ev->desktop_names = NULL;
    }
    RET();
}
static void
ev_client_list(FbEv *ev, gpointer p)
{
    ENTER;
    if (ev->client_list) {
        XFree(ev->client_list);
        ev->client_list = NULL;
    }
    RET();
}

static void
ev_client_list_stacking(FbEv *ev, gpointer p)
{
    ENTER;
    if (ev->client_list_stacking) {
        XFree(ev->client_list_stacking);
        ev->client_list_stacking = NULL;
    }
    RET();
}

int
fb_ev_current_desktop(FbEv *ev)
{
    ENTER;
    if (ev->current_desktop == -1) {
        gulong *data;

        data = get_xaproperty (GDK_ROOT_WINDOW(), a_NET_CURRENT_DESKTOP, XA_CARDINAL, 0);
        if (data) {
            ev->current_desktop = *data;
            XFree (data);
        } else
            ev->current_desktop = 0;
    }
    RET(ev->current_desktop);
}

int
fb_ev_number_of_desktops(FbEv *ev)
{
    ENTER;
     if (ev->number_of_desktops == -1) {
        gulong *data;

        data = get_xaproperty (GDK_ROOT_WINDOW(), a_NET_NUMBER_OF_DESKTOPS, XA_CARDINAL, 0);
        if (data) {
            ev->number_of_desktops = *data;
            XFree (data);
        } else
            ev->number_of_desktops = 0;
    }
    RET(ev->number_of_desktops);

}

Window *fb_ev_active_window(FbEv *ev)
{
    return &ev->active_window;
}

Window *fb_ev_client_list(FbEv *ev)
{
    return ev->client_list;
}

Window *fb_ev_client_list_stacking(FbEv *ev)
{
    return ev->client_list_stacking;
}

