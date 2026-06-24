/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   openbox.c for the Openbox window manager
   Copyright (c) 2006        Mikael Magnusson
   Copyright (c) 2003-2007   Dana Jansens

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   See the COPYING file for a copy of the GNU General Public License.
*/

#include "debug.h"
#include "openbox.h"
#include "session.h"
#include "dock.h"
#include "event.h"
#include "menu.h"
#include "client.h"
#include "screen.h"
#include "actions.h"
#include "startupnotify.h"
#include "focus.h"
#include "focus_cycle.h"
#include "focus_cycle_indicator.h"
#include "focus_cycle_popup.h"
#include "moveresize.h"
#include "frame.h"
#include "framerender.h"
#include "keyboard.h"
#include "mouse.h"
#include "menuframe.h"
#include "grab.h"
#include "group.h"
#include "config.h"
#include "ping.h"
#include "prompt.h"
#include "gettext.h"
#include "obrender/render.h"
#include "obrender/theme.h"
#include "obt/display.h"
#include "obt/xqueue.h"
#include "obt/signal.h"
#include "obt/prop.h"
#include "obt/keyboard.h"
#include "obt/xml.h"

#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif
#ifdef HAVE_SIGNAL_H
#  include <signal.h>
#endif
#ifdef HAVE_STDLIB_H
#  include <stdlib.h>
#endif
#ifdef HAVE_LOCALE_H
#  include <locale.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#  include <sys/types.h>
#  include <sys/wait.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#include <errno.h>

#include <X11/cursorfont.h>
#if USE_XCURSOR
#include <X11/Xcursor/Xcursor.h>
#endif

RrInstance   *ob_rr_inst;
RrImageCache *ob_rr_icons;
RrTheme      *ob_rr_theme;
GMainLoop    *ob_main_loop;
gint          ob_screen;
gboolean      ob_replace_wm = FALSE;
gboolean      ob_sm_use = TRUE;
gchar        *ob_sm_id = NULL;
gchar        *ob_sm_save_file = NULL;
gboolean      ob_sm_restore = TRUE;
gboolean      ob_debug_xinerama = FALSE;
const gchar  *ob_locale_msg = NULL;

static ObState   state;
static gboolean  xsync = FALSE;
static gboolean  reconfigure = FALSE;
static gboolean  restart = FALSE;
static gchar    *restart_path = NULL;
static Cursor    cursors[OB_NUM_CURSORS];
static gint      exitcode = 0;
static guint     remote_control = 0;
static gboolean  being_replaced = FALSE;
static gchar    *config_file = NULL;
static gchar    *startup_cmd = NULL;

static void signal_handler(gint signal, gpointer data);
static void remove_args(gint *argc, gchar **argv, gint index, gint num);
static void parse_env();
static void parse_args(gint *argc, gchar **argv);
static Cursor load_cursor(const gchar *name, guint fontval);
static void run_startup_cmd(void);

