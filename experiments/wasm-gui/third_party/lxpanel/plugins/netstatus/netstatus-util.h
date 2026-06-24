/*
 * Copyright (C) 2003 Sun Microsystems, Inc.
 * Copyright (C) 2004 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.

 * Authors:
 *      Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __NETSTATUS_UTIL_H__
#define __NETSTATUS_UTIL_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define NETSTATUS_ERROR        (netstatus_error_quark ())
#define NETSTATUS_TYPE_G_ERROR (netstatus_g_error_get_type ())
#define NETSTATUS_TYPE_STATS   (netstatus_stats_get_type ())

typedef enum {
  NETSTATUS_STATE_DISCONNECTED = 0,
  NETSTATUS_STATE_IDLE = 1,
  NETSTATUS_STATE_TX = 2,
  NETSTATUS_STATE_RX = 3,
  NETSTATUS_STATE_TX_RX = 4,
  NETSTATUS_STATE_ERROR = 5,
  NETSTATUS_STATE_LAST = 6
} NetstatusState;

typedef enum {
  NETSTATUS_ERROR_NONE             = 0,
  NETSTATUS_ERROR_ICONS            = 1, /* Can't locate the icon files */
  NETSTATUS_ERROR_SOCKET           = 2, /* Can't open socket */
  NETSTATUS_ERROR_STATISTICS       = 3, /* Can't find statistics on the interface */
  NETSTATUS_ERROR_IOCTL_IFFLAGS    = 4, /* SIOCGIFFLAGS failed */
  NETSTATUS_ERROR_IOCTL_IFCONF     = 5, /* SIOCGIFCONF failed */
  NETSTATUS_ERROR_NO_INTERFACES    = 6, /* No interfaces found */
  NETSTATUS_ERROR_WIRELESS_DETAILS = 7  /* Error finding wireless details
                                         * (not an error if iface isn't wireless)
					 */
} NetstatusError;

typedef enum {
  NETSTATUS_DEBUG_NONE    = 0,
  NETSTATUS_DEBUG_POLLING = 1 << 0
} NetstatusDebugFlags;

typedef struct
{
  gulong in_packets;
  gulong out_packets;
  gulong in_bytes;
  gulong out_bytes;
} NetstatusStats;

GQuark               netstatus_error_quark                (void);
GType                netstatus_g_error_get_type           (void);
GType                netstatus_stats_get_type             (void);
void                 netstatus_adopt_error                (GError         *error,
							   NetstatusError  code);

const char *         netstatus_get_state_string           (NetstatusState  state);

GList               *netstatus_list_insert_unique         (GList          *list,
							   char           *str);

void                 netstatus_connect_signal_while_alive (gpointer        object,
							   const char     *detailed_signal,
							   GCallback       func,
							   gpointer        func_data,
							   gpointer        alive_object);

#ifdef G_ENABLE_DEBUG

#include <stdio.h>

extern NetstatusDebugFlags _netstatus_debug_flags;

#ifdef G_HAVE_ISO_VARARGS
#  define dprintf(type, ...) G_STMT_START {                   \
        if (_netstatus_debug_flags & NETSTATUS_DEBUG_##type)  \
                fprintf (stderr, __VA_ARGS__);                \
        } G_STMT_END
#elif defined(G_HAVE_GNUC_VARARGS)
#  define dprintf(type, args...) G_STMT_START {               \
        if (_netstatus_debug_flags & NETSTATUS_DEBUG_##type)  \
                fprintf (stderr, args);                       \
        } G_STMT_END
#endif

void netstatus_setup_debug_flags (void);

#else /* if !defined (G_ENABLE_DEBUG) */

#ifdef G_HAVE_ISO_VARARGS
#  define dprintf(...)
#elif defined(G_HAVE_GNUC_VARARGS)
#  define dprintf(args...)
#endif

#define netstatus_setup_debug_flags()

#endif /* G_ENABLE_DEBUG */

G_END_DECLS

#endif /* __NETSTATUS_UTIL_H__ */
