/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2004-2008 Brian Tarricone <bjt23@cornell.edu>
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 *  Random portions taken from or inspired by the original xfdesktop for xfce4:
 *     Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *     Copyright (C) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
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

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <libwnck/libwnck.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include "windowlist.h"
#include "xfdesktop-common.h"

#define WLIST_MAXLEN 30

static gboolean show_windowlist = TRUE;
static gboolean wl_show_icons = TRUE;
static gboolean wl_show_ws_names = TRUE;
static gboolean wl_submenus = FALSE;
static gboolean wl_sticky_once = FALSE;
static gboolean wl_add_remove_options = TRUE;

static void
set_num_workspaces(GtkWidget *w, gpointer data)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    WnckScreen *wnck_screen = wnck_screen_get(XScreenNumberOfScreen(gdk_x11_screen_get_xscreen(gtk_widget_get_screen(w))));
G_GNUC_END_IGNORE_DEPRECATIONS
    WnckWorkspace *wnck_workspace = wnck_screen_get_active_workspace(wnck_screen);
    gint nworkspaces = wnck_screen_get_workspace_count(wnck_screen);
    const gchar *ws_name = wnck_workspace_get_name(wnck_screen_get_workspace(wnck_screen, nworkspaces -1));
    gint num = GPOINTER_TO_INT(data);
    gchar *rm_label_short = NULL, *rm_label_long = NULL;
    gint current_workspace = wnck_workspace_get_number(wnck_workspace);
    const gchar *current_workspace_name = wnck_workspace_get_name(wnck_workspace);

    g_return_if_fail(nworkspaces != num);

    TRACE("entering");

    if(num < nworkspaces) {
        if(!ws_name || atoi(ws_name) == nworkspaces) {
            rm_label_short = g_strdup_printf(_("Remove Workspace %d"), nworkspaces);
            rm_label_long = g_strdup_printf(_("Do you really want to remove workspace %d?\nNote: You are currently on workspace %d."),
                                            nworkspaces, current_workspace);
        } else {
            gchar *ws_name_esc = g_markup_escape_text(ws_name, strlen(ws_name));
            rm_label_short = g_strdup_printf(_("Remove Workspace '%s'"), ws_name_esc);
            rm_label_long = g_strdup_printf(_("Do you really want to remove workspace '%s'?\nNote: You are currently on workspace '%s'."),
                                            ws_name_esc, current_workspace_name);
            g_free(ws_name_esc);
        }

        /* Popup a dialog box confirming that the user wants to remove a
         * workspace */
        if(!xfce_dialog_confirm(NULL, NULL, _("Remove"), rm_label_long,
                                "%s", rm_label_short))
        {
            g_free(rm_label_short);
            g_free(rm_label_long);
            return;
        }
    }

    if(rm_label_short != NULL)
        g_free(rm_label_short);
    if(rm_label_long != NULL)
        g_free(rm_label_long);

    wnck_screen_change_workspace_count(wnck_screen, num);
}

static void
activate_window(GtkWidget *w, gpointer user_data)
{
    WnckWindow *wnck_window = user_data;

    if(!wnck_window_is_sticky(wnck_window)) {
        wnck_workspace_activate(wnck_window_get_workspace(wnck_window),
                                gtk_get_current_event_time());
    }
    wnck_window_activate(wnck_window, gtk_get_current_event_time());
}

static void
window_destroyed_cb(gpointer data, GObject *where_the_object_was)
{
    GtkWidget *mi = data;
    GtkWidget *menu = gtk_widget_get_parent(mi);

    if(mi && menu)
        gtk_container_remove(GTK_CONTAINER(menu), mi);
}

static void
mi_destroyed_cb(GtkWidget *object, gpointer user_data)
{
    g_object_weak_unref(G_OBJECT(user_data),
                        (GWeakNotify)window_destroyed_cb, object);
}

static void
menulist_set_label_flags(GtkWidget *widget, gpointer data)
{
    gboolean *done = data;
    if(*done)
        return;
    if(GTK_IS_LABEL(widget)) {
        GtkLabel *label = GTK_LABEL(widget);
        gtk_label_set_use_markup(label, TRUE);
        gtk_label_set_ellipsize(label, PANGO_ELLIPSIZE_MIDDLE);
        gtk_label_set_max_width_chars(label, 24);
        *done = TRUE;
    } else if(GTK_IS_CONTAINER (widget)) {
        gtk_container_forall(GTK_CONTAINER (widget), menulist_set_label_flags,
                             done);
    }
}


