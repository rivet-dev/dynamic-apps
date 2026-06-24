/*
 * gtk-run.c: Little application launcher
 * Copyright (C) 2006-2010 Hong Jen Yee (PCMan) pcman.tw(AT)gmail.com
 *               2006-2008 Jim Huang <jserv.tw@gmail.com>
 *               2008 Fred Chien <fred@lxde.org>
 *               2009 Ying-Chun Liu (PaulLiu) <grandpaul@gmail.com>
 *               2009-2010 Marty Jack <martyj19@comcast.net>
 *               2012-2013 Henry Gebhardt <hsggebhardt@gmail.com>
 *               2012 Piotr Sipika <Piotr.Sipika@gmail.com>
 *               2014 Andriy Grytsenko <andrej@rep.kiev.ua>
 *
 * This file is a part of LXPanel project.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glib/gi18n.h>
#include <string.h>
#include <unistd.h>

#include "misc.h"
#include "private.h"
#ifndef DISABLE_MENU
#include <menu-cache.h>
#endif
#include <libfm/fm-gtk.h>

#include "gtk-compat.h"

static GtkWidget* win = NULL; /* the run dialog */
#ifndef DISABLE_MENU
static MenuCache* menu_cache = NULL;
static GSList* app_list = NULL; /* all known apps in menu cache */
static gpointer reload_notify_id = NULL;
#endif

typedef struct _ThreadData
{
    gboolean cancel; /* is the loading cancelled */
    GSList* files; /* all executable files found */
    GtkEntry* entry;
}ThreadData;

static ThreadData* thread_data = NULL; /* thread data used to load availble programs in PATH */

#ifndef DISABLE_MENU
static MenuCacheApp* match_app_by_exec(const char* exec)
{
    GSList* l;
    MenuCacheApp* ret = NULL;
    char* exec_path = g_find_program_in_path(exec);
    const char* pexec;
    int path_len, exec_len, len;

    if( ! exec_path )
        return NULL;

    path_len = strlen(exec_path);
    exec_len = strlen(exec);

    for( l = app_list; l; l = l->next )
    {
        MenuCacheApp* app = MENU_CACHE_APP(l->data);
        const char* app_exec = menu_cache_app_get_exec(app);
        if ( ! app_exec)
            continue;
#if 0   /* This is useless and incorrect. */
        /* Dirty hacks to skip sudo programs. This can be a little bit buggy */
        if( g_str_has_prefix(app_exec, "gksu") )
        {
            app_exec += 4;
            if( app_exec[0] == '\0' ) /* "gksu" itself */
                app_exec -= 4;
            else if( app_exec[0] == ' ' ) /* "gksu something..." */
                ++app_exec;
            else if( g_str_has_prefix(app_exec, "do ") ) /* "gksudo something" */
                app_exec += 3;
        }
        else if( g_str_has_prefix(app_exec, "kdesu ") ) /* kdesu */
            app_exec += 6;
#endif

        if( g_path_is_absolute(app_exec) )
        {
            pexec = exec_path;
            len = path_len;
        }
        else
        {
            pexec = exec;
            len = exec_len;
        }

        if( strncmp(app_exec, pexec, len) == 0 )
        {
            /* exact match has the highest priority */
            if( app_exec[len] == '\0' )
            {
                ret = app;
                break;
            }
            /* those matches the pattern: exe_name %F|%f|%U|%u have higher priority */
            if( app_exec[len] == ' ' )
            {
                if( app_exec[len + 1] == '%' )
                {
                    if( strchr( "FfUu", app_exec[len + 2] ) )
                    {
                        ret = app;
                        break;
                    }
                }
                ret = app;
            }
        }
    }

    /* if this is a symlink */
    if( ! ret && g_file_test(exec_path, G_FILE_TEST_IS_SYMLINK) )
    {
        char target[512]; /* FIXME: is this enough? */
        len = readlink( exec_path, target, sizeof(target) - 1);
        if( len > 0 )
        {
            target[len] = '\0';
            ret = match_app_by_exec(target);
            if( ! ret )
            {
                /* FIXME: Actually, target could be relative paths.
                 *        So, actually path resolution is needed here. */
                char* basename = g_path_get_basename(target);
                char* locate = g_find_program_in_path(basename);
                if( locate && strcmp(locate, target) == 0 )
                {
                    ret = match_app_by_exec(basename);
                    g_free(locate);
                }
                g_free(basename);
            }
        }
    }

    g_free(exec_path);
    return ret;
}
#endif

static void setup_auto_complete_with_data(ThreadData* data)
{
    GtkListStore* store;
    GSList *l;
    GtkEntryCompletion* comp = gtk_entry_completion_new();
    gtk_entry_completion_set_minimum_key_length( comp, 2 );
    gtk_entry_completion_set_inline_completion( comp, TRUE );
    gtk_entry_completion_set_popup_set_width( comp, TRUE );
    gtk_entry_completion_set_popup_single_match( comp, FALSE );
    store = gtk_list_store_new( 1, G_TYPE_STRING );

    for( l = data->files; l; l = l->next )
    {
        const char *name = (const char*)l->data;
        GtkTreeIter it;
        gtk_list_store_append( store, &it );
        gtk_list_store_set( store, &it, 0, name, -1 );
    }

    gtk_entry_completion_set_model( comp, (GtkTreeModel*)store );
    g_object_unref( store );
    gtk_entry_completion_set_text_column( comp, 0 );
    gtk_entry_set_completion( (GtkEntry*)data->entry, comp );

    /* trigger entry completion */
    gtk_entry_completion_complete(comp);
    g_object_unref( comp );
}

