/*
 * Copyright (C) 2006-2010 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *               2006-2008 Jim Huang <jserv.tw@gmail.com>
 *               2008 Fred Chien <fred@lxde.org>
 *               2009 Ying-Chun Liu (PaulLiu) <grandpaul@gmail.com>
 *               2009-2010 Marty Jack <martyj19@comcast.net>
 *               2010 Jürgen Hötzel <juergen@archlinux.org>
 *               2010-2011 Julien Lavergne <julien.lavergne@gmail.com>
 *               2012-2013 Henry Gebhardt <hsggebhardt@gmail.com>
 *               2012 Michael Rawson <michaelrawson76@gmail.com>
 *               2014 Max Krummenacher <max.oss.09@gmail.com>
 *               2014 SHiNE CsyFeK <csyfek@users.sourceforge.net>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>
#include <glib/gi18n.h>

#include <menu-cache.h>
#include <libfm/fm-gtk.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "misc.h"
#include "plugin.h"
#include "menu-policy.h"

#include "dbg.h"
#include "gtk-compat.h"

/* support for libmenu-cache 0.4.x */
#ifndef MENU_CACHE_CHECK_VERSION
# ifdef HAVE_MENU_CACHE_DIR_LIST_CHILDREN
#  define MENU_CACHE_CHECK_VERSION(_a,_b,_c) (_a == 0 && _b < 5) /* < 0.5.0 */
# else
#  define MENU_CACHE_CHECK_VERSION(_a,_b,_c) 0 /* not even 0.4.0 */
# endif
#endif

#define DEFAULT_MENU_ICON PACKAGE_DATA_DIR "/images/my-computer.png"
/*
 * SuxPanel version 0.1
 * Copyright (c) 2003 Leandro Pereira <leandro@linuxmag.com.br>
 */

/*
 * menu style code was taken from suxpanel
 */

typedef struct {
    GtkWidget *menu, *box, *img, *label;
    char *fname, *caption;
    int iconsize;
    gboolean has_system_menu;
    guint show_system_menu_idle;
    LXPanel *panel;
    config_setting_t *settings;

    MenuCache* menu_cache;
    guint visibility_flags;
    gpointer reload_notify;
    FmDndSrc *ds;
} menup;

static guint idle_loader = 0;

GQuark SYS_MENU_ITEM_ID = 0;

/* FIXME: those are defined on panel main code */
void restart(void);
void gtk_run(void);
void logout(void);

static void on_data_get(FmDndSrc *ds, GtkWidget *mi)
{
    FmFileInfo *fi = g_object_get_qdata(G_OBJECT(mi), SYS_MENU_ITEM_ID);

    /* g_debug("on_data_get(...)"); */
    fm_dnd_src_set_file(ds, fi);
}

static void
menu_destructor(gpointer user_data)
{
    menup *m = (menup *)user_data;

    if( G_UNLIKELY( idle_loader ) )
    {
        g_source_remove( idle_loader );
        idle_loader = 0;
    }

    if (m->show_system_menu_idle)
        g_source_remove(m->show_system_menu_idle);

    g_signal_handlers_disconnect_matched(m->ds, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                         on_data_get, NULL);
    g_object_unref(G_OBJECT(m->ds));
    gtk_widget_destroy(m->menu);

    if( m->menu_cache )
    {
        menu_cache_remove_reload_notify(m->menu_cache, m->reload_notify);
        menu_cache_unref( m->menu_cache );
    }

    g_free(m->fname);
    g_free(m->caption);
    g_free(m);
    RET();
}

static void
spawn_app(GtkWidget *widget, gpointer data)
{
    ENTER;
    if (data) {
        fm_launch_command_simple(NULL, NULL, 0, data, NULL);
    }
    RET();
}


static void
run_command(GtkWidget *widget, void (*cmd)(void))
{
    ENTER;
    cmd();
    RET();
}

static void
menu_pos(GtkMenu *menu, gint *x, gint *y, gboolean *push_in, GtkWidget *widget)
{
    menup *m = lxpanel_plugin_get_data(widget);
    lxpanel_plugin_popup_set_position_helper(m->panel, widget, GTK_WIDGET(menu), x, y);
    *push_in = TRUE;
}

