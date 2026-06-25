/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2006      Brian Tarricone, <bjt23@cornell.edu>
 *  Copyright (c) 2010-2011 Jannis Pohlmann, <jannis@xfce.org>
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
 */
/* Random portions taken from or inspired by the original xfdesktop for xfce4:
 *     Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *     Copyright (C) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
 *  X event forwarding code:
 *     Copyright (c) 2004 Nils Rennebarth
 * Additional portions taken from https://bugzilla.xfce.org/attachment.cgi?id=3751
 * which is in xfce4-panel git commit id 2a8de2b1b019eaef543e34764c999a409fe2bef9
 * and adapted for xfdesktop.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif


#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <xfconf/xfconf.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>

#include "xfdesktop-common.h"
#include "xfce-backdrop.h"
#include "xfce-desktop.h"
#include "menu.h"
#include "windowlist.h"

#ifdef HAVE_LIBNOTIFY
#include "xfdesktop-notify.h"
#endif

#include "xfdesktop-application.h"

static void xfdesktop_application_finalize(GObject *object);

static void session_logout(void);
static void session_die(gpointer user_data);

static void event_forward_to_rootwin(GdkScreen *gscreen, GdkEvent *event);

static gboolean scroll_cb(GtkWidget *w, GdkEventScroll *evt, gpointer user_data);

static gboolean reload_idle_cb(gpointer data);
static void cb_xfdesktop_application_reload(GAction  *action,
                                            GVariant *parameter,
                                            gpointer  data);

static void cb_xfdesktop_application_next(GAction  *action,
                                          GVariant *parameter,
                                          gpointer  data);

static void xfdesktop_handle_quit_signals(gint sig, gpointer user_data);
static void cb_xfdesktop_application_quit(GAction  *action,
                                          GVariant *parameter,
                                          gpointer  data);

static void cb_xfdesktop_application_menu(GAction  *action,
                                          GVariant *parameter,
                                          gpointer  data);

static void cb_xfdesktop_application_arrange(GAction  *action,
                                             GVariant *parameter,
                                             gpointer  data);

static void cb_xfdesktop_application_debug(GAction  *action,
                                           GVariant *parameter,
                                           gpointer  data);

static gboolean cb_wait_for_window_manager(gpointer data);
static void     cb_wait_for_window_manager_destroyed(gpointer data);

static void xfdesktop_application_startup(GApplication *g_application);
static void xfdesktop_application_start(XfdesktopApplication *app);
static void xfdesktop_application_shutdown(GApplication *g_application);

static gboolean xfdesktop_application_local_command_line(GApplication *g_application,
                                                         gchar ***arguments,
                                                         int *exit_status);
static gint xfdesktop_application_command_line(GApplication *g_application,
                                               GApplicationCommandLine *command_line);

typedef struct
{
    XfdesktopApplication *app;

    Display *dpy;
    Atom *atoms;
    guint atom_count;
    guint have_wm : 1;
    guint counter;
    guint wait_for_wm_timeout_id;
} WaitForWM;

struct _XfdesktopApplication
{
    GApplication parent;

    GtkWidget *desktop;
    XfconfChannel *channel;
    guint wait_for_wm_timeout_id;
    XfceSMClient *sm_client;
    GCancellable *cancel;

    gboolean opt_disable_wm_check;
};

struct _XfdesktopApplicationClass
{
    GApplicationClass parent;
};

const gchar *fallback_CSS =
"XfdesktopIconView.view {"
"	background: transparent;"
"	color: @theme_selected_fg_color;"
"	border-radius: 3px;"
"}"
"XfdesktopIconView.view:active {"
"	background: alpha(@theme_selected_bg_color, 0.5);"
"	text-shadow: 0 1px 1px black;"
"}"
"XfdesktopIconView.view .label {"
"	text-shadow: 1px 1px 2px black;"
"}"
"XfdesktopIconView.view .label:active {"
"	color: @theme_selected_fg_color;"
"}"
"XfdesktopIconView .rubberband {"
"	background: alpha(@theme_selected_bg_color, 0.2);"
"	border: 1px solid @theme_selected_bg_color;"
"	border-radius: 0;"
"}"
"XfdesktopIconView:active {"
"	color: @theme_selected_bg_color;"
"}";

G_DEFINE_TYPE(XfdesktopApplication, xfdesktop_application, G_TYPE_APPLICATION)


static void
xfdesktop_application_class_init(XfdesktopApplicationClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GApplicationClass *gapplication_class = G_APPLICATION_CLASS(klass);

    gobject_class->finalize = xfdesktop_application_finalize;

    gapplication_class->startup = xfdesktop_application_startup;
    gapplication_class->shutdown = xfdesktop_application_shutdown;
    gapplication_class->local_command_line = xfdesktop_application_local_command_line;
    gapplication_class->command_line = xfdesktop_application_command_line;
}

static void
xfdesktop_application_add_action(XfdesktopApplication *app, GAction *action)
{
    g_action_map_add_action(G_ACTION_MAP(app), action);
}

static void
xfdesktop_application_init(XfdesktopApplication *app)
{
    GSimpleAction *action;

    app->cancel = g_cancellable_new();

    /* reload action */
    action = g_simple_action_new("reload", NULL);
    g_signal_connect(action, "activate", G_CALLBACK(cb_xfdesktop_application_reload), app);
    xfdesktop_application_add_action(app, G_ACTION(action));
    g_object_unref(action);

    /* next action */
    action = g_simple_action_new("next", NULL);
    g_signal_connect(action, "activate", G_CALLBACK(cb_xfdesktop_application_next), app);
    xfdesktop_application_add_action(app, G_ACTION(action));
    g_object_unref(action);

    /* quit action */
    action = g_simple_action_new("quit", NULL);
    g_signal_connect(action, "activate", G_CALLBACK(cb_xfdesktop_application_quit), app);
    xfdesktop_application_add_action(app, G_ACTION(action));
    g_object_unref(action);

    /* menu action, parameter pops up primary (TRUE) or windowlist menu */
    action = g_simple_action_new("menu", G_VARIANT_TYPE_BOOLEAN);
    g_signal_connect(action, "activate", G_CALLBACK(cb_xfdesktop_application_menu), app);
    xfdesktop_application_add_action(app, G_ACTION(action));
    g_object_unref(action);

    /* arrange action */
    action = g_simple_action_new("arrange", NULL);
    g_signal_connect(action, "activate", G_CALLBACK(cb_xfdesktop_application_arrange), app);
    xfdesktop_application_add_action(app, G_ACTION(action));
    g_object_unref(action);

    /* debug action, parameter toggles debug state */
    action = g_simple_action_new("debug", G_VARIANT_TYPE_BOOLEAN);
    g_signal_connect(action, "activate", G_CALLBACK(cb_xfdesktop_application_debug), app);
    xfdesktop_application_add_action(app, G_ACTION(action));
    g_object_unref(action);
}

