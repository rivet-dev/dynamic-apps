/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   screen.c for the Openbox window manager
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
#include "dock.h"
#include "grab.h"
#include "startupnotify.h"
#include "moveresize.h"
#include "config.h"
#include "screen.h"
#include "client.h"
#include "session.h"
#include "frame.h"
#include "event.h"
#include "focus.h"
#include "focus_cycle.h"
#include "popup.h"
#include "version.h"
#include "obrender/render.h"
#include "gettext.h"
#include "obt/display.h"
#include "obt/xqueue.h"
#include "obt/prop.h"

#include <X11/Xlib.h>
#ifdef HAVE_UNISTD_H
#  include <sys/types.h>
#  include <unistd.h>
#endif
#include <assert.h>

/*! The event mask to grab on the root window */
#define ROOT_EVENTMASK (StructureNotifyMask | PropertyChangeMask | \
                        EnterWindowMask | LeaveWindowMask | \
                        SubstructureRedirectMask | FocusChangeMask | \
                        ButtonPressMask | ButtonReleaseMask)

static gboolean screen_validate_layout(ObDesktopLayout *l);
static gboolean replace_wm(void);
static void     screen_tell_ksplash(void);
static void     screen_fallback_focus(void);

guint                  screen_num_desktops;
guint                  screen_num_monitors;
guint                  screen_desktop;
guint                  screen_last_desktop;
ObScreenShowDestopMode screen_show_desktop_mode;
ObDesktopLayout        screen_desktop_layout;
gchar                **screen_desktop_names;
Window                 screen_support_win;
Time                   screen_desktop_user_time = CurrentTime;

static Size     screen_physical_size;
static guint    screen_old_desktop;
static gboolean screen_desktop_timeout = TRUE;
static guint    screen_desktop_timer = 0;
/*! An array of desktops, holding an array of areas per monitor */
static Rect  *monitor_area = NULL;
/*! An array of desktops, holding an array of struts */
static GSList *struts_top = NULL;
static GSList *struts_left = NULL;
static GSList *struts_right = NULL;
static GSList *struts_bottom = NULL;

static ObPagerPopup *desktop_popup;
static guint         desktop_popup_timer = 0;
static gboolean      desktop_popup_perm;

/*! The number of microseconds that you need to be on a desktop before it will
  replace the remembered "last desktop" */
#define REMEMBER_LAST_DESKTOP_TIME 750

static gboolean replace_wm(void)
{
    gchar *wm_sn;
    Atom wm_sn_atom;
    Window current_wm_sn_owner;
    Time timestamp;

    wm_sn = g_strdup_printf("WM_S%d", ob_screen);
    wm_sn_atom = XInternAtom(obt_display, wm_sn, FALSE);
    g_free(wm_sn);

    current_wm_sn_owner = XGetSelectionOwner(obt_display, wm_sn_atom);
    if (current_wm_sn_owner == screen_support_win)
        current_wm_sn_owner = None;
    if (current_wm_sn_owner) {
        if (!ob_replace_wm) {
            g_message(_("A window manager is already running on screen %d"),
                      ob_screen);
            return FALSE;
        }
        obt_display_ignore_errors(TRUE);

        /* We want to find out when the current selection owner dies */
        XSelectInput(obt_display, current_wm_sn_owner, StructureNotifyMask);
        XSync(obt_display, FALSE);

        obt_display_ignore_errors(FALSE);
        if (obt_display_error_occured)
            current_wm_sn_owner = None;
    }

    timestamp = event_time();

    XSetSelectionOwner(obt_display, wm_sn_atom, screen_support_win,
                       timestamp);

    if (XGetSelectionOwner(obt_display, wm_sn_atom) != screen_support_win) {
        g_message(_("Could not acquire window manager selection on screen %d"),
                  ob_screen);
        return FALSE;
    }

    /* Wait for old window manager to go away */
    if (current_wm_sn_owner) {
      gulong wait = 0;
      const gulong timeout = G_USEC_PER_SEC * 15; /* wait for 15s max */
      ObtXQueueWindowType wt;

      wt.window = current_wm_sn_owner;
      wt.type = DestroyNotify;

      while (wait < timeout) {
          /* Checks the local queue and incoming events for this event */
          if (xqueue_exists_local(xqueue_match_window_type, &wt))
              break;
          g_usleep(G_USEC_PER_SEC / 10);
          wait += G_USEC_PER_SEC / 10;
      }

      if (wait >= timeout) {
          g_message(_("The WM on screen %d is not exiting"), ob_screen);
          return FALSE;
      }
    }

    /* Send client message indicating that we are now the WM */
    obt_prop_message(ob_screen, obt_root(ob_screen), OBT_PROP_ATOM(MANAGER),
                     timestamp, wm_sn_atom, screen_support_win, 0, 0,
                     SubstructureNotifyMask);

    return TRUE;
}

