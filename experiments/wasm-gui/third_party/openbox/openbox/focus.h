/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   focus.h for the Openbox window manager
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

#ifndef __focus_h
#define __focus_h

#include "misc.h"

#include <X11/Xlib.h>
#include <glib.h>

struct _ObClient;

/*! The client which is currently focused */
extern struct _ObClient *focus_client;

/*! The recent focus order on each desktop */
extern GList *focus_order;

void focus_startup(gboolean reconfig);
void focus_shutdown(gboolean reconfig);

/*! Specify which client is currently focused, this doesn't actually
  send focus anywhere, its called by the Focus event handlers */
void focus_set_client(struct _ObClient *client);

/*! Focus nothing, but let keyboard events be caught. */
void focus_nothing(void);

/*! Call this when you need to focus something! */
struct _ObClient* focus_fallback(gboolean allow_refocus,
                                 gboolean allow_pointer,
                                 gboolean allow_omnipresent,
                                 gboolean focus_lost);

/*! Add a new client into the focus order */
void focus_order_add_new(struct _ObClient *c);

/*! Remove a client from the focus order */
void focus_order_remove(struct _ObClient *c);

/*! Move a client to the top of the focus order */
void focus_order_to_top(struct _ObClient *c);

/*! Move a client to where it would be if it was newly added to the focus order
 */
void focus_order_like_new(struct _ObClient *c);

/*! Move a client to the bottom of the focus order (keeps iconic windows at the
  very bottom always though). */
void focus_order_to_bottom(struct _ObClient *c);

struct _ObClient *focus_order_find_first(guint desktop);

gboolean focus_valid_target(struct _ObClient *ft,
                            guint    desktop,
                            gboolean helper_windows,
                            gboolean iconic_windows,
                            gboolean all_desktops,
                            gboolean nonhilite_windows,
                            gboolean dock_windows,
                            gboolean desktop_windows,
                            gboolean user_request);

#endif