gint main(gint argc, gchar **argv)
{
    gchar *program_name;

    obt_signal_listen();

    ob_set_state(OB_STATE_STARTING);

    ob_debug_startup();

    /* initialize the locale */
    if (!(ob_locale_msg = setlocale(LC_MESSAGES, "")))
        g_message("Couldn't set messages locale category from environment.");
    if (!setlocale(LC_ALL, ""))
        g_message("Couldn't set locale from environment.");
    bindtextdomain(PACKAGE_NAME, LOCALEDIR);
    bind_textdomain_codeset(PACKAGE_NAME, "UTF-8");
    textdomain(PACKAGE_NAME);

    if (chdir(g_get_home_dir()) == -1)
        g_message(_("Unable to change to home directory \"%s\": %s"),
                  g_get_home_dir(), g_strerror(errno));

    /* parse the command line args, which can change the argv[0] */
    parse_args(&argc, argv);
    /* parse the environment variables */
    parse_env();

    program_name = g_path_get_basename(argv[0]);
    g_set_prgname(program_name);

    if (!remote_control)
        session_startup(argc, argv);

    if (!obt_display_open(NULL))
        ob_exit_with_error(_("Failed to open the display from the DISPLAY environment variable."));

    if (remote_control) {
        /* Send client message telling the OB process to:
         * remote_control = 1 -> reconfigure
         * remote_control = 2 -> restart */
        OBT_PROP_MSG(ob_screen, obt_root(ob_screen),
                     OB_CONTROL, remote_control, 0, 0, 0, 0);
        obt_display_close();
        exit(EXIT_SUCCESS);
    }

    ob_main_loop = g_main_loop_new(NULL, FALSE);

    /* set up signal handlers, they are called from the mainloop
       in the main program's thread */
    obt_signal_add_callback(SIGUSR1, signal_handler, NULL);
    obt_signal_add_callback(SIGUSR2, signal_handler, NULL);
    obt_signal_add_callback(SIGTERM, signal_handler, NULL);
    obt_signal_add_callback(SIGINT,  signal_handler, NULL);
    obt_signal_add_callback(SIGHUP,  signal_handler, NULL);
    obt_signal_add_callback(SIGPIPE, signal_handler, NULL);
    obt_signal_add_callback(SIGCHLD, signal_handler, NULL);
    obt_signal_add_callback(SIGTTIN, signal_handler, NULL);
    obt_signal_add_callback(SIGTTOU, signal_handler, NULL);

    ob_screen = DefaultScreen(obt_display);

    ob_rr_inst = RrInstanceNew(obt_display, ob_screen);
    if (ob_rr_inst == NULL)
        ob_exit_with_error(_("Failed to initialize the obrender library."));
    /* Saving 3 resizes of an RrImage makes a lot of sense for icons, as there
       are generally 3 icon sizes needed: the titlebar icon, the menu icon,
       and the alt-tab icon
    */
    ob_rr_icons = RrImageCacheNew(3);

    XSynchronize(obt_display, xsync);

    /* check for locale support */
    if (!XSupportsLocale())
        g_message(_("X server does not support locale."));
    if (!XSetLocaleModifiers(""))
        g_message(_("Cannot set locale modifiers for the X server."));

    /* set the DISPLAY environment variable for any lauched children, to the
       display we're using, so they open in the right place. */
    g_setenv("DISPLAY", DisplayString(obt_display), TRUE);

    /* create available cursors */
    cursors[OB_CURSOR_NONE] = None;
    cursors[OB_CURSOR_POINTER] = load_cursor("left_ptr", XC_left_ptr);
    cursors[OB_CURSOR_BUSYPOINTER] = load_cursor("left_ptr_watch",XC_left_ptr);
    cursors[OB_CURSOR_BUSY] = load_cursor("watch", XC_watch);
    cursors[OB_CURSOR_MOVE] = load_cursor("fleur", XC_fleur);
    cursors[OB_CURSOR_NORTH] = load_cursor("top_side", XC_top_side);
    cursors[OB_CURSOR_NORTHEAST] = load_cursor("top_right_corner",
                                               XC_top_right_corner);
    cursors[OB_CURSOR_EAST] = load_cursor("right_side", XC_right_side);
    cursors[OB_CURSOR_SOUTHEAST] = load_cursor("bottom_right_corner",
                                               XC_bottom_right_corner);
    cursors[OB_CURSOR_SOUTH] = load_cursor("bottom_side", XC_bottom_side);
    cursors[OB_CURSOR_SOUTHWEST] = load_cursor("bottom_left_corner",
                                               XC_bottom_left_corner);
    cursors[OB_CURSOR_WEST] = load_cursor("left_side", XC_left_side);
    cursors[OB_CURSOR_NORTHWEST] = load_cursor("top_left_corner",
                                               XC_top_left_corner);

    if (screen_annex()) { /* it will be ours! */

        /* get a timestamp from after taking over as the WM.  if we use the
           old timestamp to set focus it can fail when replacing another WM. */
        event_reset_time();

        do {
            gchar *xml_error_string = NULL;
            ObPrompt *xmlprompt = NULL;

            if (reconfigure) obt_keyboard_reload();

            {
                ObtXmlInst *i;

                /* startup the parsing so everything can register sections
                   of the rc */
                i = obt_xml_instance_new();

                /* register all the available actions */
                actions_startup(reconfigure);
                /* start up config which sets up with the parser */
                config_startup(i);

                /* parse/load user options */
                if ((config_file &&
                     obt_xml_load_file(i, config_file, "openbox_config")) ||
                    obt_xml_load_config_file(i, "openbox", "rc.xml",
                                             "openbox_config"))
                {
                    obt_xml_tree_from_root(i);
                    obt_xml_close(i);
                }
                else {
                    g_message(_("Unable to find a valid config file, using some simple defaults"));
                    config_file = NULL;
                }

                if (config_file) {
                    gchar *p = g_filename_to_utf8(config_file, -1,
                                                  NULL, NULL, NULL);
                    if (p)
                        OBT_PROP_SETS(obt_root(ob_screen), OB_CONFIG_FILE, p);
                    g_free(p);
                }
                else
                    OBT_PROP_ERASE(obt_root(ob_screen), OB_CONFIG_FILE);

                if (obt_xml_last_error(i)) {
                    xml_error_string = g_strdup_printf(
                        _("One or more XML syntax errors were found while parsing the Openbox configuration files.  See stdout for more information.  The last error seen was in file \"%s\" line %d, with message: %s"),
                        obt_xml_last_error_file(i),
                        obt_xml_last_error_line(i),
                        obt_xml_last_error_message(i));
                }

                /* we're done with parsing now, kill it */
                obt_xml_instance_unref(i);
            }

            /* load the theme specified in the rc file */
            {
                RrTheme *theme;
                if ((theme = RrThemeNew(ob_rr_inst, config_theme, TRUE,
                                        config_font_activewindow,
                                        config_font_inactivewindow,
                                        config_font_menutitle,
                                        config_font_menuitem,
                                        config_font_activeosd,
                                        config_font_inactiveosd)))
                {
                    RrThemeFree(ob_rr_theme);
                    ob_rr_theme = theme;
                }
                if (ob_rr_theme == NULL)
                    ob_exit_with_error(_("Unable to load a theme."));

                OBT_PROP_SETS(obt_root(ob_screen), OB_THEME,
                              ob_rr_theme->name);
            }

            if (reconfigure) {
                GList *it;

                /* update all existing windows for the new theme */
                for (it = client_list; it; it = g_list_next(it)) {
                    ObClient *c = it->data;
                    frame_adjust_theme(c->frame);
                }
            }
            event_startup(reconfigure);
            /* focus_backup is used for stacking, so this needs to come before
               anything that calls stacking_add */
            sn_startup(reconfigure);
            window_startup(reconfigure);
            focus_startup(reconfigure);
            focus_cycle_startup(reconfigure);
            focus_cycle_indicator_startup(reconfigure);
            focus_cycle_popup_startup(reconfigure);
            screen_startup(reconfigure);
            grab_startup(reconfigure);
            group_startup(reconfigure);
            ping_startup(reconfigure);
            client_startup(reconfigure);
            dock_startup(reconfigure);
            moveresize_startup(reconfigure);
            keyboard_startup(reconfigure);
            mouse_startup(reconfigure);
            menu_frame_startup(reconfigure);
            menu_startup(reconfigure);
            prompt_startup(reconfigure);

            if (!reconfigure) {
                /* do this after everything is started so no events will get
                   missed */
                xqueue_listen();

                guint32 xid;
                ObWindow *w;

                /* get all the existing windows */
                window_manage_all();

                /* focus what was focused if a wm was already running */
                if (OBT_PROP_GET32(obt_root(ob_screen),
                                   NET_ACTIVE_WINDOW, WINDOW, &xid) &&
                    (w = window_find(xid)) && WINDOW_IS_CLIENT(w))
                {
                    client_focus(WINDOW_AS_CLIENT(w));
                }
            } else {
                GList *it;

                /* redecorate all existing windows */
                for (it = client_list; it; it = g_list_next(it)) {
                    ObClient *c = it->data;

                    /* the new config can change the window's decorations */
                    client_setup_decor_and_functions(c, FALSE);
                    /* redraw the frames */
                    frame_adjust_area(c->frame, TRUE, TRUE, FALSE);
                    /* the decor sizes may have changed, so the windows may
                       end up in new positions */
                    client_reconfigure(c, FALSE);
                }
            }

            ob_set_state(OB_STATE_RUNNING);

            if (!reconfigure && startup_cmd) run_startup_cmd();

            reconfigure = FALSE;

            /* look for parsing errors */
            if (xml_error_string) {
                xmlprompt = prompt_show_message(xml_error_string,
                                                _("Openbox Syntax Error"),
                                                _("Close"));
                g_free(xml_error_string);
                xml_error_string = NULL;
            }

            g_main_loop_run(ob_main_loop);
            ob_set_state(reconfigure ?
                         OB_STATE_RECONFIGURING : OB_STATE_EXITING);

            if (xmlprompt) {
                prompt_unref(xmlprompt);
                xmlprompt = NULL;
            }

            if (!reconfigure)
                window_unmanage_all();

            prompt_shutdown(reconfigure);
            menu_shutdown(reconfigure);
            menu_frame_shutdown(reconfigure);
            mouse_shutdown(reconfigure);
            keyboard_shutdown(reconfigure);
            moveresize_shutdown(reconfigure);
            dock_shutdown(reconfigure);
            client_shutdown(reconfigure);
            ping_shutdown(reconfigure);
            group_shutdown(reconfigure);
            grab_shutdown(reconfigure);
            screen_shutdown(reconfigure);
            focus_cycle_popup_shutdown(reconfigure);
            focus_cycle_indicator_shutdown(reconfigure);
            focus_cycle_shutdown(reconfigure);
            focus_shutdown(reconfigure);
            window_shutdown(reconfigure);
            sn_shutdown(reconfigure);
            event_shutdown(reconfigure);
            config_shutdown();
            actions_shutdown(reconfigure);
        } while (reconfigure);
    }

    XSync(obt_display, FALSE);

    RrThemeFree(ob_rr_theme);
    RrImageCacheUnref(ob_rr_icons);
    RrInstanceFree(ob_rr_inst);

    session_shutdown(being_replaced);

    obt_display_close();

    if (restart) {
        ob_debug_shutdown();
        obt_signal_stop();
        if (restart_path != NULL) {
            gint argcp;
            gchar **argvp;
            GError *err = NULL;

            /* run other window manager */
            if (g_shell_parse_argv(restart_path, &argcp, &argvp, &err)) {
                execvp(argvp[0], argvp);
                g_strfreev(argvp);
            } else {
                g_message(
                    _("Restart failed to execute new executable \"%s\": %s"),
                    restart_path, err->message);
                g_error_free(err);
            }
        }

        /* we remove the session arguments from argv, so put them back,
           also don't restore the session on restart */
        if (ob_sm_save_file != NULL || ob_sm_id != NULL) {
            gchar **nargv;
            gint i, l;

            l = argc + 1 +
                (ob_sm_save_file != NULL ? 2 : 0) +
                (ob_sm_id != NULL ? 2 : 0);
            nargv = g_new0(gchar*, l+1);
            for (i = 0; i < argc; ++i)
                nargv[i] = argv[i];

            if (ob_sm_save_file != NULL) {
                nargv[i++] = g_strdup("--sm-save-file");
                nargv[i++] = ob_sm_save_file;
            }
            if (ob_sm_id != NULL) {
                nargv[i++] = g_strdup("--sm-client-id");
                nargv[i++] = ob_sm_id;
            }
            nargv[i++] = g_strdup("--sm-no-load");
            g_assert(i == l);
            argv = nargv;
        }

        /* re-run me */
        execvp(argv[0], argv); /* try how we were run */
        execlp(argv[0], program_name, (gchar*)NULL); /* last resort */
    }

    /* free stuff passed in from the command line or environment */
    g_free(ob_sm_save_file);
    g_free(ob_sm_id);
    g_free(program_name);

    if (!restart) {
        ob_debug_shutdown();
        obt_signal_stop();
    }

    return exitcode;
}

