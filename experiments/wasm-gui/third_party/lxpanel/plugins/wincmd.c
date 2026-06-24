/*
 * Copyright (C) 2006-2008 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *               2006-2008 Jim Huang <jserv.tw@gmail.com>
 *               2008 Fred Chien <fred@lxde.org>
 *               2009-2010 Marty Jack <martyj19@comcast.net>
 *               2012 Michael Rawson <michaelrawson76@gmail.com>
 *               2012 Julien Lavergne <julien.lavergne@gmail.com>
 *               2013 Henry Gebhardt <hsggebhardt@gmail.com>
 *               2014-2016 Andriy Grytsenko <andrej@rep.kiev.ua>
 *
 * This file is a part of LXPanel project.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdlib.h>

#include <glib/gi18n.h>

#include "misc.h"
#include "plugin.h"

/* Commands that can be issued. */
typedef enum {
    WC_NONE,
    WC_ICONIFY,
    WC_SHADE
} WindowCommand;

/* Private context for window command plugin. */
typedef struct {
    config_setting_t *settings;			/* Settings array */
    char * image;				/* Main icon */
    WindowCommand button_1_command;		/* Command for mouse button 1 */
    WindowCommand button_2_command;		/* Command for mouse button 2 */
    gboolean toggle_preference;			/* User preference: toggle iconify/shade and map */
    gboolean toggle_state;			/* State of toggle */
} WinCmdPlugin;

static const char *wincmd_names[] = {
    "none",
    "iconify",
    "shade"
};

static void wincmd_destructor(gpointer user_data);

/* Adjust the toggle state after a window command. */
static void wincmd_adjust_toggle_state(WinCmdPlugin * wc)
{
    /* Ensure that if the user changes the preference from "unconditional" to "toggle", we do a raise on the next click. */
    if (wc->toggle_preference)
        wc->toggle_state = ! wc->toggle_state;
        else wc->toggle_state = TRUE;
}

/* Execute a window command. */
static void wincmd_execute(GdkScreen * screen, WinCmdPlugin * wc, WindowCommand command)
{
    /* Get the list of all windows. */
    int client_count;
    Screen * xscreen = GDK_SCREEN_XSCREEN(screen);
    Window * client_list = get_xaproperty (RootWindowOfScreen(xscreen), a_NET_CLIENT_LIST, XA_WINDOW, &client_count);
    Display *xdisplay = DisplayOfScreen(xscreen);
    if (client_list != NULL)
    {
        /* Loop over all windows. */
        int current_desktop = get_net_current_desktop();
        int i;
        for (i = 0; i < client_count; i++)
        {
            /* Get the desktop and window type properties. */
            NetWMWindowType nwwt;
            int task_desktop = get_net_wm_desktop(client_list[i]);
            get_net_wm_window_type(client_list[i], &nwwt);

            /* If the task is visible on the current desktop and it is an ordinary window,
             * execute the requested Iconify or Shade change. */
            if (((task_desktop == -1) || (task_desktop == current_desktop))
                && (( ! nwwt.dock) && ( ! nwwt.desktop) && ( ! nwwt.splash)))
            {
                switch (command)
                {
                    case WC_NONE:
                        break;

                    case WC_ICONIFY:
                        if (( ! wc->toggle_preference) || ( ! wc->toggle_state))
                            XIconifyWindow(xdisplay, client_list[i], DefaultScreen(xdisplay));
                        else
                            XMapWindow (xdisplay, client_list[i]);
                        break;

                    case WC_SHADE:
                        Xclimsgx(xscreen, client_list[i], a_NET_WM_STATE,
                            ((( ! wc->toggle_preference) || ( ! wc->toggle_state)) ? a_NET_WM_STATE_ADD : a_NET_WM_STATE_REMOVE),
                            a_NET_WM_STATE_SHADED, 0, 0, 0);
                        break;
                }
            }
        }
        XFree(client_list);

	/* Adjust toggle state. */
        wincmd_adjust_toggle_state(wc);
    }
}

/* Handler for "clicked" signal on main widget. */
static gboolean wincmd_button_clicked(GtkWidget * widget, GdkEventButton * event, LXPanel * panel)
{
    WinCmdPlugin * wc = lxpanel_plugin_get_data(widget);

    /* Left-click to iconify. */
    if (event->button == 1)
    {
        GdkScreen* screen = gtk_widget_get_screen(widget);
        Screen *xscreen = GDK_SCREEN_XSCREEN(screen);
        static GdkAtom atom = 0;
        if( G_UNLIKELY(0 == atom) )
            atom = gdk_atom_intern("_NET_SHOWING_DESKTOP", FALSE);

        /* If window manager supports _NET_SHOWING_DESKTOP, use it.
         * Otherwise, fall back to iconifying windows individually. */
        if (gdk_x11_screen_supports_net_wm_hint(screen, atom))
        {
            int showing_desktop = ((( ! wc->toggle_preference) || ( ! wc->toggle_state)) ? 1 : 0);
            Xclimsgx(xscreen, RootWindowOfScreen(xscreen),
                    a_NET_SHOWING_DESKTOP, showing_desktop, 0, 0, 0, 0);
            wincmd_adjust_toggle_state(wc);
        }
        else
            wincmd_execute(screen, wc, WC_ICONIFY);
        return TRUE;
    }

    return FALSE;
}

