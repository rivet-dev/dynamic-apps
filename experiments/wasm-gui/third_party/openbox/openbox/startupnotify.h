/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   startupnotify.h for the Openbox window manager
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

#ifndef ob__startupnotify_h
#define ob__startupnotify_h

#include <glib.h>
#include <X11/Xlib.h>

void sn_startup(gboolean reconfig);
void sn_shutdown(gboolean reconfig);

gboolean sn_app_starting(void);

/*! Notify that an app has started
  @param wmclass the WM_CLASS[1] hint
  @param name the WM_CLASS[0] hint
 */
Time sn_app_started(const gchar *id, const gchar *wmclass, const gchar *name);

/*! Get the desktop requested via the startup-notiication protocol if one
  was requested */
gboolean sn_get_desktop(gchar *id, guint *desktop);

/* Get the environment to run the program in, with startup notification */
void sn_setup_spawn_environment(const gchar *program, const gchar *name,
                                const gchar *icon_name, const gchar *wmclass,
                                gint desktop);

/* Tell startup notification we're not actually running the program we
   told it we were
*/
void sn_spawn_cancel(void);

#endif
