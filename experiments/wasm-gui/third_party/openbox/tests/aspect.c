/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   aspect.c for the Openbox window manager
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
  XSetWindowAttributes xswa;
  unsigned long        xswamask;
  Display   *display;
  Window     win;
  XEvent     report;
  int        x=10,y=10,h=100,w=400;
  XSizeHints size;

  display = XOpenDisplay(NULL);

  if (display == NULL) {
    fprintf(stderr, "couldn't connect to X server :0\n");
    return 0;
  }

  xswa.win_gravity = StaticGravity;
  xswamask = CWWinGravity;

  win = XCreateWindow(display, RootWindow(display, 0),
		      x, y, w, h, 10, CopyFromParent, CopyFromParent,
		      CopyFromParent, xswamask, &xswa);

  XSetWindowBackground(display,win,WhitePixel(display,0));

  size.flags = PAspect;
  size.min_aspect.x = 3;
  size.min_aspect.y = 3;
  size.max_aspect.x = 3;
  size.max_aspect.y = 3;
  XSetWMNormalHints(display, win, &size);

  XMapWindow(display, win);
  XFlush(display);

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