static GtkWidget *
menu_item_from_wnck_window(WnckWindow *wnck_window,
                           gint icon_width,
                           gint icon_height,
                           gint scale_factor)
{
    GtkWidget *mi, *img = NULL;
    gchar *title;
    GString *label;
    GdkPixbuf *icon, *tmp = NULL;
    gint w, h;
    gboolean truncated = FALSE;

    title = g_markup_escape_text(wnck_window_get_name(wnck_window), -1);
    if(!title)
        return NULL;

    label = g_string_new(title);
    g_free(title);

    if(wnck_window_is_active(wnck_window)) {
        g_string_prepend(label, "<b><i>");
        g_string_append(label, "</i></b>");
    }

    /* add some padding to the right */
    if(gtk_widget_get_default_direction() == GTK_TEXT_DIR_RTL)
        g_string_prepend(label, "  ");
    else
        g_string_append(label, "  ");

    if(wl_show_icons) {
        cairo_surface_t *surface;

        icon = wnck_window_get_mini_icon(wnck_window);
        w = gdk_pixbuf_get_width(icon);
        h = gdk_pixbuf_get_height(icon);
        if(w != icon_width * scale_factor || h != icon_height * scale_factor) {
            tmp = gdk_pixbuf_scale_simple(icon,
                                          icon_width * scale_factor,
                                          icon_height * scale_factor,
                                          GDK_INTERP_BILINEAR);
        }

        if(wnck_window_is_minimized(wnck_window)) {
            if(!tmp)
                tmp = gdk_pixbuf_copy(icon);
            /* minimized window, fade out app icon */
            gdk_pixbuf_saturate_and_pixelate(tmp, tmp, 0.55, TRUE);
        }

        if(tmp) {
            surface = gdk_cairo_surface_create_from_pixbuf(tmp, scale_factor, NULL);
            g_object_unref(G_OBJECT(tmp));
        } else {
            surface = gdk_cairo_surface_create_from_pixbuf(icon, scale_factor, NULL);
        }

        img = gtk_image_new_from_surface(surface);
        cairo_surface_destroy(surface);
    }

    mi = xfdesktop_menu_create_menu_item_with_markup(label->str, img);

    g_string_free(label, TRUE);

    gtk_container_forall(GTK_CONTAINER(mi), menulist_set_label_flags,
                         &truncated);

    return mi;
}

static void
set_label_color_insensitive(GtkWidget *lbl)
{
    GdkRGBA         fg_color;
    PangoAttrList  *attrs;
    PangoAttribute *foreground;

    g_return_if_fail(GTK_IS_LABEL(lbl));

    gtk_style_context_get_color(gtk_widget_get_style_context (lbl),
                                GTK_STATE_FLAG_INSENSITIVE,
                                &fg_color);

    attrs = pango_attr_list_new();
    foreground = pango_attr_foreground_new((guint16)(fg_color.red * G_MAXUINT16),
                                           (guint16)(fg_color.green * G_MAXUINT16),
                                           (guint16)(fg_color.blue * G_MAXUINT16));
    pango_attr_list_insert(attrs, foreground);
    gtk_label_set_attributes (GTK_LABEL(lbl), attrs);
    pango_attr_list_unref (attrs);
}

GtkMenuShell *
windowlist_populate(GtkMenuShell *menu, gint scale_factor)
{
    GtkMenuShell *top_menu = menu;
    GtkWidget *submenu, *mi, *label, *img;
    GdkScreen *gscreen;
    GList *menu_children;
    WnckScreen *wnck_screen;
    gint nworkspaces, i;
    WnckWorkspace *active_workspace, *wnck_workspace;
    gchar *ws_label, *rm_label;
    const gchar *ws_name = NULL;
    GList *windows, *l;
    WnckWindow *wnck_window;
    gint w, h;

    if(!show_windowlist)
        return top_menu;

    if(gtk_widget_has_screen(GTK_WIDGET(menu)))
        gscreen = gtk_widget_get_screen(GTK_WIDGET(menu));
    else
        gscreen = gdk_display_get_default_screen(gdk_display_get_default());

    /* check to see if the menu is empty.  if not, add the windowlist to a
     * submenu */
    menu_children = gtk_container_get_children(GTK_CONTAINER(menu));
    if(menu_children) {
        GtkWidget *tmpmenu = gtk_menu_new();
        gtk_menu_set_screen(GTK_MENU(tmpmenu), gscreen);
        gtk_menu_set_reserve_toggle_size (GTK_MENU (tmpmenu), FALSE);

        mi = gtk_separator_menu_item_new();
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

        mi = gtk_menu_item_new_with_label(_("Window List"));
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

        gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), tmpmenu);
        menu = (GtkMenuShell *)tmpmenu;
        g_list_free(menu_children);
    }

    gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &w, &h);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    wnck_screen = wnck_screen_get(XScreenNumberOfScreen(gdk_x11_screen_get_xscreen(gtk_widget_get_screen(GTK_WIDGET(menu)))));
    wnck_set_default_mini_icon_size((w > h ? w : h) * scale_factor);
