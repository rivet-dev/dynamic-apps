/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   dock.h for the Openbox window manager
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

#ifndef __dock_h
#define __dock_h

#include "window.h"
#include "stacking.h"
#include "geom.h"
#include "obrender/render.h"

#include <glib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef struct _ObDock    ObDock;
typedef struct _ObDockApp ObDockApp;

struct _ObDock
{
    ObWindow obwin;

    Window frame;
    RrAppearance *a_frame;

    /* actual position (when not auto-hidden) */
    Rect area;

    gboolean hidden;

    GList *dock_apps;
    GHashTable *dock_map;
};

struct _ObDockApp {
    gint ignore_unmaps;

    Window icon_win;
    Window name_win;

    gchar *name;
    gchar *class;

    gint x;
    gint y;
    gint w;
    gint h;
};

extern StrutPartial dock_strut;

void dock_startup(gboolean reconfig);
void dock_shutdown(gboolean reconfig);

void dock_configure(void);
void dock_hide(gboolean hide);

void dock_manage(Window icon_win, Window name_win);

void dock_unmanage_all(void);
void dock_unmanage(ObDockApp *app, gboolean reparent);

void dock_app_drag(ObDockApp *app, XMotionEvent *e);
void dock_app_configure(ObDockApp *app, gint w, gint h);

void dock_get_area(Rect *a);

void dock_raise_dock(void);
void dock_lower_dock(void);

ObDockApp* dock_find_dockapp(Window xwin);

#endif