static void on_menu_item( GtkMenuItem* mi, menup* m )
{
    FmFileInfo *fi = g_object_get_qdata(G_OBJECT(mi), SYS_MENU_ITEM_ID);

    lxpanel_launch_path(m->panel, fm_file_info_get_path(fi));
}

/* load icon when mapping the menu item to speed up */
static void on_menu_item_map(GtkWidget *mi, menup *m)
{
  GtkImage* img = GTK_IMAGE(gtk_image_menu_item_get_image(GTK_IMAGE_MENU_ITEM(mi)));
    if( img )
    {
        FmFileInfo *fi;
        if (gtk_image_get_storage_type(img) == GTK_IMAGE_EMPTY &&
            (fi = g_object_get_qdata(G_OBJECT(mi), SYS_MENU_ITEM_ID)) != NULL &&
            fi != (gpointer)1 /* ignore placeholder or separator */)
        {
            FmIcon *fm_icon = fm_file_info_get_icon(fi);
            FmIcon *_fm_icon = NULL;
            GdkPixbuf *icon = NULL;

            if (fm_icon == NULL)
                fm_icon = _fm_icon = fm_icon_from_name("application-x-executable");
            icon = fm_pixbuf_from_icon_with_fallback(fm_icon, m->iconsize,
                                                         "application-x-executable");
            if (_fm_icon)
                g_object_unref(_fm_icon);
            if (icon)
            {
                gtk_image_set_from_pixbuf(img, icon);
                g_object_unref(icon);
            }
        }
    }
}

static void on_menu_item_style_set(GtkWidget* mi, GtkStyle* prev, menup *m)
{
    /* reload icon */
    on_menu_item_map(mi, m);
}

/* FIXME: this is very dirty, especially if desktop is set not to ~/Desktop
   therefore it should be removed and drag & drop gestures used instead */
static void on_add_menu_item_to_desktop(GtkMenuItem* item, GtkWidget* mi)
{
    FmFileInfo *fi = g_object_get_qdata(G_OBJECT(mi), SYS_MENU_ITEM_ID);
    FmPathList *files = fm_path_list_new();

    fm_path_list_push_tail(files, fm_file_info_get_path(fi));
    fm_link_files(NULL, files, fm_path_get_desktop());
    fm_path_list_unref(files);
}

/* TODO: add menu item to panel */
#if 0
static void on_add_menu_item_to_panel(GtkMenuItem* item, MenuCacheApp* app)
{
    /* Find a penel containing launchbar applet.
     * The launchbar with most buttons will be choosen if
     * there are several launchbar applets loaded.
     */
    GSList* l;
    Plugin* lb = NULL;
    int n_btns = -1;

    for(l = all_panels; !lb && l; l = l->next)
    {
        Panel* panel = (Panel*)l->data;
        GList* pl;
        for(pl=panel->plugins; pl; pl = pl->next)
        {
            Plugin* plugin = (Plugin*)pl;
            if( strcmp(plugin->class->type, "launchbar") == 0 )
            {
                /* FIXME: should we let the users choose which launcherbar to add the btn? */
                break;
#if 0
                int n = launchbar_get_n_btns(plugin);
                if( n > n_btns )
                {
                    lb = plugin;
                    n_btns = n;
                }
#endif
            }
        }
    }

    if( ! lb ) /* launchbar is not currently in use */
    {
        /* FIXME: add a launchbar plugin to the panel which has a menu, too. */
    }

    if( lb )
    {

    }
}
#endif

static void on_menu_item_properties(GtkMenuItem* item, GtkWidget* mi)
{
    FmFileInfo *fi = g_object_get_qdata(G_OBJECT(mi), SYS_MENU_ITEM_ID);
    FmFileInfoList *files = fm_file_info_list_new();

    fm_file_info_list_push_tail(files, fi);
    fm_show_file_properties(NULL, files);
    fm_file_info_list_unref(files);
}

#if 0
/* This following function restore_grabs is taken from menu.c of
 * gnome-panel.
 */
