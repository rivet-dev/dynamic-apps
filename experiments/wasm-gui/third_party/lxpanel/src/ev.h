/*
 * fb-background-monitor.h:
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
 *	Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __FB_EV_H__
#define __FB_EV_H__

/* FIXME: this needs to be made multiscreen aware
 *        panel_bg_get should take
 *        a GdkScreen argument.
 */

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#define FB_TYPE_EV         (fb_ev_get_type ())
#define FB_EV(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o),      \
					       FB_TYPE_EV,        \
					       FbEv))
#define FB_EV_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k),         \
					       FB_TYPE_EV,        \
					       FbEvClass))
#define FB_IS_EV(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o),      \
					       FB_TYPE_EV))
#define FB_IS_EV_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k),         \
					       FB_TYPE_EV))
#define FB_EV_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),       \
					       FB_TYPE_EV,        \
					       FbEvClass))

typedef struct _FbEvClass FbEvClass;
typedef struct _FbEv      FbEv;
enum {
    EV_CURRENT_DESKTOP,
    EV_NUMBER_OF_DESKTOPS,
    EV_DESKTOP_NAMES,
    EV_ACTIVE_WINDOW,
    EV_DESTROY_WINDOW,
    EV_CLIENT_LIST_STACKING,
    EV_CLIENT_LIST,
    LAST_SIGNAL
};

GType fb_ev_get_type       (void);
FbEv *fb_ev_new(void);
void fb_ev_notify_changed_ev(FbEv *ev);
void fb_ev_emit(FbEv *ev, int signal);
void fb_ev_emit_destroy(FbEv *ev, Window win);

extern int fb_ev_current_desktop(FbEv *ev);
extern int fb_ev_number_of_desktops(FbEv *ev);
extern Window *fb_ev_active_window(FbEv *ev);
extern Window *fb_ev_client_list(FbEv *ev);
extern Window *fb_ev_client_list_stacking(FbEv *ev);

/* it is created in the main.c */
extern FbEv *fbev;

#endif /* __FB_EV_H__ */
