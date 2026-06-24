/*
 * Copyright (C) 2006-2010 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *               2006-2008 Jim Huang <jserv.tw@gmail.com>
 *               2008 Fred Chien <fred@lxde.org>
 *               2009 Jürgen Hötzel <juergen@archlinux.org>
 *               2009-2010 Marty Jack <martyj19@comcast.net>
 *               2010 Lajos Kamocsay <lajos@panka.com>
 *               2012 Piotr Sipika <Piotr.Sipika@gmail.com>
 *               2012-2013 Henry Gebhardt <hsggebhardt@gmail.com>
 *               2012 Jack Chen <speed.up08311990@gmail.com>
 *               2012 Rafał Mużyło <galtgendo@gmail.com>
 *               2012 Michael Rawson <michaelrawson76@gmail.com>
 *               2012 Julien Lavergne <julien.lavergne@gmail.com>
 *               2013 Rouslan <rouslan-k@users.sourceforge.net>
 *               2013 peadaredwards <peadaredwards@users.sourceforge.net>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <locale.h>
#include <string.h>
#include <gdk/gdkx.h>
#include <libfm/fm-gtk.h>
#include <keybinder.h>

#define __LXPANEL_INTERNALS__

#include "private.h"
#include "misc.h"

#include "lxpanelctl.h"
#include "dbg.h"
#include "space.h"

static gchar *cfgfile = NULL;
static gchar version[] = VERSION;
static int config = 0;

static gboolean is_restarting = FALSE;

Command commands[] = {
    //{ "configure", N_("Preferences"), configure },
    { "run", N_("Run"), gtk_run },
    { "restart", N_("Restart"), restart },
    { "logout", N_("Logout"), logout },
    { NULL, NULL },
};

void restart(void)
{
    ENTER;
    is_restarting = TRUE;

    gtk_main_quit();
    RET();
}

static void process_client_msg ( XClientMessageEvent* ev )
{
    int cmd = ev->data.b[0];
    int monitor;
    int edge;
    char *plugin_type;
    char *command;
    switch( cmd )
    {
#ifndef DISABLE_MENU
        case LXPANEL_CMD_SYS_MENU:
        {
            GSList* l;
            for( l = all_panels; l; l = l->next )
            {
                LXPanel* p = (LXPanel*)l->data;
                GList *plugins, *pl;

                if (p->priv->box == NULL)
                    continue;
                plugins = gtk_container_get_children(GTK_CONTAINER(p->priv->box));
                for (pl = plugins; pl; pl = pl->next)
                {
                    const LXPanelPluginInit *init = PLUGIN_CLASS(pl->data);
                    if (init->show_system_menu)
                        /* queue to show system menu */
                        init->show_system_menu(pl->data);
                }
                g_list_free(plugins);
            }
            break;
        }
#endif
        case LXPANEL_CMD_RUN:
            gtk_run();
            break;
        case LXPANEL_CMD_CONFIG:
            {
            LXPanel * p = ((all_panels != NULL) ? all_panels->data : NULL);
            if (p != NULL)
                panel_configure(p, 0);
            }
            break;
        case LXPANEL_CMD_RESTART:
            restart();
            break;
        case LXPANEL_CMD_EXIT:
            gtk_main_quit();
            break;
        case LXPANEL_CMD_COMMAND:
            monitor = (ev->data.b[1] & 0xf) - 1; /* 0 for no monitor */
            edge = (ev->data.b[1] >> 4) & 0x7;
            if ((ev->data.b[1] & 0x80) != 0)
                /* some extension, not supported yet */
                break;
            plugin_type = g_strndup(&ev->data.b[2], 18);
            command = strchr(plugin_type, '\t');
            if (command) do /* use do{}while(0) to enable break */
            {
                LXPanel *p;
                GSList *l;
                GList *plugins, *pl;
                const LXPanelPluginInit *init;
                GtkWidget *plugin = NULL;

                *command++ = '\0';
                /* find the panel by monitor and edge */
                for (l = all_panels; l; l = l->next)
                {
                    p = (LXPanel*)l->data;
                    if (p->priv->box == NULL) /* inactive panel */
                        continue;
                    if (monitor >= 0 && p->priv->monitor != monitor)
                        continue;
                    if (edge == EDGE_NONE || p->priv->edge == edge)
                        break;
                }
                if (l == NULL) /* match not found */
                    break;
                /* find the plugin */
                init = g_hash_table_lookup(lxpanel_get_all_types(), plugin_type);
                if (init == NULL) /* no such plugin known */
                    break;
                plugins = gtk_container_get_children(GTK_CONTAINER(p->priv->box));
                for (pl = plugins; pl; pl = pl->next)
                {
                    if (init == PLUGIN_CLASS(pl->data))
                    {
                        plugin = pl->data;
                        break;
                    }
                }
                g_list_free(plugins);
                /* test for built-in commands ADD and DEL */
                if (strcmp(command, "ADD") == 0)
                {
                    if (plugin == NULL)
                    {
                        config_setting_t *cfg;

                        cfg = config_group_add_subgroup(config_root_setting(p->priv->config),
                                                        "Plugin");
                        config_group_set_string(cfg, "type", plugin_type);
                        plugin = lxpanel_add_plugin(p, plugin_type, cfg, -1);
                        if (plugin == NULL) /* failed to create */
                            config_setting_destroy(cfg);
                    }
                }
                else if (strcmp(command, "DEL") == 0)
                {
                    if (plugin != NULL)
                        lxpanel_remove_plugin(p, plugin);
                }
                /* send the command */
                else if (plugin && init->control)
                    init->control(plugin, command);
            } while(0);
            g_free(plugin_type);
            break;
    }
}

