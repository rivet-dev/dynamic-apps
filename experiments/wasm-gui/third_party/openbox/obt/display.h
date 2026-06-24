/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   obt/display.h for the Openbox window manager
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

#ifndef __obt_display_h
#define __obt_display_h

#include <X11/Xlib.h>
#include <glib.h>

#include <X11/Xutil.h> /* shape.h uses Region which is in here */
#ifdef    XKB
#include <X11/XKBlib.h>
#endif
#ifdef    SHAPE
#include <X11/extensions/shape.h>
#endif
#ifdef    XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#ifdef    XRANDR
#include <X11/extensions/Xrandr.h>
#endif
#ifdef    SYNC
#include <X11/extensions/sync.h>
#endif

G_BEGIN_DECLS

extern gboolean obt_display_error_occured;

extern gboolean obt_display_extension_xkb;
extern gint     obt_display_extension_xkb_basep;
extern gboolean obt_display_extension_shape;
extern gint     obt_display_extension_shape_basep;
extern gboolean obt_display_extension_xinerama;
extern gint     obt_display_extension_xinerama_basep;
extern gboolean obt_display_extension_randr;
extern gint     obt_display_extension_randr_basep;
extern gboolean obt_display_extension_sync;
extern gint     obt_display_extension_sync_basep;

extern Display* obt_display;

/*! Open the X display.  You should call g_set_prgname() before calling this
  function for X Input Methods to work correctly. */
gboolean obt_display_open(const char *display_name);
void     obt_display_close(void);

void     obt_display_ignore_errors(gboolean ignore);

#define  obt_root(screen) (RootWindow(obt_display, screen))

G_END_DECLS

#endif /*__obt_display_h*/