static void thread_data_free(ThreadData* data)
{
    g_slist_foreach(data->files, (GFunc)g_free, NULL);
    g_slist_free(data->files);
    g_slice_free(ThreadData, data);
}

static gboolean on_thread_finished(ThreadData* data)
{
    /* don't setup entry completion if the thread is already cancelled. */
    if( !data->cancel )
        setup_auto_complete_with_data(thread_data);
    thread_data_free(data);
    thread_data = NULL; /* global thread_data pointer */
    return FALSE;
}

static gpointer thread_func(ThreadData* data)
{
    GSList *list = NULL;
    gchar **dirname;
    gchar **dirnames = g_strsplit( g_getenv("PATH"), ":", 0 );

    for( dirname = dirnames; !thread_data->cancel && *dirname; ++dirname )
    {
        GDir *dir = g_dir_open( *dirname, 0, NULL );
        const char *name;
        if( ! dir )
            continue;
        while( !thread_data->cancel && (name = g_dir_read_name(dir)) )
        {
            char* filename = g_build_filename( *dirname, name, NULL );
            if( g_file_test( filename, G_FILE_TEST_IS_EXECUTABLE ) )
            {
                if(thread_data->cancel)
                    break;
                if( !g_slist_find_custom( list, name, (GCompareFunc)strcmp ) )
                    list = g_slist_prepend( list, g_strdup( name ) );
            }
            g_free( filename );
        }
        g_dir_close( dir );
    }
    g_strfreev( dirnames );

    data->files = list;
    /* install an idle handler to free associated data */
    g_idle_add((GSourceFunc)on_thread_finished, data);
#if GLIB_CHECK_VERSION(2, 32, 0)
    g_thread_unref(g_thread_self());
#endif

    return NULL;
}

static void setup_auto_complete( GtkEntry* entry )
{
    gboolean cache_is_available = FALSE;
    /* FIXME: consider saving the list of commands as on-disk cache. */
    if( cache_is_available )
    {
        /* load cached program list */
    }
    else
    {
        /* load in another working thread */
        thread_data = g_slice_new0(ThreadData); /* the data will be freed in idle handler later. */
        thread_data->entry = entry;
#if GLIB_CHECK_VERSION(2, 32, 0)
        g_thread_new("gtk-run-autocomplete", (GThreadFunc)thread_func, thread_data);
        /* we don't use loader_thread_id but Glib 2.32 crashes if we unref
           GThread while it's in creation progress. It is a bug of GLib
           certainly but as workaround we'll unref it in the thread itself */
#else
        g_thread_create((GThreadFunc)thread_func, thread_data, FALSE, NULL);
#endif
    }
}

#ifndef DISABLE_MENU
static void reload_apps(MenuCache* cache, gpointer user_data)
{
    g_debug("reload apps!");
    if(app_list)
    {
        g_slist_foreach(app_list, (GFunc)menu_cache_item_unref, NULL);
        g_slist_free(app_list);
    }
    app_list = menu_cache_list_all_apps(cache);
}
#endif

static void on_response( GtkDialog* dlg, gint response, gpointer user_data )
{
    GtkEntry* entry = (GtkEntry*)user_data;
    if( G_LIKELY(response == GTK_RESPONSE_OK) )
    {
        if (!fm_launch_command_simple(GTK_WINDOW(dlg), NULL, 0, gtk_entry_get_text(entry), NULL))
        {
            g_signal_stop_emission_by_name( dlg, "response" );
            return;
        }
    }

    /* cancel running thread if needed */
    if( thread_data ) /* the thread is still running */
        thread_data->cancel = TRUE; /* cancel the thread */

    gtk_widget_destroy( (GtkWidget*)dlg );
    win = NULL;

#ifndef DISABLE_MENU
    /* free app list */
    g_slist_foreach(app_list, (GFunc)menu_cache_item_unref, NULL);
    g_slist_free(app_list);
    app_list = NULL;

    /* free menu cache */
    menu_cache_remove_reload_notify(menu_cache, reload_notify_id);
    reload_notify_id = NULL;
    menu_cache_unref(menu_cache);
    menu_cache = NULL;
#endif
}

