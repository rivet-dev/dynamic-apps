/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   obt/signal.h for the Openbox window manager
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

#ifndef __obt_signal_h
#define __obt_signal_h

#include <glib.h>

G_BEGIN_DECLS

typedef void (*ObtSignalHandler)(gint signal, gpointer data);

/*! Listen for signals and report them through the default GMainContext within
   main program thread (except signals that require immediate exit).
   The app should not set its own signal handler function or it will interfere
   with this one. */
void obt_signal_listen(void);
/*! Stop listening to signals and clean up */
void obt_signal_stop(void);

/*! Adds a signal handler for a signal.  The callback function @func will be
  called when the signal @sig is fired.  @sig must not be a signal that
  would cause the core to dump as these are handled internally.
 */
void obt_signal_add_callback(gint sig, ObtSignalHandler func, gpointer data);
/*! Removes the most recently added callback with the given function. */
void obt_signal_remove_callback(gint sig, ObtSignalHandler func);

G_END_DECLS

#endif
