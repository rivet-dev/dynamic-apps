/*
 * Copyright (C) 2006-2008 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *               2006 Jim Huang <jserv.tw@gmail.com>
 *               2008 Fred Chien <fred@lxde.org>
 *               2009-2010 Marty Jack <martyj19@comcast.net>
 *               2010 Julien Lavergne <julien.lavergne@gmail.com>
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
#include <unistd.h>

#include <glib/gi18n.h>
#include <libfm/fm-gtk.h>
#include <string.h>

#include "misc.h"
#include "plugin.h"

/* Temporary for sort of directory names. */
typedef struct _directory_name {
    struct _directory_name * flink;
    char * directory_name;
    char * directory_name_collate_key;
} DirectoryName;

/* Private context for directory menu plugin. */
typedef struct {
    LXPanel * panel; /* The panel and settings are required to apply config */
    config_setting_t * settings;
    char * image;			/* Icon for top level widget */
    char * path;			/* Top level path for widget */
    char * name;			/* User's label for widget */
    GdkPixbuf * folder_icon;		/* Icon for folders */
} DirMenuPlugin;

static GtkWidget * dirmenu_create_menu(DirMenuPlugin * dm, const char * path, gboolean open_at_top);
static void dirmenu_destructor(gpointer user_data);
static gboolean dirmenu_apply_configuration(gpointer user_data);


/* Handler for activate event on popup Open menu item. */
static void dirmenu_menuitem_open_directory(GtkWidget * item, DirMenuPlugin * dm)
{
    FmPath *path = fm_path_new_for_str(g_object_get_data(G_OBJECT(gtk_widget_get_parent(item)), "path"));
    lxpanel_launch_path(dm->panel, path);
    fm_path_unref(path);
}

/* Handler for activate event on popup Open In Terminal menu item. */
static void dirmenu_menuitem_open_in_terminal(GtkWidget * item, DirMenuPlugin * dm)
{
    fm_terminal_launch(g_object_get_data(G_OBJECT(gtk_widget_get_parent(item)), "path"), NULL);
}

/* Handler for select event on popup menu item. */
static void dirmenu_menuitem_select(GtkMenuItem * item, DirMenuPlugin * dm)
{
    GtkWidget * sub = gtk_menu_item_get_submenu(item);
    if (sub != NULL)
    {
        /* On first reference, populate the submenu using the parent directory and the item directory name. */
        GtkMenu * parent = GTK_MENU(gtk_widget_get_parent(GTK_WIDGET(item)));
        char * path = (char *) g_object_get_data(G_OBJECT(sub), "path");
        if (path == NULL)
        {
            path = g_build_filename(
                (char *) g_object_get_data(G_OBJECT(parent), "path"),
                (char *) g_object_get_data(G_OBJECT(item), "name"),
                NULL);
            sub = dirmenu_create_menu(dm, path, TRUE);
            g_free(path);
            gtk_menu_item_set_submenu(item, sub);
        }
    }
}

/* Handler for deselect event on popup menu item. */
static void dirmenu_menuitem_deselect(GtkMenuItem * item, DirMenuPlugin * dm)
{
    /* Delete old menu on deselect to save resource. */
    gtk_menu_item_set_submenu(item, gtk_menu_new());
}

/* Handler for selection-done event on popup menu. */
static void dirmenu_menu_selection_done(GtkWidget * menu, DirMenuPlugin * dm)
{
    gtk_widget_destroy(menu);
}

/* Position-calculation callback for popup menu. */
static void dirmenu_popup_set_position(GtkWidget * menu, gint * px, gint * py, gboolean * push_in, GtkWidget * p)
{
    DirMenuPlugin * dm = lxpanel_plugin_get_data(p);

    /* Determine the coordinates. */
    lxpanel_plugin_popup_set_position_helper(dm->panel, p, menu, px, py);
    *push_in = TRUE;
}