gboolean screen_annex(void)
{
    XSetWindowAttributes attrib;
    pid_t pid;
    gint i, num_support;
    gulong *supported;

    /* create the netwm support window */
    attrib.override_redirect = TRUE;
    attrib.event_mask = PropertyChangeMask;
    screen_support_win = XCreateWindow(obt_display, obt_root(ob_screen),
                                       -100, -100, 1, 1, 0,
                                       CopyFromParent, InputOutput,
                                       CopyFromParent,
                                       CWEventMask | CWOverrideRedirect,
                                       &attrib);
    XMapWindow(obt_display, screen_support_win);
    XLowerWindow(obt_display, screen_support_win);

    if (!replace_wm()) {
        XDestroyWindow(obt_display, screen_support_win);
        return FALSE;
    }

    obt_display_ignore_errors(TRUE);
    XSelectInput(obt_display, obt_root(ob_screen), ROOT_EVENTMASK);
    obt_display_ignore_errors(FALSE);
    if (obt_display_error_occured) {
        g_message(_("A window manager is already running on screen %d"),
                  ob_screen);

        XDestroyWindow(obt_display, screen_support_win);
        return FALSE;
    }

    screen_set_root_cursor();

    /* set the OPENBOX_PID hint */
    pid = getpid();
    OBT_PROP_SET32(obt_root(ob_screen), OPENBOX_PID, CARDINAL, pid);

    /* set supporting window */
    OBT_PROP_SET32(obt_root(ob_screen),
                   NET_SUPPORTING_WM_CHECK, WINDOW, screen_support_win);

    /* set properties on the supporting window */
    OBT_PROP_SETS(screen_support_win, NET_WM_NAME, "Openbox");
    OBT_PROP_SET32(screen_support_win, NET_SUPPORTING_WM_CHECK,
                   WINDOW, screen_support_win);

    /* set the _NET_SUPPORTED_ATOMS hint */

    /* this is all the atoms after NET_SUPPORTED in the ObtPropAtoms enum */
    num_support = OBT_PROP_NUM_ATOMS - OBT_PROP_NET_SUPPORTED - 1;
    i = 0;
    supported = g_new(gulong, num_support);
    supported[i++] = OBT_PROP_ATOM(NET_SUPPORTING_WM_CHECK);
    supported[i++] = OBT_PROP_ATOM(NET_WM_FULL_PLACEMENT);
    supported[i++] = OBT_PROP_ATOM(NET_CURRENT_DESKTOP);
    supported[i++] = OBT_PROP_ATOM(NET_NUMBER_OF_DESKTOPS);
    supported[i++] = OBT_PROP_ATOM(NET_DESKTOP_GEOMETRY);
    supported[i++] = OBT_PROP_ATOM(NET_DESKTOP_VIEWPORT);
    supported[i++] = OBT_PROP_ATOM(NET_ACTIVE_WINDOW);
    supported[i++] = OBT_PROP_ATOM(NET_WORKAREA);
    supported[i++] = OBT_PROP_ATOM(NET_CLIENT_LIST);
    supported[i++] = OBT_PROP_ATOM(NET_CLIENT_LIST_STACKING);
    supported[i++] = OBT_PROP_ATOM(NET_DESKTOP_NAMES);
    supported[i++] = OBT_PROP_ATOM(NET_CLOSE_WINDOW);
    supported[i++] = OBT_PROP_ATOM(NET_DESKTOP_LAYOUT);
    supported[i++] = OBT_PROP_ATOM(NET_SHOWING_DESKTOP);
    supported[i++] = OBT_PROP_ATOM(NET_WM_NAME);
    supported[i++] = OBT_PROP_ATOM(NET_WM_VISIBLE_NAME);
    supported[i++] = OBT_PROP_ATOM(NET_WM_ICON_NAME);
    supported[i++] = OBT_PROP_ATOM(NET_WM_VISIBLE_ICON_NAME);
    supported[i++] = OBT_PROP_ATOM(NET_WM_DESKTOP);
    supported[i++] = OBT_PROP_ATOM(NET_WM_STRUT);
    supported[i++] = OBT_PROP_ATOM(NET_WM_STRUT_PARTIAL);
    supported[i++] = OBT_PROP_ATOM(NET_WM_ICON);
    supported[i++] = OBT_PROP_ATOM(NET_WM_ICON_GEOMETRY);
    supported[i++] = OBT_PROP_ATOM(NET_WM_WINDOW_TYPE);
    supported[i++] = OBT_PROP_ATOM(NET_WM_WINDOW_TYPE_DESKTOP);
    supported[i++] = OBT_PROP_ATOM(NET_WM_WINDOW_TYPE_DOCK);
    supported[i++] = OBT_PROP_ATOM(NET_WM_WINDOW_TYPE_TOOLBAR);
    supported[i++] = OBT_PROP_ATOM(NET_WM_WINDOW_TYPE_MENU);
    supported[i++] = OBT_PROP_ATOM(NET_WM_WINDOW_TYPE_UTILITY);
    supported[i++] = OBT_PROP_ATOM(NET_WM_WINDOW_TYPE_SPLASH);
    supported[i++] = OBT_PROP_ATOM(NET_WM_WINDOW_TYPE_DIALOG);
    supported[i++] = OBT_PROP_ATOM(NET_WM_WINDOW_TYPE_NORMAL);
    supported[i++] = OBT_PROP_ATOM(NET_WM_ALLOWED_ACTIONS);
    supported[i++] = OBT_PROP_ATOM(NET_WM_WINDOW_OPACITY);
    supported[i++] = OBT_PROP_ATOM(NET_WM_ACTION_MOVE);
    supported[i++] = OBT_PROP_ATOM(NET_WM_ACTION_RESIZE);
    supported[i++] = OBT_PROP_ATOM(NET_WM_ACTION_MINIMIZE);
    supported[i++] = OBT_PROP_ATOM(NET_WM_ACTION_SHADE);
    supported[i++] = OBT_PROP_ATOM(NET_WM_ACTION_MAXIMIZE_HORZ);
    supported[i++] = OBT_PROP_ATOM(NET_WM_ACTION_MAXIMIZE_VERT);
    supported[i++] = OBT_PROP_ATOM(NET_WM_ACTION_FULLSCREEN);
    supported[i++] = OBT_PROP_ATOM(NET_WM_ACTION_CHANGE_DESKTOP);
    supported[i++] = OBT_PROP_ATOM(NET_WM_ACTION_CLOSE);
    supported[i++] = OBT_PROP_ATOM(NET_WM_ACTION_ABOVE);
    supported[i++] = OBT_PROP_ATOM(NET_WM_ACTION_BELOW);
    supported[i++] = OBT_PROP_ATOM(NET_WM_STATE);
    supported[i++] = OBT_PROP_ATOM(NET_WM_STATE_MODAL);
    supported[i++] = OBT_PROP_ATOM(NET_WM_STATE_MAXIMIZED_VERT);
    supported[i++] = OBT_PROP_ATOM(NET_WM_STATE_MAXIMIZED_HORZ);
    supported[i++] = OBT_PROP_ATOM(NET_WM_STATE_SHADED);
    supported[i++] = OBT_PROP_ATOM(NET_WM_STATE_SKIP_TASKBAR);
    supported[i++] = OBT_PROP_ATOM(NET_WM_STATE_SKIP_PAGER);
    supported[i++] = OBT_PROP_ATOM(NET_WM_STATE_HIDDEN);
    supported[i++] = OBT_PROP_ATOM(NET_WM_STATE_FULLSCREEN);
    supported[i++] = OBT_PROP_ATOM(NET_WM_STATE_ABOVE);
    supported[i++] = OBT_PROP_ATOM(NET_WM_STATE_BELOW);
    supported[i++] = OBT_PROP_ATOM(NET_WM_STATE_DEMANDS_ATTENTION);
    supported[i++] = OBT_PROP_ATOM(NET_MOVERESIZE_WINDOW);
    supported[i++] = OBT_PROP_ATOM(NET_WM_MOVERESIZE);
    supported[i++] = OBT_PROP_ATOM(NET_WM_USER_TIME);
/*
    supported[i++] = OBT_PROP_ATOM(NET_WM_USER_TIME_WINDOW);
*/
    supported[i++] = OBT_PROP_ATOM(NET_FRAME_EXTENTS);
    supported[i++] = OBT_PROP_ATOM(NET_REQUEST_FRAME_EXTENTS);
    supported[i++] = OBT_PROP_ATOM(NET_RESTACK_WINDOW);
    supported[i++] = OBT_PROP_ATOM(NET_STARTUP_ID);
#ifdef SYNC
    supported[i++] = OBT_PROP_ATOM(NET_WM_SYNC_REQUEST);
    supported[i++] = OBT_PROP_ATOM(NET_WM_SYNC_REQUEST_COUNTER);
#endif
    supported[i++] = OBT_PROP_ATOM(NET_WM_PID);
    supported[i++] = OBT_PROP_ATOM(NET_WM_PING);

    supported[i++] = OBT_PROP_ATOM(KDE_WM_CHANGE_STATE);
    supported[i++] = OBT_PROP_ATOM(KDE_NET_WM_FRAME_STRUT);
    supported[i++] = OBT_PROP_ATOM(KDE_NET_WM_WINDOW_TYPE_OVERRIDE);

    supported[i++] = OBT_PROP_ATOM(OB_WM_ACTION_UNDECORATE);
    supported[i++] = OBT_PROP_ATOM(OB_WM_STATE_UNDECORATED);
    supported[i++] = OBT_PROP_ATOM(OPENBOX_PID);
    supported[i++] = OBT_PROP_ATOM(OB_THEME);
    supported[i++] = OBT_PROP_ATOM(OB_CONFIG_FILE);
    supported[i++] = OBT_PROP_ATOM(OB_CONTROL);
    supported[i++] = OBT_PROP_ATOM(OB_VERSION);
    supported[i++] = OBT_PROP_ATOM(OB_APP_ROLE);
    supported[i++] = OBT_PROP_ATOM(OB_APP_TITLE);
    supported[i++] = OBT_PROP_ATOM(OB_APP_NAME);
    supported[i++] = OBT_PROP_ATOM(OB_APP_CLASS);
    supported[i++] = OBT_PROP_ATOM(OB_APP_GROUP_NAME);
    supported[i++] = OBT_PROP_ATOM(OB_APP_GROUP_CLASS);
    supported[i++] = OBT_PROP_ATOM(OB_APP_TYPE);
    g_assert(i == num_support);

    OBT_PROP_SETA32(obt_root(ob_screen),
                    NET_SUPPORTED, ATOM, supported, num_support);
    g_free(supported);

    OBT_PROP_SETS(RootWindow(obt_display, ob_screen), OB_VERSION,
                  OPENBOX_VERSION);

    screen_tell_ksplash();

    return TRUE;
}

static void screen_tell_ksplash(void)
{
    XEvent e;
    char **argv;

    argv = g_new(gchar*, 6);
    argv[0] = g_strdup("dcop");
    argv[1] = g_strdup("ksplash");
    argv[2] = g_strdup("ksplash");
    argv[3] = g_strdup("upAndRunning(QString)");
    argv[4] = g_strdup("wm started");
    argv[5] = NULL;

    /* tell ksplash through the dcop server command line interface */
    g_spawn_async(NULL, argv, NULL,
                  G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD |
                  G_SPAWN_STDERR_TO_DEV_NULL | G_SPAWN_STDOUT_TO_DEV_NULL,
                  NULL, NULL, NULL, NULL);
    g_strfreev(argv);

    /* i'm not sure why we do this, kwin does it, but ksplash doesn't seem to
       hear it anyways. perhaps it is for old ksplash. or new ksplash. or
       something. oh well. */
    e.xclient.type = ClientMessage;
    e.xclient.display = obt_display;
    e.xclient.window = obt_root(ob_screen);
    e.xclient.message_type =
        XInternAtom(obt_display, "_KDE_SPLASH_PROGRESS", False);
    e.xclient.format = 8;
    strcpy(e.xclient.data.b, "wm started");
    XSendEvent(obt_display, obt_root(ob_screen),
               False, SubstructureNotifyMask, &e);
}

