/*
 *  xfce4-notifyd
 *
 *  Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib/gstdio.h>

#include <gtk/gtk.h>

#ifdef ENABLE_WAYLAND
#include <gdk/gdkwayland.h>
#include <gtk-layer-shell.h>
#endif

#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include "xfce-notify-daemon.h"

int
main(int argc,
     char **argv)
{
    XfceNotifyDaemon *xndaemon;
    GError *error = NULL;

    xfconf_init(NULL);
 	
    gtk_init(&argc, &argv);

    xfce_textdomain(GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    if(argc > 1) {
        if(!strcmp(argv[1], "--version") || !strcmp(argv[1], "-V")) {
            g_print("%s %s\n", _("Xfce Notify Daemon"), VERSION);
            return 0;
        } else {
            g_printerr(_("Unknown option \"%s\"\n"), argv[1]);
            return 1;
        }
    }

#ifdef ENABLE_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY(gdk_display_get_default())) {
        if (!gtk_layer_is_supported()) {
            g_error("Your Wayland compositor does not support the layer-shell protocol, which is required");
            return 1;
        }
    }
#endif

    xndaemon = xfce_notify_daemon_new_unique(&error);
    if(!xndaemon) {
        xfce_message_dialog(NULL, _("Xfce Notify Daemon"),
                            "dialog-error",
                            _("Unable to start notification daemon"),
                            error->message,
                            "application-exit", GTK_RESPONSE_ACCEPT,
                            NULL);
        g_error_free(error);
        return 1;
    }

    gtk_main();

    g_object_unref(G_OBJECT(xndaemon));

    return 0;
}