static gboolean wincmd_button_released(GtkWidget * widget, GdkEventButton * event, WinCmdPlugin * wc)
{
    /* Middle-click to shade. */
    if (event->button == 2)
    {
        wincmd_execute(gtk_widget_get_screen(widget), wc, WC_SHADE);
    }

    return FALSE;
}

/* Plugin constructor. */
static GtkWidget *wincmd_constructor(LXPanel *panel, config_setting_t *settings)
{
    /* Allocate plugin context and set into Plugin private data pointer. */
    WinCmdPlugin * wc = g_new0(WinCmdPlugin, 1);
    GtkWidget * p;
    const char *str;
    int tmp_int;

    /* Initialize to defaults. */
    wc->button_1_command = WC_ICONIFY;
    wc->button_2_command = WC_SHADE;

    /* Load parameters from the configuration file. */
    if (config_setting_lookup_string(settings, "Button1", &str))
    {
        if (g_ascii_strcasecmp(str, "shade") == 0)
            wc->button_1_command = WC_SHADE;
        else if (g_ascii_strcasecmp(str, "none") == 0)
            wc->button_1_command = WC_NONE;
        /* default is WC_ICONIFY */
    }
    if (config_setting_lookup_string(settings, "Button2", &str))
    {
        if (g_ascii_strcasecmp(str, "iconify") == 0)
            wc->button_2_command = WC_ICONIFY;
        else if (g_ascii_strcasecmp(str, "none") == 0)
            wc->button_2_command = WC_NONE;
    }
    if (config_setting_lookup_string(settings, "image", &str))
        wc->image = expand_tilda(str);
    if (config_setting_lookup_int(settings, "Toggle", &tmp_int))
        wc->toggle_preference = tmp_int != 0;

    /* Default the image if unspecified. */
    if (wc->image == NULL)
        wc->image = g_strdup("window-manager");

    /* Save construction pointers */
    wc->settings = settings;

    /* Allocate top level widget and set into Plugin widget pointer. */
    p = lxpanel_button_new_for_icon(panel, wc->image, NULL, NULL);
    lxpanel_plugin_set_data(p, wc, wincmd_destructor);
    gtk_widget_set_tooltip_text(p, _("Left click to iconify all windows.  Middle click to shade them."));

    g_signal_connect(G_OBJECT(p), "button-release-event",
                     G_CALLBACK(wincmd_button_released), wc);

    /* Show the widget and return. */
    return p;
}

/* Plugin destructor. */
static void wincmd_destructor(gpointer user_data)
{
    WinCmdPlugin * wc = user_data;
    g_free(wc->image);
    g_free(wc);
}

/* Callback when the configuration dialog has recorded a configuration change. */
static gboolean wincmd_apply_configuration(gpointer user_data)
{
    GtkWidget * p = user_data;
    WinCmdPlugin * wc = lxpanel_plugin_get_data(p);

    //lxpanel_button_set_icon(p, wc->image, -1);
    /* Just save settings */
    //config_group_set_string(wc->settings, "image", wc->image);
    config_group_set_string(wc->settings, "Button1",
                            wincmd_names[wc->button_1_command]);
    config_group_set_string(wc->settings, "Button2",
                            wincmd_names[wc->button_2_command]);
    config_group_set_int(wc->settings, "Toggle", wc->toggle_preference);
    return FALSE;
}

/* Callback when the configuration dialog is to be shown. */
static GtkWidget *wincmd_configure(LXPanel *panel, GtkWidget *p)
{
    WinCmdPlugin * wc = lxpanel_plugin_get_data(p);
    return lxpanel_generic_config_dlg(_("Minimize All Windows"),
        panel, wincmd_apply_configuration, p,
        _("Alternately iconify/shade and raise"), &wc->toggle_preference, CONF_TYPE_BOOL,
        /* FIXME: configure buttons 1 and 2 */
        NULL);
}


/* Plugin descriptor. */
LXPanelPluginInit lxpanel_static_plugin_wincmd = {
    .name = N_("Minimize All Windows"),
    .description = N_("Sends commands to all desktop windows.\nSupported commands are 1) iconify and 2) shade"),

    .new_instance = wincmd_constructor,
    .config = wincmd_configure,
    .button_press_event = wincmd_button_clicked
};