#ifndef DISABLE_MENU
static void on_entry_changed( GtkEntry* entry, GtkImage* img )
{
    const char* str = gtk_entry_get_text(entry);
    MenuCacheApp* app = NULL;
    if( str && *str )
        app = match_app_by_exec(str);

    if( app )
    {
        int w, h;
        const char *name = menu_cache_item_get_icon(MENU_CACHE_ITEM(app));
        FmIcon * fm_icon;
        GdkPixbuf* pix;

        gtk_icon_size_lookup(GTK_ICON_SIZE_DIALOG, &w, &h);
        fm_icon = fm_icon_from_name(name ? name : "application-x-executable");
        pix = fm_pixbuf_from_icon_with_fallback(fm_icon, h, "application-x-executable");
        g_object_unref(fm_icon);
        gtk_image_set_from_pixbuf(img, pix);
        g_object_unref(pix);
    }
    else
    {
        gtk_image_set_from_stock(img, GTK_STOCK_EXECUTE, GTK_ICON_SIZE_DIALOG);
    }
}
#endif

static void activate_window(GtkWindow* toplevel_window)
{
    /* Calling gtk_window_present() cannot support
     * source indication. Use our own implementation.
     * Without this, the window activated might be
     * put in background by WM to avoid stealing
     * of focus.
     * See EWMH spec, source indication part:
     * http://standards.freedesktop.org/wm-spec/wm-spec-latest.html#sourceindication
     */
    GtkWidget* widget = GTK_WIDGET(toplevel_window);
    GdkScreen* screen = gtk_widget_get_screen(widget);
    if (gdk_x11_screen_supports_net_wm_hint (screen,
					   gdk_atom_intern_static_string ("_NET_ACTIVE_WINDOW")))
    {
        GdkWindow* window = gtk_widget_get_window(widget);

        /* show the window first */
        gtk_widget_show(widget);

        /* then, activate it */
        Xclimsgx(GDK_SCREEN_XSCREEN(screen), GDK_WINDOW_XID(window),
                 a_NET_ACTIVE_WINDOW, 2, /* source indication: 2 represents direct user actions. */
                 gtk_get_current_event_time(),
                 None /* currently active window */, 0, 0);
    } /* if _NET_ACTIVE_WINDOW command is not supported */
    else
        gtk_window_present(toplevel_window);
}

void gtk_run()
{
    GtkWidget *entry, *hbox, *img, *dlg_vbox;

    if(!win)
    {
        win = gtk_dialog_new_with_buttons( _("Run"),
                                           NULL,
                                           GTK_DIALOG_NO_SEPARATOR,
                                           GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                           GTK_STOCK_OK, GTK_RESPONSE_OK,
                                           NULL );
        gtk_dialog_set_alternative_button_order((GtkDialog*)win,
                                GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL, -1);
        gtk_dialog_set_default_response( (GtkDialog*)win, GTK_RESPONSE_OK );
        entry = gtk_entry_new();

        gtk_entry_set_activates_default( (GtkEntry*)entry, TRUE );
        dlg_vbox = gtk_dialog_get_content_area((GtkDialog*)win);
        gtk_box_pack_start( (GtkBox*)dlg_vbox,
                             gtk_label_new(_("Enter the command you want to execute:")),
                             FALSE, FALSE, 8 );
        hbox = gtk_hbox_new( FALSE, 2 );
        img = gtk_image_new_from_stock( GTK_STOCK_EXECUTE, GTK_ICON_SIZE_DIALOG );
        gtk_box_pack_start( (GtkBox*)hbox, img,
                             FALSE, FALSE, 4 );
        gtk_box_pack_start( (GtkBox*)hbox, entry, TRUE, TRUE, 4 );
        gtk_box_pack_start( (GtkBox*)dlg_vbox,
                             hbox, FALSE, FALSE, 8 );
        g_signal_connect( win, "response", G_CALLBACK(on_response), entry );
        gtk_window_set_position( (GtkWindow*)win, GTK_WIN_POS_CENTER );
        gtk_window_set_default_size( (GtkWindow*)win, 360, -1 );
        gtk_widget_show_all( win );

        setup_auto_complete( (GtkEntry*)entry );
        gtk_widget_show(win);

#ifndef DISABLE_MENU
        g_signal_connect(entry ,"changed", G_CALLBACK(on_entry_changed), img);

        /* get all apps */
#ifndef MENU_CACHE_CHECK_VERSION
#define MENU_CACHE_CHECK_VERSION(a,b,c) 0
#endif
#if MENU_CACHE_CHECK_VERSION(0, 6, 1)
        menu_cache = menu_cache_lookup_sync(g_getenv("XDG_MENU_PREFIX") ? "applications.menu" : "lxde-applications.menu" );
        if( menu_cache )
        {
#else
        /* SF bug #689: menu_cache_lookup_sync() was fail-prone before 0.6.1 */
        menu_cache = menu_cache_lookup(g_getenv("XDG_MENU_PREFIX") ? "applications.menu" : "lxde-applications.menu" );
        if( menu_cache )
        {
            menu_cache_reload(menu_cache);
#endif
            app_list = menu_cache_list_all_apps(menu_cache);
            reload_notify_id = menu_cache_add_reload_notify(menu_cache, reload_apps, NULL);
        }
#endif
    }

    activate_window(GTK_WINDOW(win));
}


/* vim: set sw=4 et sts=4 ts=4 : */
