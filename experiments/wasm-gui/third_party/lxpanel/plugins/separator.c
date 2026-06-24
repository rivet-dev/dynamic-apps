/*
 * Copyright (C) 2006-2008 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *               2006-2008 Jim Huang <jserv.tw@gmail.com>
 *               2009 Marty Jack <martyj19@comcast.net>
 *               2010 Julien Lavergne <julien.lavergne@gmail.com>
 *               2013 Henry Gebhardt <hsggebhardt@gmail.com>
 *               2014 Andriy Grytsenko <andrej@rep.kiev.ua>
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

#include "plugin.h"

#include <glib/gi18n.h>

/* Plugin constructor. */
static GtkWidget *separator_constructor(LXPanel *panel, config_setting_t *settings)
{
    GtkWidget *instance, *sep;

    /* Allocate top level widget and set into Plugin widget pointer. */
    instance = gtk_event_box_new();
    gtk_widget_set_has_window(instance, FALSE);

    /* Allocate separator as a child of top level. */
    sep = panel_separator_new(panel);
    gtk_container_add(GTK_CONTAINER(instance), sep);
    gtk_widget_show(sep);

    /* Show the widget and return. */
    return instance;
}

/* Callback when panel configuration changes. */
static void separator_reconfigure(LXPanel *panel, GtkWidget *instance)
{
    /* Determine if the orientation changed in a way that requires action. */
    GtkWidget * sep = gtk_bin_get_child(GTK_BIN(instance));
    if (GTK_IS_VSEPARATOR(sep))
    {
        if (panel_get_orientation(panel) == GTK_ORIENTATION_HORIZONTAL)
            return;
    }
    else
    {
        if (panel_get_orientation(panel) == GTK_ORIENTATION_VERTICAL)
            return;
    }

    /* If the orientation changed, recreate the separator. */
    gtk_widget_destroy(sep);
    sep = panel_separator_new(panel);
    gtk_widget_show(sep);
    gtk_container_add(GTK_CONTAINER(instance), sep);
}

/* Plugin descriptor. */
LXPanelPluginInit lxpanel_static_plugin_separator = {
    .name = N_("Separator"),
    .description = N_("Add a separator to the panel"),

    .new_instance = separator_constructor,
    .reconfigure = separator_reconfigure
};