static void signal_handler(gint signal, gpointer data)
{
    switch (signal) {
    case SIGUSR1:
        ob_debug("Caught signal %d. Restarting.", signal);
        ob_restart();
        break;
    case SIGUSR2:
        ob_debug("Caught signal %d. Reconfiguring.", signal);
        ob_reconfigure();
        break;
    case SIGCHLD:
        /* reap children */
        while (waitpid(-1, NULL, WNOHANG) > 0);
        break;
    case SIGTTIN:
    case SIGTTOU:
        ob_debug("Caught signal %d. Ignoring.", signal);
        break;
    default:
        ob_debug("Caught signal %d. Exiting.", signal);
        /* TERM and INT return a 0 code */
        ob_exit(!(signal == SIGTERM || signal == SIGINT));
    }
}

static void print_version(void)
{
    g_print("Openbox %s\n", PACKAGE_VERSION);
    g_print(_("Copyright (c)"));
    g_print(" 2004   Mikael Magnusson\n");
    g_print(_("Copyright (c)"));
    g_print(" 2002   Dana Jansens\n\n");
    g_print("This program comes with ABSOLUTELY NO WARRANTY.\n");
    g_print("This is free software, and you are welcome to redistribute it\n");
    g_print("under certain conditions. See the file COPYING for details.\n\n");
}