/*most of this function stolen from the real gtk_menu_popup*/
static void restore_grabs(GtkWidget *w, gpointer data)
{
    GtkWidget *menu_item = data;
    GtkMenu *menu = GTK_MENU(gtk_widget_get_parent(menu_item));
    GtkWidget *xgrab_shell;
    GtkWidget *parent;

    /* Find the last viewable ancestor, and make an X grab on it
    */
    parent = GTK_WIDGET (menu);
    xgrab_shell = NULL;
    while (parent)
    {
        gboolean viewable = TRUE;
        GtkWidget *tmp = parent;

        while (tmp)
        {
            if (!gtk_widget_get_mapped(tmp))
            {
                viewable = FALSE;
                break;
            }
            tmp = gtk_widget_get_parent(tmp);
        }

        if (viewable)
            xgrab_shell = parent;

        parent = gtk_widget_get_parent(parent);
    }

    /*only grab if this HAD a grab before*/
    if (xgrab_shell && (gtk_widget_has_focus(xgrab_shell)))
     {
        if (gdk_pointer_grab (gtk_widget_get_window(xgrab_shell), TRUE,
                    GDK_BUTTON_PRESS_MASK |
                    GDK_BUTTON_RELEASE_MASK |
                    GDK_ENTER_NOTIFY_MASK |
                    GDK_LEAVE_NOTIFY_MASK,
                    NULL, NULL, 0) == 0)
        {
            if (gdk_keyboard_grab (gtk_widget_get_window(xgrab_shell), TRUE,
                    GDK_CURRENT_TIME) == 0)
                gtk_widget_grab_focus (xgrab_shell);
            else
                gdk_pointer_ungrab (GDK_CURRENT_TIME);
        }
    }
    gtk_grab_add (GTK_WIDGET (menu));
}
#endif

static void restore_submenu(GtkMenuItem *mi, GtkWidget *submenu)
{
    g_signal_handlers_disconnect_by_func(mi, restore_submenu, submenu);
    gtk_menu_item_set_submenu(mi, submenu);
    g_object_set_data(G_OBJECT(mi), "PanelMenuItemSubmenu", NULL);
}

static gboolean on_menu_button_press(GtkWidget* mi, GdkEventButton* evt, menup* m)
{
    if( evt->button == 3 )  /* right */
    {
        GtkWidget* item;
        GtkMenu* p;

        /* don't make duplicates */
        if (g_signal_handler_find(mi, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
            restore_submenu, NULL))
        {
            return FALSE;
        }

        p = GTK_MENU(gtk_menu_new());

        item = gtk_menu_item_new_with_label(_("Add to desktop"));
        g_signal_connect(item, "activate", G_CALLBACK(on_add_menu_item_to_desktop), mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(p), item);

        item = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(p), item);

        item = gtk_menu_item_new_with_label(_("Properties"));
        g_signal_connect(item, "activate", G_CALLBACK(on_menu_item_properties), mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(p), item);

        item = gtk_menu_item_get_submenu(GTK_MENU_ITEM(mi)); /* reuse it */
        if (item)
        {
            /* set object data to keep reference on the submenu we preserve */
            g_object_set_data_full(G_OBJECT(mi), "PanelMenuItemSubmenu",
                                   g_object_ref(item), g_object_unref);
            gtk_menu_popdown(GTK_MENU(item));
        }
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), GTK_WIDGET(p));
        g_signal_connect(mi, "deselect", G_CALLBACK(restore_submenu), item);
        gtk_widget_show_all(GTK_WIDGET(p));
    }
    else if (evt->button == 1) /* allow drag on clicked item */
    {
        /* disconnect previous menu item */
        g_signal_handlers_disconnect_matched(m->ds, G_SIGNAL_MATCH_FUNC, 0, 0,
                                             NULL, on_data_get, NULL);
        /* remap FmDndSrc onto current item */
        fm_dnd_src_set_widget(m->ds, mi);
        g_signal_connect(m->ds, "data-get", G_CALLBACK(on_data_get), mi);
    }
    return FALSE;
}