void screen_startup(gboolean reconfig)
{
    gchar **names = NULL;
    guint32 d;
    gboolean namesexist = FALSE;

    desktop_popup = pager_popup_new();
    desktop_popup_perm = FALSE;
    pager_popup_height(desktop_popup, POPUP_HEIGHT);

    if (reconfig) {
        /* update the pager popup's width */
        pager_popup_text_width_to_strings(desktop_popup,
                                          screen_desktop_names,
                                          screen_num_desktops);
        return;
    }

    /* get the initial size */
    screen_resize();

    /* have names already been set for the desktops? */
    if (OBT_PROP_GETSS_UTF8(obt_root(ob_screen), NET_DESKTOP_NAMES, &names)) {
        g_strfreev(names);
        namesexist = TRUE;
    }

    /* if names don't exist and we have session names, set those.
       do this stuff BEFORE setting the number of desktops, because that
       will create default names for them
    */
    if (!namesexist && session_desktop_names != NULL) {
        guint i, numnames;
        GSList *it;

        /* get the desktop names */
        numnames = g_slist_length(session_desktop_names);
        names = g_new(gchar*, numnames + 1);
        names[numnames] = NULL;
        for (i = 0, it = session_desktop_names; it; ++i, it = g_slist_next(it))
            names[i] = g_strdup(it->data);

        /* set the root window property */
        OBT_PROP_SETSS(obt_root(ob_screen),
                       NET_DESKTOP_NAMES, (const gchar*const*)names);

        g_strfreev(names);
    }

    /* set the number of desktops, if it's not already set.

       this will also set the default names from the config file up for
       desktops that don't have names yet */
    screen_num_desktops = 0;
    if (OBT_PROP_GET32(obt_root(ob_screen),
                       NET_NUMBER_OF_DESKTOPS, CARDINAL, &d))
    {
        if (d != config_desktops_num) {
            /* TRANSLATORS: If you need to specify a different order of the
               arguments, you can use %1$d for the first one and %2$d for the
               second one. For example,
               "The current session has %2$d desktops, but Openbox is configured for %1$d ..." */
            g_warning(ngettext("Openbox is configured for %d desktop, but the current session has %d.  Overriding the Openbox configuration.", "Openbox is configured for %d desktops, but the current session has %d.  Overriding the Openbox configuration.", config_desktops_num),
                      config_desktops_num, d);
        }
        screen_set_num_desktops(d);
    }
    /* restore from session if possible */
    else if (session_num_desktops)
        screen_set_num_desktops(session_num_desktops);
    else
        screen_set_num_desktops(config_desktops_num);

    screen_desktop = screen_num_desktops;  /* something invalid */
    /* start on the current desktop when a wm was already running */
    if (OBT_PROP_GET32(obt_root(ob_screen),
                       NET_CURRENT_DESKTOP, CARDINAL, &d) &&
        d < screen_num_desktops)
    {
        screen_set_desktop(d, FALSE);
    } else if (session_desktop >= 0)
        screen_set_desktop(MIN((guint)session_desktop,
                               screen_num_desktops), FALSE);
    else
        screen_set_desktop(MIN(config_screen_firstdesk,
                               screen_num_desktops) - 1, FALSE);
    screen_last_desktop = screen_desktop;

    /* don't start in showing-desktop mode */
    screen_show_desktop_mode = SCREEN_SHOW_DESKTOP_NO;
    OBT_PROP_SET32(obt_root(ob_screen),
                   NET_SHOWING_DESKTOP, CARDINAL, screen_showing_desktop());

    if (session_desktop_layout_present &&
        screen_validate_layout(&session_desktop_layout))
    {
        screen_desktop_layout = session_desktop_layout;
    }
    else
        screen_update_layout();
}

void screen_shutdown(gboolean reconfig)
{
    pager_popup_free(desktop_popup);

    if (reconfig)
        return;

    XSelectInput(obt_display, obt_root(ob_screen), NoEventMask);

    /* we're not running here no more! */
    OBT_PROP_ERASE(obt_root(ob_screen), OPENBOX_PID);
    /* not without us */
    OBT_PROP_ERASE(obt_root(ob_screen), NET_SUPPORTED);
    /* don't keep this mode */
    OBT_PROP_ERASE(obt_root(ob_screen), NET_SHOWING_DESKTOP);

    XDestroyWindow(obt_display, screen_support_win);

    g_strfreev(screen_desktop_names);
    screen_desktop_names = NULL;
}

void screen_resize(void)
{
    gint w, h;
    GList *it;
    gulong geometry[2];

    w = WidthOfScreen(ScreenOfDisplay(obt_display, ob_screen));
    h = HeightOfScreen(ScreenOfDisplay(obt_display, ob_screen));

    /* Set the _NET_DESKTOP_GEOMETRY hint */
    screen_physical_size.width = geometry[0] = w;
    screen_physical_size.height = geometry[1] = h;
    OBT_PROP_SETA32(obt_root(ob_screen),
                    NET_DESKTOP_GEOMETRY, CARDINAL, geometry, 2);

    if (ob_state() != OB_STATE_RUNNING)
        return;

    /* this calls screen_update_areas(), which we need ! */
    dock_configure();

    for (it = client_list; it; it = g_list_next(it)) {
        client_move_onscreen(it->data, FALSE);
        client_reconfigure(it->data, FALSE);
    }
}

void screen_set_num_desktops(guint num)
{
    gulong *viewport;
    GList *it, *stacking_copy;

    g_assert(num > 0);

    if (screen_num_desktops == num) return;

    screen_num_desktops = num;
    OBT_PROP_SET32(obt_root(ob_screen), NET_NUMBER_OF_DESKTOPS, CARDINAL, num);

    /* set the viewport hint */
    viewport = g_new0(gulong, num * 2);
    OBT_PROP_SETA32(obt_root(ob_screen),
                    NET_DESKTOP_VIEWPORT, CARDINAL, viewport, num * 2);
    g_free(viewport);

    /* the number of rows/columns will differ */
    screen_update_layout();

    /* move windows on desktops that will no longer exist!
       make a copy of the list cuz we're changing it */
    stacking_copy = g_list_copy(stacking_list);
    for (it = g_list_last(stacking_copy); it; it = g_list_previous(it)) {
        if (WINDOW_IS_CLIENT(it->data)) {
            ObClient *c = it->data;
            if (c->desktop != DESKTOP_ALL && c->desktop >= num)
                client_set_desktop(c, num - 1, FALSE, TRUE);
            /* raise all the windows that are on the current desktop which
               is being merged */
            else if (screen_desktop == num - 1 &&
                     (c->desktop == DESKTOP_ALL ||
                      c->desktop == screen_desktop))
                stacking_raise(CLIENT_AS_WINDOW(c));
        }
    }
    g_list_free(stacking_copy);

    /* change our struts/area to match (after moving windows) */
    screen_update_areas();

    /* may be some unnamed desktops that we need to fill in with names
       (after updating the areas so the popup can resize) */
    screen_update_desktop_names();

    /* change our desktop if we're on one that no longer exists! */
    if (screen_desktop >= screen_num_desktops)
        screen_set_desktop(num - 1, TRUE);
}

static void screen_fallback_focus(void)
{
    ObClient *c;
    gboolean allow_omni;

    /* only allow omnipresent windows to get focus on desktop change if
       an omnipresent window is already focused (it'll keep focus probably, but
       maybe not depending on mouse-focus options) */
    allow_omni = focus_client && (client_normal(focus_client) &&
                                  focus_client->desktop == DESKTOP_ALL);

    /* the client moved there already so don't move focus. prevent flicker
       on sendtodesktop + follow */
    if (focus_client && focus_client->desktop == screen_desktop)
        return;

    /* have to try focus here because when you leave an empty desktop
       there is no focus out to watch for. also, we have different rules
       here. we always allow it to look under the mouse pointer if
       config_focus_last is FALSE

       do this before hiding the windows so if helper windows are coming
       with us, they don't get hidden
    */
    if ((c = focus_fallback(TRUE, !config_focus_last, allow_omni,
                            !allow_omni)))
    {
        /* only do the flicker reducing stuff ahead of time if we are going
           to call xsetinputfocus on the window ourselves. otherwise there is
           no guarantee the window will actually take focus.. */
        if (c->can_focus) {
            /* reduce flicker by hiliting now rather than waiting for the
               server FocusIn event */
            frame_adjust_focus(c->frame, TRUE);
            /* do this here so that if you switch desktops to a window with
               helper windows then the helper windows won't flash */
            client_bring_helper_windows(c);
        }
    }
}

static gboolean last_desktop_func(gpointer data)
{
    screen_desktop_timeout = TRUE;
    screen_desktop_timer = 0;
    return FALSE; /* don't repeat */
}

