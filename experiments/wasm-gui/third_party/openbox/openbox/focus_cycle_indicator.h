/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   focus_cycle_indicator.h for the Openbox window manager
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

#ifndef __focus_cycle_indicator_h
#define __focus_cycle_indicator_h

struct _ObClient;

void focus_cycle_indicator_startup(gboolean reconfig);
void focus_cycle_indicator_shutdown(gboolean reconfig);

void focus_cycle_update_indicator(struct _ObClient *c);
void focus_cycle_draw_indicator(struct _ObClient *c);

#endif