static GtkWidget* create_item(MenuCacheItem *item, menup *m)
{
    GtkWidget* mi;
    if( menu_cache_item_get_type(item) == MENU_CACHE_TYPE_SEP )
    {
        mi = gtk_separator_menu_item_new();
        g_object_set_qdata(G_OBJECT(mi), SYS_MENU_ITEM_ID, (gpointer)1);
    }
    else
    {
        GtkWidget* img;
        /* create FmFileInfo for the item, it will be used in callbacks */
        char *mpath = menu_cache_dir_make_path(MENU_CACHE_DIR(item));
        FmPath *path = fm_path_new_relative(fm_path_get_apps_menu(), mpath+13);
                                                    /* skip "/Applications" */
        FmFileInfo *fi = fm_file_info_new_from_menu_cache_item(path, item);

        g_free(mpath);
        fm_path_unref(path);
        mi = gtk_image_menu_item_new_with_mnemonic( menu_cache_item_get_name(item) );
        g_object_set_qdata_full(G_OBJECT(mi), SYS_MENU_ITEM_ID, fi,
                                (GDestroyNotify)fm_file_info_unref);
        img = gtk_image_new();
        gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM(mi), img );
        if( menu_cache_item_get_type(item) == MENU_CACHE_TYPE_APP )
        {
            const char *comment = menu_cache_item_get_comment(item);
            if (comment != NULL)
                gtk_widget_set_tooltip_text(mi, comment);
            g_signal_connect(mi, "activate", G_CALLBACK(on_menu_item), m);
        }
        g_signal_connect(mi, "map", G_CALLBACK(on_menu_item_map), m);
        g_signal_connect(mi, "style-set", G_CALLBACK(on_menu_item_style_set), m);
        g_signal_connect(mi, "button-press-event", G_CALLBACK(on_menu_button_press), m);
        /* allow drag and add empty set for now to allow dragging the item
           the rest will be done by FmDndSrc after drag begins */
        gtk_drag_source_set(mi, GDK_BUTTON1_MASK, NULL, 0, GDK_ACTION_COPY);
    }
    gtk_widget_show( mi );
    return mi;
}

static int load_menu(menup* m, MenuCacheDir* dir, GtkWidget* menu, int pos )
{
    GSList * l;
    /* number of visible entries */
    gint count = 0;
#if MENU_CACHE_CHECK_VERSION(0, 4, 0)
    GSList *children;
# if MENU_CACHE_CHECK_VERSION(0, 5, 0)
#  if !MENU_CACHE_CHECK_VERSION(1, 0, 0)
    char *kfpath;
    GKeyFile *kf;
#  endif
    if (!menu_cache_dir_is_visible(dir)) /* directory is hidden, ignore children */
        return 0;
#  if !MENU_CACHE_CHECK_VERSION(1, 0, 0)
    /* version 1.0.0 has NoDisplay checked internally */
    kfpath = menu_cache_item_get_file_path(MENU_CACHE_ITEM(dir));
    kf = g_key_file_new();
    /* for version 0.5.0 we enable hidden so should test NoDisplay flag */
    if (kfpath && g_key_file_load_from_file(kf, kfpath, 0, NULL) &&
        g_key_file_get_boolean(kf, "Desktop Entry", "NoDisplay", NULL))
        count = -1;
    g_free(kfpath);
    g_key_file_free(kf);
    if (count < 0) /* directory is hidden, ignore children */
        return 0;
#  endif /* < 1.0.0 */
# endif /* < 0.5.0 */
    children = menu_cache_dir_list_children(dir);
    for (l = children; l; l = l->next)
#else /* < 0.4.0 */
    for( l = menu_cache_dir_get_children(dir); l; l = l->next )
#endif
    {
        MenuCacheItem* item = MENU_CACHE_ITEM(l->data);

        gboolean is_visible = ((menu_cache_item_get_type(item) != MENU_CACHE_TYPE_APP) ||
			       (panel_menu_item_evaluate_visibility(item, m->visibility_flags)));

	if (is_visible)
	{
            GtkWidget * mi = create_item(item, m);
	    count++;
            if (mi != NULL)
                gtk_menu_shell_insert( (GtkMenuShell*)menu, mi, pos );
            if( pos >= 0 )
                ++pos;
	    /* process subentries */
	    if (menu_cache_item_get_type(item) == MENU_CACHE_TYPE_DIR)
	    {
                GtkWidget* sub = gtk_menu_new();
		/*  always pass -1 for position */
		gint s_count = load_menu( m, MENU_CACHE_DIR(item), sub, -1 );
                if (s_count)
		    gtk_menu_item_set_submenu( GTK_MENU_ITEM(mi), sub );
		else
		{
		    /* don't keep empty submenus */
		    gtk_widget_destroy( sub );
		    gtk_widget_destroy( mi );
		    if (pos > 0)
			pos--;
		}
	    }
	}
    }
#if MENU_CACHE_CHECK_VERSION(0, 4, 0)
    g_slist_foreach(children, (GFunc)menu_cache_item_unref, NULL);
    g_slist_free(children);
#endif
    return count;
}