void screen_set_desktop(guint num, gboolean dofocus)
{
    GList *it;
    guint previous;
    gulong ignore_start;

    g_assert(num < screen_num_desktops);

    previous = screen_desktop;
    screen_desktop = num;

    if (previous == num) return;

    OBT_PROP_SET32(obt_root(ob_screen), NET_CURRENT_DESKTOP, CARDINAL, num);

    /* This whole thing decides when/how to save the screen_last_desktop so
       that it can be restored later if you want */
    if (screen_desktop_timeout) {
        /* If screen_desktop_timeout is true, then we've been on this desktop
           long enough and we can save it as the last desktop. */

        if (screen_last_desktop == previous)
            /* this is the startup state only */
            screen_old_desktop = screen_desktop;
        else {
            /* save the "last desktop" as the "old desktop" */
            screen_old_desktop = screen_last_desktop;
            /* save the desktop we're coming from as the "last desktop" */
            screen_last_desktop = previous;
        }
    }
    else {
        /* If screen_desktop_timeout is false, then we just got to this desktop
           and we are moving away again. */

        if (screen_desktop == screen_last_desktop) {
            /* If we are moving to the "last desktop" .. */
            if (previous == screen_old_desktop) {
                /* .. from the "old desktop", change the last desktop to
                   be where we are coming from */
                screen_last_desktop = screen_old_desktop;
            }
            else if (screen_last_desktop == screen_old_desktop) {
                /* .. and also to the "old desktop", change the "last
                   desktop" to be where we are coming from */
                screen_last_desktop = previous;
            }
            else {
                /* .. from some other desktop, then set the "last desktop" to
                   be the saved "old desktop", i.e. where we were before the
                   "last desktop" */
                screen_last_desktop = screen_old_desktop;
            }
        }
        else {
            /* If we are moving to any desktop besides the "last desktop"..
               (this is the normal case) */
            if (screen_desktop == screen_old_desktop) {
                /* If moving to the "old desktop", which is not the
                   "last desktop", don't save anything */
            }
            else if (previous == screen_old_desktop) {
                /* If moving from the "old desktop", and not to the
                   "last desktop", don't save anything */
            }
            else if (screen_last_desktop == screen_old_desktop) {
                /* If the "last desktop" is the same as "old desktop" and
                   you're not moving to the "last desktop" then save where
                   we're coming from as the "last desktop" */
                screen_last_desktop = previous;
            }
            else {
                /* If the "last desktop" is different from the "old desktop"
                   and you're not moving to the "last desktop", then don't save
                   anything */
            }
        }
    }
    screen_desktop_timeout = FALSE;
    if (screen_desktop_timer) g_source_remove(screen_desktop_timer);
    screen_desktop_timer = g_timeout_add(REMEMBER_LAST_DESKTOP_TIME,
                                         last_desktop_func, NULL);

    ob_debug("Moving to desktop %d", num+1);

    if (ob_state() == OB_STATE_RUNNING)
        screen_show_desktop_popup(screen_desktop, FALSE);

    /* ignore enter events caused by the move */
    ignore_start = event_start_ignore_all_enters();

    if (moveresize_client)
        client_set_desktop(moveresize_client, num, TRUE, FALSE);

    /* show windows before hiding the rest to lessen the enter/leave events */

    /* show windows from top to bottom */
    for (it = stacking_list; it; it = g_list_next(it)) {
        if (WINDOW_IS_CLIENT(it->data)) {
            ObClient *c = it->data;
            client_show(c);
        }
    }

    if (dofocus) screen_fallback_focus();

    /* hide windows from bottom to top */
    for (it = g_list_last(stacking_list); it; it = g_list_previous(it)) {
        if (WINDOW_IS_CLIENT(it->data)) {
            ObClient *c = it->data;
            if (client_hide(c)) {
                if (c == focus_client) {
                    /* c was focused and we didn't do fallback clearly so make
                       sure openbox doesnt still consider the window focused.
                       this happens when using NextWindow with allDesktops,
                       since it doesnt want to move focus on desktop change,
                       but the focus is not going to stay with the current
                       window, which has now disappeared.
                       only do this if the client was actually hidden,
                       otherwise it can keep focus. */
                    focus_set_client(NULL);
                }
            }
        }
    }

    focus_cycle_addremove(NULL, TRUE);

    event_end_ignore_all_enters(ignore_start);

    if (event_source_time() != CurrentTime)
        screen_desktop_user_time = event_source_time();
}

void screen_add_desktop(gboolean current)
{
    gulong ignore_start;

    /* ignore enter events caused by this */
    ignore_start = event_start_ignore_all_enters();

    screen_set_num_desktops(screen_num_desktops+1);

    /* move all the clients over */
    if (current) {
        GList *it;

        for (it = client_list; it; it = g_list_next(it)) {
            ObClient *c = it->data;
            if (c->desktop != DESKTOP_ALL && c->desktop >= screen_desktop &&
                /* don't move direct children, they'll be moved with their
                   parent - which will have to be on the same desktop */
                !client_direct_parent(c))
            {
                ob_debug("moving window %s", c->title);
                client_set_desktop(c, c->desktop+1, FALSE, TRUE);
            }
        }
    }

    event_end_ignore_all_enters(ignore_start);
}

void screen_remove_desktop(gboolean current)
{
    guint rmdesktop, movedesktop;
    GList *it, *stacking_copy;
    gulong ignore_start;

    if (screen_num_desktops <= 1) return;

    /* ignore enter events caused by this */
    ignore_start = event_start_ignore_all_enters();

    /* what desktop are we removing and moving to? */
    if (current)
        rmdesktop = screen_desktop;
    else
        rmdesktop = screen_num_desktops - 1;
    if (rmdesktop < screen_num_desktops - 1)
        movedesktop = rmdesktop + 1;
    else
        movedesktop = rmdesktop;

    /* make a copy of the list cuz we're changing it */
    stacking_copy = g_list_copy(stacking_list);
    for (it = g_list_last(stacking_copy); it; it = g_list_previous(it)) {
        if (WINDOW_IS_CLIENT(it->data)) {
            ObClient *c = it->data;
            guint d = c->desktop;
            if (d != DESKTOP_ALL && d >= movedesktop &&
                /* don't move direct children, they'll be moved with their
                   parent - which will have to be on the same desktop */
                !client_direct_parent(c))
            {
                ob_debug("moving window %s", c->title);
                client_set_desktop(c, c->desktop - 1, TRUE, TRUE);
            }
            /* raise all the windows that are on the current desktop which
               is being merged */
            if ((screen_desktop == rmdesktop - 1 ||
                 screen_desktop == rmdesktop) &&
                (d == DESKTOP_ALL || d == screen_desktop))
            {
                stacking_raise(CLIENT_AS_WINDOW(c));
                ob_debug("raising window %s", c->title);
            }
        }
    }
    g_list_free(stacking_copy);

    /* fallback focus like we're changing desktops */
    if (screen_desktop < screen_num_desktops - 1) {
        screen_fallback_focus();
        ob_debug("fake desktop change");
    }

    screen_set_num_desktops(screen_num_desktops-1);

    event_end_ignore_all_enters(ignore_start);
}

static void get_row_col(guint d, guint *r, guint *c)
{
    switch (screen_desktop_layout.orientation) {
    case OB_ORIENTATION_HORZ:
        switch (screen_desktop_layout.start_corner) {
        case OB_CORNER_TOPLEFT:
            *r = d / screen_desktop_layout.columns;
            *c = d % screen_desktop_layout.columns;
            break;
        case OB_CORNER_BOTTOMLEFT:
            *r = screen_desktop_layout.rows - 1 -
                d / screen_desktop_layout.columns;
            *c = d % screen_desktop_layout.columns;
            break;
        case OB_CORNER_TOPRIGHT:
            *r = d / screen_desktop_layout.columns;
            *c = screen_desktop_layout.columns - 1 -
                d % screen_desktop_layout.columns;
            break;
        case OB_CORNER_BOTTOMRIGHT:
            *r = screen_desktop_layout.rows - 1 -
                d / screen_desktop_layout.columns;
            *c = screen_desktop_layout.columns - 1 -
                d % screen_desktop_layout.columns;
            break;
        }
        break;
    case OB_ORIENTATION_VERT:
        switch (screen_desktop_layout.start_corner) {
        case OB_CORNER_TOPLEFT:
            *r = d % screen_desktop_layout.rows;
            *c = d / screen_desktop_layout.rows;
            break;
        case OB_CORNER_BOTTOMLEFT:
            *r = screen_desktop_layout.rows - 1 -
                d % screen_desktop_layout.rows;
            *c = d / screen_desktop_layout.rows;
            break;
        case OB_CORNER_TOPRIGHT:
            *r = d % screen_desktop_layout.rows;
            *c = screen_desktop_layout.columns - 1 -
                d / screen_desktop_layout.rows;
            break;
        case OB_CORNER_BOTTOMRIGHT:
            *r = screen_desktop_layout.rows - 1 -
                d % screen_desktop_layout.rows;
            *c = screen_desktop_layout.columns - 1 -
                d / screen_desktop_layout.rows;
            break;
        }
        break;
    }
}

static guint translate_row_col(guint r, guint c)
{
    switch (screen_desktop_layout.orientation) {
    case OB_ORIENTATION_HORZ:
        switch (screen_desktop_layout.start_corner) {
        case OB_CORNER_TOPLEFT:
            return r % screen_desktop_layout.rows *
                screen_desktop_layout.columns +
                c % screen_desktop_layout.columns;
        case OB_CORNER_BOTTOMLEFT:
            return (screen_desktop_layout.rows - 1 -
                    r % screen_desktop_layout.rows) *
                screen_desktop_layout.columns +
                c % screen_desktop_layout.columns;
        case OB_CORNER_TOPRIGHT:
            return r % screen_desktop_layout.rows *
                screen_desktop_layout.columns +
                (screen_desktop_layout.columns - 1 -
                 c % screen_desktop_layout.columns);
        case OB_CORNER_BOTTOMRIGHT:
            return (screen_desktop_layout.rows - 1 -
                    r % screen_desktop_layout.rows) *
                screen_desktop_layout.columns +
                (screen_desktop_layout.columns - 1 -
                 c % screen_desktop_layout.columns);
        }
    case OB_ORIENTATION_VERT:
        switch (screen_desktop_layout.start_corner) {
        case OB_CORNER_TOPLEFT:
            return c % screen_desktop_layout.columns *
                screen_desktop_layout.rows +
                r % screen_desktop_layout.rows;
        case OB_CORNER_BOTTOMLEFT:
            return c % screen_desktop_layout.columns *
                screen_desktop_layout.rows +
                (screen_desktop_layout.rows - 1 -
                 r % screen_desktop_layout.rows);
        case OB_CORNER_TOPRIGHT:
            return (screen_desktop_layout.columns - 1 -
                    c % screen_desktop_layout.columns) *
                screen_desktop_layout.rows +
                r % screen_desktop_layout.rows;
        case OB_CORNER_BOTTOMRIGHT:
            return (screen_desktop_layout.columns - 1 -
                    c % screen_desktop_layout.columns) *
                screen_desktop_layout.rows +
                (screen_desktop_layout.rows - 1 -
                 r % screen_desktop_layout.rows);
        }
    }
    g_assert_not_reached();
    return 0;
}

