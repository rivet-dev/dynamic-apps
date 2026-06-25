/*
 *  Copyright (c) Stephan Arts 2006-2012 <stephan@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#include "util.h"
#include "main_window.h"
#include "preferences_dialog.h"

#include <locale.h>

#include <xfconf/xfconf.h>
#include <libxfce4ui/libxfce4ui.h>



gboolean version = FALSE;
gboolean start_fullscreen = FALSE;
gboolean start_slideshow = FALSE;
gboolean show_settings = FALSE;



static gboolean
cb_rstto_open_files (gpointer user_data);



static GOptionEntry entries[] =
{
    {    "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &version,
        N_("Version information"),
        NULL
    },
    {    "fullscreen", 'f', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &start_fullscreen,
        N_("Start in fullscreen mode"),
        NULL
    },
    {    "slideshow", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &start_slideshow,
        N_("Start a slideshow"),
        NULL
    },
    { "settings",
            'S',
            G_OPTION_FLAG_IN_MAIN,
            G_OPTION_ARG_NONE,
            &show_settings,
            N_("Show settings dialog"),
        NULL
    },
    { NULL, ' ', 0, 0, NULL, NULL, NULL }
};



typedef struct {
    gint argc;
    gchar **argv;
    RsttoMainWindow *window;
} RsttoOpenFiles;



int
main (int argc, char **argv)
{
    GError *error = NULL;
    RsttoSettings *settings;
    RsttoImageList *image_list;
    GtkWidget *window;
    gboolean xfconf_disabled;

    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    if (!gtk_init_with_args (&argc, &argv, NULL, entries, PACKAGE, &error))
    {
        if (error != NULL)
        {
            g_printerr (
                    _("%s: %s\n\n"
                      "Try %s --help to see a full list of\n"
                      "available command line options.\n"),
                    PACKAGE,
                    error->message,
                    PACKAGE_NAME);

            g_error_free (error);
            return 1;
        }
    }

    if (version)
    {
        g_print ("%s\n", PACKAGE_STRING);
        return 0;
    }

    xfconf_disabled = ! xfconf_init (&error);
    rstto_settings_set_xfconf_disabled (xfconf_disabled);
    if (xfconf_disabled)
    {
        g_warning ("Failed to initialize Xfconf: %s", error->message);
        g_error_free (error);
    }

    gtk_window_set_default_icon_name (RISTRETTO_APP_ID);
    settings = rstto_settings_new ();

    if (! show_settings)
    {
        image_list = rstto_image_list_new ();
        window = rstto_main_window_new (image_list, FALSE);

        if (argc > 1)
        {
            RsttoOpenFiles rof;

            rof.argc = argc;
            rof.argv = argv;
            rof.window = RSTTO_MAIN_WINDOW (window);

            /* add a weak pointer to guard our handler */
            g_object_add_weak_pointer (G_OBJECT (window), (gpointer *) &(rof.window));
            g_idle_add (cb_rstto_open_files, &rof);

            if (rstto_settings_get_boolean_property (settings, "maximize-on-startup"))
            {
                gtk_window_maximize (GTK_WINDOW (window));
            }
        }

        /* Start fullscreen */
        if (start_fullscreen)
        {
           gtk_window_fullscreen (GTK_WINDOW (window));
        }

        g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
        gtk_widget_show_all (window);

        gtk_main ();

        g_object_unref (image_list);
    }
    else
    {
        window = rstto_preferences_dialog_new (NULL);
        while (gtk_dialog_run (GTK_DIALOG (window)) == GTK_RESPONSE_HELP)
        {
            xfce_dialog_show_help (
                    GTK_WINDOW (window),
                    "ristretto",
                    "preferences",
                    NULL);
        }
        gtk_widget_destroy (window);
    }

    g_object_unref (settings);

    if (! xfconf_disabled)
        xfconf_shutdown ();

    return 0;
}

static gboolean
cb_rstto_open_files (gpointer user_data)
{
    RsttoOpenFiles *rof = user_data;
    GSList *files = NULL;
    GFile *file;
    gint n;

    if (rof->window == NULL)
        return FALSE;

    for (n = 1; n < rof->argc; n++)
    {
        file = g_file_new_for_commandline_arg (rof->argv[n]);
        files = g_slist_prepend (files, file);
    }

    files = g_slist_reverse (files);
    rstto_main_window_open (rof->window, files);
    g_slist_free_full (files, g_object_unref);

    if (start_slideshow)
        rstto_main_window_play_slideshow (rof->window);

    return FALSE;
}