static gboolean sys_menu_item_has_data( GtkMenuItem* item )
{
   return (g_object_get_qdata( G_OBJECT(item), SYS_MENU_ITEM_ID ) != NULL);
}

static void _unload_old_icons(GtkMenu* menu, GtkIconTheme* theme, menup* m)
{
    GList *children, *child;
    GtkMenuItem* item;
    GtkWidget* sub_menu=NULL;

    children = gtk_container_get_children( GTK_CONTAINER(menu) );
    for( child = children; child; child = child->next )
    {
        item = GTK_MENU_ITEM( child->data );
        if( sys_menu_item_has_data( item ) )
        {
            GtkImage* img;
            item = GTK_MENU_ITEM( child->data );
            if( GTK_IS_IMAGE_MENU_ITEM(item) )
            {
	        img = GTK_IMAGE(gtk_image_menu_item_get_image(GTK_IMAGE_MENU_ITEM(item)));
                gtk_image_clear(img);
                if (gtk_widget_get_mapped(GTK_WIDGET(img)))
		    on_menu_item_map(GTK_WIDGET(item), m);
            }
        }
        else if( ( sub_menu = gtk_menu_item_get_submenu( item ) ) )
        {
	    _unload_old_icons(GTK_MENU(sub_menu), theme, m);
        }
    }
    g_list_free( children );
}

static void unload_old_icons(GtkIconTheme* theme, menup* m)
{
    _unload_old_icons(GTK_MENU(m->menu), theme, m);
}

static void remove_change_handler(gpointer id, GObject* menu)
{
    g_signal_handler_disconnect(gtk_icon_theme_get_default(), GPOINTER_TO_INT(id));
}

/*
 * Insert application menus into specified menu
 * menu: The parent menu to which the items should be inserted
 * pisition: Position to insert items.
             Passing -1 in this parameter means append all items
             at the end of menu.
 */
static void sys_menu_insert_items( menup* m, GtkMenu* menu, int position )
{
    MenuCacheDir* dir;
    guint change_handler;

    if( G_UNLIKELY( SYS_MENU_ITEM_ID == 0 ) )
        SYS_MENU_ITEM_ID = g_quark_from_static_string( "SysMenuItem" );

#if MENU_CACHE_CHECK_VERSION(0, 4, 0)
    dir = menu_cache_dup_root_dir(m->menu_cache);
#else
    dir = menu_cache_get_root_dir( m->menu_cache );
#endif
    if(dir)
    {
        load_menu( m, dir, GTK_WIDGET(menu), position );
#if MENU_CACHE_CHECK_VERSION(0, 4, 0)
        menu_cache_item_unref(MENU_CACHE_ITEM(dir));
#endif
    }
    else /* menu content is empty */
    {
        /* add a place holder */
        GtkWidget* mi = gtk_menu_item_new();
        g_object_set_qdata( G_OBJECT(mi), SYS_MENU_ITEM_ID, GINT_TO_POINTER(1) );
        gtk_menu_shell_insert(GTK_MENU_SHELL(menu), mi, position);
    }

    change_handler = g_signal_connect(gtk_icon_theme_get_default(), "changed", G_CALLBACK(unload_old_icons), m);
    g_object_weak_ref( G_OBJECT(menu), remove_change_handler, GINT_TO_POINTER(change_handler) );
}


