/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   session.h for the Openbox window manager
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

#ifndef __ob__session_h
#define __ob__session_h

#include "client.h"
#include "screen.h"

#include <glib.h>

typedef struct _ObSessionState ObSessionState;

struct _ObSessionState {
    gchar *id, *command, *name, *class, *role;
    ObClientType type;
    guint desktop;
    gint x, y, w, h;
    gboolean shaded, iconic, skip_pager, skip_taskbar, fullscreen;
    gboolean above, below, max_horz, max_vert, undecorated;
    gboolean focused;

    gboolean matched;
};

/*! The desktop being viewed when the session was saved. A valud of -1 means
  it was not saved */
extern gint session_desktop;
extern gint session_num_desktops;
extern gboolean session_desktop_layout_present;
extern ObDesktopLayout session_desktop_layout;
extern GSList *session_desktop_names;

extern GList *session_saved_state;

void session_startup(gint argc, gchar **argv);
void session_shutdown(gboolean permanent);

GList* session_state_find(struct _ObClient *c);

void session_request_logout(gboolean silent);

gboolean session_connected(void);

#endif
