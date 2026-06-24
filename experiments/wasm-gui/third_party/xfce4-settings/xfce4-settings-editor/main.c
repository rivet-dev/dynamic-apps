/*
 *  Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
 *  Copyright (c) 2015 Ali Abdallah <ali@aliov.org>
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
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>
#include <gtk/gtkx.h>

#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include "xfce-settings-editor-box.h"

/* Main xfconf channel */
XfconfChannel *channel;

/* option entries */
static gint32 opt_socket_id = 0;
static gboolean opt_version = FALSE;

static GOptionEntry option_entries[] =
{
	{ "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &opt_socket_id, N_("Settings manager socket"), N_("SOCKET ID") },
    { "version", 'V', 0, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { NULL }
};

static void
save_window_size (GtkWidget *dialog, XfceSettingsEditorBox *settings_editor)
{
	GdkWindowState            state;
	gint                      width, height;
	gint                      paned_pos;

	g_object_get (G_OBJECT(settings_editor), "paned-pos", &paned_pos, NULL);

	state = gdk_window_get_state (gtk_widget_get_window(dialog));

	if ((state & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN)) == 0)
	{
		/* save window size */
		gtk_window_get_size (GTK_WINDOW (dialog), &width, &height);
		xfconf_channel_set_int (channel, "/last/window-width", width),
			xfconf_channel_set_int (channel, "/last/window-height", height);
		xfconf_channel_set_int (channel, "/last/paned-position", paned_pos);

	}
}

static void
settings_dialog_response (GtkWidget *dialog,
						  gint       response_id,
						  XfceSettingsEditorBox *settings_editor)
{
    if (response_id == GTK_RESPONSE_HELP)
		xfce_dialog_show_help_with_version (GTK_WINDOW (dialog),
                                            "xfce4-settings",
                                            "editor", NULL,
                                            XFCE4_SETTINGS_VERSION_SHORT);
	else
	{
		save_window_size (dialog, settings_editor);
		gtk_main_quit ();
	}
}

static gboolean plug_delete_event (GtkWidget *widget,
								   GdkEvent *ev,
								   XfceSettingsEditorBox *settings_editor)
{
	save_window_size (widget, settings_editor);
	gtk_main_quit ();
	return TRUE;
}

gint
main(gint argc, gchar **argv)
{
	GtkWidget     *dialog;
    GtkWidget     *settings_editor;
	GtkWidget     *plug;
	GError        *error = NULL;

    /* setup translation domain */
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    /* initialize Gtk+ */
    if (!gtk_init_with_args (&argc, &argv, NULL, option_entries, GETTEXT_PACKAGE, &error))
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

    /* print version information */
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
    if (G_UNLIKELY (!xfconf_init (&error)))
    {
        /* print error and leave */
        g_critical ("Failed to connect to Xfconf daemon: %s", error->message);
        g_error_free (error);

        return EXIT_FAILURE;
    }

	channel = xfconf_channel_new ("xfce4-settings-editor");

	settings_editor = xfce_settings_editor_box_new (
		xfconf_channel_get_int (channel, "/last/paned-position", 180));

	if (G_UNLIKELY (opt_socket_id == 0))
    {
		dialog = xfce_titled_dialog_new_with_mixed_buttons (_("Settings Editor"), NULL,
					GTK_DIALOG_DESTROY_WITH_PARENT,
					"help-browser", _("_Help"), GTK_RESPONSE_HELP,
					"window-close-symbolic", _("_Close"), GTK_RESPONSE_OK,
					NULL);

		gtk_window_set_icon_name (GTK_WINDOW (dialog), "org.xfce.settings.editor");
		gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_NORMAL);
		gtk_window_set_default_size (GTK_WINDOW (dialog),
          xfconf_channel_get_int (channel, "/last/window-width", 640),
          xfconf_channel_get_int (channel, "/last/window-height", 500));

		gtk_container_add_with_properties(
			GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
						  settings_editor,
						  "expand", TRUE,
						  "fill", TRUE,
						  NULL);

		g_signal_connect (dialog, "response",
                    G_CALLBACK (settings_dialog_response), settings_editor);

		gtk_widget_show_all (dialog);

	}
	else
	{
		/* Create plug widget */
		plug = gtk_plug_new (opt_socket_id);
		g_signal_connect (plug, "delete-event", G_CALLBACK (plug_delete_event), settings_editor);

		gtk_window_set_default_size (GTK_WINDOW (plug),
		  xfconf_channel_get_int (channel, "/last/window-width", 640),
          xfconf_channel_get_int (channel, "/last/window-height", 500));

		gtk_widget_show (plug);

		gtk_container_add (GTK_CONTAINER(plug), settings_editor);

		/* Stop startup notification */
		gdk_notify_startup_complete ();

		gtk_widget_show (GTK_WIDGET (settings_editor));
	}

    gtk_main ();

	g_object_unref(channel);

    /* shutdown xfconf */
    xfconf_shutdown ();

    return EXIT_SUCCESS;
}