static gboolean hide_desktop_popup_func(gpointer data)
{
    pager_popup_hide(desktop_popup);
    desktop_popup_timer = 0;
    return FALSE; /* don't repeat */
}

void screen_show_desktop_popup(guint d, gboolean perm)
{
    const Rect *a;

    /* 0 means don't show the popup */
    if (!config_desktop_popup_time) return;

    a = screen_physical_area_primary(FALSE);
    pager_popup_position(desktop_popup, CenterGravity,
                         a->x + a->width / 2, a->y + a->height / 2);
    pager_popup_icon_size_multiplier(desktop_popup,
                                     (screen_desktop_layout.columns /
                                      screen_desktop_layout.rows) / 2,
                                     (screen_desktop_layout.rows/
                                      screen_desktop_layout.columns) / 2);
    pager_popup_max_width(desktop_popup,
                          MAX(a->width/3, POPUP_WIDTH));
    pager_popup_show(desktop_popup, screen_desktop_names[d], d);

    if (desktop_popup_timer) g_source_remove(desktop_popup_timer);
    desktop_popup_timer = 0;
    if (!perm && !desktop_popup_perm)
        /* only hide if its not already being show permanently */
        desktop_popup_timer = g_timeout_add(config_desktop_popup_time,
                                            hide_desktop_popup_func,
                                            desktop_popup);
    if (perm)
        desktop_popup_perm = TRUE;
}

void screen_hide_desktop_popup(void)
{
    if (desktop_popup_timer) g_source_remove(desktop_popup_timer);
    desktop_popup_timer = 0;
    pager_popup_hide(desktop_popup);
    desktop_popup_perm = FALSE;
}

guint screen_find_desktop(guint from, ObDirection dir,
                          gboolean wrap, gboolean linear)
{
    guint r, c;
    guint d;

    d = from;
    get_row_col(d, &r, &c);
    if (linear) {
        switch (dir) {
        case OB_DIRECTION_EAST:
            if (d < screen_num_desktops - 1)
                ++d;
            else if (wrap)
                d = 0;
            else
                return from;
            break;
        case OB_DIRECTION_WEST:
            if (d > 0)
                --d;
            else if (wrap)
                d = screen_num_desktops - 1;
            else
                return from;
            break;
        default:
            g_assert_not_reached();
            return from;
        }
    } else {
        switch (dir) {
        case OB_DIRECTION_EAST:
            ++c;
            if (c >= screen_desktop_layout.columns) {
                if (wrap)
                    c = 0;
                else
                    return from;
            }
            d = translate_row_col(r, c);
            if (d >= screen_num_desktops) {
                if (wrap)
                    ++c;
                else
                    return from;
            }
            break;
        case OB_DIRECTION_WEST:
            --c;
            if (c >= screen_desktop_layout.columns) {
                if (wrap)
                    c = screen_desktop_layout.columns - 1;
                else
                    return from;
            }
            d = translate_row_col(r, c);
            if (d >= screen_num_desktops) {
                if (wrap)
                    --c;
                else
                    return from;
            }
            break;
        case OB_DIRECTION_SOUTH:
            ++r;
            if (r >= screen_desktop_layout.rows) {
                if (wrap)
                    r = 0;
                else
                    return from;
            }
            d = translate_row_col(r, c);
            if (d >= screen_num_desktops) {
                if (wrap)
                    ++r;
                else
                    return from;
            }
            break;
        case OB_DIRECTION_NORTH:
            --r;
            if (r >= screen_desktop_layout.rows) {
                if (wrap)
                    r = screen_desktop_layout.rows - 1;
                else
                    return from;
            }
            d = translate_row_col(r, c);
            if (d >= screen_num_desktops) {
                if (wrap)
                    --r;
                else
                    return from;
            }
            break;
        default:
            g_assert_not_reached();
            return from;
        }

        d = translate_row_col(r, c);
    }
    return d;
}

static gboolean screen_validate_layout(ObDesktopLayout *l)
{
    if (l->columns == 0 && l->rows == 0) /* both 0's is bad data.. */
        return FALSE;

    /* fill in a zero rows/columns */
    if (l->columns == 0) {
        l->columns = screen_num_desktops / l->rows;
        if (l->rows * l->columns < screen_num_desktops)
            l->columns++;
        if (l->rows * l->columns >= screen_num_desktops + l->columns)
            l->rows--;
    } else if (l->rows == 0) {
        l->rows = screen_num_desktops / l->columns;
        if (l->columns * l->rows < screen_num_desktops)
            l->rows++;
        if (l->columns * l->rows >= screen_num_desktops + l->rows)
            l->columns--;
    }

    /* bounds checking */
    if (l->orientation == OB_ORIENTATION_HORZ) {
        l->columns = MIN(screen_num_desktops, l->columns);
        l->rows = MIN(l->rows,
                      (screen_num_desktops + l->columns - 1) / l->columns);
        l->columns = screen_num_desktops / l->rows +
            !!(screen_num_desktops % l->rows);
    } else {
        l->rows = MIN(screen_num_desktops, l->rows);
        l->columns = MIN(l->columns,
                         (screen_num_desktops + l->rows - 1) / l->rows);
        l->rows = screen_num_desktops / l->columns +
            !!(screen_num_desktops % l->columns);
    }
    return TRUE;
}

void screen_update_layout(void)

{
    ObDesktopLayout l;
    guint32 *data;
    guint num;

    screen_desktop_layout.orientation = OB_ORIENTATION_HORZ;
    screen_desktop_layout.start_corner = OB_CORNER_TOPLEFT;
    screen_desktop_layout.rows = 1;
    screen_desktop_layout.columns = screen_num_desktops;

    if (OBT_PROP_GETA32(obt_root(ob_screen),
                        NET_DESKTOP_LAYOUT, CARDINAL, &data, &num)) {
        if (num == 3 || num == 4) {

            if (data[0] == OBT_PROP_ATOM(NET_WM_ORIENTATION_VERT))
                l.orientation = OB_ORIENTATION_VERT;
            else if (data[0] == OBT_PROP_ATOM(NET_WM_ORIENTATION_HORZ))
                l.orientation = OB_ORIENTATION_HORZ;
            else
                return;

            if (num < 4)
                l.start_corner = OB_CORNER_TOPLEFT;
            else {
                if (data[3] == OBT_PROP_ATOM(NET_WM_TOPLEFT))
                    l.start_corner = OB_CORNER_TOPLEFT;
                else if (data[3] == OBT_PROP_ATOM(NET_WM_TOPRIGHT))
                    l.start_corner = OB_CORNER_TOPRIGHT;
                else if (data[3] == OBT_PROP_ATOM(NET_WM_BOTTOMRIGHT))
                    l.start_corner = OB_CORNER_BOTTOMRIGHT;
                else if (data[3] == OBT_PROP_ATOM(NET_WM_BOTTOMLEFT))
                    l.start_corner = OB_CORNER_BOTTOMLEFT;
                else
                    return;
            }

            l.columns = data[1];
            l.rows = data[2];

            if (screen_validate_layout(&l))
                screen_desktop_layout = l;

            g_free(data);
        }
    }
}

void screen_update_desktop_names(void)
{
    guint i;

    /* empty the array */
    g_strfreev(screen_desktop_names);
    screen_desktop_names = NULL;

    if (OBT_PROP_GETSS(obt_root(ob_screen),
                       NET_DESKTOP_NAMES, &screen_desktop_names))
        for (i = 0; screen_desktop_names[i] && i < screen_num_desktops; ++i);
    else
        i = 0;
    if (i < screen_num_desktops) {
        GSList *it;

        screen_desktop_names = g_renew(gchar*, screen_desktop_names,
                                       screen_num_desktops + 1);
        screen_desktop_names[screen_num_desktops] = NULL;

        it = g_slist_nth(config_desktops_names, i);

        for (; i < screen_num_desktops; ++i) {
            if (it && ((char*)it->data)[0]) /* not empty */
                /* use the names from the config file when possible */
                screen_desktop_names[i] = g_strdup(it->data);
            else
                /* make up a nice name if it's not though */
                screen_desktop_names[i] = g_strdup_printf(_("desktop %i"),
                                                          i + 1);
            if (it) it = g_slist_next(it);
        }

        /* if we changed any names, then set the root property so we can
           all agree on the names */
        OBT_PROP_SETSS(obt_root(ob_screen), NET_DESKTOP_NAMES,
                       (const gchar*const*)screen_desktop_names);
    }

    /* resize the pager for these names */
    pager_popup_text_width_to_strings(desktop_popup,
                                      screen_desktop_names,
                                      screen_num_desktops);
}

