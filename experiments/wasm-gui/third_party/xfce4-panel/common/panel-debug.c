/*
 * Copyright (C) 2010-2011 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib.h>
#include <common/panel-debug.h>
#include <common/panel-private.h>



static PanelDebugFlag panel_debug_flags = 0;



/* additional debug levels */
static const GDebugKey panel_debug_keys[] =
{
  /* external plugin proxy modes */
  { "gdb", PANEL_DEBUG_GDB },
  { "valgrind", PANEL_DEBUG_VALGRIND },

  /* domains for debug messages in the code */
  { "application", PANEL_DEBUG_APPLICATION },
  { "applicationsmenu", PANEL_DEBUG_APPLICATIONSMENU },
  { "base-window", PANEL_DEBUG_BASE_WINDOW },
  { "display-layout", PANEL_DEBUG_DISPLAY_LAYOUT },
  { "external", PANEL_DEBUG_EXTERNAL },
  { "main", PANEL_DEBUG_MAIN },
  { "module-factory", PANEL_DEBUG_MODULE_FACTORY },
  { "module", PANEL_DEBUG_MODULE },
  { "positioning", PANEL_DEBUG_POSITIONING },
  { "struts", PANEL_DEBUG_STRUTS },
  { "systray", PANEL_DEBUG_SYSTRAY },
  { "tasklist", PANEL_DEBUG_TASKLIST },
  { "pager", PANEL_DEBUG_PAGER },
  { "itembar", PANEL_DEBUG_ITEMBAR },
  { "clock", PANEL_DEBUG_CLOCK },
};



static PanelDebugFlag
panel_debug_init (void)
{
  static gsize inited__volatile = 0;
  const gchar *value;

  if (g_once_init_enter (&inited__volatile))
    {
      value = g_getenv ("PANEL_DEBUG");
      if (value != NULL && *value != '\0')
        {
          panel_debug_flags = g_parse_debug_string (value, panel_debug_keys,
                                                    G_N_ELEMENTS (panel_debug_keys));

          /* always enable (unfiltered) debugging messages */
          PANEL_SET_FLAG (panel_debug_flags, PANEL_DEBUG_YES);

          /* unset gdb and valgrind in 'all' mode */
          if (g_ascii_strcasecmp (value, "all") == 0)
            PANEL_UNSET_FLAG (panel_debug_flags, PANEL_DEBUG_GDB | PANEL_DEBUG_VALGRIND);
        }

      g_once_init_leave (&inited__volatile, 1);
    }

  return panel_debug_flags;
}



static void
panel_debug_print (PanelDebugFlag  domain,
                   const gchar    *message,
                   va_list         args)
{
  gchar       *string;
  const gchar *domain_name = NULL;
  guint        i;

  /* lookup domain name */
  for (i = 0; i < G_N_ELEMENTS (panel_debug_keys); i++)
    {
      if (panel_debug_keys[i].value == domain)
        {
          domain_name = panel_debug_keys[i].key;
          break;
        }
    }

  panel_assert (domain_name != NULL);

  string = g_strdup_vprintf (message, args);
  g_printerr (PACKAGE_NAME "(%s): %s\n", domain_name, string);
  g_free (string);
}



gboolean
panel_debug_has_domain (PanelDebugFlag domain)
{
  return PANEL_HAS_FLAG (panel_debug_flags, domain);
}



void
panel_debug (PanelDebugFlag  domain,
             const gchar    *message,
             ...)
{
  va_list args;

  panel_return_if_fail (domain > 0);
  panel_return_if_fail (message != NULL);

  /* leave when debug is disabled */
  if (panel_debug_init () == 0)
    return;

  va_start (args, message);
  panel_debug_print (domain, message, args);
  va_end (args);
}



void
panel_debug_filtered (PanelDebugFlag  domain,
                      const gchar    *message,
                      ...)
{
  va_list args;

  panel_return_if_fail (domain > 0);
  panel_return_if_fail (message != NULL);

  /* leave when the filter does not match */
  if (!PANEL_HAS_FLAG (panel_debug_init (), domain))
    return;

  va_start (args, message);
  panel_debug_print (domain, message, args);
  va_end (args);
}
