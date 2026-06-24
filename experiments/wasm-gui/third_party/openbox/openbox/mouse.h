/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   mouse.h for the Openbox window manager
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

#ifndef ob__mouse_h
#define ob__mouse_h

#include "frame.h"
#include "misc.h"

#include <X11/Xlib.h>

struct _ObActionsAct;

void mouse_startup(gboolean reconfig);
void mouse_shutdown(gboolean reconfig);

gboolean mouse_bind(const gchar *buttonstr, ObFrameContext context,
                    ObMouseAction mact, struct _ObActionsAct *action);
void mouse_unbind_all(void);

gboolean mouse_event(struct _ObClient *client, XEvent *e);

void mouse_grab_for_client(struct _ObClient *client, gboolean grab);

ObFrameContext mouse_button_frame_context(ObFrameContext context,
                                          guint button, guint state);

/*! If a replay pointer is needed, then do it.  Call this when windows are
  going to be moving/appearing/disappearing, so that you know the mouse click
  will go to the right window */
void mouse_replay_pointer(void);

#endif