void screen_show_desktop(ObScreenShowDestopMode show_mode, ObClient *show_only)
{
    GList *it;

    ObScreenShowDestopMode before_mode = screen_show_desktop_mode;

    gboolean showing_before = screen_showing_desktop();
    screen_show_desktop_mode = show_mode;
    gboolean showing_after = screen_showing_desktop();

    if (showing_before == showing_after) {
        /* No change. */
        screen_show_desktop_mode = before_mode;
        return;
    }

    if (screen_show_desktop_mode == SCREEN_SHOW_DESKTOP_UNTIL_TOGGLE &&
        show_only != NULL)
    {
        /* If we're showing the desktop until the show-mode is toggled, we
           don't allow breaking out of showing-desktop mode unless we're
           showing all the windows again. */
        screen_show_desktop_mode = before_mode;
        return;
    }

    if (showing_after) {
        /* hide windows bottom to top */
        for (it = g_list_last(stacking_list); it; it = g_list_previous(it)) {
            if (WINDOW_IS_CLIENT(it->data)) {
                ObClient *client = it->data;
                client_showhide(client);
            }
        }
    }
    else {
        /* restore windows top to bottom */
        for (it = stacking_list; it; it = g_list_next(it)) {
            if (WINDOW_IS_CLIENT(it->data)) {
                ObClient *client = it->data;
                if (client_should_show(client)) {
                    if (!show_only || client == show_only)
                        client_show(client);
                    else
                        client_iconify(client, TRUE, FALSE, TRUE);
                }
            }
        }
    }

    if (showing_after) {
        /* focus the desktop */
        for (it = focus_order; it; it = g_list_next(it)) {
            ObClient *c = it->data;
            if (c->type == OB_CLIENT_TYPE_DESKTOP &&
                (c->desktop == screen_desktop || c->desktop == DESKTOP_ALL) &&
                client_focus(it->data))
                break;
        }
    }
    else if (!show_only) {
        ObClient *c;

        if ((c = focus_fallback(TRUE, FALSE, TRUE, FALSE))) {
            /* only do the flicker reducing stuff ahead of time if we are going
               to call xsetinputfocus on the window ourselves. otherwise there
               is no guarantee the window will actually take focus.. */
            if (c->can_focus) {
                /* reduce flicker by hiliting now rather than waiting for the
                   server FocusIn event */
                frame_adjust_focus(c->frame, TRUE);
            }
        }
    }

    OBT_PROP_SET32(obt_root(ob_screen),
                   NET_SHOWING_DESKTOP,
                   CARDINAL,
                   !!showing_after);
}

gboolean screen_showing_desktop()
{
    switch (screen_show_desktop_mode) {
    case SCREEN_SHOW_DESKTOP_NO:
        return FALSE;
    case SCREEN_SHOW_DESKTOP_UNTIL_WINDOW:
    case SCREEN_SHOW_DESKTOP_UNTIL_TOGGLE:
        return TRUE;
    }
    g_assert_not_reached();
    return FALSE;
}

void screen_install_colormap(ObClient *client, gboolean install)
{
    if (client == NULL || client->colormap == None) {
        if (install)
            XInstallColormap(obt_display, RrColormap(ob_rr_inst));
        else
            XUninstallColormap(obt_display, RrColormap(ob_rr_inst));
    } else {
        obt_display_ignore_errors(TRUE);
        if (install)
            XInstallColormap(obt_display, client->colormap);
        else
            XUninstallColormap(obt_display, client->colormap);
        obt_display_ignore_errors(FALSE);
    }
}

typedef struct {
    guint desktop;
    StrutPartial *strut;
} ObScreenStrut;

#define RESET_STRUT_LIST(sl) \
    while (sl) { \
        g_slice_free(ObScreenStrut, (sl)->data); \
        sl = g_slist_delete_link(sl, sl); \
    }

#define ADD_STRUT_TO_LIST(sl, d, s) \
{ \
    ObScreenStrut *ss = g_slice_new(ObScreenStrut); \
    ss->desktop = d; \
    ss->strut = s;  \
    sl = g_slist_prepend(sl, ss); \
}

#define VALIDATE_STRUTS(sl, side, max) \
{ \
    GSList *it; \
    for (it = sl; it; it = g_slist_next(it)) { \
      ObScreenStrut *ss = it->data; \
      ss->strut->side = MIN(max, ss->strut->side); \
    } \
}

static void get_xinerama_screens(Rect **xin_areas, guint *nxin)
{
    guint i;
    gint l, r, t, b;
#ifdef XINERAMA
    gint n;
    XineramaScreenInfo *info;
#endif

    if (ob_debug_xinerama) {
        gint w = WidthOfScreen(ScreenOfDisplay(obt_display, ob_screen));
        gint h = HeightOfScreen(ScreenOfDisplay(obt_display, ob_screen));
        *nxin = 2;
        *xin_areas = g_new(Rect, *nxin + 1);
        RECT_SET((*xin_areas)[0], 0, 0, w/2, h);
        RECT_SET((*xin_areas)[1], w/2, 0, w-(w/2), h);
    }
#ifdef XINERAMA
    else if (obt_display_extension_xinerama &&
             (info = XineramaQueryScreens(obt_display, &n))) {
        *nxin = n;
        *xin_areas = g_new(Rect, *nxin + 1);
        for (i = 0; i < *nxin; ++i)
            RECT_SET((*xin_areas)[i], info[i].x_org, info[i].y_org,
                     info[i].width, info[i].height);
        XFree(info);
    }
#endif
    else {
        *nxin = 1;
        *xin_areas = g_new(Rect, *nxin + 1);
        RECT_SET((*xin_areas)[0], 0, 0,
                 WidthOfScreen(ScreenOfDisplay(obt_display, ob_screen)),
                 HeightOfScreen(ScreenOfDisplay(obt_display, ob_screen)));
    }

    /* returns one extra with the total area in it */
    l = (*xin_areas)[0].x;
    t = (*xin_areas)[0].y;
    r = (*xin_areas)[0].x + (*xin_areas)[0].width - 1;
    b = (*xin_areas)[0].y + (*xin_areas)[0].height - 1;
    for (i = 1; i < *nxin; ++i) {
        l = MIN(l, (*xin_areas)[i].x);
        t = MIN(l, (*xin_areas)[i].y);
        r = MAX(r, (*xin_areas)[i].x + (*xin_areas)[i].width - 1);
        b = MAX(b, (*xin_areas)[i].y + (*xin_areas)[i].height - 1);
    }
    RECT_SET((*xin_areas)[*nxin], l, t, r - l + 1, b - t + 1);

    for (i = 0; i < *nxin; ++i)
        ob_debug("Monitor %d @ %d,%d %dx%d\n", i,
                 (*xin_areas)[i].x, (*xin_areas)[i].y,
                 (*xin_areas)[i].width, (*xin_areas)[i].height);
    ob_debug("Full desktop @ %d,%d %dx%d\n",
             (*xin_areas)[i].x, (*xin_areas)[i].y,
             (*xin_areas)[i].width, (*xin_areas)[i].height);
}