static GdkFilterReturn
panel_event_filter(GdkXEvent *xevent, GdkEvent *event, gpointer not_used)
{
    Atom at;
    Window win;
    XEvent *ev = (XEvent *) xevent;

    ENTER;
    DBG("win = 0x%x\n", ev->xproperty.window);
    if (ev->type != PropertyNotify )
    {
        /* private client message from lxpanelctl */
        if( ev->type == ClientMessage && ev->xproperty.atom == a_LXPANEL_CMD )
        {
            process_client_msg( (XClientMessageEvent*)ev );
        }
        else if( ev->type == DestroyNotify )
        {
            fb_ev_emit_destroy( fbev, ((XDestroyWindowEvent*)ev)->window );
        }
        RET(GDK_FILTER_CONTINUE);
    }

    at = ev->xproperty.atom;
    win = ev->xproperty.window;
    if (win == GDK_ROOT_WINDOW())
    {
        if (at == a_NET_CLIENT_LIST)
        {
            fb_ev_emit(fbev, EV_CLIENT_LIST);
        }
        else if (at == a_NET_CURRENT_DESKTOP)
        {
            GSList* l;
            for( l = all_panels; l; l = l->next )
                ((LXPanel*)l->data)->priv->curdesk = get_net_current_desktop();
            fb_ev_emit(fbev, EV_CURRENT_DESKTOP);
        }
        else if (at == a_NET_NUMBER_OF_DESKTOPS)
        {
            GSList* l;
            for( l = all_panels; l; l = l->next )
                ((LXPanel*)l->data)->priv->desknum = get_net_number_of_desktops();
            fb_ev_emit(fbev, EV_NUMBER_OF_DESKTOPS);
        }
        else if (at == a_NET_DESKTOP_NAMES)
        {
            fb_ev_emit(fbev, EV_DESKTOP_NAMES);
        }
        else if (at == a_NET_ACTIVE_WINDOW)
        {
            fb_ev_emit(fbev, EV_ACTIVE_WINDOW );
        }
        else if (at == a_NET_CLIENT_LIST_STACKING)
        {
            fb_ev_emit(fbev, EV_CLIENT_LIST_STACKING);
        }
        else if (at == a_XROOTPMAP_ID)
        {
            GSList* l;
            for( l = all_panels; l; l = l->next )
                _panel_queue_update_background((LXPanel*)l->data);
        }
        else
            return GDK_FILTER_CONTINUE;

        return GDK_FILTER_REMOVE;
    }
    return GDK_FILTER_CONTINUE;
}

