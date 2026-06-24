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

#ifndef __focus_cycle_popup_h
#define __focus_cycle_popup_h

struct _ObClient;

#include <glib.h>

typedef enum {
    OB_FOCUS_CYCLE_POPUP_MODE_NONE,
    OB_FOCUS_CYCLE_POPUP_MODE_ICONS,
    OB_FOCUS_CYCLE_POPUP_MODE_LIST
} ObFocusCyclePopupMode;

void focus_cycle_popup_startup(gboolean reconfig);
void focus_cycle_popup_shutdown(gboolean reconfig);

void focus_cycle_popup_show(struct _ObClient *c, ObFocusCyclePopupMode mode,
                            gboolean linear);
void focus_cycle_popup_hide(void);

void focus_cycle_popup_single_show(struct _ObClient *c);
void focus_cycle_popup_single_hide(void);

gboolean focus_cycle_popup_is_showing(struct _ObClient *c);

/*! Redraws the focus cycle popup, and returns the current target.  If
    the target given to the function is no longer valid, this will return
    a different target that is valid, and which should be considered the
    current focus cycling target. */
struct _ObClient *focus_cycle_popup_refresh(struct _ObClient *target,
                                            gboolean redraw,
                                            gboolean linear);

#endif
