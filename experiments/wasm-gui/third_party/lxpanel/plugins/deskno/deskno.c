/*
 * Desktop number plugin to lxpanel
 *
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *               2006 Jim Huang <jserv.tw@gmail.com>
 *               2009 Marty Jack <martyj19@comcast.net>
 *               2014 Andriy Grytsenko <andrej@rep.kiev.ua>
 *               2016 Charles Lehner <cel10@users.sf.net>
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

// reused dclock.c and variables from pager.c
// 11/23/04 by cmeury

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib/gi18n.h>

#include "plugin.h"
#include "misc.h"
#include "ev.h"

/* Private context for desktop number plugin. */
typedef struct {
    LXPanel * panel;			/* Back pointer to Panel */
    config_setting_t *settings;
    GtkWidget * label;			/* The label */
    int number_of_desktops;		/* Number of desktops */
    char * * desktop_labels;		/* Vector of desktop labels */
    gboolean bold;			/* User preference: True if bold font */
    gboolean wm_labels;			/* User preference: True to display window manager labels */
} DesknoPlugin;

static void deskno_destructor(gpointer user_data);

/* Handler for current_desktop event from window manager. */
static gboolean deskno_name_update(GtkWidget * widget, DesknoPlugin * dc)
{
    /* Compute and redraw the desktop number. */
    int desktop_number = get_net_current_desktop();
    if (desktop_number < dc->number_of_desktops)
        lxpanel_draw_label_text(dc->panel, dc->label, dc->desktop_labels[desktop_number], dc->bold, 1, TRUE);
    return TRUE;
}

/* Handler for desktop_name and number_of_desktops events from window manager.
 * Also used on a configuration change to get a full redraw. */
static void deskno_redraw(GtkWidget * widget, DesknoPlugin * dc)
{
    /* Get the NET_DESKTOP_NAMES property. */
    dc->number_of_desktops = get_net_number_of_desktops();
    int number_of_desktop_names;
    char * * desktop_names;
    desktop_names = get_utf8_property_list(GDK_ROOT_WINDOW(), a_NET_DESKTOP_NAMES, &number_of_desktop_names);

    /* Reallocate the vector of labels. */
    if (dc->desktop_labels != NULL)
        g_strfreev(dc->desktop_labels);
    dc->desktop_labels = g_new0(gchar *, dc->number_of_desktops + 1);

    /* Loop to copy the desktop names to the vector of labels.
     * If there are more desktops than labels, label the extras with a decimal number. */
    int i = 0;
    if (dc->wm_labels)
        for ( ; ((desktop_names != NULL) && (i < MIN(dc->number_of_desktops, number_of_desktop_names))); i++)
            dc->desktop_labels[i] = g_strdup(desktop_names[i]);
    for ( ; i < dc->number_of_desktops; i++)
        dc->desktop_labels[i] = g_strdup_printf("%d", i + 1);

    /* Free the property. */
    if (desktop_names != NULL)
        g_strfreev(desktop_names);

    /* Redraw the label. */
    deskno_name_update(widget, dc);
}

/* Handler for button-press-event on top level widget. */
static gboolean deskno_button_press_event(GtkWidget * widget, GdkEventButton * event, LXPanel * p)
{
    /* Right-click goes to next desktop, wrapping around to first. */
    int desknum = get_net_current_desktop();
    int desks = get_net_number_of_desktops();
    int newdesk = desknum + 1;
    Screen *xscreen = GDK_SCREEN_XSCREEN(gtk_widget_get_screen(widget));
    if (newdesk >= desks)
        newdesk = 0;

    /* Ask the window manager to make the new desktop current. */
    Xclimsgx(xscreen, RootWindowOfScreen(xscreen), a_NET_CURRENT_DESKTOP, newdesk, 0, 0, 0, 0);
    return TRUE;
}

/* Handler for scroll events on the plugin */
static gboolean deskno_scrolled(GtkWidget * p, GdkEventScroll * ev, DesknoPlugin * dc)
{
    int desknum = get_net_current_desktop();
    int desks = get_net_number_of_desktops();
    Screen *xscreen = GDK_SCREEN_XSCREEN(gtk_widget_get_screen(p));

    switch (ev->direction) {
        case GDK_SCROLL_DOWN:
            desknum++;
            break;
        case GDK_SCROLL_UP:
            desknum--;
            break;
        default:
            return FALSE;
    }

    if (desknum < 0 || desknum >= desks)
        return TRUE;

    Xclimsgx(xscreen, RootWindowOfScreen(xscreen), a_NET_CURRENT_DESKTOP, desknum, 0, 0, 0, 0);
    return TRUE;
}