/* The same for new plugins type - they will be not unloaded by FmModule */
#define REGISTER_STATIC_MODULE(pc) do { \
    extern LXPanelPluginInit lxpanel_static_plugin_##pc; \
    lxpanel_register_plugin_type(#pc, &lxpanel_static_plugin_##pc); } while (0)

/* Initialize the static plugins. */
static void init_static_plugins(void)
{
#ifdef STATIC_SEPARATOR
    REGISTER_STATIC_MODULE(separator);
#endif

#ifdef STATIC_LAUNCHTASKBAR
    REGISTER_STATIC_MODULE(launchtaskbar);
#endif

#ifdef STATIC_DCLOCK
    REGISTER_STATIC_MODULE(dclock);
#endif

#ifdef STATIC_WINCMD
    REGISTER_STATIC_MODULE(wincmd);
#endif

#ifdef STATIC_DIRMENU
    REGISTER_STATIC_MODULE(dirmenu);
#endif

#ifdef STATIC_PAGER
    REGISTER_STATIC_MODULE(pager);
#endif

#ifdef STATIC_TRAY
    REGISTER_STATIC_MODULE(tray);
#endif

#ifndef DISABLE_MENU
#ifdef STATIC_MENU
    REGISTER_STATIC_MODULE(menu);
#endif
#endif
}

static void
usage()
{
    g_print(_("lxpanel %s - lightweight GTK+ panel for UNIX desktops\n"), version);
    g_print(_("Command line options:\n"));
    g_print(_(" --help      -- print this help and exit\n"));
    g_print(_(" --version   -- print version and exit\n"));
//    g_print(_(" --log <number> -- set log level 0-5. 0 - none 5 - chatty\n"));
//    g_print(_(" --configure -- launch configuration utility\n"));
    g_print(_(" --profile name -- use specified profile\n"));
    g_print("\n");
    g_print(_(" -h  -- same as --help\n"));
    g_print(_(" -p  -- same as --profile\n"));
    g_print(_(" -v  -- same as --version\n"));
 //   g_print(_(" -C  -- same as --configure\n"));
    g_print(_("\nVisit http://lxde.org/ for detail.\n\n"));
}

/* Lightweight lock related functions - X clipboard hacks */

#define CLIPBOARD_NAME "LXPANEL_SELECTION"

/*
 * clipboard_get_func - dummy get_func for gtk_clipboard_set_with_data ()
 */
static void
clipboard_get_func(
    GtkClipboard *clipboard G_GNUC_UNUSED,
    GtkSelectionData *selection_data G_GNUC_UNUSED,
    guint info G_GNUC_UNUSED,
    gpointer user_data_or_owner G_GNUC_UNUSED)
{
}

/*
 * clipboard_clear_func - dummy clear_func for gtk_clipboard_set_with_data ()
 */
static void clipboard_clear_func(
    GtkClipboard *clipboard G_GNUC_UNUSED,
    gpointer user_data_or_owner G_GNUC_UNUSED)
{
}

/*
 * Lightweight version for checking single instance.
 * Try and get the CLIPBOARD_NAME clipboard instead of using file manipulation.
 *
 * Returns TRUE if successfully retrieved and FALSE otherwise.
 */
static gboolean check_main_lock()
{
    static const GtkTargetEntry targets[] = { { CLIPBOARD_NAME, 0, 0 } };
    gboolean retval = FALSE;
    GtkClipboard *clipboard;
    Atom atom;
    Display *xdisplay = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());

    atom = gdk_x11_get_xatom_by_name(CLIPBOARD_NAME);

    XGrabServer(xdisplay);

    if (XGetSelectionOwner(xdisplay, atom) != None)
        goto out;

    clipboard = gtk_clipboard_get(gdk_atom_intern(CLIPBOARD_NAME, FALSE));

    if (gtk_clipboard_set_with_data(clipboard, targets,
                                    G_N_ELEMENTS (targets),
                                    clipboard_get_func,
                                    clipboard_clear_func, NULL))
        retval = TRUE;

out:
    XUngrabServer (xdisplay);
    gdk_flush ();

    return retval;
}
#undef CLIPBOARD_NAME

static void _start_panels_from_dir(const char *panel_dir)
{
    GDir* dir = g_dir_open( panel_dir, 0, NULL );
    const gchar* name;

    if( ! dir )
    {
        return;
    }

    while((name = g_dir_read_name(dir)) != NULL)
    {
        char* panel_config = g_build_filename( panel_dir, name, NULL );
        if (strchr(panel_config, '~') == NULL)    /* Skip editor backup files in case user has hand edited in this directory */
        {
            LXPanel* panel = panel_new( panel_config, name );
            if( panel )
                all_panels = g_slist_prepend( all_panels, panel );
        }
        g_free( panel_config );
    }
    g_dir_close( dir );
}

static gboolean start_all_panels( )
{
    char *panel_dir;
    const gchar * const * dir;

    /* try user panels */
    panel_dir = _user_config_file_name("panels", NULL);
    _start_panels_from_dir(panel_dir);
    g_free(panel_dir);
    if (all_panels != NULL)
        return TRUE;
    /* else try XDG fallbacks */
    dir = g_get_system_config_dirs();
    if (dir) while (dir[0])
    {
        panel_dir = _system_config_file_name(dir[0], "panels");
        _start_panels_from_dir(panel_dir);
        g_free(panel_dir);
        if (all_panels != NULL)
            return TRUE;
        dir++;
    }
    /* last try at old fallback for compatibility reasons */
    panel_dir = _old_system_config_file_name("panels");
    _start_panels_from_dir(panel_dir);
    g_free(panel_dir);
    return all_panels != NULL;
}