static void
xfdesktop_application_finalize(GObject *object)
{
    G_OBJECT_CLASS(xfdesktop_application_parent_class)->finalize(object);
}

/**
 * xfdesktop_application_get:
 *
 * Singleton. Additional calls increase the reference count.
 *
 * Return value: #XfdesktopApplication, free with g_object_unref.
 **/
XfdesktopApplication *
xfdesktop_application_get(void)
{
    static XfdesktopApplication *app = NULL;

    if(app == NULL) {
       app = g_object_new(XFDESKTOP_TYPE_APPLICATION,
                          "application-id", "org.xfce.xfdesktop",
                          "flags", G_APPLICATION_HANDLES_COMMAND_LINE,
                          NULL);
    } else {
        g_object_ref(app);
    }

    return app;
}

static void
session_logout(void)
{
    XfceSMClient *sm_client;

    sm_client = xfce_sm_client_get();
    xfce_sm_client_request_shutdown(sm_client, XFCE_SM_CLIENT_SHUTDOWN_HINT_ASK);
}

static void
session_die(gpointer user_data)
{
    gint main_level;
    XfdesktopApplication *app;

    TRACE("entering");

    /* Ensure we always have a valid reference so we can quit xfdesktop */
    app = xfdesktop_application_get();

    /* Cancel the wait for wm check if it's still running */
    g_cancellable_cancel(app->cancel);

    for(main_level = gtk_main_level(); main_level > 0; --main_level)
        gtk_main_quit();

    g_application_quit(G_APPLICATION(app));
}

