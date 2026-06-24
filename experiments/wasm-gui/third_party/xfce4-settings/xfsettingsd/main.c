/*
 *  Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
 *  Copyright (c) 2008 Nick Schermer <nick@xfce.org>
 *  Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>
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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#endif

#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include <locale.h>

#include "debug.h"
#include "accessibility.h"
#include "pointers.h"
#include "keyboards.h"
#include "keyboard-layout.h"
#include "keyboard-shortcuts.h"
#include "workspaces.h"
#include "clipboard-manager.h"
#include "gtk-decorations.h"
#include "xsettings.h"

#ifdef HAVE_XRANDR
#include "displays.h"
#endif

#define XFSETTINGS_DBUS_NAME    "org.xfce.SettingsDaemon"
#define XFSETTINGS_DESKTOP_FILE (SYSCONFIGDIR "/xdg/autostart/xfsettingsd.desktop")

#define UNREF_GOBJECT(obj) \
    if (obj) \
    g_object_unref (G_OBJECT(obj))

static gboolean opt_version = FALSE;
static gboolean opt_daemon = FALSE;
static gboolean opt_disable_wm_check = FALSE;
static gboolean opt_replace = FALSE;
static guint owner_id;

struct t_data_set
{
    XfceSMClient         *sm_client;
    GObject              *pointer_helper;
    GObject              *keyboards_helper;
    GObject              *accessibility_helper;
    GObject              *shortcuts_helper;
    GObject              *keyboard_layout_helper;
    GObject              *gtk_decorations_helper;
    GObject              *xsettings_helper;
    GObject              *clipboard_daemon;
#ifdef HAVE_XRANDR
    GObject              *displays_helper;
#endif
    GObject              *workspaces_helper;
};


static GOptionEntry option_entries[] =
{
    { "version", 'V', 0, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { "daemon", 0, 0, G_OPTION_ARG_NONE, &opt_daemon, N_("Fork to the background"), NULL },
    { "disable-wm-check", 'D', 0, G_OPTION_ARG_NONE, &opt_disable_wm_check, N_("Do not wait for a window manager on startup"), NULL },
    { "replace", 0, 0, G_OPTION_ARG_NONE, &opt_replace, N_("Replace running xsettings daemon (if any)"), NULL },
    { NULL }
};

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{

    g_printerr (G_LOG_DOMAIN ": %s\n", "Another instance took over. Leaving...");
    gtk_main_quit ();
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
    GBusNameOwnerFlags         dbus_flags;
    struct t_data_set         *s_data;
    GError                    *error = NULL;

    s_data = (struct t_data_set*) user_data;

    /* launch settings manager */
    s_data->xsettings_helper = g_object_new (XFCE_TYPE_XSETTINGS_HELPER, NULL);
    xfce_xsettings_helper_register (XFCE_XSETTINGS_HELPER (s_data->xsettings_helper),
                                    gdk_display_get_default (), opt_replace);

    /* create the sub daemons */
#ifdef HAVE_XRANDR
    s_data->displays_helper = g_object_new (XFCE_TYPE_DISPLAYS_HELPER, NULL);
#endif
    s_data->pointer_helper = g_object_new (XFCE_TYPE_POINTERS_HELPER, NULL);
    s_data->keyboards_helper = g_object_new (XFCE_TYPE_KEYBOARDS_HELPER, NULL);
    s_data->accessibility_helper = g_object_new (XFCE_TYPE_ACCESSIBILITY_HELPER, NULL);
    s_data->shortcuts_helper = g_object_new (XFCE_TYPE_KEYBOARD_SHORTCUTS_HELPER, NULL);
    s_data->keyboard_layout_helper = g_object_new (XFCE_TYPE_KEYBOARD_LAYOUT_HELPER, NULL);
#ifdef GDK_WINDOWING_X11
    xfce_workspaces_helper_disable_wm_check (opt_disable_wm_check);