void screen_update_areas(void)
{
    guint i;
    gulong *dims;
    GList *it, *onscreen;

    /* collect the clients that are on screen */
    onscreen = NULL;
    for (it = client_list; it; it = g_list_next(it)) {
        if (client_monitor(it->data) != screen_num_monitors)
            onscreen = g_list_prepend(onscreen, it->data);
    }

    g_free(monitor_area);
    get_xinerama_screens(&monitor_area, &screen_num_monitors);

    /* set up the user-specified margins */
    config_margins.top_start = RECT_LEFT(monitor_area[screen_num_monitors]);
    config_margins.top_end = RECT_RIGHT(monitor_area[screen_num_monitors]);
    config_margins.bottom_start = RECT_LEFT(monitor_area[screen_num_monitors]);
    config_margins.bottom_end = RECT_RIGHT(monitor_area[screen_num_monitors]);
    config_margins.left_start = RECT_TOP(monitor_area[screen_num_monitors]);
    config_margins.left_end = RECT_BOTTOM(monitor_area[screen_num_monitors]);
    config_margins.right_start = RECT_TOP(monitor_area[screen_num_monitors]);
    config_margins.right_end = RECT_BOTTOM(monitor_area[screen_num_monitors]);

    RESET_STRUT_LIST(struts_left);
    RESET_STRUT_LIST(struts_top);
    RESET_STRUT_LIST(struts_right);
    RESET_STRUT_LIST(struts_bottom);

    /* collect the struts */
    for (it = client_list; it; it = g_list_next(it)) {
        ObClient *c = it->data;
        if (c->strut.left)
            ADD_STRUT_TO_LIST(struts_left, c->desktop, &c->strut);
        if (c->strut.top)
            ADD_STRUT_TO_LIST(struts_top, c->desktop, &c->strut);
        if (c->strut.right)
            ADD_STRUT_TO_LIST(struts_right, c->desktop, &c->strut);
        if (c->strut.bottom)
            ADD_STRUT_TO_LIST(struts_bottom, c->desktop, &c->strut);
    }
    if (dock_strut.left)
        ADD_STRUT_TO_LIST(struts_left, DESKTOP_ALL, &dock_strut);
    if (dock_strut.top)
        ADD_STRUT_TO_LIST(struts_top, DESKTOP_ALL, &dock_strut);
    if (dock_strut.right)
        ADD_STRUT_TO_LIST(struts_right, DESKTOP_ALL, &dock_strut);
    if (dock_strut.bottom)
        ADD_STRUT_TO_LIST(struts_bottom, DESKTOP_ALL, &dock_strut);

    if (config_margins.left)
        ADD_STRUT_TO_LIST(struts_left, DESKTOP_ALL, &config_margins);
    if (config_margins.top)
        ADD_STRUT_TO_LIST(struts_top, DESKTOP_ALL, &config_margins);
    if (config_margins.right)
        ADD_STRUT_TO_LIST(struts_right, DESKTOP_ALL, &config_margins);
    if (config_margins.bottom)
        ADD_STRUT_TO_LIST(struts_bottom, DESKTOP_ALL, &config_margins);

    VALIDATE_STRUTS(struts_left, left,
                    monitor_area[screen_num_monitors].width / 2);
    VALIDATE_STRUTS(struts_right, right,
                    monitor_area[screen_num_monitors].width / 2);
    VALIDATE_STRUTS(struts_top, top,
                    monitor_area[screen_num_monitors].height / 2);
    VALIDATE_STRUTS(struts_bottom, bottom,
                    monitor_area[screen_num_monitors].height / 2);

    dims = g_new(gulong, 4 * screen_num_desktops);
    for (i = 0; i < screen_num_desktops; ++i) {
        Rect *area = screen_area(i, SCREEN_AREA_ALL_MONITORS, NULL);
        dims[i*4+0] = area->x;
        dims[i*4+1] = area->y;
        dims[i*4+2] = area->width;
        dims[i*4+3] = area->height;
        g_slice_free(Rect, area);
    }

    /* set the legacy workarea hint to the union of all the monitors */
    OBT_PROP_SETA32(obt_root(ob_screen), NET_WORKAREA, CARDINAL,
                    dims, 4 * screen_num_desktops);

    /* the area has changed, adjust all the windows if they need it */
    for (it = onscreen; it; it = g_list_next(it))
        client_reconfigure(it->data, FALSE);

    g_free(dims);
}

#if 0
Rect* screen_area_all_monitors(guint desktop)
{
    guint i;
    Rect *a;

    a = screen_area_monitor(desktop, 0);

    /* combine all the monitors together */
    for (i = 1; i < screen_num_monitors; ++i) {
        Rect *m = screen_area_monitor(desktop, i);
        gint l, r, t, b;

        l = MIN(RECT_LEFT(*a), RECT_LEFT(*m));
        t = MIN(RECT_TOP(*a), RECT_TOP(*m));
        r = MAX(RECT_RIGHT(*a), RECT_RIGHT(*m));
        b = MAX(RECT_BOTTOM(*a), RECT_BOTTOM(*m));

        RECT_SET(*a, l, t, r - l + 1, b - t + 1);

        g_free(m);
    }

    return a;
}
#endif

#define STRUT_LEFT_IN_SEARCH(s, search) \
    (RANGES_INTERSECT(search->y, search->height, \
                      s->left_start, s->left_end - s->left_start + 1))
#define STRUT_RIGHT_IN_SEARCH(s, search) \
    (RANGES_INTERSECT(search->y, search->height, \
                      s->right_start, s->right_end - s->right_start + 1))
#define STRUT_TOP_IN_SEARCH(s, search) \
    (RANGES_INTERSECT(search->x, search->width, \
                      s->top_start, s->top_end - s->top_start + 1))
#define STRUT_BOTTOM_IN_SEARCH(s, search) \
    (RANGES_INTERSECT(search->x, search->width, \
                      s->bottom_start, s->bottom_end - s->bottom_start + 1))

#define STRUT_LEFT_IGNORE(s, us, search) \
    (head == SCREEN_AREA_ALL_MONITORS && us && \
     RECT_LEFT(monitor_area[i]) + s->left > RECT_LEFT(*search))
#define STRUT_RIGHT_IGNORE(s, us, search) \
    (head == SCREEN_AREA_ALL_MONITORS && us && \
     RECT_RIGHT(monitor_area[i]) - s->right < RECT_RIGHT(*search))
#define STRUT_TOP_IGNORE(s, us, search) \
    (head == SCREEN_AREA_ALL_MONITORS && us && \
     RECT_TOP(monitor_area[i]) + s->top > RECT_TOP(*search))
#define STRUT_BOTTOM_IGNORE(s, us, search) \
    (head == SCREEN_AREA_ALL_MONITORS && us && \
     RECT_BOTTOM(monitor_area[i]) - s->bottom < RECT_BOTTOM(*search))

Rect* screen_area(guint desktop, guint head, Rect *search)
{
    Rect *a;
    GSList *it;
    gint l, r, t, b;
    guint i, d;
    gboolean us = search != NULL; /* user provided search */

    g_assert(desktop < screen_num_desktops || desktop == DESKTOP_ALL);
    g_assert(head < screen_num_monitors || head == SCREEN_AREA_ONE_MONITOR ||
             head == SCREEN_AREA_ALL_MONITORS);
    g_assert(!(head == SCREEN_AREA_ONE_MONITOR && search == NULL));

    /* find any struts for this monitor
       which will be affecting the search area.
    */

    /* search everything if search is null */
    if (!search) {
        if (head < screen_num_monitors) search = &monitor_area[head];
        else search = &monitor_area[screen_num_monitors];
    }
    if (head == SCREEN_AREA_ONE_MONITOR) head = screen_find_monitor(search);

    /* al is "all left" meaning the furthest left you can get, l is our
       "working left" meaning our current strut edge which we're calculating
    */

    /* only include monitors which the search area lines up with */
    if (RECT_INTERSECTS_RECT(monitor_area[screen_num_monitors], *search)) {
        l = RECT_RIGHT(monitor_area[screen_num_monitors]);
        t = RECT_BOTTOM(monitor_area[screen_num_monitors]);
        r = RECT_LEFT(monitor_area[screen_num_monitors]);
        b = RECT_TOP(monitor_area[screen_num_monitors]);
        for (i = 0; i < screen_num_monitors; ++i) {
            /* add the monitor if applicable */
            if (RANGES_INTERSECT(search->x, search->width,
                                 monitor_area[i].x, monitor_area[i].width))
            {
                t = MIN(t, RECT_TOP(monitor_area[i]));
                b = MAX(b, RECT_BOTTOM(monitor_area[i]));
            }
            if (RANGES_INTERSECT(search->y, search->height,
                                 monitor_area[i].y, monitor_area[i].height))
            {
                l = MIN(l, RECT_LEFT(monitor_area[i]));
                r = MAX(r, RECT_RIGHT(monitor_area[i]));
            }
        }
    } else {
        l = RECT_LEFT(monitor_area[screen_num_monitors]);
        t = RECT_TOP(monitor_area[screen_num_monitors]);
        r = RECT_RIGHT(monitor_area[screen_num_monitors]);
        b = RECT_BOTTOM(monitor_area[screen_num_monitors]);
    }

    for (d = 0; d < screen_num_desktops; ++d) {
        if (d != desktop && desktop != DESKTOP_ALL) continue;

        for (i = 0; i < screen_num_monitors; ++i) {
            if (head != SCREEN_AREA_ALL_MONITORS && head != i) continue;

            for (it = struts_left; it; it = g_slist_next(it)) {
                ObScreenStrut *s = it->data;
                if ((s->desktop == d || s->desktop == DESKTOP_ALL) &&
                    STRUT_LEFT_IN_SEARCH(s->strut, search) &&
                    !STRUT_LEFT_IGNORE(s->strut, us, search))
                    l = MAX(l, RECT_LEFT(monitor_area[screen_num_monitors])
                               + s->strut->left);
            }
            for (it = struts_top; it; it = g_slist_next(it)) {
                ObScreenStrut *s = it->data;
                if ((s->desktop == d || s->desktop == DESKTOP_ALL) &&
                    STRUT_TOP_IN_SEARCH(s->strut, search) &&
                    !STRUT_TOP_IGNORE(s->strut, us, search))
                    t = MAX(t, RECT_TOP(monitor_area[screen_num_monitors])
                               + s->strut->top);
            }
            for (it = struts_right; it; it = g_slist_next(it)) {
                ObScreenStrut *s = it->data;
                if ((s->desktop == d || s->desktop == DESKTOP_ALL) &&
                    STRUT_RIGHT_IN_SEARCH(s->strut, search) &&
                    !STRUT_RIGHT_IGNORE(s->strut, us, search))
                    r = MIN(r, RECT_RIGHT(monitor_area[screen_num_monitors])
                               - s->strut->right);
            }
            for (it = struts_bottom; it; it = g_slist_next(it)) {
                ObScreenStrut *s = it->data;
                if ((s->desktop == d || s->desktop == DESKTOP_ALL) &&
                    STRUT_BOTTOM_IN_SEARCH(s->strut, search) &&
                    !STRUT_BOTTOM_IGNORE(s->strut, us, search))
                    b = MIN(b, RECT_BOTTOM(monitor_area[screen_num_monitors])
                               - s->strut->bottom);
            }

            /* limit to this monitor */
            if (head == i) {
                l = MAX(l, RECT_LEFT(monitor_area[i]));
                t = MAX(t, RECT_TOP(monitor_area[i]));
                r = MIN(r, RECT_RIGHT(monitor_area[i]));
                b = MIN(b, RECT_BOTTOM(monitor_area[i]));
            }
        }
    }

    a = g_slice_new(Rect);
    a->x = l;
    a->y = t;
    a->width = r - l + 1;
    a->height = b - t + 1;
    return a;
}