static void
event_forward_to_rootwin(GdkScreen *gscreen, GdkEvent *event)
{
    XButtonEvent xev, xev2;
    Display *dpy = GDK_DISPLAY_XDISPLAY(gdk_screen_get_display(gscreen));

    if(event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE) {
        if(event->type == GDK_BUTTON_PRESS) {
            xev.type = ButtonPress;
            /*
             * rox has an option to disable the next
             * instruction. it is called "blackbox_hack". Does
             * anyone know why exactly it is needed?
             */
            XUngrabPointer(dpy, event->button.time);
        } else
            xev.type = ButtonRelease;

        xev.button = event->button.button;
        xev.x = event->button.x;    /* Needed for icewm */
        xev.y = event->button.y;
        xev.x_root = event->button.x_root;
        xev.y_root = event->button.y_root;
        xev.state = event->button.state;

        xev2.type = 0;
    } else if(event->type == GDK_SCROLL) {
        xev.type = ButtonPress;
        xev.button = event->scroll.direction + 4;
        xev.x = event->scroll.x;    /* Needed for icewm */
        xev.y = event->scroll.y;
        xev.x_root = event->scroll.x_root;
        xev.y_root = event->scroll.y_root;
        xev.state = event->scroll.state;

        xev2.type = ButtonRelease;
        xev2.button = xev.button;
    } else
        return;
    xev.window = GDK_WINDOW_XID(gdk_screen_get_root_window(gscreen));
    xev.root =  xev.window;
    xev.subwindow = None;
    xev.time = event->button.time;
    xev.same_screen = True;

    XSendEvent(dpy, xev.window, False, ButtonPressMask | ButtonReleaseMask,
            (XEvent *)&xev);
    if(xev2.type == 0)
        return;

    /* send button release for scroll event */
    xev2.window = xev.window;
    xev2.root = xev.root;
    xev2.subwindow = xev.subwindow;
    xev2.time = xev.time;
    xev2.x = xev.x;
    xev2.y = xev.y;
    xev2.x_root = xev.x_root;
    xev2.y_root = xev.y_root;
    xev2.state = xev.state;
    xev2.same_screen = xev.same_screen;

    XSendEvent(dpy, xev2.window, False, ButtonPressMask | ButtonReleaseMask,
            (XEvent *)&xev2);
}

static gboolean
scroll_cb(GtkWidget *w, GdkEventScroll *evt, gpointer user_data)
{
    event_forward_to_rootwin(gtk_widget_get_screen(w), (GdkEvent*)evt);
    return TRUE;
}

static gboolean
reload_idle_cb(gpointer data)
{
    XfdesktopApplication *app = XFDESKTOP_APPLICATION(data);

    TRACE("entering");

    /* If xfdesktop never started there's nothing to reload, xfdesktop will
     * now startup */
    if(!app->desktop)
        return FALSE;

    /* reload all the desktop */
    if(app->desktop)
        xfce_desktop_refresh(XFCE_DESKTOP(app->desktop), FALSE, TRUE);

    g_application_release(G_APPLICATION(app));

    return FALSE;
}

static void
cb_xfdesktop_application_reload(GAction  *action,
                                GVariant *parameter,
                                gpointer  data)
{
    GApplication *g_application;

    if(!data || !G_IS_APPLICATION(data))
        return;

    g_application = G_APPLICATION(data);

    /* hold the app so it doesn't quit while a queue up a refresh */
    g_application_hold(g_application);
    g_idle_add(reload_idle_cb, g_application);
}

