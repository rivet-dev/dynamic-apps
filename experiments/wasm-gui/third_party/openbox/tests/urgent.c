/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   urgent.c for the Openbox window manager
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
#include <X11/Xutil.h>

int main () {
  Display   *display;
  Window     win;
  XEvent     report;
  Atom       _net_fs, _net_state;
  XEvent     msg;
  int        x=50,y=50,h=100,w=400;
  XWMHints   hint;

  display = XOpenDisplay(NULL);

  if (display == NULL) {
    fprintf(stderr, "couldn't connect to X server :0\n");
    return 0;
  }

  _net_state = XInternAtom(display, "_NET_WM_STATE", False);
  _net_fs = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);

  win = XCreateWindow(display, RootWindow(display, 0),
		      x, y, w, h, 10, CopyFromParent, CopyFromParent,
		      CopyFromParent, 0, NULL);

  XSetWindowBackground(display,win,WhitePixel(display,0));

  XMapWindow(display, win);
  XFlush(display);
  sleep(1);

  printf("urgent on\n");
  hint.flags = XUrgencyHint;
  XSetWMHints(display, win, &hint);
  XFlush(display);

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
