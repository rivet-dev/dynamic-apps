/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   title.c for the Openbox window manager
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

#include <string.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

int main (int argc, char **argv) {
  Display   *display;
  Window     win;
  XEvent     report;
  int        x=10,y=10,h=100,w=400;
  XSizeHints size;
  XTextProperty name;
  Atom nameprop,nameenc;

  if (argc < 2) return 1;

  display = XOpenDisplay(NULL);

  if (display == NULL) {
    fprintf(stderr, "couldn't connect to X server :0\n");
    return 0;
  }

  if (argc > 2)
    nameprop = XInternAtom(display,argv[2],False);
  else
    nameprop = XInternAtom(display,"WM_NAME",False);
  if (argc > 3)
    nameenc = XInternAtom(display,argv[3],False);
  else
    nameenc = XInternAtom(display,"STRING",False);

  win = XCreateWindow(display, RootWindow(display, 0),
		      x, y, w, h, 10, CopyFromParent, CopyFromParent,
		      CopyFromParent, 0, NULL);

  XSetWindowBackground(display,win,WhitePixel(display,0));

//  XStringListToTextProperty(&argv[1], 1, &name);
//  XSetWMName(display, win, &name);
  XChangeProperty(display, win, nameprop, nameenc, 8,
                  PropModeAppend, argv[1], strlen(argv[1]));

  XFlush(display);
  XMapWindow(display, win);

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