static void
cb_xfdesktop_application_next(GAction  *action,
                              GVariant *parameter,
                              gpointer  data)
{
    XfdesktopApplication *app;

    TRACE("entering");

    g_return_if_fail(XFDESKTOP_IS_APPLICATION(data));

    app = XFDESKTOP_APPLICATION(data);

    /* If xfdesktop never started there's nothing to do here */
    if(!app->desktop)
        return;

    /* reload the desktop forcing the wallpaper to advance */
    if(app->desktop)
        xfce_desktop_refresh(XFCE_DESKTOP(app->desktop), TRUE, TRUE);
}

static void
xfdesktop_handle_quit_signals(gint sig,
                              gpointer user_data)
{
    TRACE("entering");

    g_return_if_fail(XFDESKTOP_IS_APPLICATION(user_data));

    session_die(user_data);
}

static void
cb_xfdesktop_application_quit(GAction  *action,
                              GVariant *parameter,
                              gpointer  data)
{
    XfdesktopApplication *app;

    TRACE("entering");

    g_return_if_fail(XFDESKTOP_IS_APPLICATION(data));

    app = XFDESKTOP_APPLICATION(data);

    /* If the user told xfdesktop to quit, set the restart style to something
     * where it won't restart itself */
    if(app->sm_client && XFCE_IS_SM_CLIENT(app->sm_client)) {
        xfce_sm_client_set_restart_style(app->sm_client,
                                         XFCE_SM_CLIENT_RESTART_NORMAL);
    }

    session_die(app);
}


/* parameter is a boolean that determines whether to popup the primay or
 * windowlist menu */
static void
cb_xfdesktop_application_menu(GAction  *action,
                              GVariant *parameter,
                              gpointer  data)
{
    XfdesktopApplication *app;
    gboolean popup_root_menu;

    TRACE("entering");

    /* sanity checks */
    if(!data || !XFDESKTOP_IS_APPLICATION(data))
        return;

    if(!g_variant_is_of_type(parameter, G_VARIANT_TYPE_BOOLEAN))
        return;

    popup_root_menu = g_variant_get_boolean(parameter);
    app = XFDESKTOP_APPLICATION(data);

    if(popup_root_menu) {
        xfce_desktop_popup_root_menu(XFCE_DESKTOP(app->desktop),
                                     0, GDK_CURRENT_TIME);
    } else {
        xfce_desktop_popup_secondary_root_menu(XFCE_DESKTOP(app->desktop),
                                               0, GDK_CURRENT_TIME);
    }
}

static void
cb_xfdesktop_application_arrange(GAction  *action,
                                 GVariant *parameter,
                                 gpointer  data)
{
    XfdesktopApplication *app;

    TRACE("entering");

    /* sanity check */
    if(!data || !XFDESKTOP_IS_APPLICATION(data))
        return;

    app = XFDESKTOP_APPLICATION(data);

    xfce_desktop_arrange_icons(XFCE_DESKTOP(app->desktop));
}

/* parameter is a boolean that determines whether to enable or disable the
 * debug messages */
static void
cb_xfdesktop_application_debug(GAction  *action,
                               GVariant *parameter,
                               gpointer  data)
{
    TRACE("entering");

    /* sanity checks */
    if(!data || !XFDESKTOP_IS_APPLICATION(data))
        return;

    if(!g_variant_is_of_type(parameter, G_VARIANT_TYPE_BOOLEAN))
        return;

    /* Toggle the debug state */
    xfdesktop_debug_set(g_variant_get_boolean(parameter));
}

/* Cleans up the associated wait for wm resources */
static void
wait_for_window_manager_cleanup(WaitForWM *wfwm)
{
    g_free(wfwm->atoms);
    XCloseDisplay(wfwm->dpy);
    g_slice_free(WaitForWM, wfwm);
}

