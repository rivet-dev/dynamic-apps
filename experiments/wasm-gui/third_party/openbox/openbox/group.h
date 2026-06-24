/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   group.h for the Openbox window manager
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

#ifndef __group_h
#define __group_h

#include <X11/Xlib.h>
#include <glib.h>

typedef struct _ObGroup ObGroup;

struct _ObClient;

struct _ObGroup
{
    Window leader;

    /* list of clients */
    GSList *members;
};

void group_startup(gboolean reconfig);
void group_shutdown(gboolean reconfig);

ObGroup *group_add(Window leader, struct _ObClient *client);

void group_remove(ObGroup *self, struct _ObClient *client);

#endif