static void print_help(void)
{
    g_print(_("Syntax: openbox [options]\n"));
    g_print(_("\nOptions:\n"));
    g_print(_("  --help              Display this help and exit\n"));
    g_print(_("  --version           Display the version and exit\n"));
    g_print(_("  --replace           Replace the currently running window manager\n"));
    /* TRANSLATORS: if you translate "FILE" here, make sure to keep the "Specify..."
       aligned still, if you have to, make a new line with \n and 22 spaces. It's
       fine to leave it as FILE though. */
    g_print(_("  --config-file FILE  Specify the path to the config file to use\n"));
    g_print(_("  --sm-disable        Disable connection to the session manager\n"));
    g_print(_("\nPassing messages to a running Openbox instance:\n"));
    g_print(_("  --reconfigure       Reload Openbox's configuration\n"));
    g_print(_("  --restart           Restart Openbox\n"));
    g_print(_("  --exit              Exit Openbox\n"));
    g_print(_("\nDebugging options:\n"));
    g_print(_("  --sync              Run in synchronous mode\n"));
    g_print(_("  --startup CMD       Run CMD after starting\n"));
    g_print(_("  --debug             Display debugging output\n"));
    g_print(_("  --debug-focus       Display debugging output for focus handling\n"));
    g_print(_("  --debug-session     Display debugging output for session management\n"));
    g_print(_("  --debug-xinerama    Split the display into fake xinerama screens\n"));
    g_print(_("\nPlease report bugs at %s\n"), PACKAGE_BUGREPORT);
}