static void
reload_system_menu( menup* m, GtkMenu* menu )
{
    GList *children, *child;
    GtkMenuItem* item;
    GtkWidget* sub_menu;
    gint idx;

    children = gtk_container_get_children( GTK_CONTAINER(menu) );
    for( child = children, idx = 0; child; child = child->next, ++idx )
    {
        item = GTK_MENU_ITEM( child->data );
        if( sys_menu_item_has_data( item ) )
        {
            do
            {
                item = GTK_MENU_ITEM( child->data );
                child = child->next;
                gtk_widget_destroy( GTK_WIDGET(item) );
            }while( child && sys_menu_item_has_data( child->data ) );
            sys_menu_insert_items( m, menu, idx );
            if( ! child )
                break;
        }
        else if( ( sub_menu = gtk_menu_item_get_submenu( item ) ) )
        {
            reload_system_menu( m, GTK_MENU(sub_menu) );
        }
    }
    g_list_free( children );
}

static void show_menu( GtkWidget* widget, menup* m, int btn, guint32 time )
{
    gtk_menu_popup(GTK_MENU(m->menu),
                   NULL, NULL,
                   (GtkMenuPositionFunc)menu_pos, widget,
                   btn, time);
}

static gboolean
menu_button_press_event(GtkWidget * widget, GdkEventButton * event, LXPanel * panel)
{
    ENTER;

    if (event->button == 1)
    {
        show_menu( widget, lxpanel_plugin_get_data(widget), event->button, event->time );
        RET(TRUE);
    }
    RET(FALSE);
}

static gboolean show_system_menu_idle(gpointer user_data)
{
    menup* m = (menup*)user_data;
    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;
    show_menu( m->box, m, 0, GDK_CURRENT_TIME );
    m->show_system_menu_idle = 0;
    return FALSE;
}

static void show_system_menu(GtkWidget *p)
{
    menup *m = lxpanel_plugin_get_data(p);

    if (m->has_system_menu && m->show_system_menu_idle == 0)
        /* FIXME: I've no idea why this doesn't work without timeout
                              under some WMs, like icewm. */
        m->show_system_menu_idle = g_timeout_add(200, show_system_menu_idle, m);
}

static GtkWidget *
make_button(menup *m, const gchar *fname, const gchar *name, GdkColor* tint, GtkWidget *menu)
{
    char* title = NULL;

    ENTER;
    m->menu = menu;

    if( name )
    {
        /* load the name from *.directory file if needed */
        if( g_str_has_suffix( name, ".directory" ) )
        {
            GKeyFile* kf = g_key_file_new();
            char* dir_file = g_build_filename( "desktop-directories", name, NULL );
            if( g_key_file_load_from_data_dirs( kf, dir_file, NULL, 0, NULL ) )
            {
                title = g_key_file_get_locale_string( kf, "Desktop Entry", "Name", NULL, NULL );
            }
            g_free( dir_file );
            g_key_file_free( kf );
        }

        m->img = lxpanel_button_new_for_icon(m->panel, fname, tint, title ? title : name);

        g_free( title );
    }
    else
    {
        m->img = lxpanel_button_new_for_icon(m->panel, fname, tint, NULL);
    }

    gtk_container_add(GTK_CONTAINER(m->box), m->img);

    m->ds = fm_dnd_src_new(NULL);

    RET(m->img);
}

/* those were in configurator.c initially but it's safer to have those here */
typedef struct {
    char *name;
    char *disp_name;
    void (*cmd)(void);
} Command;

static Command commands[] = {
    //{ "configure", N_("Preferences"), configure },
    { "run", N_("Run"), gtk_run },
    { "restart", N_("Restart"), restart },
    { "logout", N_("Logout"), logout },
    { NULL, NULL },
};

static GtkWidget *
read_item(menup *m, config_setting_t *s)
{
    const gchar *name, *fname, *action, *str;
    GtkWidget *item;
    Command *cmd_entry = NULL;
    char *tmp;

    ENTER;
    name = fname = action = NULL;

    config_setting_lookup_string(s, "name", &name);
    config_setting_lookup_string(s, "image", &fname);
    config_setting_lookup_string(s, "action", &action);
    if (config_setting_lookup_string(s, "command", &str))
    {
        Command *tmp;

        for (tmp = commands; tmp->name; tmp++) {
            if (!g_ascii_strcasecmp(str, tmp->name)) {
                cmd_entry = tmp;
                break;
            }
        }
    }
    /* menu button */
    if( cmd_entry ) /* built-in commands */
    {
        item = gtk_image_menu_item_new_with_label( _(cmd_entry->disp_name) );
        g_signal_connect(G_OBJECT(item), "activate", (GCallback)run_command, cmd_entry->cmd);
    }
    else if (action)
    {
        item = gtk_image_menu_item_new_with_label(name ? name : "");
        tmp = g_strdup(action);
        g_object_weak_ref(G_OBJECT(item), (GWeakNotify)g_free, tmp);
        g_signal_connect(G_OBJECT(item), "activate", (GCallback)spawn_app, tmp);
    }
    else
        goto error;
    gtk_container_set_border_width(GTK_CONTAINER(item), 0);
    if (fname) {
        GtkWidget *img;

        tmp = expand_tilda(fname);
        img = lxpanel_image_new_for_icon(m->panel, tmp, m->iconsize, NULL);
        gtk_widget_show(img);
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
        g_free(tmp);
    }
    RET(item);

 error:
    RET(NULL);
}

