/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * @file tests/test-server-info.c Retrieves the server info and caps
 *
 * @Copyright (C) 2004 Mike Hearn <mike@navi.cx>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA  02111-1307, USA.
 */

#include <libnotify/notify.h>
#include <stdio.h>
#include <stdlib.h>

int
main (int argc, char **argv)
{
        GList          *l, *caps;
        char           *name, *vendor, *version, *spec_version;

        notify_init ("TestCaps");

        if (!notify_get_server_info (&name, &vendor, &version, &spec_version)) {
                fprintf (stderr, "Failed to receive server info.\n");
                exit (1);
        }

        printf ("Name:         %s\n", name);
        printf ("Vendor:       %s\n", vendor);
        printf ("Version:      %s\n", version);
        printf ("Spec Version: %s\n", spec_version);
        printf ("Capabilities:\n");

        caps = notify_get_server_caps ();

        if (caps == NULL) {
                fprintf (stderr, "Failed to receive server caps.\n");
                exit (1);
        }

        for (l = caps; l != NULL; l = l->next)
                printf ("\t%s\n", (char *) l->data);

        g_list_foreach (caps, (GFunc) g_free, NULL);
        g_list_free (caps);

        return 0;
}
