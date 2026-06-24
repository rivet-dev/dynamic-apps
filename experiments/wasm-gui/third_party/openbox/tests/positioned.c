/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   positioned.c for the Openbox window manager
   Copyright (c) 2007        Dana Jansens

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

#include <string.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

int main (int argc, char **argv) {
    Display    *display;
    Window      win;
    XEvent      report;
    int         x=200,y=200,h=100,w=400,s;
    XSizeHints *size;

    display = XOpenDisplay(NULL);

    if (display == NULL) {
        fprintf(stderr, "couldn't connect to X server :0\n");
        return 0;
    }

    win = XCreateWindow(display, RootWindow(display, 0),
                        x, y, w, h, 0, CopyFromParent, CopyFromParent,
                        CopyFromParent, 0, NULL);
    XSetWindowBackground(display,win,WhitePixel(display,0));

    size = XAllocSizeHints();
    size->flags = PPosition;
    XSetWMNormalHints(display,win,size);
    XFree(size);

    XFlush(display);
    XMapWindow(display, win);

    XSelectInput(display, win, StructureNotifyMask | ButtonPressMask);

    while (1) {
        XNextEvent(display, &report);

        switch (report.type) {
        case ButtonPress:
            XUnmapWindow(display, win);
            break;
        case ConfigureNotify:
            x = report.xconfigure.x;
            y = report.xconfigure.y;
            w = report.xconfigure.width;
            h = report.xconfigure.height;
            s = report.xconfigure.send_event;
            printf("confignotify %i,%i-%ix%i (send: %d)\n",x,y,w,h,s);
            break;
        }

    }

    return 1;
}
