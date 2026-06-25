/*
 *  xfce4-notifyd
 *
 *  Copyright (c) 2016 Simon Steinbeiß <ochosi@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

 /* The following tests will try to determine if the notification server acts in
    conformance with the following wording of the freedesktop.org specification:
    "An implementation which only displays one image or icon must choose which
     one to display using the following order:"
      "image-data"
      "image-path"
      app_icon parameter
      for compatibility reason, "icon_data" */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gprintf.h>
#include <stdlib.h>
#include <libnotify/notify.h>
#include <gtk/gtk.h>

#define IMAGE_DATA "applications-internet"
#define IMAGE_PATH "applications-games"
#define APP_ICON "applications-graphics"
#define SYMBOLIC_ICON "audio-volume-medium-symbolic"

static gboolean
show_notification (NotifyNotification *notification) {
    if (!notify_notification_show (notification, NULL))
    {
        g_error ("Failed");
        g_object_unref (notification);

        return EXIT_FAILURE;
    }
    g_object_unref (notification);
    return EXIT_SUCCESS;
}

int main (int argc, char **argv)
{
    NotifyNotification *notification;
    gint scale_factor;
    GdkPixbuf *image_data;
    gchar test_body[60];

    if (!notify_init ("Notification with icon tests"))
    {
        g_error ("Failed to initialize libnotify.");

        return EXIT_FAILURE;
    }

    gtk_init(&argc, &argv);

    scale_factor = gdk_monitor_get_scale_factor (gdk_display_get_primary_monitor (gdk_display_get_default ()));
    image_data = gtk_icon_theme_load_icon_for_scale (gtk_icon_theme_get_default (),
                                                     IMAGE_DATA,
                                                     48, scale_factor,
                                                     GTK_ICON_LOOKUP_GENERIC_FALLBACK
                                                     | GTK_ICON_LOOKUP_FORCE_SIZE, NULL);

    g_print ("%s\n * %s\n * %s\n * %s\n * %s\n\n",
             "The following icons are required in your icon-theme for these tests to work:",
             APP_ICON, IMAGE_DATA, IMAGE_PATH, SYMBOLIC_ICON
            );

    g_print ("General icon tests:\n");

    g_print ("%s", " * Testing notification with app_icon parameter\n");
    g_snprintf(test_body, sizeof (test_body), "%s should be shown", APP_ICON);
    notification = notify_notification_new ("Test app_icon support",
                                            test_body,
                                            APP_ICON);
    show_notification (notification);
    g_usleep (500000);

    g_print ("%s", " * Testing notification with the image-path hint\n");
    g_snprintf(test_body, sizeof (test_body), "%s should be shown", IMAGE_PATH);
    notification = notify_notification_new ("Test 'image-path' hint support",
                                            test_body,
                                            NULL);
    notify_notification_set_hint_string (notification,
                                         "image-path",
                                         IMAGE_PATH);
    show_notification (notification);
    g_usleep (500000);

    g_print ("%s", " * Testing notification with the image-data hint\n\n");
    g_snprintf(test_body, sizeof (test_body), "%s should be shown", IMAGE_DATA);
    notification = notify_notification_new ("Test 'image-data' and 'image_data' hint support",
                                            test_body,
                                            NULL);
    notify_notification_set_image_from_pixbuf (notification, image_data);
    show_notification (notification);
    g_usleep (500000);

    /* The priority tests are still dummies. Need to decide whether to ship our own
       icons for testing or whether to use standard named icons and hope that they're
       installed or available in the currently selected theme and its fallbacks. */
    g_print ("Icon priority tests:\n");
    g_print ("%s", " * Testing priorities with app_icon versus image-path\n");
    g_snprintf(test_body, sizeof (test_body), "%s should be shown", IMAGE_PATH);
    notification = notify_notification_new ("Test priorities: app_icon vs. image-path",
                                            test_body,
                                            APP_ICON);
    notify_notification_set_hint_string (notification,
                                         "image-path",
                                         IMAGE_PATH);
    show_notification (notification);
    g_usleep (500000);

    g_print ("%s", " * Testing priorities with app_icon versus image-data\n");
    g_snprintf(test_body, sizeof (test_body), "%s should be shown", IMAGE_DATA);
    notification = notify_notification_new ("Test priorities: app_icon vs. image-data",
                                            test_body,
                                            APP_ICON);
    notify_notification_set_image_from_pixbuf (notification, image_data);
    show_notification (notification);
    g_usleep (500000);

    g_print ("%s", " * Testing priorities with image-path versus image-data\n");
    g_snprintf(test_body, sizeof (test_body), "%s should be shown", IMAGE_DATA);
    notification = notify_notification_new ("Test priorities: image-path vs. image-data",
                                            test_body,
                                            NULL);
    notify_notification_set_hint_string (notification,
                                         "image-path",
                                         IMAGE_PATH);
    notify_notification_set_image_from_pixbuf (notification, image_data);
    show_notification (notification);
    g_usleep (500000);

    g_print ("%s", " * Testing priorities with app_icon vs. image-path vs. image-data\n\n");
    g_snprintf(test_body, sizeof (test_body), "%s should be shown", IMAGE_DATA);
    notification = notify_notification_new ("Test priorities: app_icon vs. image-path vs. image-data",
                                            test_body,
                                            APP_ICON);
    notify_notification_set_hint_string (notification,
                                         "image-path",
                                         IMAGE_PATH);
    notify_notification_set_image_from_pixbuf (notification, image_data);
    show_notification (notification);
    g_usleep (500000);

    g_print ("%s", "Testing support for symbolic icons:\n * This test partly depends on the capabilities of your currently selected notifyd and icon theme.\n");
    notification = notify_notification_new ("Test support for symbolic icons",
                                            "Is it correctly colored?",
                                            SYMBOLIC_ICON);
    show_notification (notification);

    if (image_data)
        g_object_unref (image_data);

    return EXIT_SUCCESS;
}