static void _ensure_user_config_dirs(void)
{
    char *dir = g_build_filename(g_get_user_config_dir(), "lxpanel", cprofile,
                                 "panels", NULL);

    /* make sure the private profile and panels dir exists */
    g_mkdir_with_parents(dir, 0700);
    g_free(dir);
}

int main(int argc, char *argv[], char *env[])
{
    int i;
    const char* desktop_name;
    char *file;

    setlocale(LC_CTYPE, "");

#if !GLIB_CHECK_VERSION(2, 32, 0)
    g_thread_init(NULL);
#endif
/*    gdk_threads_init();
    gdk_threads_enter(); */

    gtk_init(&argc, &argv);
    keybinder_init();

#ifdef ENABLE_NLS
    bindtextdomain ( GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR );
    bind_textdomain_codeset ( GETTEXT_PACKAGE, "UTF-8" );
    textdomain ( GETTEXT_PACKAGE );
#endif

    XSetLocaleModifiers("");
    XSetErrorHandler((XErrorHandler) panel_handle_x_error);

    resolve_atoms();

    desktop_name = g_getenv("XDG_CURRENT_DESKTOP");
    is_in_lxde = desktop_name && (0 == strcmp(desktop_name, "LXDE"));

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage();
            exit(0);
        } else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--version")) {
            printf("lxpanel %s\n", version);
            exit(0);
        } else if (!strcmp(argv[i], "--log")) {
            i++;
            if (i == argc) {
                g_critical( "lxpanel: missing log level");
                usage();
                exit(1);
            } else {
                /* deprecated */
            }
        } else if (!strcmp(argv[i], "--configure") || !strcmp(argv[i], "-C")) {
            config = 1;
        } else if (!strcmp(argv[i], "--profile") || !strcmp(argv[i], "-p")) {
            i++;
            if (i == argc) {
                g_critical( "lxpanel: missing profile name");
                usage();
                exit(1);
            } else {
                cprofile = g_strdup(argv[i]);
            }
        } else {
            printf("lxpanel: unknown option - %s\n", argv[i]);
            usage();
            exit(1);
        }
    }

    /* Add a gtkrc file to be parsed too. */
    file = _user_config_file_name("gtkrc", NULL);
    gtk_rc_parse(file);
    g_free(file);

    /* Check for duplicated lxpanel instances */
    if (!check_main_lock() && !config) {
        printf("There is already an instance of LXPanel.  Now to exit\n");
        exit(1);
    }

    _ensure_user_config_dirs();

    /* Add our own icons to the search path of icon theme */
    gtk_icon_theme_append_search_path( gtk_icon_theme_get_default(), PACKAGE_DATA_DIR "/images" );

    fbev = fb_ev_new();

    is_restarting = FALSE;

    /* init LibFM */
    fm_gtk_init(NULL);

    /* prepare modules data */
    lxpanel_prepare_modules();
    lxpanel_register_plugin_type("space", &_lxpanel_static_plugin_space);
    init_static_plugins();

    load_global_config();

    /* NOTE: StructureNotifyMask is required by XRandR
     * See init_randr_support() in gdkscreen-x11.c of gtk+ for detail.
     */
    gdk_window_set_events(gdk_get_default_root_window(), GDK_STRUCTURE_MASK |
            GDK_SUBSTRUCTURE_MASK | GDK_PROPERTY_CHANGE_MASK);
    gdk_window_add_filter(gdk_get_default_root_window (), (GdkFilterFunc)panel_event_filter, NULL);

    if( G_UNLIKELY( ! start_all_panels() ) )
        g_warning( "Config files are not found.\n" );
/*
 * FIXME: configure??
    if (config)
        configure();
*/
    gtk_main();

    XSelectInput (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), GDK_ROOT_WINDOW(), NoEventMask);
    gdk_window_remove_filter(gdk_get_default_root_window (), (GdkFilterFunc)panel_event_filter, NULL);

    /* destroy all panels */
    g_slist_foreach( all_panels, (GFunc) gtk_widget_destroy, NULL );
    g_slist_free( all_panels );
    all_panels = NULL;
    g_free( cfgfile );

    free_global_config();

    lxpanel_unload_modules();
    fm_gtk_finalize();

    /* gdk_threads_leave(); */

    g_object_unref(fbev);

    if (!is_restarting)
        return 0;
    if (strchr(argv[0], G_DIR_SEPARATOR))
        execve(argv[0], argv, env);
    else
        execve(g_find_program_in_path(argv[0]), argv, env);
    return 1;
}
