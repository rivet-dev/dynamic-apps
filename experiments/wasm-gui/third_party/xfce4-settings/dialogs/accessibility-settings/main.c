/*
 *  Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
 *                     Jannis Pohlmann <jannis@xfce.org>
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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>

#include <gdk/gdkx.h>

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#include "accessibility-dialog_ui.h"



static gint opt_socket_id = 0;
static gboolean opt_version = FALSE;
static GOptionEntry entries[] =
{
    { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &opt_socket_id, N_("Settings manager socket"), N_("SOCKET ID") },
    { "version", 'v', 0, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { NULL }
};



/* global xfconf channel */
static XfconfChannel *accessibility_channel = NULL;
static XfconfChannel *session_channel = NULL;



static void
accessibility_settings_sensitivity (GtkToggleButton *button,
                                    GtkWidget       *box)
{
    gtk_widget_set_sensitive (GTK_WIDGET (box),
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)));
}



static void
accessibility_settings_at (GtkToggleButton *button,
                           GtkBuilder      *builder)
{
  AtkObject  *atkobj;
  GObject    *info_logout;
  GObject    *no_atspi;
  gchar     **atspi;

  info_logout = gtk_builder_get_object (builder, "info-logout");
  no_atspi = gtk_builder_get_object (builder, "info-no-at");

  gtk_widget_hide (GTK_WIDGET (info_logout));
  gtk_widget_hide (GTK_WIDGET (no_atspi));

  if (gtk_toggle_button_get_active (button))
    {
      atspi = xfce_resource_match (XFCE_RESOURCE_CONFIG, "autostart/at-spi-*.desktop", TRUE);
      atkobj = gtk_widget_get_accessible (GTK_WIDGET (button));

      if (atspi == NULL || g_strv_length (atspi) == 0)
        gtk_widget_show (GTK_WIDGET (no_atspi));
      else if (!GTK_IS_ACCESSIBLE (atkobj))
        gtk_widget_show (GTK_WIDGET (info_logout));

      g_strfreev (atspi);
    }
}



static void
accessibility_settings_dialog_configure_widgets (GtkBuilder *builder)
{
    GObject *box, *object;

    /* assistive technologies */
    object = gtk_builder_get_object (builder, "start-at");
    xfconf_g_property_bind (session_channel, "/general/StartAssistiveTechnologies", G_TYPE_BOOLEAN, object, "active");
    g_signal_connect (object, "toggled", G_CALLBACK (accessibility_settings_at), builder);
    accessibility_settings_at (GTK_TOGGLE_BUTTON (object), builder);

    /* Sticky keys */
    object = gtk_builder_get_object (builder, "sticky-keys-enabled");
    box = gtk_builder_get_object (builder, "sticky-keys-box");
    g_signal_connect (object, "toggled", G_CALLBACK (accessibility_settings_sensitivity), box);
    xfconf_g_property_bind (accessibility_channel, "/StickyKeys", G_TYPE_BOOLEAN, object, "active");

    object = gtk_builder_get_object (builder, "sticky-keys-latch-to-lock");
    xfconf_g_property_bind (accessibility_channel, "/StickyKeys/LatchToLock", G_TYPE_BOOLEAN, object, "active");

    object = gtk_builder_get_object (builder, "sticky-keys-two-keys-disable");
    xfconf_g_property_bind (accessibility_channel, "/StickyKeys/TwoKeysDisable", G_TYPE_BOOLEAN, object, "active");

    /* Slow keys */
    object = gtk_builder_get_object (builder, "slow-keys-enabled");
    box = gtk_builder_get_object (builder, "slow-keys-box");
    g_signal_connect (object, "toggled", G_CALLBACK (accessibility_settings_sensitivity), box);
    xfconf_g_property_bind (accessibility_channel, "/SlowKeys", G_TYPE_BOOLEAN, object, "active");

    object = gtk_builder_get_object (builder, "slow-keys-delay");
    xfconf_g_property_bind (accessibility_channel, "/SlowKeys/Delay", G_TYPE_INT, object, "value");

    /* Bounce keys */
    object = gtk_builder_get_object (builder, "bounce-keys-enabled");
    box = gtk_builder_get_object (builder, "bounce-keys-box");
    g_signal_connect (object, "toggled", G_CALLBACK (accessibility_settings_sensitivity), box);
    xfconf_g_property_bind (accessibility_channel, "/BounceKeys", G_TYPE_BOOLEAN, object, "active");

    object = gtk_builder_get_object (builder, "bounce-keys-delay");
    xfconf_g_property_bind (accessibility_channel, "/BounceKeys/Delay", G_TYPE_INT, object, "value");

    /* Mouse keys */
    object = gtk_builder_get_object (builder, "mouse-emulation-enabled");
    box = gtk_builder_get_object (builder, "mouse-emulation-grid");
    g_signal_connect (object, "toggled", G_CALLBACK (accessibility_settings_sensitivity), box);
    xfconf_g_property_bind (accessibility_channel, "/MouseKeys", G_TYPE_BOOLEAN, object, "active");
    gtk_widget_set_sensitive (GTK_WIDGET(box), xfconf_channel_get_bool(accessibility_channel, "/MouseKeys", TRUE));

    object = gtk_builder_get_object (builder, "mouse-emulation-delay");
    xfconf_g_property_bind (accessibility_channel, "/MouseKeys/Delay", G_TYPE_INT, object, "value");

    object = gtk_builder_get_object (builder, "mouse-emulation-interval");
    xfconf_g_property_bind (accessibility_channel, "/MouseKeys/Interval", G_TYPE_INT, object, "value");

    object = gtk_builder_get_object (builder, "mouse-emulation-time-to-max");
    xfconf_g_property_bind (accessibility_channel, "/MouseKeys/TimeToMax", G_TYPE_INT, object, "value");

    object = gtk_builder_get_object (builder, "mouse-emulation-max-speed");
    xfconf_g_property_bind (accessibility_channel, "/MouseKeys/MaxSpeed", G_TYPE_INT, object, "value");

    object = gtk_builder_get_object (builder, "mouse-emulation-curve");
    xfconf_g_property_bind (accessibility_channel, "/MouseKeys/Curve", G_TYPE_INT, object, "value");

    object = gtk_builder_get_object (builder, "find-cursor");
    xfconf_g_property_bind (accessibility_channel, "/FindCursor", G_TYPE_BOOLEAN, object, "active");
}