static gboolean
cb_wait_for_window_manager(gpointer data)
{
    WaitForWM *wfwm = data;
    guint i;
    gboolean have_wm = TRUE;

    /* Check if it was canceled. This way xfdesktop doesn't start up if
     * we're quitting */
    if(g_cancellable_is_cancelled(wfwm->app->cancel)) {
        g_application_release(G_APPLICATION(wfwm->app));
        wait_for_window_manager_cleanup(wfwm);
        return FALSE;
    }

    for(i = 0; i < wfwm->atom_count; i++) {
        if(XGetSelectionOwner(wfwm->dpy, wfwm->atoms[i]) == None) {
            XF_DEBUG("window manager not ready on screen %d", i);
            have_wm = FALSE;
            break;
        }
    }

    wfwm->have_wm = have_wm;

    /* abort if a window manager is found or 5 seconds expired */
    return wfwm->counter++ < 20 * 5 && !wfwm->have_wm;
}

static void
cb_wait_for_window_manager_destroyed(gpointer data)
{
    WaitForWM *wfwm = data;

    g_return_if_fail(wfwm->app != NULL);

    /* Check if it was canceled. This way xfdesktop doesn't start up if
     * we're quitting */
    if(g_cancellable_is_cancelled(wfwm->app->cancel)) {
        g_application_release(G_APPLICATION(wfwm->app));
        wait_for_window_manager_cleanup(wfwm);
        return;
    }


    wfwm->app->wait_for_wm_timeout_id = 0;

    if(!wfwm->have_wm) {
        g_printerr("No window manager registered on screen 0. "
                   "To start the xfdesktop without this check, run with --disable-wm-check.\n");
    } else {
        XF_DEBUG("found window manager after %d tries", wfwm->counter);
    }

    /* start loading the desktop, hopefully a window manager is found, but it
     * also works without it */
    xfdesktop_application_start(wfwm->app);

    wait_for_window_manager_cleanup(wfwm);
}

static void
xfdesktop_application_startup(GApplication *g_application)
{
    XfdesktopApplication *app = XFDESKTOP_APPLICATION(g_application);
    WaitForWM *wfwm;
    guint i;
    gchar **atom_names;

    TRACE("entering");

    /* hold so it does not exit on us before the main loop gets going */
    g_application_hold(g_application);

    if(!app->opt_disable_wm_check) {
        /* setup data for wm checking */
        wfwm = g_slice_new0(WaitForWM);
        wfwm->dpy = XOpenDisplay(NULL);
        wfwm->have_wm = FALSE;
        wfwm->counter = 0;
        wfwm->app = app;

        /* preload wm atoms for all screens */
        wfwm->atom_count = XScreenCount(wfwm->dpy);
        wfwm->atoms = g_new(Atom, wfwm->atom_count);
        atom_names = g_new0(gchar *, wfwm->atom_count + 1);

        for(i = 0; i < wfwm->atom_count; i++)
            atom_names[i] = g_strdup_printf ("WM_S%d", i);

        if(!XInternAtoms(wfwm->dpy, atom_names, wfwm->atom_count, False, wfwm->atoms))
            wfwm->atom_count = 0;

        g_strfreev(atom_names);

        /* setup timeout to check for a window manager */
        app->wait_for_wm_timeout_id = g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE,
                                                         50,
                                                         cb_wait_for_window_manager,
                                                         wfwm,
                                                         cb_wait_for_window_manager_destroyed);
    } else {
        /* directly launch */
        xfdesktop_application_start(app);
    }

    /* let the parent class do it's startup as well */
    G_APPLICATION_CLASS(xfdesktop_application_parent_class)->startup(g_application);
}

