/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or(at your option) any later version.
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
#include <unistd.h>
#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>

static gboolean
server_has_persistence (void)
{
        gboolean has;
        GList   *caps;
        GList   *l;

        caps = notify_get_server_caps ();
        if (caps == NULL) {
                fprintf (stderr, "Failed to receive server caps.\n");
                return FALSE;
        }

        l = g_list_find_custom (caps, "persistence", (GCompareFunc)strcmp);
        has = l != NULL;

        g_list_foreach (caps, (GFunc) g_free, NULL);
        g_list_free (caps);

        return has;
}

static void
install_callback (NotifyNotification *n,
                  const char         *action)
{
        g_assert (action != NULL);
        g_assert (strcmp (action, "install") == 0);

        printf ("You clicked Install\n");

        gtk_main_quit ();
}

int
main (int argc, char *argv[])
{
        NotifyNotification *n;

        gtk_init (&argc, &argv);
        notify_init ("Persistence Test");

        n = notify_notification_new ("Software Updates Available",
                                     "Important updates for your apps are now available.",
                                     "software-update-available-symbolic");
        notify_notification_add_action (n,
                                        "install",
                                        "Install now",
                                        (NotifyActionCallback) install_callback,
                                        NULL,
                                        NULL);

        notify_notification_set_timeout (n, 0); //don't timeout
        notify_notification_show (n, NULL);

        if (!server_has_persistence ()) {
                GtkStatusIcon *icon;
                g_message ("Server does not support persistence; using a status icon");
                icon = gtk_status_icon_new_from_icon_name ("software-update-available-symbolic");
                gtk_status_icon_set_tooltip_markup (icon, "<b>Software Updates Available</b>\nImportant updates for your apps are now available.");
                gtk_status_icon_set_visible (icon, TRUE);
        } else {
                g_message ("Server supports persistence; status icon not needed");
        }

        gtk_main ();

        return 0;
}