static void
accessibility_settings_dialog_response (GtkWidget *dialog,
                                        gint       response_id)
{
    if (response_id == GTK_RESPONSE_HELP)
        xfce_dialog_show_help_with_version (GTK_WINDOW (dialog), "xfce4-settings", "accessibility",
                                            NULL, XFCE4_SETTINGS_VERSION_SHORT);
    else
        gtk_main_quit ();
}



gint
main (gint argc, gchar **argv)
{
    GObject    *dialog, *plug_child;
    GtkWidget  *plug;
    GtkBuilder *builder;
    GError     *error = NULL;

    /* setup translation domain */
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    /* initialize Gtk+ */
    if(!gtk_init_with_args (&argc, &argv, NULL, entries, PACKAGE, &error))
    {
        if (G_LIKELY (error))
        {
            /* print error */
            g_print ("%s: %s.\n", G_LOG_DOMAIN, error->message);
            g_print (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
            g_print ("\n");

            /* cleanup */
            g_error_free (error);
        }
        else
        {
            g_error ("Unable to open display.");
        }

        return EXIT_FAILURE;
    }

    /* check if we should print version information */
    if (G_UNLIKELY (opt_version))
    {
        g_print ("%s %s (Xfce %s)\n\n", G_LOG_DOMAIN, PACKAGE_VERSION, xfce_version_string ());
        g_print ("%s\n", "Copyright (c) 2008-2023");
        g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
        g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
        g_print ("\n");

        return EXIT_SUCCESS;
    }

    /* initialize xfconf */
    if (!xfconf_init (&error))
    {
        /* print error and exit */
        g_error ("Failed to connect to xfconf daemon: %s.", error->message);
        g_error_free (error);

        return EXIT_FAILURE;
    }

    /* open the channels */
    accessibility_channel = xfconf_channel_new ("accessibility");
    session_channel = xfconf_channel_new ("xfce4-session");

    /* hook to make sure the libxfce4ui library is linked */
    if (xfce_titled_dialog_get_type () == 0)
        return EXIT_FAILURE;

    /* load the Gtk user-interface file */
    builder = gtk_builder_new ();
    if (gtk_builder_add_from_string (builder, accessibility_dialog_ui,
                                     accessibility_dialog_ui_length, &error) != 0)
    {
        /* Configure widgets */
        accessibility_settings_dialog_configure_widgets (builder);

        if (G_UNLIKELY (opt_socket_id == 0))
        {
            /* Get the dialog widget */
            dialog = gtk_builder_get_object (builder, "dialog");

            g_signal_connect (dialog, "response",
                G_CALLBACK (accessibility_settings_dialog_response), NULL);
            gtk_window_present (GTK_WINDOW (dialog));

            /* To prevent the settings dialog to be saved in the session */
            gdk_x11_set_sm_client_id ("FAKE ID");

            gtk_main ();
        }
        else
        {
            /* Create plug widget */
            plug = gtk_plug_new (opt_socket_id);
            g_signal_connect (plug, "delete-event", G_CALLBACK (gtk_main_quit), NULL);
            gtk_widget_show (plug);

            /* Stop startup notification */
            gdk_notify_startup_complete ();

            /* Get plug child widget */
            plug_child = gtk_builder_get_object (builder, "plug-child");
            xfce_widget_reparent (GTK_WIDGET (plug_child), plug);
            gtk_widget_show (GTK_WIDGET (plug_child));

            /* To prevent the settings dialog to be saved in the session */
            gdk_x11_set_sm_client_id ("FAKE ID");

            /* Enter main loop */
            gtk_main ();
        }
    }
    else
    {
        g_error ("Failed to load the UI file: %s.", error->message);
        g_error_free (error);
    }

    /* Release Builder */
    g_object_unref (G_OBJECT (builder));

    /* release the channels */
    g_object_unref (G_OBJECT (accessibility_channel));
    g_object_unref (G_OBJECT (session_channel));

    /* shutdown xfconf */
    xfconf_shutdown();

    return EXIT_SUCCESS;
}
