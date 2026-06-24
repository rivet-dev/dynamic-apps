/*
 * netstatus-util.c: Utility methods.
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
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

#include <config.h>

#include "netstatus-util.h"

#include <glib-object.h>
#include <glib/gi18n.h>
#include <string.h>

#ifdef G_ENABLE_DEBUG
NetstatusDebugFlags _netstatus_debug_flags = NETSTATUS_DEBUG_NONE;

void
netstatus_setup_debug_flags (void)
{
  const char       *env_str;
  static GDebugKey  debug_keys [] =
    {
      { "polling", NETSTATUS_DEBUG_POLLING }
    };

  env_str = g_getenv ("NETSTATUS_DEBUG");

  if (env_str)
    _netstatus_debug_flags |= g_parse_debug_string (env_str,
						    debug_keys,
						    G_N_ELEMENTS (debug_keys));
}
#endif /* G_ENABLE_DEBUG */

GQuark
netstatus_error_quark (void)
{
  static GQuark error_quark = 0;

  if (error_quark == 0)
    error_quark = g_quark_from_static_string ("netstatus-error-quark");

  return error_quark;
}

/* Remove this if there is ever a boxed type for
 * GError with glib as standard.
 */
GType
netstatus_g_error_get_type (void)
{
  static GType type_id = 0;

  if (type_id == 0)
    type_id = g_boxed_type_register_static ("NetstatusGError",
					    (GBoxedCopyFunc) g_error_copy,
					    (GBoxedFreeFunc) g_error_free);

  return type_id;
}

static NetstatusStats *
netstatus_stats_copy (NetstatusStats *stats)
{
  return (NetstatusStats *)g_memdup (stats, sizeof (NetstatusStats));
}

static void
netstatus_stats_free (NetstatusStats *stats)
{
  g_free (stats);
}

GType
netstatus_stats_get_type (void)
{
  static GType type_id = 0;

  if (type_id == 0)
    type_id = g_boxed_type_register_static ("NetstatusStats",
					    (GBoxedCopyFunc) netstatus_stats_copy,
					    (GBoxedFreeFunc) netstatus_stats_free);

  return type_id;
}

/* Adopt an existing error into our domain.
 */
void
netstatus_adopt_error (GError         *error,
		       NetstatusError  code)
{
  g_return_if_fail (error != NULL);

  error->domain = NETSTATUS_ERROR;
  error->code   = code;
}

void
netstatus_connect_signal_while_alive (gpointer    object,
				      const char *detailed_signal,
				      GCallback   func,
				      gpointer    func_data,
				      gpointer    alive_object)
{
  GClosure *closure;
  GType     type;
  guint     signal_id = 0;
  GQuark    detail = 0;

  type = G_OBJECT_TYPE (object);

  if (!g_signal_parse_name (detailed_signal, type, &signal_id, &detail, FALSE))
    {
      g_warning (G_STRLOC ": unable to parse signal \"%s\" for type \"%s\"",
		 detailed_signal, g_type_name (type));
      return;
    }

  closure = g_cclosure_new (func, func_data, NULL);
  g_object_watch_closure (G_OBJECT (alive_object), closure);
  g_signal_connect_closure_by_id (object, signal_id, detail, closure, FALSE);
}

const char *
netstatus_get_state_string (NetstatusState state)
{
  char *retval = NULL;

  switch (state)
    {
    case NETSTATUS_STATE_DISCONNECTED:
      retval = _("Disconnected");
      break;
    case NETSTATUS_STATE_IDLE:
      retval = _("Idle");
      break;
    case NETSTATUS_STATE_TX:
      retval = _("Sending");
      break;
    case NETSTATUS_STATE_RX:
      retval = _("Receiving");
      break;
    case NETSTATUS_STATE_TX_RX:
      retval = _("Sending/Receiving");
      break;
    case NETSTATUS_STATE_ERROR:
      retval = _("Error");
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  return retval;
}

GList *
netstatus_list_insert_unique (GList *list,
			      char  *str)
{
  GList *l;

  g_return_val_if_fail (str != NULL, list);

  for (l = list; l; l = l->next)
    if (!strcmp (str, l->data))
      return list;

  return g_list_prepend (list, str);
}