static GtkWidget *
read_separator(menup *m, config_setting_t *s)
{
    ENTER;
    RET(gtk_separator_menu_item_new());
}

static void on_reload_menu(MenuCache* cache, gpointer menu_pointer)
{
    menup *m = menu_pointer;
    /* g_debug("reload system menu!!"); */
    reload_system_menu( m, GTK_MENU(m->menu) );
}

static void
read_system_menu(GtkMenu *menu, menup *m, config_setting_t *s)
{
    if (m->menu_cache == NULL)
    {
        guint32 flags;
        m->menu_cache = panel_menu_cache_new(&flags);
        if (m->menu_cache == NULL)
        {
            g_warning("error loading applications menu");
            return;
        }
        m->visibility_flags = flags;
        m->reload_notify = menu_cache_add_reload_notify(m->menu_cache, on_reload_menu, m);
        sys_menu_insert_items( m, menu, -1 );
    }

    m->has_system_menu = TRUE;
}

#if 0
static void
read_include(Plugin *p, char **fp)
{
    ENTER;
    gchar *name;
    line s;
    menup *m = (menup *)p->priv;
    /* FIXME: this is disabled */
    ENTER;
    s.len = 256;
    name = NULL;
    if( fp )
    {
        while (lxpanel_get_line(fp, &s) != LINE_BLOCK_END) {
            if (s.type == LINE_VAR) {
                if (!g_ascii_strcasecmp(s.t[0], "name"))
                    name = expand_tilda(s.t[1]);
                else  {
                    ERR( "menu/include: unknown var %s\n", s.t[0]);
                    RET();
                }
            }
        }
    }
    if ((fp = fopen(name, "r"))) {
        LOG(LOG_INFO, "Including %s\n", name);
        m->files = g_slist_prepend(m->files, fp);
        p->fp = fp;
    } else {
        ERR("Can't include %s\n", name);
    }
    if (name) g_free(name);
    RET();
}
#endif

static GtkWidget *
read_submenu(menup *m, config_setting_t *s, gboolean as_item)
{
    GtkWidget *mi, *menu;
    const gchar *name, *fname, *str;
    config_setting_t *list = config_setting_add(s, "", PANEL_CONF_TYPE_LIST);
    GdkColor color={0, 0, 36 * 0xffff / 0xff, 96 * 0xffff / 0xff};
    guint i;

    ENTER;

    menu = gtk_menu_new ();
    gtk_container_set_border_width(GTK_CONTAINER(menu), 0);

    fname = NULL;
    name = NULL;
    config_setting_lookup_string(s, "name", &name);
    config_setting_lookup_string(s, "image", &fname);
    if (config_setting_lookup_string(s, "tintcolor", &str))
        gdk_color_parse(str, &color);

    for (i = 0; (s = config_setting_get_elem(list, i)) != NULL; i++)
    {
        str = config_setting_get_name(s);
        if (!g_ascii_strcasecmp(str, "item")) {
            mi = read_item(m, s);
        } else if (!g_ascii_strcasecmp(str, "separator")) {
            mi = read_separator(m, s);
        } else if (!g_ascii_strcasecmp(str, "system")) {
            read_system_menu(GTK_MENU(menu), m, s); /* add system menu items */
            continue;
        } else if (!g_ascii_strcasecmp(str, "menu")) {
            mi = read_submenu(m, s, TRUE);
#if 0
        } else if (!g_ascii_strcasecmp(str, "include")) {
            read_include(p, fp);
            continue;
#endif
        } else {
            g_warning("menu: unknown block %s", str);
            goto error;
        }
        if (!mi) {
            g_warning("menu: can't create menu item");
            goto error;
        }
        gtk_widget_show(mi);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
    }
    if (as_item) {
        mi = gtk_image_menu_item_new_with_label(name);
        if (fname) {
            GtkWidget *img;
            char *expanded = expand_tilda(fname);
            img = lxpanel_image_new_for_icon(m->panel, expanded, m->iconsize, NULL);
            gtk_widget_show(img);
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
            g_free(expanded);
        }
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi), menu);
    } else {
        m->fname = fname ? expand_tilda(fname) : g_strdup(DEFAULT_MENU_ICON);
        m->caption = g_strdup(name);
        mi = make_button(m, m->fname, name, &color, menu);
    }

    RET(mi);