/* Create a menu populated with all subdirectories. */
static GtkWidget * dirmenu_create_menu(DirMenuPlugin * dm, const char * path, gboolean open_at_top)
{
    /* Create a menu. */
    GtkWidget * menu = gtk_menu_new();

    if (dm->folder_icon == NULL)
    {
        int w;
        int h;
        gtk_icon_size_lookup_for_settings(gtk_widget_get_settings(menu), GTK_ICON_SIZE_MENU, &w, &h);
        dm->folder_icon = gtk_icon_theme_load_icon(
            panel_get_icon_theme(dm->panel),
            "gnome-fs-directory", MAX(w, h), 0, NULL);
        if (dm->folder_icon == NULL)
            dm->folder_icon = gtk_widget_render_icon(menu, GTK_STOCK_DIRECTORY, GTK_ICON_SIZE_MENU, NULL);
    }

    g_object_set_data_full(G_OBJECT(menu), "path", g_strdup(path), g_free);

    /* Scan the specified directory to populate the menu with its subdirectories. */
    DirectoryName * dir_list = NULL;
    GDir * dir = g_dir_open(path, 0, NULL);
    if (dir != NULL)
    {
        const char * name;
        while ((name = g_dir_read_name(dir)) != NULL)	/* Memory owned by glib */
        {
            /* Omit hidden files. */
            if (name[0] != '.')
            {
                char * full = g_build_filename(path, name, NULL);
                if (g_file_test(full, G_FILE_TEST_IS_DIR))
                {
                    /* Convert name to UTF-8 and to the collation key. */
                    char * directory_name = g_filename_display_name(name);
                    char * directory_name_collate_key = g_utf8_collate_key(directory_name, -1);

                    /* Locate insertion point. */
                    DirectoryName * dir_pred = NULL;
                    DirectoryName * dir_cursor;
                    for (dir_cursor = dir_list; dir_cursor != NULL; dir_pred = dir_cursor, dir_cursor = dir_cursor->flink)
                    {
                        if (strcmp(directory_name_collate_key, dir_cursor->directory_name_collate_key) <= 0)
                            break;
                    }

                    /* Allocate and initialize sorted directory name entry. */
                    dir_cursor = g_new0(DirectoryName, 1);
                    dir_cursor->directory_name = directory_name;
                    dir_cursor->directory_name_collate_key = directory_name_collate_key;
                    if (dir_pred == NULL)
                    {
                        dir_cursor->flink = dir_list;
                        dir_list = dir_cursor;
                    }
                    else
                    {
                        dir_cursor->flink = dir_pred->flink;
                        dir_pred->flink = dir_cursor;
                    }
                }
                g_free(full);
            }
        }
        g_dir_close(dir);
    }

    /* The sorted directory name list is complete.  Loop to create the menu. */
    DirectoryName * dir_cursor;
    while ((dir_cursor = dir_list) != NULL)
    {
        /* Create and initialize menu item. */
        GtkWidget * item = gtk_image_menu_item_new_with_label(dir_cursor->directory_name);
        gtk_image_menu_item_set_image(
            GTK_IMAGE_MENU_ITEM(item),
            gtk_image_new_from_stock(GTK_STOCK_DIRECTORY, GTK_ICON_SIZE_MENU));
        GtkWidget * dummy = gtk_menu_new();
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), dummy);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

        /* Unlink and free sorted directory name element, but reuse the directory name string. */
        dir_list = dir_cursor->flink;
        g_object_set_data_full(G_OBJECT(item), "name", dir_cursor->directory_name, g_free);
        g_free(dir_cursor->directory_name_collate_key);
        g_free(dir_cursor);

        /* Connect signals. */
        g_signal_connect(G_OBJECT(item), "select", G_CALLBACK(dirmenu_menuitem_select), dm);
        g_signal_connect(G_OBJECT(item), "deselect", G_CALLBACK(dirmenu_menuitem_deselect), dm);
    }

    /* Create "Open" and "Open in Terminal" items. */
    GtkWidget * item = gtk_image_menu_item_new_from_stock( GTK_STOCK_OPEN, NULL );
    g_signal_connect(item, "activate", G_CALLBACK(dirmenu_menuitem_open_directory), dm);
    GtkWidget * term = gtk_menu_item_new_with_mnemonic( _("Open in _Terminal") );
    g_signal_connect(term, "activate", G_CALLBACK(dirmenu_menuitem_open_in_terminal), dm);

    /* Insert or append based on caller's preference. */
    if (open_at_top)
    {
        gtk_menu_shell_insert(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new(), 0);
        gtk_menu_shell_insert(GTK_MENU_SHELL(menu), term, 0);
        gtk_menu_shell_insert(GTK_MENU_SHELL(menu), item, 0);
    }
    else {
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), term);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    }

    /* Show the menu and return. */
    gtk_widget_show_all(menu);
    return menu;
}

/* Show a menu of subdirectories. */
static void dirmenu_show_menu(GtkWidget * widget, DirMenuPlugin * dm, int btn, guint32 time)
{
    /* Create a menu populated with all subdirectories. */
    GtkWidget * menu = dirmenu_create_menu(dm, dm->path, FALSE);
    g_signal_connect(menu, "selection-done", G_CALLBACK(dirmenu_menu_selection_done), dm);

    /* Show the menu.  Use a positioning function to get it placed next to the top level widget. */
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, (GtkMenuPositionFunc) dirmenu_popup_set_position, widget, btn, time);
}