G_GNUC_END_IGNORE_DEPRECATIONS
    wnck_screen_force_update (wnck_screen);
    nworkspaces = wnck_screen_get_workspace_count(wnck_screen);
    active_workspace = wnck_screen_get_active_workspace(wnck_screen);

    for(i = 0; i < nworkspaces; i++) {
        wnck_workspace = wnck_screen_get_workspace(wnck_screen, i);
        submenu = (GtkWidget *)menu;

        if(wl_show_ws_names || wl_submenus) {
            ws_name = wnck_workspace_get_name(wnck_workspace);

            /* Workspace header */
            if(ws_name == NULL || *ws_name == '\0')
                ws_label = g_strdup_printf(_("<b>Workspace %d</b>"), i+1);
            else {
                gchar *ws_name_esc = g_markup_escape_text(ws_name, strlen(ws_name));
                ws_label = g_strdup_printf("<b>%s</b>", ws_name_esc);
                g_free(ws_name_esc);
            }

            mi = gtk_menu_item_new_with_label(ws_label);
            g_free(ws_label);
            label = gtk_bin_get_child(GTK_BIN(mi));
            gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
            /* center the workspace header */
            gtk_label_set_xalign (GTK_LABEL(label), 0.5f);
            /* If it's not the active workspace, make the color insensitive */
            if(wnck_workspace != active_workspace) {
                set_label_color_insensitive(label);
            }
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            if(!wl_submenus) {
                g_signal_connect_swapped(G_OBJECT(mi), "activate",
                                         G_CALLBACK(wnck_workspace_activate),
                                         wnck_workspace);
            }

            if(wl_submenus) {
                submenu = gtk_menu_new();
                gtk_menu_set_reserve_toggle_size (GTK_MENU (submenu), FALSE);
                gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), submenu);
            }
        }

        windows = wnck_screen_get_windows_stacked(wnck_screen);

        for(l = windows; l; l = l->next) {
            wnck_window = l->data;

            if((wnck_window_get_workspace(wnck_window) != wnck_workspace
                && (!wnck_window_is_sticky(wnck_window)
                    || (wl_sticky_once && wnck_workspace != active_workspace)))
                || wnck_window_is_skip_pager(wnck_window)
                || wnck_window_is_skip_tasklist(wnck_window))
            {
                /* the window isn't on the current WS AND isn't sticky,
                 * OR,
                 * the window is set to skip the pager,
                 * OR,
                 * the window is set to skip the tasklist
                 */
                continue;
            }

            mi = menu_item_from_wnck_window(wnck_window, w, h, scale_factor);
            if(!mi)
                continue;

            if(wnck_workspace != active_workspace
               && (!wnck_window_is_sticky(wnck_window)
                   || wnck_workspace != active_workspace))
            {
                /* The menu item has a GtkBox of which one of the children
                 * is the label we want to modify */
                GList *items = gtk_container_get_children(GTK_CONTAINER(gtk_bin_get_child(GTK_BIN(mi))));
                GList *li;

                for(li = items; li != NULL; li = li->next)
                {
                    if(GTK_IS_LABEL(li->data))
                    {
                        set_label_color_insensitive(li->data);
                        break;
                    }
                }
            }

            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(submenu), mi);
            g_object_weak_ref(G_OBJECT(wnck_window),
                              (GWeakNotify)window_destroyed_cb, mi);
            g_signal_connect(G_OBJECT(mi), "activate",
                             G_CALLBACK(activate_window), wnck_window);
            g_signal_connect(G_OBJECT(mi), "destroy",
                             G_CALLBACK(mi_destroyed_cb), wnck_window);
        }

        if(!wl_submenus && (i < nworkspaces-1 || wl_add_remove_options)) {
            mi = gtk_separator_menu_item_new();
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(submenu), mi);
        }
    }

    if(wl_add_remove_options) {
        /* 'add workspace' item */
        if(wl_show_icons) {
            img = gtk_image_new_from_icon_name("list-add", GTK_ICON_SIZE_MENU);
            mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("_Add Workspace"), img);
        } else
            mi = gtk_menu_item_new_with_mnemonic(_("_Add Workspace"));
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        g_signal_connect(G_OBJECT(mi), "activate",
                         G_CALLBACK(set_num_workspaces), GINT_TO_POINTER(nworkspaces+1));

        /* 'remove workspace' item */
        if(!ws_name || atoi(ws_name) == nworkspaces)
            rm_label = g_strdup_printf(_("_Remove Workspace %d"), nworkspaces);
        else {
            gchar *ws_name_esc = g_markup_escape_text(ws_name, strlen(ws_name));
            rm_label = g_strdup_printf(_("_Remove Workspace '%s'"), ws_name_esc);
            g_free(ws_name_esc);
        }
        if(wl_show_icons) {
            img = gtk_image_new_from_icon_name("list-remove", GTK_ICON_SIZE_MENU);
            mi = xfdesktop_menu_create_menu_item_with_mnemonic(rm_label, img);
        } else
            mi = gtk_menu_item_new_with_mnemonic(rm_label);
        g_free(rm_label);
        if(nworkspaces == 1)
            gtk_widget_set_sensitive(mi, FALSE);
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        g_signal_connect(G_OBJECT(mi), "activate",
                         G_CALLBACK(set_num_workspaces),
                         GINT_TO_POINTER(nworkspaces-1));
    }

    return top_menu;
}