static void
xfdesktop_application_theme_changed (GtkSettings *settings,
                                     XfdesktopApplication *app)
{
    GtkCssProvider *provider = NULL;
    static GtkCssProvider *custom_provider = NULL;
    gchar *theme;
    gchar *css;

    g_object_get(settings, "gtk-theme-name", &theme, NULL);

    provider = gtk_css_provider_get_named(theme, NULL);
    css = gtk_css_provider_to_string (provider);

    if (g_strrstr (css, "XfdesktopIconView") != NULL) {
        DBG("XfdesktopIconView section found in theme %s", theme);
        if (custom_provider != NULL) {
            gtk_style_context_remove_provider_for_screen (gdk_screen_get_default (),
                                                          GTK_STYLE_PROVIDER(custom_provider));
            g_clear_object (&custom_provider);
        }
    } else {
        DBG("XfdesktopIconView section not found in theme %s, setting our fallback", theme);
        if (custom_provider != NULL) {
            gtk_style_context_remove_provider_for_screen (gdk_screen_get_default (),
                                                          GTK_STYLE_PROVIDER(custom_provider));
            g_clear_object (&custom_provider);
        }
        custom_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(custom_provider,
                                        fallback_CSS,
                                        -1,
                                        NULL);
        gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                                   GTK_STYLE_PROVIDER(custom_provider),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    g_free(css);
}

static void
xfdesktop_application_start(XfdesktopApplication *app)
{
    GtkSettings *settings;
    GdkDisplay *gdpy;
    GdkScreen *gscreen;
    gint screen_num;
    GError *error = NULL;
    gchar buf[1024];

    TRACE("entering");

    g_return_if_fail(app != NULL);

    settings = gtk_settings_get_default();
    g_signal_connect (settings, "notify::gtk-theme-name", G_CALLBACK (xfdesktop_application_theme_changed), NULL);
    xfdesktop_application_theme_changed (settings, app);

    /* stop autostart timeout */
    if(app->wait_for_wm_timeout_id != 0)
        g_source_remove(app->wait_for_wm_timeout_id);

    gdpy = gdk_display_get_default();
    gscreen = gdk_display_get_default_screen(gdpy);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    screen_num = gdk_screen_get_number(gscreen);
G_GNUC_END_IGNORE_DEPRECATIONS

    /* setup the session management options */
    app->sm_client = xfce_sm_client_get();
    g_object_add_weak_pointer(G_OBJECT(app->sm_client), (gpointer *)&app->sm_client);
    xfce_sm_client_set_restart_style(app->sm_client, XFCE_SM_CLIENT_RESTART_IMMEDIATELY);
    xfce_sm_client_set_priority(app->sm_client, XFCE_SM_CLIENT_PRIORITY_DESKTOP);
    g_signal_connect(app->sm_client, "quit", G_CALLBACK(session_die), app);

    if(!xfce_sm_client_connect(app->sm_client, &error) && error) {
        g_printerr("Failed to connect to session manager: %s\n", error->message);
        g_clear_error(&error);
    }


    if(!xfconf_init(&error)) {
        g_warning("%s: unable to connect to settings daemon: %s.  Defaults will be used",
                  PACKAGE, error->message);
        g_clear_error(&error);
        error = NULL;
    } else {
        app->channel = xfconf_channel_get(XFDESKTOP_CHANNEL);
        g_object_add_weak_pointer(G_OBJECT(app->channel), (gpointer *)&app->channel);
    }

    g_snprintf(buf, sizeof(buf), "/backdrop/screen%d/", screen_num);
    app->desktop = xfce_desktop_new(gscreen, app->channel, buf);
    g_object_add_weak_pointer(G_OBJECT(app->desktop), (gpointer *)&app->desktop);

    /* hook into the scroll event so we can forward it to the window
     * manager */
    gtk_widget_add_events(app->desktop, GDK_BUTTON_PRESS_MASK
                          | GDK_BUTTON_RELEASE_MASK | GDK_SCROLL_MASK);
    g_signal_connect(G_OBJECT(app->desktop), "scroll-event",
                     G_CALLBACK(scroll_cb), app);

    /* display the desktop and try to put it at the bottom */
    gtk_widget_realize(app->desktop);
    gdk_window_lower(gtk_widget_get_window(app->desktop));

    xfce_desktop_set_session_logout_func(XFCE_DESKTOP(app->desktop),
                                         session_logout);


    menu_init(app->channel);
    windowlist_init(app->channel);

    /* hook up to the different quit signals */
    if(xfce_posix_signal_handler_init(&error)) {
        xfce_posix_signal_handler_set_handler(SIGHUP,
                                              xfdesktop_handle_quit_signals,
                                              app, NULL);
        xfce_posix_signal_handler_set_handler(SIGINT,
                                              xfdesktop_handle_quit_signals,
                                              app, NULL);
        xfce_posix_signal_handler_set_handler(SIGTERM,
                                              xfdesktop_handle_quit_signals,
                                              app, NULL);
    } else {
        g_warning("Unable to set up POSIX signal handlers: %s", error->message);
        g_clear_error(&error);
    }

    gtk_main();

    /* now that main has started we can release our hold */
    g_application_release(G_APPLICATION(app));
}

gint
xfdesktop_application_run(XfdesktopApplication *app, int argc, char **argv)
{
    return g_application_run(G_APPLICATION(app), argc, argv);
}

static void
xfdesktop_application_shutdown(GApplication *g_application)
{
    XfdesktopApplication *app = XFDESKTOP_APPLICATION(g_application);

    TRACE("entering");

    if(app->wait_for_wm_timeout_id != 0) {
        g_source_remove(app->wait_for_wm_timeout_id);
        app->wait_for_wm_timeout_id = 0;
    }

    menu_cleanup();
    windowlist_cleanup();

    if (app->desktop != NULL) {
        gtk_widget_destroy(app->desktop);
    }

    xfconf_shutdown();

    if (app->sm_client != NULL) {
        g_object_unref(app->sm_client);
    }

#ifdef HAVE_LIBNOTIFY
    xfdesktop_notify_uninit();
#endif

    G_APPLICATION_CLASS(xfdesktop_application_parent_class)->shutdown(g_application);
}

static gboolean
xfdesktop_application_local_command_line(GApplication *g_application,
                                         gchar ***arguments,
                                         int *exit_status)
{
    XfdesktopApplication *app = XFDESKTOP_APPLICATION(g_application);
    GOptionContext *octx;
    gint argc;
    GError *error = NULL;
    gboolean opt_version = FALSE;
    gboolean opt_reload = FALSE;
    gboolean opt_next = FALSE;
    gboolean opt_menu = FALSE;
    gboolean opt_windowlist = FALSE;
    gboolean opt_arrange = FALSE;
    gboolean opt_quit = FALSE;
    gboolean opt_enable_debug = FALSE;
    gboolean opt_disable_debug = FALSE;
    gboolean option_set = FALSE;
    const GOptionEntry main_entries[] = {
        { "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_version, N_("Display version information"), NULL },
        { "reload", 'R', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_reload, N_("Reload all settings"), NULL },
        { "next", 'N', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_next, N_("Advance to the next wallpaper on the current workspace"), NULL },
        { "menu", 'M', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_menu, N_("Pop up the menu (at the current mouse position)"), NULL },
        { "windowlist", 'W', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_windowlist, N_("Pop up the window list (at the current mouse position)"), NULL },
#ifdef ENABLE_FILE_ICONS
        { "arrange", 'A', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_arrange, N_("Automatically arrange all the icons on the desktop"), NULL },
#endif
        { "enable-debug", 'e', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_enable_debug, N_("Enable debug messages"), NULL },
        { "disable-debug", 'd', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_disable_debug, N_("Disable debug messages"), NULL },
        { "disable-wm-check", 'D', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &app->opt_disable_wm_check, N_("Do not wait for a window manager on startup"), NULL },
        { "quit", 'Q', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_quit, N_("Cause xfdesktop to quit"), NULL },
        { NULL, 0, 0, 0, NULL, NULL, NULL }
    };

    TRACE("entering");

    argc = g_strv_length(*arguments);

    octx = g_option_context_new("");
    g_option_context_set_ignore_unknown_options(octx, TRUE);
    g_option_context_add_main_entries(octx, main_entries, NULL);
    g_option_context_add_group(octx, gtk_get_option_group(TRUE));
    g_option_context_add_group(octx, xfce_sm_client_get_option_group(argc, *arguments));

    if(!g_option_context_parse(octx, &argc, arguments, &error)) {
        g_printerr(_("Failed to parse arguments: %s\n"), error->message);
        g_option_context_free(octx);
        g_clear_error(&error);
        *exit_status = 1;
        return TRUE;
    }

    g_option_context_free(octx);

    /* Print the version info and exit */
    if(opt_version) {
        g_print(_("This is %s version %s, running on Xfce %s.\n"), PACKAGE,
                VERSION, xfce_version_string());
        g_print(_("Built with GTK+ %d.%d.%d, linked with GTK+ %d.%d.%d."),
                GTK_MAJOR_VERSION,GTK_MINOR_VERSION, GTK_MICRO_VERSION,
                gtk_major_version, gtk_minor_version, gtk_micro_version);
        g_print("\n");
        g_print(_("Build options:\n"));
        g_print(_("    Desktop Menu:        %s\n"),
#ifdef ENABLE_DESKTOP_MENU
                _("enabled")
#else
                _("disabled")
#endif
                );
        g_print(_("    Desktop Icons:       %s\n"),
#ifdef ENABLE_DESKTOP_ICONS
                _("enabled")
#else
                _("disabled")
#endif
                );
        g_print(_("    Desktop File Icons:  %s\n"),
#ifdef ENABLE_FILE_ICONS
                _("enabled")
#else
                _("disabled")
#endif
                );

        *exit_status = 0;
        return TRUE;
    }

    /* This will call xfdesktop_application_startup if it needs to */
    g_application_register(g_application, NULL, NULL);

    /* handle our defined options */
    if(opt_quit) {
        g_action_group_activate_action(G_ACTION_GROUP(g_application), "quit", NULL);
        option_set = TRUE;
    } else if(opt_reload) {
        g_action_group_activate_action(G_ACTION_GROUP(g_application), "reload", NULL);
        option_set = TRUE;
    } else if(opt_next) {
        g_action_group_activate_action(G_ACTION_GROUP(g_application), "next", NULL);
        option_set = TRUE;
    } else if(opt_menu) {
        g_action_group_activate_action(G_ACTION_GROUP(g_application), "menu",
                                       g_variant_new_boolean(TRUE));
        option_set = TRUE;
    } else if(opt_windowlist) {
        g_action_group_activate_action(G_ACTION_GROUP(g_application), "menu",
                                       g_variant_new_boolean(FALSE));
        option_set = TRUE;
    } else if(opt_arrange) {
        g_action_group_activate_action(G_ACTION_GROUP(g_application), "arrange", NULL);
        option_set = TRUE;
    } else if(opt_enable_debug) {
        g_action_group_activate_action(G_ACTION_GROUP(g_application), "debug",
                                       g_variant_new_boolean(TRUE));
        option_set = TRUE;
    } else if(opt_disable_debug) {
        g_action_group_activate_action(G_ACTION_GROUP(g_application), "debug",
                                       g_variant_new_boolean(FALSE));
        option_set = TRUE;
    }

    /* We handled the command line option */
    if(option_set) {
        *exit_status = 0;
        return TRUE;
    }

    /* propagate it up */
    return G_APPLICATION_CLASS(xfdesktop_application_parent_class)->local_command_line(g_application, arguments, exit_status);
}

static gint
xfdesktop_application_command_line(GApplication *g_application,
                                   GApplicationCommandLine *command_line)
{
    /* If we don't process everything in the local command line then the options
     * won't show up during xfdesktop --help */

    return 0;
}