/* Handler for button-press-event on top level widget. */
static gboolean dirmenu_button_press_event(GtkWidget * widget, GdkEventButton * event, LXPanel * p)
{
    DirMenuPlugin * dm = lxpanel_plugin_get_data(widget);

    if (event->button == 1)
    {
        dirmenu_show_menu(widget, dm, event->button, event->time);
        return TRUE;
    }
    return FALSE;
}

static gboolean dirmenu_button_release_event(GtkWidget * widget, GdkEventButton * event, DirMenuPlugin * dm)
{
    if (event->button == 2)
    {
        fm_terminal_launch(dm->path, NULL);
    }
    return FALSE;
}

/* Plugin constructor. */
static GtkWidget *dirmenu_constructor(LXPanel *panel, config_setting_t *settings)
{
    /* Allocate and initialize plugin context and set into Plugin private data pointer. */
    DirMenuPlugin * dm = g_new0(DirMenuPlugin, 1);
    GtkWidget * p;
    const char *str;

    /* Load parameters from the configuration file. */
    if (config_setting_lookup_string(settings, "image", &str))
        dm->image = g_strdup(str);
    if (config_setting_lookup_string(settings, "path", &str))
        dm->path = expand_tilda(str);
    else
        dm->path = g_strdup(fm_get_home_dir());
    if (config_setting_lookup_string(settings, "name", &str))
        dm->name = g_strdup(str);

    /* Save construction pointers */
    dm->panel = panel;
    dm->settings = settings;

    /* Allocate top level widget and set into Plugin widget pointer.
     * It is not known why, but the button text will not draw if it is edited from empty to non-empty
     * unless this strategy of initializing it with a non-empty value first is followed. */
    p = lxpanel_button_new_for_icon(panel,
                            ((dm->image != NULL) ? dm->image : "file-manager"),
                            NULL, "Temp");
    lxpanel_plugin_set_data(p, dm, dirmenu_destructor);

    /* Initialize the widget. */
    dirmenu_apply_configuration(p);

    g_signal_connect(G_OBJECT(p), "button-release-event",
                     G_CALLBACK(dirmenu_button_release_event), dm);

    /* Show the widget and return. */
    return p;
}

/* Plugin destructor. */
static void dirmenu_destructor(gpointer user_data)
{
    DirMenuPlugin * dm = (DirMenuPlugin *)user_data;

    /* Release a reference on the folder icon if held. */
    if (dm->folder_icon)
        g_object_unref(dm->folder_icon);

    /* Deallocate all memory. */
    g_free(dm->image);
    g_free(dm->path);
    g_free(dm->name);
    g_free(dm);
}

/* Callback when the configuration dialog has recorded a configuration change. */
static gboolean dirmenu_apply_configuration(gpointer user_data)
{
    GtkWidget * p = user_data;
    DirMenuPlugin * dm = lxpanel_plugin_get_data(p);
    char * path = dm->path;

    /* Normalize path */
    if (path == NULL)
        dm->path = g_strdup(fm_get_home_dir());
    else if (path[0] == '~')
    {
        dm->path = expand_tilda(path);
        g_free(path);
    }

    /* Save configuration */
    config_group_set_string(dm->settings, "path", dm->path);
    config_group_set_string(dm->settings, "name", dm->name);
    config_group_set_string(dm->settings, "image", dm->image);

    lxpanel_button_set_icon(p, ((dm->image != NULL) ? dm->image : "file-manager"), -1);
    lxpanel_button_set_label(p, dm->name);

    gtk_widget_set_tooltip_text(p, dm->path);
    return FALSE;
}

/* Callback when the configuration dialog is to be shown. */
static GtkWidget *dirmenu_configure(LXPanel *panel, GtkWidget *p)
{
    DirMenuPlugin * dm = lxpanel_plugin_get_data(p);
    return lxpanel_generic_config_dlg(_("Directory Menu"),
        panel, dirmenu_apply_configuration, p,
        _("Directory"), &dm->path, CONF_TYPE_DIRECTORY_ENTRY,
        _("Label"), &dm->name, CONF_TYPE_STR,
        _("Icon"), &dm->image, CONF_TYPE_FILE_ENTRY,
        NULL);
}

/* Plugin descriptor. */
LXPanelPluginInit lxpanel_static_plugin_dirmenu = {
    .name = N_("Directory Menu"),
    .description = N_("Browse directory tree via menu (Author = PCMan)"),

    .new_instance = dirmenu_constructor,
    .config = dirmenu_configure,
    .button_press_event = dirmenu_button_press_event
};