static void
windowlist_settings_changed(XfconfChannel *channel,
                            const gchar *property,
                            const GValue *value,
                            gpointer user_data)
{
    if(!strcmp(property, "/windowlist-menu/show"))
        show_windowlist = G_VALUE_TYPE(value) ? g_value_get_boolean(value) : TRUE;
    else if(!strcmp(property, "/windowlist-menu/show-icons"))
        wl_show_icons = G_VALUE_TYPE(value) ? g_value_get_boolean(value) : TRUE;
    else if(!strcmp(property, "/windowlist-menu/show-workspace-names"))
        wl_show_ws_names = G_VALUE_TYPE(value) ? g_value_get_boolean(value) : TRUE;
    else if(!strcmp(property, "/windowlist-menu/show-submenus"))
        wl_submenus = G_VALUE_TYPE(value) ? g_value_get_boolean(value) : FALSE;
    else if(!strcmp(property, "/windowlist-menu/show-sticky-once"))
        wl_sticky_once = G_VALUE_TYPE(value) ? g_value_get_boolean(value) : FALSE;
    else if(!strcmp(property, "/windowlist-menu/show-add-remove-workspaces"))
        wl_add_remove_options = G_VALUE_TYPE(value) ? g_value_get_boolean(value) : TRUE;
}

void
windowlist_init(XfconfChannel *channel)
{
    if(channel) {
        show_windowlist = xfconf_channel_get_bool(channel,
                                                  "/windowlist-menu/show",
                                                  TRUE);

        wl_show_icons = xfconf_channel_get_bool(channel,
                                                "/windowlist-menu/show-icons",
                                                TRUE);

        wl_show_ws_names = xfconf_channel_get_bool(channel,
                                                   "/windowlist-menu/show-workspace-names",
                                                   TRUE);

        wl_submenus = xfconf_channel_get_bool(channel,
                                              "/windowlist-menu/show-submenus",
                                              FALSE);

        wl_sticky_once = xfconf_channel_get_bool(channel,
                                                 "/windowlist-menu/show-sticky-once",
                                                 FALSE);

        wl_add_remove_options = xfconf_channel_get_bool(channel,
                                                        "/windowlist-menu/show-add-remove-workspaces",
                                                        TRUE);

        g_signal_connect(G_OBJECT(channel), "property-changed",
                         G_CALLBACK(windowlist_settings_changed), NULL);
    }
}

void
windowlist_cleanup(void)
{
    /* notused */
}