/* Plugin constructor. */
static GtkWidget *deskno_constructor(LXPanel *panel, config_setting_t *settings)
{
    /* Allocate plugin context and set into Plugin private data pointer. */
    DesknoPlugin * dc = g_new0(DesknoPlugin, 1);
    GtkWidget *p;
    int tmp_int;

    g_return_val_if_fail(dc != NULL, 0);
    dc->panel = panel;
    dc->settings = settings;

    /* Default parameters. */
    dc->wm_labels = TRUE;

    /* Load parameters from the configuration file. */
    if (config_setting_lookup_int(settings, "BoldFont", &tmp_int))
        dc->bold = tmp_int != 0;
    if (config_setting_lookup_int(settings, "WMLabels", &tmp_int))
        dc->wm_labels = tmp_int != 0;

    /* Allocate top level widget and set into Plugin widget pointer. */
    p = gtk_event_box_new();
    lxpanel_plugin_set_data(p, dc, deskno_destructor);

    /* Allocate label widget and add to top level. */
    dc->label = gtk_label_new(NULL);
    gtk_container_add(GTK_CONTAINER(p), dc->label);

    /* Connect signals.  Note use of window manager event object. */
    g_signal_connect(G_OBJECT(fbev), "current-desktop", G_CALLBACK(deskno_name_update), (gpointer) dc);
    g_signal_connect(G_OBJECT(fbev), "desktop-names", G_CALLBACK(deskno_redraw), (gpointer) dc);
    g_signal_connect(G_OBJECT(fbev), "number-of-desktops", G_CALLBACK(deskno_redraw), (gpointer) dc);

    gtk_widget_add_events(p, GDK_SCROLL_MASK);
    g_signal_connect(G_OBJECT(p), "scroll-event", G_CALLBACK(deskno_scrolled), (gpointer) dc);

    /* Initialize value and show the widget. */
    deskno_redraw(NULL, dc);
    gtk_widget_show_all(p);
    return p;
}

/* Plugin destructor. */
static void deskno_destructor(gpointer user_data)
{
    DesknoPlugin * dc = (DesknoPlugin *) user_data;
    GtkWidget * p = gtk_widget_get_parent(dc->label);

    /* Disconnect signal from window manager event object. */
    g_signal_handlers_disconnect_by_func(G_OBJECT(fbev), deskno_name_update, dc);
    g_signal_handlers_disconnect_by_func(G_OBJECT(fbev), deskno_redraw, dc);
    g_signal_handlers_disconnect_by_func(G_OBJECT(p), deskno_scrolled, dc);

    /* Deallocate all memory. */
    if (dc->desktop_labels != NULL)
        g_strfreev(dc->desktop_labels);
    g_free(dc);
}

/* Callback when the configuration dialog has recorded a configuration change. */
static gboolean deskno_apply_configuration(gpointer user_data)
{
    DesknoPlugin * dc = lxpanel_plugin_get_data(user_data);
    deskno_redraw(NULL, dc);
    config_group_set_int(dc->settings, "BoldFont", dc->bold);
    config_group_set_int(dc->settings, "WMLabels", dc->wm_labels);
    return FALSE;
}

/* Callback when the configuration dialog is to be shown. */
static GtkWidget *deskno_configure(LXPanel *panel, GtkWidget *p)
{
    DesknoPlugin * dc = lxpanel_plugin_get_data(p);
    GtkWidget * dlg = lxpanel_generic_config_dlg(_("Desktop Number / Workspace Name"),
        panel, deskno_apply_configuration, p,
        _("Bold font"), &dc->bold, CONF_TYPE_BOOL,
        _("Display desktop names"), &dc->wm_labels, CONF_TYPE_BOOL,
        NULL);
    gtk_widget_set_size_request(GTK_WIDGET(dlg), 400, -1);	/* Improve geometry */
    return dlg;
}

/* Callback when panel configuration changes. */
static void deskno_panel_configuration_changed(LXPanel *panel, GtkWidget *p)
{
    DesknoPlugin * dc = lxpanel_plugin_get_data(p);
    deskno_name_update(NULL, dc);
}

FM_DEFINE_MODULE(lxpanel_gtk, deskno)

/* Plugin descriptor. */
LXPanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = N_("Desktop Number / Workspace Name"),
    .description = N_("Display workspace number, by cmeury@users.sf.net"),

    .new_instance = deskno_constructor,
    .config = deskno_configure,
    .reconfigure = deskno_panel_configuration_changed,
    .button_press_event = deskno_button_press_event
};
