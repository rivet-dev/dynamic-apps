/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   strut.c for the Openbox window manager
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
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

int main () {
  Display   *display;
  Window     win;
  XEvent     report;
  Atom       _net_strut;
  XEvent     msg;
  int        x=10,y=10,h=100,w=400;
  int        s[4];

  display = XOpenDisplay(NULL);

  if (display == NULL) {
    fprintf(stderr, "couldn't connect to X server :0\n");
    return 0;
  }

  _net_strut = XInternAtom(display, "_NET_WM_STRUT", False);

  win = XCreateWindow(display, RootWindow(display, 0),
		      x, y, w, h, 10, CopyFromParent, CopyFromParent,
		      CopyFromParent, 0, NULL);

  XSetWindowBackground(display,win,WhitePixel(display,0));

  XMapWindow(display, win);
  XFlush(display);
  sleep(2);

  printf("top\n");
  s[0] = 0; s[1] = 0; s[2] = 20; s[3] = 0;
  XChangeProperty(display, win, _net_strut, XA_CARDINAL, 32,
                  PropModeReplace, (unsigned char*) s, 4);
  XFlush(display);
  sleep(2);

  printf("none\n");
  XDeleteProperty(display, win, _net_strut);
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