error:
    // FIXME: we need to recursivly destroy all child menus and their items
    gtk_widget_destroy(menu);
    RET(NULL);
}

static GtkWidget *
menu_constructor(LXPanel *panel, config_setting_t *settings)
{
    menup *m;
    config_setting_t *s;
    int iw, ih;

    m = g_new0(menup, 1);
    g_return_val_if_fail(m != NULL, 0);

    gtk_icon_size_lookup( GTK_ICON_SIZE_MENU, &iw, &ih );
    m->iconsize = MAX(iw, ih);

    m->box = gtk_event_box_new();
    gtk_widget_set_has_window(m->box, FALSE);
    lxpanel_plugin_set_data(m->box, m, menu_destructor);

    /* Save construction pointers */
    m->panel = panel;
    m->settings = settings;

    /* Check if configuration exists */
    settings = config_setting_add(settings, "", PANEL_CONF_TYPE_LIST);
    if (config_setting_get_elem(settings, 0) == NULL)
    {
        /* create default menu */
        config_setting_add(settings, "system", PANEL_CONF_TYPE_GROUP);
        config_setting_add(settings, "separator", PANEL_CONF_TYPE_GROUP);
        s = config_setting_add(settings, "item", PANEL_CONF_TYPE_GROUP);
            config_group_set_string(s, "command", "run");
        config_setting_add(settings, "separator", PANEL_CONF_TYPE_GROUP);
        s = config_setting_add(settings, "item", PANEL_CONF_TYPE_GROUP);
            config_group_set_string(s, "command", "logout");
            config_group_set_string(s, "image", "gnome-logout");
        config_group_set_string(m->settings, "image", DEFAULT_MENU_ICON);
    }

    if (!read_submenu(m, m->settings, FALSE)) {
        g_warning("menu: plugin init failed");
        gtk_widget_destroy(m->box);
        return NULL;
    }

    /* FIXME: allow bind a global key to toggle menu using libkeybinder */
    return m->box;
}

static gboolean apply_config(gpointer user_data)
{
    GtkWidget *p = user_data;
    menup* m = lxpanel_plugin_get_data(p);

    if( m->fname ) {
        lxpanel_button_set_icon(m->img, m->fname, -1);
    }
    config_group_set_string(m->settings, "image", m->fname);
    /* config_group_set_int(m->settings, "panelSize", m->match_panel); */
    config_group_set_string(m->settings, "name", m->caption);
    return FALSE;
}

static GtkWidget *menu_config(LXPanel *panel, GtkWidget *p)
{
    menup* menu = lxpanel_plugin_get_data(p);
    return lxpanel_generic_config_dlg(_("Menu"), panel, apply_config, p,
                                      _("Icon"), &menu->fname, CONF_TYPE_FILE_ENTRY,
                                      /* _("Use panel size as icon size"), &menu->match_panel, CONF_TYPE_INT, */
                                      /* _("Caption"), &menu->caption, CONF_TYPE_STR, */
                                      NULL);
}

LXPanelPluginInit lxpanel_static_plugin_menu = {
    .name = N_("Menu"),
    .description = N_("Application Menu"),

    .new_instance = menu_constructor,
    .config = menu_config,
    .button_press_event = menu_button_press_event,
    .show_system_menu = show_system_menu
};

/* vim: set sw=4 et sts=4 : */