typedef struct {
    Rect r;
    gboolean subtract;
} RectArithmetic;

guint screen_find_monitor(const Rect *search)
{
    guint i;
    guint mostpx_index = screen_num_monitors;
    glong mostpx = 0;
    guint closest_distance_index = screen_num_monitors;
    guint closest_distance = G_MAXUINT;
    GSList *counted = NULL;

    /* we want to count the number of pixels search has on each monitor, but not
       double count.  so if a pixel is counted on monitor A then we should not
       count it again on monitor B. in the end we want to return the monitor
       that had the most pixels counted under this scheme.

       this assumes that monitors earlier in the list are more desirable to be
       considered the search area's monitor.  we try the configured primary
       monitor first, so it gets the highest preference.

       if we have counted an area A, then we want to subtract the intersection
       of A with the area on future monitors.
       but now consider if we count an area B that intersects A. we want to
       subtract the area B from that counted on future monitors, but not
       subtract the intersection of A and B twice! so we would add the
       intersection of A and B back, to account for it being subtracted both
       for A and B.

       this is the idea behind the algorithm.  we always subtract the full area
       for monitor M intersected with the search area. we'll call that AREA.
       but then we go through the list |counted| and for each rectangle in
       the list that is being subtracted from future monitors, we insert a
       request to add back the intersection of the subtracted rect with AREA.
       vice versa for a rect in |counted| that is getting added back.
    */

    if (config_primary_monitor_index < screen_num_monitors) {
        const Rect *monitor;
        Rect on_current_monitor;
        glong area;

        monitor = screen_physical_area_monitor(config_primary_monitor_index);

        if (RECT_INTERSECTS_RECT(*monitor, *search)) {
            RECT_SET_INTERSECTION(on_current_monitor, *monitor, *search);
            area = RECT_AREA(on_current_monitor);

            if (area > mostpx) {
                mostpx = area;
                mostpx_index = config_primary_monitor_index;
            }

            /* add the intersection rect on the current monitor to the
               counted list. that's easy for the first one, we just mark it for
               subtraction */
            {
                RectArithmetic *ra = g_slice_new(RectArithmetic);
                ra->r = on_current_monitor;
                ra->subtract = TRUE;
                counted = g_slist_prepend(counted, ra);
            }
        }
    }

    for (i = 0; i < screen_num_monitors; ++i) {
        const Rect *monitor;
        Rect on_current_monitor;
        glong area;
        GSList *it;

        monitor = screen_physical_area_monitor(i);

        if (!RECT_INTERSECTS_RECT(*monitor, *search)) {
            /* If we don't intersect then find the distance between the search
               rect and the monitor. We'll use the closest monitor from this
               metric if none of the monitors intersect. */
            guint distance = rect_manhatten_distance(*monitor, *search);

            if (distance < closest_distance) {
                closest_distance = distance;
                closest_distance_index = i;
            }
            continue;
        }

        if (i == config_primary_monitor_index)
            continue;  /* already did this one */

        RECT_SET_INTERSECTION(on_current_monitor, *monitor, *search);
        area = RECT_AREA(on_current_monitor);

        /* remove pixels we already counted on any previous monitors. */
        for (it = counted; it; it = g_slist_next(it)) {
            RectArithmetic *ra = it->data;
            Rect intersection;

            RECT_SET_INTERSECTION(intersection, ra->r, *search);
            if (ra->subtract) area -= RECT_AREA(intersection);
            else area += RECT_AREA(intersection);
        }

        if (area > mostpx) {
            mostpx = area;
            mostpx_index = i;
        }

        /* add the intersection rect on the current monitor I to the counted
           list.
           but now we need to compensate for every rectangle R already in the
           counted list, and add a new rect R' that is the intersection of
           R and I, but with the reverse subtraction/addition operation.
        */
        for (it = counted; it; it = g_slist_next(it)) {
            RectArithmetic *saved = it->data;

            if (!RECT_INTERSECTS_RECT(saved->r, on_current_monitor))
                continue;
            /* we are going to subtract our rect from future monitors, but
               part of it may already be being subtracted/added, so compensate
               to not double add/subtract. */
            RectArithmetic *reverse = g_slice_new(RectArithmetic);
            RECT_SET_INTERSECTION(reverse->r, saved->r, on_current_monitor);
            reverse->subtract = !saved->subtract;
            /* prepend so we can continue thru the list uninterupted */
            counted = g_slist_prepend(counted, reverse);
        }
        {
            RectArithmetic *ra = g_slice_new(RectArithmetic);
            ra->r = on_current_monitor;
            ra->subtract = TRUE;
            counted = g_slist_prepend(counted, ra);
        }
    }

    while (counted) {
        g_slice_free(RectArithmetic, counted->data);
        counted = g_slist_delete_link(counted, counted);
    }

    if (mostpx_index < screen_num_monitors)
        return mostpx_index;

    g_assert(closest_distance_index < screen_num_monitors);
    return closest_distance_index;
}

const Rect* screen_physical_area_all_monitors(void)
{
    return screen_physical_area_monitor(screen_num_monitors);
}

const Rect* screen_physical_area_monitor(guint head)
{
    g_assert(head <= screen_num_monitors);

    return &monitor_area[head];
}

gboolean screen_physical_area_monitor_contains(guint head, Rect *search)
{
    g_assert(head <= screen_num_monitors);
    g_assert(search);
    return RECT_INTERSECTS_RECT(monitor_area[head], *search);
}

guint screen_monitor_active(void)
{
    if (moveresize_client)
        return client_monitor(moveresize_client);
    else if (focus_client)
        return client_monitor(focus_client);
    else
        return screen_monitor_pointer();
}

const Rect* screen_physical_area_active(void)
{
    return screen_physical_area_monitor(screen_monitor_active());
}

guint screen_monitor_primary(gboolean fixed)
{
    if (config_primary_monitor_index > 0) {
        if (config_primary_monitor_index-1 < screen_num_monitors)
            return config_primary_monitor_index - 1;
        else
            return 0;
    }
    else if (fixed)
        return 0;
    else if (config_primary_monitor == OB_PLACE_MONITOR_ACTIVE)
        return screen_monitor_active();
    else /* config_primary_monitor == OB_PLACE_MONITOR_MOUSE */
        return screen_monitor_pointer();
}

const Rect* screen_physical_area_primary(gboolean fixed)
{
    return screen_physical_area_monitor(screen_monitor_primary(fixed));
}

void screen_set_root_cursor(void)
{
    if (sn_app_starting())
        XDefineCursor(obt_display, obt_root(ob_screen),
                      ob_cursor(OB_CURSOR_BUSYPOINTER));
    else
        XDefineCursor(obt_display, obt_root(ob_screen),
                      ob_cursor(OB_CURSOR_POINTER));
}

guint screen_find_monitor_point(guint x, guint y)
{
    Rect mon;
    RECT_SET(mon, x, y, 1, 1);
    return screen_find_monitor(&mon);
}

guint screen_monitor_pointer()
{
    gint x, y;
    if (!screen_pointer_pos(&x, &y))
        x = y = 0;
    return screen_find_monitor_point(x, y);
}

gboolean screen_pointer_pos(gint *x, gint *y)
{
    Window w;
    gint i;
    guint u;
    gboolean ret;

    ret = !!XQueryPointer(obt_display, obt_root(ob_screen),
                          &w, &w, x, y, &i, &i, &u);
    if (!ret) {
        for (i = 0; i < ScreenCount(obt_display); ++i)
            if (i != ob_screen)
                if (XQueryPointer(obt_display, obt_root(i),
                                  &w, &w, x, y, &i, &i, &u))
                    break;
    }
    return ret;
}

gboolean screen_compare_desktops(guint a, guint b)
{
    if (a == DESKTOP_ALL)
        a = screen_desktop;
    if (b == DESKTOP_ALL)
        b = screen_desktop;
    return a == b;
}

void screen_apply_gravity_point(gint *x, gint *y, gint width, gint height,
                                const GravityPoint *position, const Rect *area)
{
    if (position->x.center)
        *x = area->width / 2 - width / 2;
    else {
        *x = position->x.pos;
        if (position->x.denom)
            *x = (*x * area->width) / position->x.denom;
        if (position->x.opposite)
            *x = area->width - width - *x;
    }

    if (position->y.center)
        *y = area->height / 2 - height / 2;
    else {
        *y = position->y.pos;
        if (position->y.denom)
            *y = (*y * area->height) / position->y.denom;
        if (position->y.opposite)
            *y = area->height - height - *y;
    }

    *x += area->x;
    *y += area->y;
}