static void remove_args(gint *argc, gchar **argv, gint index, gint num)
{
    gint i;

    for (i = index; i < *argc - num; ++i)
        argv[i] = argv[i+num];
    for (; i < *argc; ++i)
        argv[i] = NULL;
    *argc -= num;
}

static void run_startup_cmd(void)
{
    gchar **argv = NULL;
    GError *e = NULL;
    gboolean ok;

    if (!g_shell_parse_argv(startup_cmd, NULL, &argv, &e)) {
        g_message("Error parsing startup command: %s",
                  e->message);
        g_error_free(e);
        e = NULL;
    }
    ok = g_spawn_async(NULL, argv, NULL,
                       G_SPAWN_SEARCH_PATH |
                       G_SPAWN_DO_NOT_REAP_CHILD,
                       NULL, NULL, NULL, &e);
    if (!ok) {
        g_message("Error launching startup command: %s",
                  e->message);
        g_error_free(e);
        e = NULL;
    }
}

static void parse_env(void)
{
    const gchar *id;

    /* unset this so we don't pass it on unknowingly */
    g_unsetenv("DESKTOP_STARTUP_ID");

    /* this is how gnome-session passes in a session client id */
    id = g_getenv("DESKTOP_AUTOSTART_ID");
    if (id) {
        g_unsetenv("DESKTOP_AUTOSTART_ID");
        if (ob_sm_id) g_free(ob_sm_id);
        ob_sm_id = g_strdup(id);
        ob_debug_type(OB_DEBUG_SM,
                      "DESKTOP_AUTOSTART_ID %s supercedes --sm-client-id\n",
                      ob_sm_id);
    }
}

