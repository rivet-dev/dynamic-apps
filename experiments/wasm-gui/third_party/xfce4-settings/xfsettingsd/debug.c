/*
 *  Copyright (c) 2011 Nick Schermer <nick@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib.h>

#include "debug.h"



static const GDebugKey dbg_keys[] =
{
    { "xsettings",  XFSD_DEBUG_XSETTINGS },
    { "fontconfig", XFSD_DEBUG_FONTCONFIG },
    { "keyboard-layout", XFSD_DEBUG_KEYBOARD_LAYOUT },
    { "keyboards", XFSD_DEBUG_KEYBOARDS },
    { "keyboard-shortcuts", XFSD_DEBUG_KEYBOARD_SHORTCUTS },
    { "workspaces", XFSD_DEBUG_WORKSPACES },
    { "accessibility", XFSD_DEBUG_ACCESSIBILITY },
    { "pointers", XFSD_DEBUG_POINTERS },
    { "displays", XFSD_DEBUG_DISPLAYS },
};


static XfsdDebugDomain
xfsettings_dbg_init (void)
{
    static gboolean         inited = FALSE;
    static XfsdDebugDomain  dbg_domains = 0;
    const gchar            *value;

    if (!inited)
    {
        value = g_getenv ("XFSETTINGSD_DEBUG");
        if (value != NULL && *value != '\0')
        {
            dbg_domains = g_parse_debug_string (value, dbg_keys,
                                                G_N_ELEMENTS (dbg_keys));

            dbg_domains |= XFSD_DEBUG_YES;
        }

        inited = TRUE;
    }

    return dbg_domains;
}



static void __attribute__((format (gnu_printf, 2,0)))
xfsettings_dbg_print (XfsdDebugDomain  domain,
                      const gchar     *message,
                      va_list          args)
{
    const gchar *domain_name = NULL;
    guint        i;
    gchar       *string;

    /* lookup domain name */
    for (i = 0; i < G_N_ELEMENTS (dbg_keys); i++)
    {
        if (dbg_keys[i].value == domain)
        {
            domain_name = dbg_keys[i].key;
            break;
        }
    }

    g_assert (domain_name != NULL);

    string = g_strdup_vprintf (message, args);
    g_printerr (PACKAGE_NAME "(%s): %s\n", domain_name, string);
    g_free (string);
}



void
xfsettings_dbg (XfsdDebugDomain  domain,
                const gchar     *message,
                ...)
{
    va_list args;

    g_return_if_fail (message != NULL);

    /* leave when debug is disabled */
    if (xfsettings_dbg_init () == 0)
        return;

    va_start (args, message);
    xfsettings_dbg_print (domain, message, args);
    va_end (args);
}



void
xfsettings_dbg_filtered (XfsdDebugDomain  domain,
                         const gchar     *message,
                         ...)
{
    va_list args;

    g_return_if_fail (message != NULL);

    /* leave when the filter does not match */
    if ((xfsettings_dbg_init () & domain) == 0)
        return;

    va_start (args, message);
    xfsettings_dbg_print (domain, message, args);
    va_end (args);
}
