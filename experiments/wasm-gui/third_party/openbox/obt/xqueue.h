/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   obt/xqueue.h for the Openbox window manager
   Copyright (c) 2010        Dana Jansens

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

#ifndef __obt_xqueue_h
#define __obt_xqueue_h

#include <glib.h>
#include <X11/Xlib.h>

G_BEGIN_DECLS

typedef struct _ObtXQueueWindowType {
    Window window;
    int type;
} ObtXQueueWindowType;

typedef struct _ObtXQueueWindowMessage {
    Window window;
    Atom message;
} ObtXQueueWindowMessage;

typedef gboolean (*xqueue_match_func)(XEvent *e, gpointer data);

/*! Returns TRUE if the event matches the window pointed to by @data */
gboolean xqueue_match_window(XEvent *e, gpointer data);

/*! Returns TRUE if the event matches the type contained in the value of @data */
gboolean xqueue_match_type(XEvent *e, gpointer data);

/*! Returns TRUE if the event matches the type and window in the
  ObtXQueueWindowType pointed to by @data */
gboolean xqueue_match_window_type(XEvent *e, gpointer data);

/*! Returns TRUE if a ClientMessage event matches the message and window in the
  ObtXQueueWindowMessage pointed to by @data */
gboolean xqueue_match_window_message(XEvent *e, gpointer data);

/*! Returns TRUE and passes the next event in the queue and removes it from
  the queue.  On error, returns FALSE */
gboolean xqueue_next(XEvent *event_return);

/*! Returns TRUE and passes the next event in the local queue and removes it
  from the queue.  If no event is in the local queue, it returns FALSE. */
gboolean xqueue_next_local(XEvent *event_return);

/*! Returns TRUE if there is anything in the local event queue, and FALSE
  otherwise. */
gboolean xqueue_pending_local(void);

/*! Returns TRUE and passes the next event in the queue, or FALSE if there
  is an error */
gboolean xqueue_peek(XEvent *event_return);

/*! Returns TRUE and passes the next event in the queue, if there is one,
  and returns FALSE otherwise. */
gboolean xqueue_peek_local(XEvent *event_return);

/*! Returns TRUE if xqueue_match_func returns TRUE for some event in the
  current event queue or in the stream of events from the server,
  and passes the matching event without removing it from the queue.
  This blocks until an event is found or an error occurs. */
gboolean xqueue_exists(xqueue_match_func match, gpointer data);

/*! Returns TRUE if xqueue_match_func returns TRUE for some event in the
  current event queue, and passes the matching event without removing it
  from the queue. */
gboolean xqueue_exists_local(xqueue_match_func match, gpointer data);

/*! Returns TRUE if xqueue_match_func returns TRUE for some event in the
  current event queue, and passes the matching event while removing it
  from the queue. */
gboolean xqueue_remove_local(XEvent *event_return,
                             xqueue_match_func match, gpointer data);

typedef void (*ObtXQueueFunc)(const XEvent *ev, gpointer data);

/*! Begin listening for X events in the default GMainContext, and feed them
  to the registered callback functions, added with xqueue_add_callback(). */
void xqueue_listen(void);

void xqueue_add_callback(ObtXQueueFunc f, gpointer data);
void xqueue_remove_callback(ObtXQueueFunc f, gpointer data);

G_END_DECLS

#endif
