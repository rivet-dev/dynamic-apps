/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   test.c for the Openbox window manager
   Copyright (c) 2006        Mikael Magnusson
   Copyright (c) 2003-2007   Dana Jansens
   Copyright (c) 2003        Derek Foreman

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

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <string.h>
#include <stdlib.h>
#include "render.h"
#include <glib.h>

static gint x_error_handler(Display * disp, XErrorEvent * error)
{
    gchar buf[1024];
    XGetErrorText(disp, error->error_code, buf, 1024);
    printf("%s\n", buf);
    return 0;
}

Display *ob_display;
gint ob_screen;
Window ob_root;

gint main()
{
    Window win;
    RrInstance *inst;
    RrAppearance *look;
    int done;

    XEvent report;
    gint h = 500, w = 500;

    ob_display = XOpenDisplay(NULL);
    XSetErrorHandler(x_error_handler);
    ob_screen = DefaultScreen(ob_display);
    ob_root = RootWindow(ob_display, ob_screen);
    win =
        XCreateWindow(ob_display, RootWindow(ob_display, 0),
                      10, 10, w, h, 10,
                      CopyFromParent,    /* depth */
                      CopyFromParent,    /* class */
                      CopyFromParent,    /* visual */
                      0,                    /* valuemask */
                      0);                    /* attributes */
    XMapWindow(ob_display, win);
    XSelectInput(ob_display, win, ExposureMask | StructureNotifyMask);
    inst = RrInstanceNew(ob_display, ob_screen);

    look = RrAppearanceNew(inst, 0);
    look->surface.grad = RR_SURFACE_MIRROR_HORIZONTAL;
    look->surface.secondary = RrColorParse(inst, "Yellow");
    look->surface.split_secondary = RrColorParse(inst, "Red");
    look->surface.split_primary = RrColorParse(inst, "Green");
    look->surface.primary = RrColorParse(inst, "Blue");
    look->surface.interlaced = FALSE;
    if (ob_display == NULL) {
        fprintf(stderr, "couldn't connect to X server :0\n");
        return 0;
    }

#if BIGTEST
    int i;
    look->surface.pixel_data = g_new(RrPixel32, w*h);
    for (i = 0; i < 10000; ++i) {
        printf("\r%d", i);
        fflush(stdout);
        RrRender(look, w, h);
    }
    exit (0);
#endif

    RrPaint(look, win, w, h);
    done = 0;
    while (!done) {
        XNextEvent(ob_display, &report);
        switch (report.type) {
        case Expose:
            break;
        case ConfigureNotify:
            RrPaint(look, win,
                    report.xconfigure.width,
                    report.xconfigure.height);
            break;
        case UnmapNotify:
            done = 1;
            break;
        }
    }

    RrAppearanceFree (look);
    RrInstanceFree (inst);

    return 1;
}
