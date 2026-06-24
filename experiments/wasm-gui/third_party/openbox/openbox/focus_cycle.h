/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   focus_cycle.h for the Openbox window manager
   Copyright (c) 2006        Mikael Magnusson
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

#ifndef __focus_cycle_h
#define __focus_cycle_h

#include "misc.h"
#include "focus_cycle_popup.h"

#include <X11/Xlib.h>
#include <glib.h>

struct _ObClient;

/*! The client which appears focused during a focus cycle operation */
extern struct _ObClient *focus_cycle_target;

void focus_cycle_startup(gboolean reconfig);
void focus_cycle_shutdown(gboolean reconfig);

/*! Cycle focus amongst windows. */
struct _ObClient* focus_cycle(gboolean forward, gboolean all_desktops,
                              gboolean nonhilite_windows,
                              gboolean dock_windows, gboolean desktop_windows,
                              gboolean linear, gboolean showbar,
                              ObFocusCyclePopupMode mode,
                              gboolean done, gboolean cancel);
struct _ObClient* focus_directional_cycle(ObDirection dir,
                                          gboolean dock_windows,
                                          gboolean desktop_windows,
                                          gboolean interactive,
                                          gboolean showbar,
                                          gboolean dialog,
                                          gboolean done, gboolean cancel);

/*! Set @redraw to FALSE if there are more clients to be added/removed first */
void focus_cycle_addremove(struct _ObClient *ifclient, gboolean redraw);
void focus_cycle_reorder();

gboolean focus_cycle_valid(struct _ObClient *client);

#endif