static void parse_args(gint *argc, gchar **argv)
{
    gint i;

    for (i = 1; i < *argc; ++i) {
        if (!strcmp(argv[i], "--version")) {
            print_version();
            exit(0);
        }
        else if (!strcmp(argv[i], "--help")) {
            print_help();
            exit(0);
        }
        else if (!strcmp(argv[i], "--g-fatal-warnings")) {
            g_log_set_always_fatal(G_LOG_LEVEL_CRITICAL);
        }
        else if (!strcmp(argv[i], "--replace")) {
            ob_replace_wm = TRUE;
            remove_args(argc, argv, i, 1);
            --i; /* this arg was removed so go back */
        }
        else if (!strcmp(argv[i], "--sync")) {
            xsync = TRUE;
        }
        else if (!strcmp(argv[i], "--startup")) {
            if (i == *argc - 1) /* no args left */
                g_printerr(_("%s requires an argument\n"), "--startup");
            else {
                /* this will be in the current locale encoding, which is
                   what we want */
                startup_cmd = argv[i+1];
                remove_args(argc, argv, i, 2);
                --i; /* this arg was removed so go back */
                ob_debug("--startup %s", startup_cmd);
            }
        }
        else if (!strcmp(argv[i], "--debug")) {
            ob_debug_enable(OB_DEBUG_NORMAL, TRUE);
            ob_debug_enable(OB_DEBUG_APP_BUGS, TRUE);
        }
        else if (!strcmp(argv[i], "--debug-focus")) {
            ob_debug_enable(OB_DEBUG_FOCUS, TRUE);
        }
        else if (!strcmp(argv[i], "--debug-session")) {
            ob_debug_enable(OB_DEBUG_SM, TRUE);
        }
        else if (!strcmp(argv[i], "--debug-xinerama")) {
            ob_debug_xinerama = TRUE;
        }
        else if (!strcmp(argv[i], "--reconfigure")) {
            remote_control = 1;
        }
        else if (!strcmp(argv[i], "--restart")) {
            remote_control = 2;
        }
        else if (!strcmp(argv[i], "--exit")) {
            remote_control = 3;
        }
        else if (!strcmp(argv[i], "--config-file")) {
            if (i == *argc - 1) /* no args left */
                g_printerr(_("%s requires an argument\n"), "--config-file");
            else {
                /* this will be in the current locale encoding, which is
                   what we want */
                config_file = argv[i+1];
                ++i; /* skip the argument */
                ob_debug("--config-file %s", config_file);
            }
        }
        else if (!strcmp(argv[i], "--sm-save-file")) {
            if (i == *argc - 1) /* no args left */
                /* not translated cuz it's sekret */
                g_printerr("--sm-save-file requires an argument\n");
            else {
                ob_sm_save_file = g_strdup(argv[i+1]);
                remove_args(argc, argv, i, 2);
                --i; /* this arg was removed so go back */
                ob_debug_type(OB_DEBUG_SM, "--sm-save-file %s",
                              ob_sm_save_file);
            }
        }
        else if (!strcmp(argv[i], "--sm-client-id")) {
            if (i == *argc - 1) /* no args left */
                /* not translated cuz it's sekret */
                g_printerr("--sm-client-id requires an argument\n");
            else {
                ob_sm_id = g_strdup(argv[i+1]);
                remove_args(argc, argv, i, 2);
                --i; /* this arg was removed so go back */
                ob_debug_type(OB_DEBUG_SM, "--sm-client-id %s", ob_sm_id);
            }
        }
        else if (!strcmp(argv[i], "--sm-disable")) {
            ob_sm_use = FALSE;
        }
        else if (!strcmp(argv[i], "--sm-no-load")) {
            ob_sm_restore = FALSE;
            remove_args(argc, argv, i, 1);
            --i; /* this arg was removed so go back */
        }
        else {
            /* this is a memleak.. oh well.. heh */
            gchar *err = g_strdup_printf
                (_("Invalid command line argument \"%s\"\n"), argv[i]);
            ob_exit_with_error(err);
        }
    }
}

static Cursor load_cursor(const gchar *name, guint fontval)
{
    Cursor c = None;

#if USE_XCURSOR
    c = XcursorLibraryLoadCursor(obt_display, name);
#endif
    if (c == None)
        c = XCreateFontCursor(obt_display, fontval);
    return c;
}

void ob_exit_with_error(const gchar *msg)
{
    g_message("%s", msg);
    session_shutdown(TRUE);
    exit(EXIT_FAILURE);
}

void ob_restart_other(const gchar *path)
{
    restart_path = g_strdup(path);
    ob_restart();
}

void ob_restart(void)
{
    restart = TRUE;
    ob_exit(0);
}

void ob_reconfigure(void)
{
    reconfigure = TRUE;
    ob_exit(0);
}

void ob_exit(gint code)
{
    exitcode = code;
    g_main_loop_quit(ob_main_loop);
}

void ob_exit_replace(void)
{
    exitcode = 0;
    being_replaced = TRUE;
    g_main_loop_quit(ob_main_loop);
}

Cursor ob_cursor(ObCursor cursor)
{
    g_assert(cursor < OB_NUM_CURSORS);
    return cursors[cursor];
}

ObState ob_state(void)
{
    return state;
}

void ob_set_state(ObState s)
{
    state = s;
}
