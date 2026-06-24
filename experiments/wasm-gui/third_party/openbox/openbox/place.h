/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   place.h for the Openbox window manager
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

#ifndef ob__place_h
#define ob__place_h

#include "geom.h"

#include <glib.h>

struct _ObClient;
struct _ObAppSettings;

typedef enum
{
    OB_PLACE_POLICY_SMART,
    OB_PLACE_POLICY_MOUSE
} ObPlacePolicy;

typedef enum
{
    OB_PLACE_MONITOR_ANY,
    OB_PLACE_MONITOR_ACTIVE,
    OB_PLACE_MONITOR_MOUSE,
    OB_PLACE_MONITOR_PRIMARY,
    OB_PLACE_MONITOR_ALL
} ObPlaceMonitor;

/*! Return TRUE if openbox chose the position for the window, and FALSE if
  the application chose it */
gboolean place_client(struct _ObClient *client,
                      gboolean client_to_be_foregrounded,
                      Rect* client_area, struct _ObAppSettings *settings);

#endif