#endif
    s_data->workspaces_helper = g_object_new (XFCE_TYPE_WORKSPACES_HELPER, NULL);
    s_data->gtk_decorations_helper = g_object_new (XFCE_TYPE_DECORATIONS_HELPER, NULL);

    /* connect to session always, even if we quit below.  this way the
     * session manager won't wait for us to time out. */
    s_data->sm_client = xfce_sm_client_get ();
    xfce_sm_client_set_restart_style (s_data->sm_client, XFCE_SM_CLIENT_RESTART_IMMEDIATELY);
    xfce_sm_client_set_desktop_file (s_data->sm_client, XFSETTINGS_DESKTOP_FILE);
    xfce_sm_client_set_priority (s_data->sm_client, 20);
    g_signal_connect (G_OBJECT (s_data->sm_client), "quit", G_CALLBACK (gtk_main_quit), NULL);
    if (!xfce_sm_client_connect (s_data->sm_client, &error) && error)
    {
        g_printerr ("Failed to connect to session manager: %s\n", error->message);
        g_clear_error (&error);
    }

    if (g_getenv ("XFSETTINGSD_NO_CLIPBOARD") == NULL)
    {
        s_data->clipboard_daemon = g_object_new (GSD_TYPE_CLIPBOARD_MANAGER, NULL);
        if (!gsd_clipboard_manager_start (GSD_CLIPBOARD_MANAGER (s_data->clipboard_daemon), opt_replace))
        {
            UNREF_GOBJECT (G_OBJECT (s_data->clipboard_daemon));
            s_data->clipboard_daemon = NULL;

            g_printerr (G_LOG_DOMAIN ": %s\n", "Another clipboard manager is already running.");
        }
    }

    /* Update the name flags to allow replacement */
    dbus_flags = G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT;
    g_bus_own_name_on_connection (connection, XFSETTINGS_DBUS_NAME, dbus_flags, NULL, NULL, NULL, NULL );
}

static void
signal_handler (gint signum,
                gpointer user_data)
{
    /* quit the main loop */
    gtk_main_quit ();
}

static gint
daemonize (void)
{
#ifdef HAVE_DAEMON
    return daemon (1, 1);
#else
    pid_t pid;

    pid = fork ();
    if (pid < 0)
        return -1;

    if (pid > 0)
        _exit (EXIT_SUCCESS);

#ifdef HAVE_SETSID
    if (setsid () < 0)
        return -1;
#endif

    return 0;
#endif
}



