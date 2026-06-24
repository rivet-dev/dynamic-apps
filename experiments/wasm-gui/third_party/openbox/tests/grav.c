/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   grav.c for the Openbox window manager
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

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

int main () {
  Display   *display;
  Window     win;
  XEvent     report;
  int        x=10,y=10,h=100,w=400,b=10;
  XSizeHints *hints;

  display = XOpenDisplay(NULL);

  if (display == NULL) {
    fprintf(stderr, "couldn't connect to X server :0\n");
    return 0;
  }

  win = XCreateWindow(display, RootWindow(display, 0),
		      x, y, w, h, b, CopyFromParent, CopyFromParent,
		      CopyFromParent, 0, NULL);

  hints = XAllocSizeHints();
  hints->flags = PWinGravity;
  hints->win_gravity = SouthEastGravity;
  XSetWMNormalHints(display, win, hints);
  XFree(hints);

  XSetWindowBackground(display,win,WhitePixel(display,0));

  XMapWindow(display, win);
  XFlush(display);

  w = 600;
  h = 160;
  XMoveResizeWindow(display, win, 1172-w-b*2, 668-h-b*2, w, h);
  XFlush(display);
  sleep(1);
  XResizeWindow(display, win, 900, 275);

  XSelectInput(display, win, ExposureMask | StructureNotifyMask);

  while (1) {
    XNextEvent(display, &report);

    switch (report.type) {
    case Expose:
      printf("exposed\n");
      break;
    case ConfigureNotify:
      x = report.xconfigure.x;
      y = report.xconfigure.y;
      w = report.xconfigure.width;
      h = report.xconfigure.height;
      printf("confignotify %i,%i-%ix%i\n",x,y,w,h);
      break;
    }

  }

  return 1;
}