gint
main (gint argc, gchar **argv)
{
    GError               *error = NULL;
    GOptionContext       *context;
    struct t_data_set     s_data;
    guint                 i;
    const gint            signums[] = { SIGQUIT, SIGTERM };
    GDBusConnection      *dbus_connection;
    GBusNameOwnerFlags    dbus_flags;
    gboolean              name_owned;
    GVariant             *name_owned_variant;

    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    context = g_option_context_new (NULL);
    g_option_context_add_main_entries (context, option_entries, GETTEXT_PACKAGE);
    /* We can't add the following command because it will invoke gtk_init
       before we have a chance to fork.
       g_option_context_add_group (context, gtk_get_option_group (FALSE));
    */
    g_option_context_add_group (context, xfce_sm_client_get_option_group (argc, argv));
    g_option_context_set_ignore_unknown_options(context, TRUE);

    /* parse options */
    if (!g_option_context_parse (context, &argc, &argv, &error))
    {
        g_print ("%s: %s.\n", G_LOG_DOMAIN, error->message);
        g_print (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
        g_print ("\n");

        g_error_free (error);
        g_option_context_free (context);

        return EXIT_FAILURE;
    }

    g_option_context_free (context);

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

    /* daemonize the process */
    if (opt_daemon)
    {
        if (daemonize () == -1)
        {
            /* show message and continue in normal mode */
            g_warning ("Failed to fork the process: %s. Continuing in non-daemon mode.", g_strerror (errno));
        }
    }

    if (!gtk_init_check (&argc, &argv))
    {
        if (G_LIKELY (error))
        {
            g_printerr ("%s: %s.\n", G_LOG_DOMAIN, error->message);
            g_printerr (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
            g_printerr ("\n");
            g_error_free (error);
        }
        else
        {
            g_error ("Unable to open display.");
        }

        return EXIT_FAILURE;
    }

    setlocale(LC_NUMERIC,"C");

    /* Initialize our data set */
    memset (&s_data, 0, sizeof (struct t_data_set));

    dbus_connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
    if (G_LIKELY (!error))
    {
        g_object_set(G_OBJECT (dbus_connection), "exit-on-close", TRUE, NULL);

        name_owned_variant = g_dbus_connection_call_sync (dbus_connection,
                                                          "org.freedesktop.DBus",
                                                          "/org/freedesktop/DBus",
                                                          "org.freedesktop.DBus",
                                                          "NameHasOwner",
                                                          g_variant_new ("(s)", XFSETTINGS_DBUS_NAME),
                                                          G_VARIANT_TYPE ("(b)"),
                                                          G_DBUS_CALL_FLAGS_NONE,
                                                          -1,
                                                          NULL,
                                                          &error);

        if (G_UNLIKELY (error)) {
            g_printerr ("%s: %s.\n", G_LOG_DOMAIN, error->message);
            g_error_free (error);
            return EXIT_FAILURE;
        }

        name_owned = FALSE;
        g_variant_get(name_owned_variant, "(b)", &name_owned, NULL);

        if(G_UNLIKELY (name_owned && !opt_replace)) {
            xfsettings_dbg (XFSD_DEBUG_XSETTINGS, "Another instance is already running. Leaving.");
            g_dbus_connection_close_sync (dbus_connection, NULL, NULL);
            return EXIT_SUCCESS;
        }

        /* Allow the settings daemon to be replaced */
        dbus_flags = G_BUS_NAME_OWNER_FLAGS_NONE;
        if (opt_replace || name_owned)
            dbus_flags = G_BUS_NAME_OWNER_FLAGS_REPLACE ;

        owner_id = g_bus_own_name_on_connection (dbus_connection, XFSETTINGS_DBUS_NAME, dbus_flags, on_name_acquired, on_name_lost, &s_data, NULL );
    }
    else
    {
        g_printerr ("%s: %s.\n", G_LOG_DOMAIN, error->message);
        g_error ("Failed to connect to the dbus session bus.");
        g_error_free (error);
        return EXIT_FAILURE;
    }

    if (!xfconf_init (&error))
    {
        g_error ("Failed to connect to xfconf daemon: %s.", error->message);
        g_error_free (error);

        return EXIT_FAILURE;
    }

    /* setup signal handlers to properly quit the main loop */
    if (xfce_posix_signal_handler_init (NULL))
    {
        for (i = 0; i < G_N_ELEMENTS (signums); i++)
            xfce_posix_signal_handler_set_handler (signums[i], signal_handler, NULL, NULL);
    }

    gtk_main();

    /* release the sub daemons */
    UNREF_GOBJECT(s_data.xsettings_helper);

#ifdef HAVE_XRANDR
    UNREF_GOBJECT (s_data.displays_helper);
#endif
    UNREF_GOBJECT (s_data.pointer_helper);
    UNREF_GOBJECT (s_data.keyboards_helper);
    UNREF_GOBJECT (s_data.accessibility_helper);
    UNREF_GOBJECT (s_data.shortcuts_helper);
    UNREF_GOBJECT (s_data.keyboard_layout_helper);
    UNREF_GOBJECT (s_data.workspaces_helper);
    UNREF_GOBJECT (s_data.gtk_decorations_helper);

    if (G_LIKELY (s_data.clipboard_daemon != NULL))
    {
        gsd_clipboard_manager_stop (GSD_CLIPBOARD_MANAGER (s_data.clipboard_daemon));
        UNREF_GOBJECT (s_data.clipboard_daemon);
    }

    xfconf_shutdown ();

    UNREF_GOBJECT (s_data.sm_client);

    /* release the dbus name */
    if (dbus_connection != NULL)
    {
        g_bus_unown_name (owner_id);
        g_dbus_connection_close_sync (dbus_connection, NULL, NULL);
    }

    return EXIT_SUCCESS;
}
