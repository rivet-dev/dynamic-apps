/* screen object */
/* vim: set sw=2 et: */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Kim Woelders
 * Copyright (C) 2003 Red Hat, Inc.
 * Copyright (C) 2006-2007 Vincent Untz
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#undef WNCK_DISABLE_DEPRECATED

#include <config.h>

#include <glib/gi18n-lib.h>
#include "screen.h"
#include "window.h"
#include "workspace.h"
#include "application.h"
#include "class-group.h"
#include "xutils.h"
#include "private.h"
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <string.h>
#include <stdlib.h>

/**
 * SECTION:screen
 * @short_description: an object representing a screen.
 * @see_also: #WnckWindow, #WnckWorkspace
 * @stability: Unstable
 *
 * The #WnckScreen represents a physical screen. A screen may consist of
 * multiple monitors which are merged to form a large screen area. The
 * #WnckScreen is at the bottom of the libwnck stack of objects: #WnckWorkspace
 * objects exist a #WnckScreen and #WnckWindow objects are displayed on a
 * #WnckWorkspace.
 *
 * The #WnckScreen corresponds to the notion of
 * <classname>GdkScreen</classname> in GDK.
 *
 * The #WnckScreen objects are always owned by libwnck and must not be
 * referenced or unreferenced.
 */

#define _NET_WM_ORIENTATION_HORZ 0
#define _NET_WM_ORIENTATION_VERT 1

#define _NET_WM_TOPLEFT     0
#define _NET_WM_TOPRIGHT    1
#define _NET_WM_BOTTOMRIGHT 2
#define _NET_WM_BOTTOMLEFT  3

static WnckScreen** screens = NULL;

struct _WnckScreenPrivate
{
  int number;
  Window xroot;
  Screen *xscreen;

  int orig_event_mask;

  /* in map order */
  GList *mapped_windows;
  /* in stacking order */
  GList *stacked_windows;
  /* in 0-to-N order */
  GList *workspaces;

  /* previously_active_window is used in tandem with active_window to
   * determine return status of wnck_window_is_most_recently_actived().
   * These are usually shared for all screens, although this is not guaranteed
   * to be true.
   */
  WnckWindow *active_window;
  WnckWindow *previously_active_window;

  WnckWorkspace *active_workspace;

  /* Provides the sorting order number for the next window, to make
   * sure windows remain sorted in the order they appear.
   */
  gint window_order;

  Pixmap bg_pixmap;

  char *wm_name;

  guint update_handler;

#ifdef HAVE_STARTUP_NOTIFICATION
  SnDisplay *sn_display;
#endif

  guint showing_desktop : 1;

  guint vertical_workspaces : 1;
  _WnckLayoutCorner starting_corner;
  gint rows_of_workspaces;
  gint columns_of_workspaces;

  /* if you add flags, be sure to set them
   * when we create the screen so we get an initial update
   */
  guint need_update_stack_list : 1;
  guint need_update_workspace_list : 1;
  guint need_update_viewport_settings : 1;
  guint need_update_active_workspace : 1;
  guint need_update_active_window : 1;
  guint need_update_workspace_layout : 1;
  guint need_update_workspace_names : 1;
  guint need_update_bg_pixmap : 1;
  guint need_update_showing_desktop : 1;
  guint need_update_wm : 1;
};

G_DEFINE_TYPE (WnckScreen, wnck_screen, G_TYPE_OBJECT);
#define WNCK_SCREEN_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), WNCK_TYPE_SCREEN, WnckScreenPrivate))

enum {
  ACTIVE_WINDOW_CHANGED,
  ACTIVE_WORKSPACE_CHANGED,
  WINDOW_STACKING_CHANGED,
  WINDOW_OPENED,
  WINDOW_CLOSED,
  WORKSPACE_CREATED,
  WORKSPACE_DESTROYED,
  APPLICATION_OPENED,
  APPLICATION_CLOSED,
  CLASS_GROUP_OPENED,
  CLASS_GROUP_CLOSED,
  BACKGROUND_CHANGED,
  SHOWING_DESKTOP_CHANGED,
  VIEWPORTS_CHANGED,
  WM_CHANGED,
  LAST_SIGNAL
};

static void wnck_screen_init        (WnckScreen      *screen);
static void wnck_screen_class_init  (WnckScreenClass *klass);
static void wnck_screen_finalize    (GObject         *object);

static void update_client_list        (WnckScreen      *screen);
static void update_workspace_list     (WnckScreen      *screen);
static void update_viewport_settings  (WnckScreen      *screen);
static void update_active_workspace   (WnckScreen      *screen);
static void update_active_window      (WnckScreen      *screen);
static void update_workspace_layout   (WnckScreen      *screen);
static void update_workspace_names    (WnckScreen      *screen);
static void update_showing_desktop    (WnckScreen      *screen);

static void queue_update            (WnckScreen      *screen);
static void unqueue_update          (WnckScreen      *screen);
static void do_update_now           (WnckScreen      *screen);

static void emit_active_window_changed    (WnckScreen      *screen);
static void emit_active_workspace_changed (WnckScreen      *screen,
                                           WnckWorkspace   *previous_space);
static void emit_window_stacking_changed  (WnckScreen      *screen);
static void emit_window_opened            (WnckScreen      *screen,
                                           WnckWindow      *window);
static void emit_window_closed            (WnckScreen      *screen,
                                           WnckWindow      *window);
static void emit_workspace_created        (WnckScreen      *screen,
                                           WnckWorkspace   *space);
static void emit_workspace_destroyed      (WnckScreen      *screen,
                                           WnckWorkspace   *space);
static void emit_application_opened       (WnckScreen      *screen,
                                           WnckApplication *app);
static void emit_application_closed       (WnckScreen      *screen,
                                           WnckApplication *app);
static void emit_class_group_opened       (WnckScreen      *screen,
                                           WnckClassGroup  *class_group);
static void emit_class_group_closed       (WnckScreen      *screen,
                                           WnckClassGroup  *class_group);
static void emit_background_changed       (WnckScreen      *screen);
static void emit_showing_desktop_changed  (WnckScreen      *screen);
static void emit_viewports_changed        (WnckScreen      *screen);
static void emit_wm_changed               (WnckScreen *screen);

static guint signals[LAST_SIGNAL] = { 0 };

static void
wnck_screen_init (WnckScreen *screen)
{
  screen->priv = WNCK_SCREEN_GET_PRIVATE (screen);

  screen->priv->number = -1;
  screen->priv->starting_corner = WNCK_LAYOUT_CORNER_TOPLEFT;
  screen->priv->rows_of_workspaces = 1;
  screen->priv->columns_of_workspaces = -1;
}

static void
wnck_screen_class_init (WnckScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  _wnck_init ();

  g_type_class_add_private (klass, sizeof (WnckScreenPrivate));

  object_class->finalize = wnck_screen_finalize;

  /**
   * WnckScreen::active-window-changed:
   * @screen: the #WnckScreen which emitted the signal.
   * @previously_active_window: the previously active #WnckWindow before this
   * change.
   *
   * Emitted when the active window on @screen has changed.
   */
  signals[ACTIVE_WINDOW_CHANGED] =
    g_signal_new ("active_window_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckScreenClass, active_window_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, WNCK_TYPE_WINDOW);

  /**
   * WnckScreen::active-workspace-changed:
   * @screen: the #WnckScreen which emitted the signal.
   * @previously_active_space: the previously active #WnckWorkspace before this
   * change.
   *
   * Emitted when the active workspace on @screen has changed.
   */
  signals[ACTIVE_WORKSPACE_CHANGED] =
    g_signal_new ("active_workspace_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckScreenClass, active_workspace_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, WNCK_TYPE_WORKSPACE);

  /**
   * WnckScreen::window-stacking-changed:
   * @screen: the #WnckScreen which emitted the signal.
   *
   * Emitted when the stacking order of #WnckWindow on @screen has changed.
   */
  signals[WINDOW_STACKING_CHANGED] =
    g_signal_new ("window_stacking_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckScreenClass, window_stacking_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * WnckScreen::window-opened:
   * @screen: the #WnckScreen which emitted the signal.
   * @window: the opened #WnckWindow.
   *
   * Emitted when a new #WnckWindow is opened on @screen.
   */
  signals[WINDOW_OPENED] =
    g_signal_new ("window_opened",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckScreenClass, window_opened),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, WNCK_TYPE_WINDOW);

  /**
   * WnckScreen::window-closed:
   * @screen: the #WnckScreen which emitted the signal.
   * @window: the closed #WnckWindow.
   *
   * Emitted when a #WnckWindow is closed on @screen.
   */
  signals[WINDOW_CLOSED] =
    g_signal_new ("window_closed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckScreenClass, window_closed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, WNCK_TYPE_WINDOW);

  /**
   * WnckScreen::workspace-created:
   * @screen: the #WnckScreen which emitted the signal.
   * @space: the workspace that has been created.
   *
   * Emitted when a #WnckWorkspace is created on @screen.
   */
  signals[WORKSPACE_CREATED] =
    g_signal_new ("workspace_created",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckScreenClass, workspace_created),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, WNCK_TYPE_WORKSPACE);

  /**
   * WnckScreen::workspace-destroyed:
   * @screen: the #WnckScreen which emitted the signal.
   * @space: the workspace that has been destroyed.
   *
   * Emitted when a #WnckWorkspace is destroyed on @screen.
   */
  signals[WORKSPACE_DESTROYED] =
    g_signal_new ("workspace_destroyed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckScreenClass, workspace_destroyed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, WNCK_TYPE_WORKSPACE);

  /**
   * WnckScreen::application-opened:
   * @screen: the #WnckScreen which emitted the signal.
   * @app: the opened #WnckApplication.
   *
   * Emitted when a new #WnckApplication is opened on @screen.
   */
  signals[APPLICATION_OPENED] =
    g_signal_new ("application_opened",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckScreenClass, application_opened),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, WNCK_TYPE_APPLICATION);

  /**
   * WnckScreen::application-closed:
   * @screen: the #WnckScreen which emitted the signal.
   * @app: the closed #WnckApplication.
   *
   * Emitted when a #WnckApplication is closed on @screen.
   */
  signals[APPLICATION_CLOSED] =
    g_signal_new ("application_closed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckScreenClass, application_closed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, WNCK_TYPE_APPLICATION);

  /**
   * WnckScreen::class-group-opened:
   * @screen: the #WnckScreen which emitted the signal.
   * @class_group: the opened #WnckClassGroup.
   *
   * Emitted when a new #WnckClassGroup is opened on @screen.
   *
   * Since: 2.20
   */
  signals[CLASS_GROUP_OPENED] =
    g_signal_new ("class_group_opened",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckScreenClass, class_group_opened),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, WNCK_TYPE_CLASS_GROUP);

  /**
   * WnckScreen::class-group-closed:
   * @screen: the #WnckScreen which emitted the signal.
   * @class_group: the closed #WnckClassGroup.
   *
   * Emitted when a #WnckClassGroup is closed on @screen.
   *
   * Since: 2.20
   */
  signals[CLASS_GROUP_CLOSED] =
    g_signal_new ("class_group_closed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckScreenClass, class_group_closed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, WNCK_TYPE_CLASS_GROUP);

  /**
   * WnckScreen::background-changed:
   * @screen: the #WnckScreen which emitted the signal.
   *
   * Emitted when the background on the root window of @screen has changed.
   */
  signals[BACKGROUND_CHANGED] =
    g_signal_new ("background_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckScreenClass, background_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * WnckScreen::showing-desktop-changed:
   * @screen: the #WnckScreen which emitted the signal.
   *
   * Emitted when "showing the desktop" mode of @screen is toggled.
   *
   * Since: 2.20
   */
  signals[SHOWING_DESKTOP_CHANGED] =
    g_signal_new ("showing_desktop_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckScreenClass, showing_desktop_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * WnckScreen::viewports-changed:
   * @screen: the #WnckScreen which emitted the signal.
   *
   * Emitted when a viewport position has changed in a #WnckWorkspace of
   * @screen or when a #WnckWorkspace of @screen gets or loses its viewport.
   *
   * Since: 2.20
   */
    signals[VIEWPORTS_CHANGED] =
    g_signal_new ("viewports_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckScreenClass, viewports_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * WnckScreen::window-manager-changed:
   * @screen: the #WnckScreen which emitted the signal.
   *
   * Emitted when the window manager on @screen has changed.
   *
   * Since: 2.20
   */
    signals[WM_CHANGED] =
    g_signal_new ("window_manager_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckScreenClass, window_manager_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
wnck_screen_finalize (GObject *object)
{
  WnckScreen *screen;
  GList *tmp;
  gpointer weak_pointer;

  screen = WNCK_SCREEN (object);

  _wnck_select_input (screen->priv->xscreen,
                      screen->priv->xroot,
                      screen->priv->orig_event_mask,
                      FALSE);

  unqueue_update (screen);

  for (tmp = screen->priv->stacked_windows; tmp; tmp = tmp->next)
    {
      screen->priv->mapped_windows = g_list_remove (screen->priv->mapped_windows,
                                                    tmp->data);
      _wnck_window_destroy (WNCK_WINDOW (tmp->data));
    }

  for (tmp = screen->priv->mapped_windows; tmp; tmp = tmp->next)
    _wnck_window_destroy (WNCK_WINDOW (tmp->data));

  for (tmp = screen->priv->workspaces; tmp; tmp = tmp->next)
    g_object_unref (tmp->data);

  g_list_free (screen->priv->mapped_windows);
  screen->priv->mapped_windows = NULL;
  g_list_free (screen->priv->stacked_windows);
  screen->priv->stacked_windows = NULL;

  g_list_free (screen->priv->workspaces);
  screen->priv->workspaces = NULL;

  weak_pointer = &screen->priv->active_window;
  if (screen->priv->active_window != NULL)
    g_object_remove_weak_pointer (G_OBJECT (screen->priv->active_window),
                                  weak_pointer);
  screen->priv->active_window = NULL;

  weak_pointer = &screen->priv->previously_active_window;
  if (screen->priv->previously_active_window != NULL)
    g_object_remove_weak_pointer (G_OBJECT (screen->priv->previously_active_window),
                                  weak_pointer);
  screen->priv->previously_active_window = NULL;

  g_free (screen->priv->wm_name);
  screen->priv->wm_name = NULL;

  screens[screen->priv->number] = NULL;

#ifdef HAVE_STARTUP_NOTIFICATION
  sn_display_unref (screen->priv->sn_display);
  screen->priv->sn_display = NULL;
#endif

  G_OBJECT_CLASS (wnck_screen_parent_class)->finalize (object);
}

#ifdef HAVE_STARTUP_NOTIFICATION
static void
sn_error_trap_push (SnDisplay *display,
                    Display   *xdisplay)
{
  _wnck_error_trap_push (xdisplay);
}

static void
sn_error_trap_pop (SnDisplay *display,
                   Display   *xdisplay)
{
  _wnck_error_trap_pop (xdisplay);
}
#endif /* HAVE_STARTUP_NOTIFICATION */

static void
wnck_screen_construct (Display    *display,
                       WnckScreen *screen,
                       int         number)
{
  /* Create the initial state of the screen. */
  screen->priv->xroot = RootWindow (display, number);
  screen->priv->xscreen = ScreenOfDisplay (display, number);
  screen->priv->number = number;

#ifdef HAVE_STARTUP_NOTIFICATION
  screen->priv->sn_display = sn_display_new (display,
                                             sn_error_trap_push,
                                             sn_error_trap_pop);
#endif

  screen->priv->bg_pixmap = None;

  screen->priv->orig_event_mask = _wnck_select_input (screen->priv->xscreen,
                                                      screen->priv->xroot,
                                                      PropertyChangeMask,
                                                      TRUE);

  screen->priv->need_update_workspace_list = TRUE;
  screen->priv->need_update_stack_list = TRUE;
  screen->priv->need_update_viewport_settings = TRUE;
  screen->priv->need_update_active_workspace = TRUE;
  screen->priv->need_update_active_window = TRUE;
  screen->priv->need_update_workspace_layout = TRUE;
  screen->priv->need_update_workspace_names = TRUE;
  screen->priv->need_update_bg_pixmap = TRUE;
  screen->priv->need_update_showing_desktop = TRUE;
  screen->priv->need_update_wm = TRUE;

  queue_update (screen);
}

/**
 * wnck_screen_get:
 * @index: screen number, starting from 0.
 *
 * Gets the #WnckScreen for a given screen on the default display.
 *
 * Return value: (transfer none): the #WnckScreen for screen @index, or %NULL
 * if no such screen exists. The returned #WnckScreen is owned by libwnck and
 * must not be referenced or unreferenced.
 **/
WnckScreen*
wnck_screen_get (int index)
{
  Display *display;

  display = _wnck_get_default_display ();

  g_return_val_if_fail (display != NULL, NULL);

  if (index >= ScreenCount (display))
    return NULL;

  if (screens == NULL)
    {
      screens = g_new0 (WnckScreen*, ScreenCount (display));
      _wnck_event_filter_init ();
    }

  if (screens[index] == NULL)
    {
      screens[index] = g_object_new (WNCK_TYPE_SCREEN, NULL);

      wnck_screen_construct (display, screens[index], index);
    }

  return screens[index];
}

WnckScreen*
_wnck_screen_get_existing (int number)
{
  Display *display;

  display = _wnck_get_default_display ();

  g_return_val_if_fail (display != NULL, NULL);
  g_return_val_if_fail (number < ScreenCount (display), NULL);

  if (screens != NULL)
    return screens[number];
  else
    return NULL;
}

/**
 * wnck_screen_get_default:
 *
 * Gets the default #WnckScreen on the default display.
 *
 * Return value: (transfer none): the default #WnckScreen. The returned
 * #WnckScreen is owned by libwnck and must not be referenced or unreferenced.
 **/
WnckScreen*
wnck_screen_get_default (void)
{
  int default_screen;

  default_screen = DefaultScreen (_wnck_get_default_display ());

  return wnck_screen_get (default_screen);
}

/**
 * wnck_screen_get_for_root:
 * @root_window_id: an X window ID.
 *
 * Gets the #WnckScreen for the root window at @root_window_id, or
 * %NULL if no #WnckScreen exists for this root window.
 *
 * This function does not work if wnck_screen_get() was not called for the
 * sought #WnckScreen before, and returns %NULL.
 *
 * Return value: (transfer none): the #WnckScreen for the root window at
 * @root_window_id, or %NULL. The returned #WnckScreen is owned by libwnck and
 * must not be referenced or unreferenced.
 **/
WnckScreen*
wnck_screen_get_for_root (gulong root_window_id)
{
  int i;
  Display *display;

  if (screens == NULL)
    return NULL;

  i = 0;
  display = _wnck_get_default_display ();

  while (i < ScreenCount (display))
    {
      if (screens[i] != NULL && screens[i]->priv->xroot == root_window_id)
        return screens[i];

      ++i;
    }

  return NULL;
}

/**
 * wnck_screen_get_number:
 * @screen: a #WnckScreen.
 *
 * Gets the index of @screen on the display to which it belongs. The first
 * #WnckScreen has an index of 0.
 *
 * Return value: the index of @space on @screen, or -1 on errors.
 *
 * Since: 2.20
 **/
int
wnck_screen_get_number (WnckScreen *screen)
{
  g_return_val_if_fail (WNCK_IS_SCREEN (screen), -1);

  return screen->priv->number;
}

/**
 * wnck_screen_get_workspaces:
 * @screen: a #WnckScreen.
 *
 * Gets the list of #WnckWorkspace on @screen. The list is ordered: the
 * first element in the list is the first #WnckWorkspace, etc..
 *
 * Return value: (element-type WnckWorkspace) (transfer none): the list of
 * #WnckWorkspace on @screen. The list should not be modified nor freed, as it
 * is owned by @screen.
 *
 * Since: 2.20
 **/
GList*
wnck_screen_get_workspaces (WnckScreen *screen)
{
  g_return_val_if_fail (WNCK_IS_SCREEN (screen), NULL);

  return screen->priv->workspaces;
}

/**
 * wnck_screen_get_workspace:
 * @screen: a #WnckScreen.
 * @workspace: a workspace index, starting from 0.
 *
 * Gets the #WnckWorkspace numbered @workspace on @screen.
 *
 * Return value: (transfer none): the #WnckWorkspace numbered @workspace on
 * @screen, or %NULL if no such workspace exists. The returned #WnckWorkspace
 * is owned by libwnck and must not be referenced or unreferenced.
 **/
WnckWorkspace*
wnck_screen_get_workspace (WnckScreen *screen,
			   int         workspace)
{
  GList *list;

  g_return_val_if_fail (WNCK_IS_SCREEN (screen), NULL);

  /* We trust this function with property-provided numbers, it
   * must reliably return NULL on bad data
   */
  list = g_list_nth (screen->priv->workspaces, workspace);

  if (list == NULL)
    return NULL;

  return WNCK_WORKSPACE (list->data);
}

/**
 * wnck_screen_get_active_workspace:
 * @screen: a #WnckScreen.
 *
 * Gets the active #WnckWorkspace on @screen. May return %NULL sometimes,
 * if libwnck is in a weird state due to the asynchronous nature of the
 * interaction with the window manager.
 *
 * Return value: (transfer none): the active #WnckWorkspace on @screen, or
 * %NULL. The returned #WnckWorkspace is owned by libwnck and must not be
 * referenced or unreferenced.
 **/
WnckWorkspace*
wnck_screen_get_active_workspace (WnckScreen *screen)
{
  g_return_val_if_fail (WNCK_IS_SCREEN (screen), NULL);

  return screen->priv->active_workspace;
}

/**
 * wnck_screen_get_active_window:
 * @screen: a #WnckScreen.
 *
 * Gets the active #WnckWindow on @screen. May return %NULL sometimes, since
 * not all window managers guarantee that a window is always active.
 *
 * Return value: (transfer none): the active #WnckWindow on @screen, or %NULL.
 * The returned #WnckWindow is owned by libwnck and must not be referenced or
 * unreferenced.
 **/
WnckWindow*
wnck_screen_get_active_window (WnckScreen *screen)
{
  g_return_val_if_fail (WNCK_IS_SCREEN (screen), NULL);

  return screen->priv->active_window;
}

/**
 * wnck_screen_get_previously_active_window:
 * @screen: a #WnckScreen.
 *
 * Gets the previously active #WnckWindow on @screen. May return %NULL
 * sometimes, since not all window managers guarantee that a window is always
 * active.
 *
 * Return value: (transfer none): the previously active #WnckWindow on @screen,
 * or %NULL. The returned #WnckWindow is owned by libwnck and must not be
 * referenced or unreferenced.
 *
 * Since: 2.8
 **/
WnckWindow*
wnck_screen_get_previously_active_window (WnckScreen *screen)
{
  g_return_val_if_fail (WNCK_IS_SCREEN (screen), NULL);

  return screen->priv->previously_active_window;
}

/**
 * wnck_screen_get_windows:
 * @screen: a #WnckScreen.
 *
 * Gets the list of #WnckWindow on @screen. The list is not in a defined
 * order, but should be "stable" (windows should not be reordered in it).
 * However, the stability of the list is established by the window manager, so
 * don't blame libwnck if it breaks down.
 *
 * Return value: (element-type WnckWindow) (transfer none): the list of
 * #WnckWindow on @screen, or %NULL if there is no window on @screen. The list
 * should not be modified nor freed, as it is owned by @screen.
 **/
GList*
wnck_screen_get_windows (WnckScreen *screen)
{
  g_return_val_if_fail (WNCK_IS_SCREEN (screen), NULL);

  return screen->priv->mapped_windows;
}

/**
 * wnck_screen_get_windows_stacked:
 * @screen: a #WnckScreen.
 *
 * Gets the list of #WnckWindow on @screen in bottom-to-top order.
 *
 * Return value: (element-type WnckWindow) (transfer none): the list of
 * #WnckWindow in stacking order on @screen, or %NULL if there is no window on
 * @screen. The list should not be modified nor freed, as it is owned by
 * @screen.
 **/
GList*
wnck_screen_get_windows_stacked (WnckScreen *screen)
{
  g_return_val_if_fail (WNCK_IS_SCREEN (screen), NULL);

  return screen->priv->stacked_windows;
}

/**
 * _wnck_screen_get_gdk_screen:
 * @screen: a #WnckScreen.
 *
 * Gets the <classname>GdkScreen</classname referring to the same screen as
 * @screen.
 *
 * Return value: the <classname>GdkScreen</classname referring to the same
 * screen as @screen.
 **/
GdkScreen *
_wnck_screen_get_gdk_screen (WnckScreen *screen)
{
  Display    *display;
  GdkDisplay *gdkdisplay;

  g_return_val_if_fail (WNCK_IS_SCREEN (screen), NULL);

  display = DisplayOfScreen (screen->priv->xscreen);
  gdkdisplay = _wnck_gdk_display_lookup_from_display (display);
  if (!gdkdisplay)
    return NULL;

  return gdk_display_get_screen (gdkdisplay, screen->priv->number);
}

/**
 * wnck_screen_force_update:
 * @screen: a #WnckScreen.
 *
 * Synchronously and immediately updates the list of #WnckWindow on @screen.
 * This bypasses the standard update mechanism, where the list of #WnckWindow
 * is updated in the idle loop.
 *
 * This is usually a bad idea for both performance and correctness reasons (to
 * get things right, you need to write model-view code that tracks changes, not
 * get a static list of open windows). However, this function can be useful for
 * small applications that just do something and then exit.
 **/
void
wnck_screen_force_update (WnckScreen *screen)
{
  g_return_if_fail (WNCK_IS_SCREEN (screen));

  do_update_now (screen);
}

/**
 * wnck_screen_get_workspace_count:
 * @screen: a #WnckScreen.
 *
 * Gets the number of #WnckWorkspace on @screen.
 *
 * Return value: the number of #WnckWorkspace on @screen.
 **/
int
wnck_screen_get_workspace_count (WnckScreen *screen)
{
  g_return_val_if_fail (WNCK_IS_SCREEN (screen), 0);

  return g_list_length (screen->priv->workspaces);
}

/**
 * wnck_screen_change_workspace_count:
 * @screen: a #WnckScreen.
 * @count: the number of #WnckWorkspace to request.
 *
 * Asks the window manager to change the number of #WnckWorkspace on @screen.
 *
 * Since: 2.2
 **/
void
wnck_screen_change_workspace_count (WnckScreen *screen,
                                    int         count)
{
  Display *display;
  XEvent xev;

  g_return_if_fail (WNCK_IS_SCREEN (screen));
  g_return_if_fail (count >= 1);

  display = DisplayOfScreen (screen->priv->xscreen);

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.window = screen->priv->xroot;
  xev.xclient.send_event = True;
  xev.xclient.display = display;
  xev.xclient.message_type = _wnck_atom_get ("_NET_NUMBER_OF_DESKTOPS");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = count;

  _wnck_error_trap_push (display);
  XSendEvent (display,
              screen->priv->xroot,
              False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              &xev);
  _wnck_error_trap_pop (display);
}

void
_wnck_screen_process_property_notify (WnckScreen *screen,
                                      XEvent     *xevent)
{
  /* most frequently-changed properties first */
  if (xevent->xproperty.atom ==
      _wnck_atom_get ("_NET_ACTIVE_WINDOW"))
    {
      screen->priv->need_update_active_window = TRUE;
      queue_update (screen);
    }
  else if (xevent->xproperty.atom ==
           _wnck_atom_get ("_NET_CURRENT_DESKTOP"))
    {
      screen->priv->need_update_active_workspace = TRUE;
      queue_update (screen);
    }
  else if (xevent->xproperty.atom ==
           _wnck_atom_get ("_NET_CLIENT_LIST_STACKING") ||
           xevent->xproperty.atom ==
           _wnck_atom_get ("_NET_CLIENT_LIST"))
    {
      screen->priv->need_update_stack_list = TRUE;
      queue_update (screen);
    }
  else if (xevent->xproperty.atom ==
           _wnck_atom_get ("_NET_DESKTOP_VIEWPORT"))
    {
      screen->priv->need_update_viewport_settings = TRUE;
      queue_update (screen);
    }
  else if (xevent->xproperty.atom ==
           _wnck_atom_get ("_NET_DESKTOP_GEOMETRY"))
    {
      screen->priv->need_update_viewport_settings = TRUE;
      queue_update (screen);
    }
  else if (xevent->xproperty.atom ==
           _wnck_atom_get ("_NET_NUMBER_OF_DESKTOPS"))
    {
      screen->priv->need_update_workspace_list = TRUE;
      queue_update (screen);
    }
  else if (xevent->xproperty.atom ==
           _wnck_atom_get ("_NET_DESKTOP_LAYOUT"))
    {
      screen->priv->need_update_workspace_layout = TRUE;
      queue_update (screen);
    }
  else if (xevent->xproperty.atom ==
           _wnck_atom_get ("_NET_DESKTOP_NAMES"))
    {
      screen->priv->need_update_workspace_names = TRUE;
      queue_update (screen);
    }
  else if (xevent->xproperty.atom ==
           _wnck_atom_get ("_XROOTPMAP_ID"))
    {
      screen->priv->need_update_bg_pixmap = TRUE;
      queue_update (screen);
    }
  else if (xevent->xproperty.atom ==
           _wnck_atom_get ("_NET_SHOWING_DESKTOP"))
    {
      screen->priv->need_update_showing_desktop = TRUE;
      queue_update (screen);
    }
  else if (xevent->xproperty.atom ==
           _wnck_atom_get ("_NET_SUPPORTING_WM_CHECK"))
    {
      screen->priv->need_update_wm = TRUE;
      queue_update (screen);
    }
}

/**
 * wnck_screen_calc_workspace_layout:
 * @screen: a #WnckScreen.
 * @num_workspaces: the number of #WnckWorkspace on @screen, or -1 to let
 * wnck_screen_calc_workspace_layout() find this number.
 * @space_index: the index of a #WnckWorkspace.
 * @layout: return location for the layout of #WnckWorkspace with additional
 * information.
 *
 * Calculates the layout of #WnckWorkspace, with additional information like
 * the row and column of the #WnckWorkspace with index @space_index.
 *
 * Since: 2.12
 * Deprecated:2.20:
 */
/* TODO: when we make this private, remove num_workspaces since we can get it
 * from screen! */
void
wnck_screen_calc_workspace_layout (WnckScreen          *screen,
                                   int                  num_workspaces,
                                   int                  space_index,
                                   WnckWorkspaceLayout *layout)
{
  int rows, cols;
  int grid_area;
  int *grid;
  int i, r, c;
  int current_row, current_col;

  g_return_if_fail (WNCK_IS_SCREEN (screen));
  g_return_if_fail (layout != NULL);

  if (num_workspaces < 0)
    num_workspaces = wnck_screen_get_workspace_count (screen);

  rows = screen->priv->rows_of_workspaces;
  cols = screen->priv->columns_of_workspaces;

  if (rows <= 0 && cols <= 0)
    cols = num_workspaces;

  if (rows <= 0)
    rows = num_workspaces / cols + ((num_workspaces % cols) > 0 ? 1 : 0);
  if (cols <= 0)
    cols = num_workspaces / rows + ((num_workspaces % rows) > 0 ? 1 : 0);

  /* paranoia */
  if (rows < 1)
    rows = 1;
  if (cols < 1)
    cols = 1;

  g_assert (rows != 0 && cols != 0);

  grid_area = rows * cols;

  grid = g_new (int, grid_area);

  current_row = -1;
  current_col = -1;
  i = 0;

  switch (screen->priv->starting_corner)
    {
    case WNCK_LAYOUT_CORNER_TOPLEFT:
      if (screen->priv->vertical_workspaces)
        {
          c = 0;
          while (c < cols)
            {
              r = 0;
              while (r < rows)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  ++r;
                }
              ++c;
            }
        }
      else
        {
          r = 0;
          while (r < rows)
            {
              c = 0;
              while (c < cols)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  ++c;
                }
              ++r;
            }
        }
      break;
    case WNCK_LAYOUT_CORNER_TOPRIGHT:
      if (screen->priv->vertical_workspaces)
        {
          c = cols - 1;
          while (c >= 0)
            {
              r = 0;
              while (r < rows)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  ++r;
                }
              --c;
            }
        }
      else
        {
          r = 0;
          while (r < rows)
            {
              c = cols - 1;
              while (c >= 0)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  --c;
                }
              ++r;
            }
        }
      break;
    case WNCK_LAYOUT_CORNER_BOTTOMLEFT:
      if (screen->priv->vertical_workspaces)
        {
          c = 0;
          while (c < cols)
            {
              r = rows - 1;
              while (r >= 0)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  --r;
                }
              ++c;
            }
        }
      else
        {
          r = rows - 1;
          while (r >= 0)
            {
              c = 0;
              while (c < cols)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  ++c;
                }
              --r;
            }
        }
      break;
    case WNCK_LAYOUT_CORNER_BOTTOMRIGHT:
      if (screen->priv->vertical_workspaces)
        {
          c = cols - 1;
          while (c >= 0)
            {
              r = rows - 1;
              while (r >= 0)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  --r;
                }
              --c;
            }
        }
      else
        {
          r = rows - 1;
          while (r >= 0)
            {
              c = cols - 1;
              while (c >= 0)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  --c;
                }
              --r;
            }
        }
      break;
    }

  current_row = 0;
  current_col = 0;
  r = 0;
  while (r < rows)
    {
      c = 0;
      while (c < cols)
        {
          if (grid[r*cols+c] == space_index)
            {
              current_row = r;
              current_col = c;
            }
          else if (grid[r*cols+c] >= num_workspaces)
            {
              /* flag nonexistent spaces with -1 */
              grid[r*cols+c] = -1;
            }
          ++c;
        }
      ++r;
    }
  layout->rows = rows;
  layout->cols = cols;
  layout->grid = grid;
  layout->grid_area = grid_area;
  layout->current_row = current_row;
  layout->current_col = current_col;
}

/**
 * wnck_screen_free_workspace_layout:
 * @layout: a #WnckWorkspaceLayout.
 *
 * Frees the content of @layout. This does not free @layout itself, so you
 * might want to free @layout yourself after calling this.
 *
 * Since: 2.12
 * Deprecated:2.20:
 */
void
wnck_screen_free_workspace_layout (WnckWorkspaceLayout *layout)
{
  g_return_if_fail (layout != NULL);

  g_free (layout->grid);
}

static void
set_active_window (WnckScreen *screen,
                   WnckWindow *window)
{
  gpointer weak_pointer;

  weak_pointer = &screen->priv->active_window;

  /* we need the weak pointer since the active window might be shared between
   * two screens, and so the value for one screen might become invalid when
   * the window is destroyed on another screen */
  if (screen->priv->active_window != NULL)
    g_object_remove_weak_pointer (G_OBJECT (screen->priv->active_window),
                                  weak_pointer);

  screen->priv->active_window = window;
  if (screen->priv->active_window != NULL)
    g_object_add_weak_pointer (G_OBJECT (screen->priv->active_window),
                               weak_pointer);
}

static void
set_previously_active_window (WnckScreen *screen,
                              WnckWindow *window)
{
  gpointer weak_pointer;

  weak_pointer = &screen->priv->previously_active_window;

  /* we need the weak pointer since the active window might be shared between
   * two screens, and so the value for one screen might become invalid when
   * the window is destroyed on another screen */
  if (screen->priv->previously_active_window != NULL)
    g_object_remove_weak_pointer (G_OBJECT (screen->priv->previously_active_window),
                                  weak_pointer);

  screen->priv->previously_active_window = window;
  if (screen->priv->previously_active_window != NULL)
    g_object_add_weak_pointer (G_OBJECT (screen->priv->previously_active_window),
                               weak_pointer);
}

static gboolean
lists_equal (GList *a,
             GList *b)
{
  GList *a_iter;
  GList *b_iter;

  a_iter = a;
  b_iter = b;

  while (a_iter && b_iter)
    {
      if (a_iter->data != b_iter->data)
        return FALSE;

      a_iter = a_iter->next;
      b_iter = b_iter->next;
    }

  if (a_iter || b_iter)
    return FALSE;

  return TRUE;
}

static int
wincmp (const void *a,
        const void *b)
{
  const Window *aw = a;
  const Window *bw = b;

  if (*aw < *bw)
    return -1;
  else if (*aw > *bw)
    return 1;
  else
    return 0;
}

static gboolean
arrays_contain_same_windows (Window *a,
                             int     a_len,
                             Window *b,
                             int     b_len)
{
  Window *a_tmp;
  Window *b_tmp;
  gboolean result;

  if (a_len != b_len)
    return FALSE;

  if (a_len == 0)
    return TRUE; /* both are empty */

  a_tmp = g_new (Window, a_len);
  b_tmp = g_new (Window, b_len);

  memcpy (a_tmp, a, a_len * sizeof (Window));
  memcpy (b_tmp, b, b_len * sizeof (Window));

  qsort (a_tmp, a_len, sizeof (Window), wincmp);
  qsort (b_tmp, b_len, sizeof (Window), wincmp);

  result = memcmp (a_tmp, b_tmp, sizeof (Window) * a_len) == 0;

  g_free (a_tmp);
  g_free (b_tmp);

  return result;
}

static void
update_client_list (WnckScreen *screen)
{
  /* stacking order */
  Window *stack;
  int stack_length;
  /* mapping order */
  Window *mapping;
  int mapping_length;
  GList *new_stack_list;
  GList *new_list;
  GList *created;
  GList *closed;
  GList *created_apps, *closed_apps;
  GList *created_class_groups, *closed_class_groups;
  GList *tmp;
  int i;
  GHashTable *new_hash;
  static int reentrancy_guard = 0;
  gboolean active_changed;
  gboolean stack_changed;
  gboolean list_changed;

  g_return_if_fail (reentrancy_guard == 0);

  if (!screen->priv->need_update_stack_list)
    return;

  ++reentrancy_guard;

  screen->priv->need_update_stack_list = FALSE;

  stack = NULL;
  stack_length = 0;
  _wnck_get_window_list (screen->priv->xscreen,
                         screen->priv->xroot,
                         _wnck_atom_get ("_NET_CLIENT_LIST_STACKING"),
                         &stack,
                         &stack_length);

  mapping = NULL;
  mapping_length = 0;
  _wnck_get_window_list (screen->priv->xscreen,
                         screen->priv->xroot,
                         _wnck_atom_get ("_NET_CLIENT_LIST"),
                         &mapping,
                         &mapping_length);

  if (!arrays_contain_same_windows (stack, stack_length,
                                    mapping, mapping_length))
    {
      /* Don't update until we're in a consistent state */
      g_free (stack);
      g_free (mapping);
      --reentrancy_guard;
      return;
    }

  created = NULL;
  closed = NULL;
  created_apps = NULL;
  closed_apps = NULL;
  created_class_groups = NULL;
  closed_class_groups = NULL;

  new_hash = g_hash_table_new (NULL, NULL);

  new_list = NULL;
  i = 0;
  while (i < mapping_length)
    {
      WnckWindow *window;

      window = wnck_window_get (mapping[i]);

      if (window == NULL)
        {
          Window leader;
          WnckApplication *app;
	  const char *res_class;
	  WnckClassGroup *class_group;

          window = _wnck_window_create (mapping[i],
                                        screen,
                                        screen->priv->window_order++);

          created = g_list_prepend (created, window);

	  /* Application */

          leader = wnck_window_get_group_leader (window);

          app = wnck_application_get (leader);
          if (app == NULL)
            {
              app = _wnck_application_create (leader, screen);
              created_apps = g_list_prepend (created_apps, app);
            }

          _wnck_application_add_window (app, window);

	  /* Class group */

	  res_class = wnck_window_get_class_group_name (window);

	  class_group = wnck_class_group_get (res_class);
	  if (class_group == NULL)
	    {
	      class_group = _wnck_class_group_create (res_class);
	      created_class_groups = g_list_prepend (created_class_groups, class_group);
	    }

	  _wnck_class_group_add_window (class_group, window);
        }

      new_list = g_list_prepend (new_list, window);

      g_hash_table_insert (new_hash, window, window);

      ++i;
    }

  /* put list back in order */
  new_list = g_list_reverse (new_list);

  /* Now we need to find windows in the old list that aren't
   * in this new list
   */
  tmp = screen->priv->mapped_windows;
  while (tmp != NULL)
    {
      WnckWindow *window = tmp->data;

      if (g_hash_table_lookup (new_hash, window) == NULL)
        {
          WnckApplication *app;
	  WnckClassGroup *class_group;

          closed = g_list_prepend (closed, window);

	  /* Remove from the app */

          app = wnck_window_get_application (window);
          _wnck_application_remove_window (app, window);

          if (wnck_application_get_windows (app) == NULL)
            closed_apps = g_list_prepend (closed_apps, app);

	  /* Remove from the class group */

          class_group = wnck_window_get_class_group (window);
          _wnck_class_group_remove_window (class_group, window);

          if (wnck_class_group_get_windows (class_group) == NULL)
            closed_class_groups = g_list_prepend (closed_class_groups, class_group);
        }

      tmp = tmp->next;
    }

  g_hash_table_destroy (new_hash);

  /* Now get the mapping in list form */
  new_stack_list = NULL;
  i = 0;
  while (i < stack_length)
    {
      WnckWindow *window;

      window = wnck_window_get (stack[i]);

      g_assert (window != NULL);

      new_stack_list = g_list_prepend (new_stack_list, window);

      ++i;
    }

  g_free (stack);
  g_free (mapping);

  /* put list back in order */
  new_stack_list = g_list_reverse (new_stack_list);

  /* Now new_stack_list becomes screen->priv->stack_windows, new_list
   * becomes screen->priv->mapped_windows, and we emit the opened/closed
   * signals as appropriate
   */

  stack_changed = !lists_equal (screen->priv->stacked_windows, new_stack_list);
  list_changed = !lists_equal (screen->priv->mapped_windows, new_list);

  if (!(stack_changed || list_changed))
    {
      g_assert (created == NULL);
      g_assert (closed == NULL);
      g_assert (created_apps == NULL);
      g_assert (closed_apps == NULL);
      g_assert (created_class_groups == NULL);
      g_assert (closed_class_groups == NULL);
      g_list_free (new_stack_list);
      g_list_free (new_list);
      --reentrancy_guard;
      return;
    }

  g_list_free (screen->priv->mapped_windows);
  g_list_free (screen->priv->stacked_windows);
  screen->priv->mapped_windows = new_list;
  screen->priv->stacked_windows = new_stack_list;

  /* Here we could get reentrancy if someone ran the main loop in
   * signal callbacks; though that would be a bit pathological, so we
   * don't handle it, but we do warn about it using reentrancy_guard
   */

  /* Sequence is: class_group_opened, application_opened, window_opened,
   * window_closed, application_closed, class_group_closed. We have to do all
   * window list changes BEFORE doing any other signals, so that any observers
   * have valid state for the window structure before they take further action
   */
  for (tmp = created_class_groups; tmp; tmp = tmp->next)
    emit_class_group_opened (screen, WNCK_CLASS_GROUP (tmp->data));

  for (tmp = created_apps; tmp; tmp = tmp->next)
    emit_application_opened (screen, WNCK_APPLICATION (tmp->data));

  for (tmp = created; tmp; tmp = tmp->next)
    emit_window_opened (screen, WNCK_WINDOW (tmp->data));

  active_changed = FALSE;
  for (tmp = closed; tmp; tmp = tmp->next)
    {
      WnckWindow *window;

      window = WNCK_WINDOW (tmp->data);

      if (window == screen->priv->previously_active_window)
        {
          set_previously_active_window (screen, NULL);
        }

      if (window == screen->priv->active_window)
        {
          set_previously_active_window (screen, screen->priv->active_window);
          set_active_window (screen, NULL);
          active_changed = TRUE;
        }

      emit_window_closed (screen, window);
    }

  for (tmp = closed_apps; tmp; tmp = tmp->next)
    emit_application_closed (screen, WNCK_APPLICATION (tmp->data));

  for (tmp = closed_class_groups; tmp; tmp = tmp->next)
    emit_class_group_closed (screen, WNCK_CLASS_GROUP (tmp->data));

  if (stack_changed)
    emit_window_stacking_changed (screen);

  if (active_changed)
    emit_active_window_changed (screen);

  /* Now free the closed windows */
  for (tmp = closed; tmp; tmp = tmp->next)
    _wnck_window_destroy (WNCK_WINDOW (tmp->data));

  /* Free the closed apps */
  for (tmp = closed_apps; tmp; tmp = tmp->next)
    _wnck_application_destroy (WNCK_APPLICATION (tmp->data));

  /* Free the closed class groups */
  for (tmp = closed_class_groups; tmp; tmp = tmp->next)
    _wnck_class_group_destroy (WNCK_CLASS_GROUP (tmp->data));

  g_list_free (closed);
  g_list_free (created);
  g_list_free (closed_apps);
  g_list_free (created_apps);
  g_list_free (closed_class_groups);
  g_list_free (created_class_groups);

  --reentrancy_guard;

  /* Maybe the active window is now valid if it wasn't */
  if (screen->priv->active_window == NULL)
    {
      screen->priv->need_update_active_window = TRUE;
      queue_update (screen);
    }
}

static void
update_workspace_list (WnckScreen *screen)
{
  int n_spaces;
  int old_n_spaces;
  GList *tmp;
  GList *deleted;
  GList *created;
  static int reentrancy_guard = 0;

  g_return_if_fail (reentrancy_guard == 0);

  if (!screen->priv->need_update_workspace_list)
    return;

  screen->priv->need_update_workspace_list = FALSE;

  ++reentrancy_guard;

  n_spaces = 0;
  if (!_wnck_get_cardinal (screen->priv->xscreen,
                           screen->priv->xroot,
                           _wnck_atom_get ("_NET_NUMBER_OF_DESKTOPS"),
                           &n_spaces))
    n_spaces = 1;

  if (n_spaces <= 0)
    {
      g_warning ("Someone set a weird number of desktops in _NET_NUMBER_OF_DESKTOPS, assuming the value is 1\n");
      n_spaces = 1;
    }

  old_n_spaces = g_list_length (screen->priv->workspaces);

  deleted = NULL;
  created = NULL;

  if (old_n_spaces == n_spaces)
    {
      --reentrancy_guard;
      return; /* nothing changed */
    }
  else if (old_n_spaces > n_spaces)
    {
      /* Need to delete some workspaces */
      deleted = g_list_nth (screen->priv->workspaces, n_spaces);
      if (deleted->prev)
        deleted->prev->next = NULL;
      deleted->prev = NULL;

      if (deleted == screen->priv->workspaces)
        screen->priv->workspaces = NULL;
    }
  else
    {
      int i;

      g_assert (old_n_spaces < n_spaces);

      /* Need to create some workspaces */
      i = 0;
      while (i < (n_spaces - old_n_spaces))
        {
          WnckWorkspace *space;

          space = _wnck_workspace_create (old_n_spaces + i, screen);

          screen->priv->workspaces = g_list_append (screen->priv->workspaces,
                                                    space);

          created = g_list_prepend (created, space);

          ++i;
        }

      created = g_list_reverse (created);
    }

  /* Here we allow reentrancy, going into the main
   * loop could confuse us
   */
  tmp = deleted;
  while (tmp != NULL)
    {
      WnckWorkspace *space = WNCK_WORKSPACE (tmp->data);

      if (space == screen->priv->active_workspace)
        {
          screen->priv->active_workspace = NULL;
          emit_active_workspace_changed (screen, space);
        }

      emit_workspace_destroyed (screen, space);

      tmp = tmp->next;
    }

  tmp = created;
  while (tmp != NULL)
    {
      emit_workspace_created (screen, WNCK_WORKSPACE (tmp->data));

      tmp = tmp->next;
    }
  g_list_free (created);

  tmp = deleted;
  while (tmp != NULL)
    {
      g_object_unref (tmp->data);

      tmp = tmp->next;
    }
  g_list_free (deleted);

  /* Active workspace property may now be interpretable,
   * if it was a number larger than the active count previously
   */
  if (screen->priv->active_workspace == NULL)
    {
      screen->priv->need_update_active_workspace = TRUE;
      queue_update (screen);
    }

  --reentrancy_guard;
}

static void
update_viewport_settings (WnckScreen *screen)
{
  int i, n_spaces;
  WnckWorkspace *space;
  gulong *p_coord;
  int n_coord;
  gboolean do_update;
  int space_width, space_height;
  gboolean got_viewport_prop;

  if (!screen->priv->need_update_viewport_settings)
    return;

  screen->priv->need_update_viewport_settings = FALSE;

  do_update = FALSE;

  n_spaces = wnck_screen_get_workspace_count (screen);

  /* If no property, use the screen's size */
  space_width = wnck_screen_get_width (screen);
  space_height = wnck_screen_get_height (screen);

  p_coord = NULL;
  n_coord = 0;
  if (_wnck_get_cardinal_list (screen->priv->xscreen,
                               screen->priv->xroot,
			       _wnck_atom_get ("_NET_DESKTOP_GEOMETRY"),
                               &p_coord, &n_coord) &&
      p_coord != NULL)
    {
      if (n_coord == 2)
	{
          space_width = p_coord[0];
          space_height = p_coord[1];

          if (space_width < wnck_screen_get_width (screen))
            space_width = wnck_screen_get_width (screen);

          if (space_height < wnck_screen_get_height (screen))
            space_height = wnck_screen_get_height (screen);
	}

      g_free (p_coord);
    }

  for (i = 0; i < n_spaces; i++)
    {
      space = wnck_screen_get_workspace (screen, i);
      g_assert (space != NULL);

      if (_wnck_workspace_set_geometry (space, space_width, space_height))
        do_update = TRUE;
    }

  got_viewport_prop = FALSE;

  p_coord = NULL;
  n_coord = 0;
  if (_wnck_get_cardinal_list (screen->priv->xscreen,
                               screen->priv->xroot,
                               _wnck_atom_get ("_NET_DESKTOP_VIEWPORT"),
                               &p_coord, &n_coord) &&
      p_coord != NULL)
    {
      if (n_coord == 2 * n_spaces)
        {
          int screen_width, screen_height;

          got_viewport_prop = TRUE;

          screen_width = wnck_screen_get_width (screen);
          screen_height = wnck_screen_get_height (screen);

	  for (i = 0; i < n_spaces; i++)
	    {
              int x = 2 * i;
              int y = 2 * i + 1;

              space = wnck_screen_get_workspace (screen, i);
              g_assert (space != NULL);

	      /* p_coord[x] is unsigned, and thus >= 0 */
              if (p_coord[x] > space_width - screen_width)
                p_coord[x] = space_width - screen_width;

	      /* p_coord[y] is unsigned, and thus >= 0 */
              if (p_coord[y] > space_height - screen_height)
                p_coord[y] = space_height - screen_height;

	      if (_wnck_workspace_set_viewport (space,
                                                p_coord[x], p_coord[y]))
                do_update = TRUE;
	    }
	}

      g_free (p_coord);
    }

  if (!got_viewport_prop)
    {
      for (i = 0; i < n_spaces; i++)
        {
          space = wnck_screen_get_workspace (screen, i);
          g_assert (space != NULL);

          if (_wnck_workspace_set_viewport (space, 0, 0))
            do_update = TRUE;
        }
    }

  if (do_update)
    emit_viewports_changed (screen);
}

static void
update_active_workspace (WnckScreen *screen)
{
  int number;
  WnckWorkspace *previous_space;
  WnckWorkspace *space;

  if (!screen->priv->need_update_active_workspace)
    return;

  screen->priv->need_update_active_workspace = FALSE;

  number = 0;
  if (!_wnck_get_cardinal (screen->priv->xscreen,
                           screen->priv->xroot,
                           _wnck_atom_get ("_NET_CURRENT_DESKTOP"),
                           &number))
    number = -1;

  space = wnck_screen_get_workspace (screen, number);

  if (space == screen->priv->active_workspace)
    return;

  previous_space = screen->priv->active_workspace;
  screen->priv->active_workspace = space;

  emit_active_workspace_changed (screen, previous_space);
}

static void
update_active_window (WnckScreen *screen)
{
  WnckWindow *window;
  Window xwindow;

  if (!screen->priv->need_update_active_window)
    return;

  screen->priv->need_update_active_window = FALSE;

  xwindow = None;
  _wnck_get_window (screen->priv->xscreen,
                    screen->priv->xroot,
                    _wnck_atom_get ("_NET_ACTIVE_WINDOW"),
                    &xwindow);

  window = wnck_window_get (xwindow);

  if (window == screen->priv->active_window)
    return;

  set_previously_active_window (screen, screen->priv->active_window);
  set_active_window (screen, window);

  emit_active_window_changed (screen);
}

static void
update_workspace_layout (WnckScreen *screen)
{
  gulong *list;
  int n_items;

  if (!screen->priv->need_update_workspace_layout)
    return;

  screen->priv->need_update_workspace_layout = FALSE;

  list = NULL;
  n_items = 0;
  if (_wnck_get_cardinal_list (screen->priv->xscreen,
                               screen->priv->xroot,
                               _wnck_atom_get ("_NET_DESKTOP_LAYOUT"),
                               &list,
                               &n_items))
    {
      if (n_items == 3 || n_items == 4)
        {
          int cols, rows;

          switch (list[0])
            {
            case _NET_WM_ORIENTATION_HORZ:
              screen->priv->vertical_workspaces = FALSE;
              break;
            case _NET_WM_ORIENTATION_VERT:
              screen->priv->vertical_workspaces = TRUE;
              break;
            default:
              g_warning ("Someone set a weird orientation in _NET_DESKTOP_LAYOUT\n");
              break;
            }

          cols = list[1];
          rows = list[2];

          if (rows <= 0 && cols <= 0)
            {
              g_warning ("Columns = %d rows = %d in _NET_DESKTOP_LAYOUT makes no sense\n", rows, cols);
            }
          else
            {
              int num_workspaces;

              num_workspaces = wnck_screen_get_workspace_count (screen);

              if (rows > 0)
                screen->priv->rows_of_workspaces = rows;
              else
                screen->priv->rows_of_workspaces =
                                      num_workspaces / cols
                                      + ((num_workspaces % cols) > 0 ? 1 : 0);

              if (cols > 0)
                screen->priv->columns_of_workspaces = cols;
              else
                screen->priv->columns_of_workspaces =
                                      num_workspaces / rows
                                      + ((num_workspaces % rows) > 0 ? 1 : 0);
            }
          if (n_items == 4)
            {
              switch (list[3])
                {
                  case _NET_WM_TOPLEFT:
                    screen->priv->starting_corner = WNCK_LAYOUT_CORNER_TOPLEFT;
                    break;
                  case _NET_WM_TOPRIGHT:
                    screen->priv->starting_corner = WNCK_LAYOUT_CORNER_TOPRIGHT;
                    break;
                  case _NET_WM_BOTTOMRIGHT:
                    screen->priv->starting_corner = WNCK_LAYOUT_CORNER_BOTTOMRIGHT;
                    break;
                  case _NET_WM_BOTTOMLEFT:
                    screen->priv->starting_corner = WNCK_LAYOUT_CORNER_BOTTOMLEFT;
                    break;
                  default:
                    g_warning ("Someone set a weird starting corner in _NET_DESKTOP_LAYOUT\n");
                    break;
                }
            }
          else
            screen->priv->starting_corner = WNCK_LAYOUT_CORNER_TOPLEFT;
        }
      else
        {
          g_warning ("Someone set _NET_DESKTOP_LAYOUT to %d integers instead of 4 (3 is accepted for backwards compat)\n", n_items);
        }
      g_free (list);
    }
}

static void
update_workspace_names (WnckScreen *screen)
{
  char **names;
  int i;
  GList *tmp;
  GList *copy;

  if (!screen->priv->need_update_workspace_names)
    return;

  screen->priv->need_update_workspace_names = FALSE;

  names = _wnck_get_utf8_list (screen->priv->xscreen,
                               screen->priv->xroot,
                               _wnck_atom_get ("_NET_DESKTOP_NAMES"));

  copy = g_list_copy (screen->priv->workspaces);

  i = 0;
  tmp = copy;
  while (tmp != NULL)
    {
      if (names && names[i])
        {
          _wnck_workspace_update_name (tmp->data, names[i]);
          ++i;
        }
      else
        _wnck_workspace_update_name (tmp->data, NULL);

      tmp = tmp->next;
    }

  g_strfreev (names);

  g_list_free (copy);
}

static void
update_bg_pixmap (WnckScreen *screen)
{
  Pixmap p;

  if (!screen->priv->need_update_bg_pixmap)
    return;

  screen->priv->need_update_bg_pixmap = FALSE;

  p = None;
  _wnck_get_pixmap (screen->priv->xscreen,
                    screen->priv->xroot,
                    _wnck_atom_get ("_XROOTPMAP_ID"),
                    &p);
  /* may have failed, so p may still be None */

  screen->priv->bg_pixmap = p;

  emit_background_changed (screen);
}

static void
update_showing_desktop (WnckScreen *screen)
{
  int showing_desktop;

  if (!screen->priv->need_update_showing_desktop)
    return;

  screen->priv->need_update_showing_desktop = FALSE;

  showing_desktop = FALSE;
  _wnck_get_cardinal (screen->priv->xscreen,
                      screen->priv->xroot,
                      _wnck_atom_get ("_NET_SHOWING_DESKTOP"),
                      &showing_desktop);

  screen->priv->showing_desktop = showing_desktop != 0;

  emit_showing_desktop_changed (screen);
}

static void
update_wm (WnckScreen *screen)
{
  Window wm_window;

  if (!screen->priv->need_update_wm)
    return;

  screen->priv->need_update_wm = FALSE;

  wm_window = None;
  _wnck_get_window (screen->priv->xscreen,
                    screen->priv->xroot,
                    _wnck_atom_get ("_NET_SUPPORTING_WM_CHECK"),
                    &wm_window);

  g_free (screen->priv->wm_name);

  if (wm_window != None)
    screen->priv->wm_name = _wnck_get_utf8_property (screen->priv->xscreen,
                                                     wm_window,
                                                     _wnck_atom_get ("_NET_WM_NAME"));
  else
    screen->priv->wm_name = NULL;

  emit_wm_changed (screen);
}

static void
do_update_now (WnckScreen *screen)
{
  if (screen->priv->update_handler)
    {
      g_source_remove (screen->priv->update_handler);
      screen->priv->update_handler = 0;
    }

  /* if number of workspaces changes, we have to
   * update the per-workspace information as well
   * in case the WM changed the per-workspace info
   * first and number of spaces second.
   */
  if (screen->priv->need_update_workspace_list)
    {
      screen->priv->need_update_viewport_settings = TRUE;
      screen->priv->need_update_workspace_names = TRUE;
    }

  /* First get our big-picture state in order */
  update_workspace_list (screen);
  update_client_list (screen);

  /* Then note any smaller-scale changes */
  update_active_workspace (screen);
  update_viewport_settings (screen);
  update_active_window (screen);
  update_workspace_layout (screen);
  update_workspace_names (screen);
  update_showing_desktop (screen);
  update_wm (screen);

  update_bg_pixmap (screen);
}

static gboolean
update_idle (gpointer data)
{
  WnckScreen *screen;

  screen = data;

  screen->priv->update_handler = 0;

  do_update_now (screen);

  return FALSE;
}

static void
queue_update (WnckScreen *screen)
{
  if (screen->priv->update_handler != 0)
    return;

  screen->priv->update_handler = g_idle_add (update_idle, screen);
}

static void
unqueue_update (WnckScreen *screen)
{
  if (screen->priv->update_handler != 0)
    {
      g_source_remove (screen->priv->update_handler);
      screen->priv->update_handler = 0;
    }
}

static void
emit_active_window_changed (WnckScreen *screen)
{
  g_signal_emit (G_OBJECT (screen),
                 signals[ACTIVE_WINDOW_CHANGED],
                 0, screen->priv->previously_active_window);
}

static void
emit_active_workspace_changed (WnckScreen    *screen,
                               WnckWorkspace *previous_space)
{
  g_signal_emit (G_OBJECT (screen),
                 signals[ACTIVE_WORKSPACE_CHANGED],
                 0, previous_space);
}

static void
emit_window_stacking_changed (WnckScreen *screen)
{
  g_signal_emit (G_OBJECT (screen),
                 signals[WINDOW_STACKING_CHANGED],
                 0);
}

static void
emit_window_opened (WnckScreen *screen,
                    WnckWindow *window)
{
  g_signal_emit (G_OBJECT (screen),
                 signals[WINDOW_OPENED],
                 0, window);
}

static void
emit_window_closed (WnckScreen *screen,
                    WnckWindow *window)
{
  g_signal_emit (G_OBJECT (screen),
                 signals[WINDOW_CLOSED],
                 0, window);
}

static void
emit_workspace_created (WnckScreen    *screen,
                        WnckWorkspace *space)
{
  g_signal_emit (G_OBJECT (screen),
                 signals[WORKSPACE_CREATED],
                 0, space);
}

static void
emit_workspace_destroyed (WnckScreen    *screen,
                          WnckWorkspace *space)
{
  g_signal_emit (G_OBJECT (screen),
                 signals[WORKSPACE_DESTROYED],
                 0, space);
}

static void
emit_application_opened (WnckScreen      *screen,
                         WnckApplication *app)
{
  g_signal_emit (G_OBJECT (screen),
                 signals[APPLICATION_OPENED],
                 0, app);
}

static void
emit_application_closed (WnckScreen      *screen,
                         WnckApplication *app)
{
  g_signal_emit (G_OBJECT (screen),
                 signals[APPLICATION_CLOSED],
                 0, app);
}

static void
emit_class_group_opened (WnckScreen     *screen,
                         WnckClassGroup *class_group)
{
  g_signal_emit (G_OBJECT (screen),
                 signals[CLASS_GROUP_OPENED],
                 0, class_group);
}

static void
emit_class_group_closed (WnckScreen     *screen,
                         WnckClassGroup *class_group)
{
  g_signal_emit (G_OBJECT (screen),
                 signals[CLASS_GROUP_CLOSED],
                 0, class_group);
}

static void
emit_background_changed (WnckScreen *screen)
{
  g_signal_emit (G_OBJECT (screen),
                 signals[BACKGROUND_CHANGED],
                 0);
}

static void
emit_showing_desktop_changed (WnckScreen *screen)
{
  g_signal_emit (G_OBJECT (screen),
                 signals[SHOWING_DESKTOP_CHANGED],
                 0);
}

static void
emit_viewports_changed (WnckScreen *screen)
{
  g_signal_emit (G_OBJECT (screen),
                 signals[VIEWPORTS_CHANGED],
                 0);
}

static void
emit_wm_changed (WnckScreen *screen)
{
  g_signal_emit (G_OBJECT (screen),
                 signals[WM_CHANGED],
                 0);
}

/**
 * wnck_screen_get_window_manager_name:
 * @screen: a #WnckScreen.
 *
 * Gets the name of the window manager.
 *
 * Return value: the name of the window manager, or %NULL if the window manager
 * does not comply with the <ulink
 * url="http://standards.freedesktop.org/wm-spec/wm-spec-latest.html">EWMH</ulink>
 * specification.
 *
 * Since: 2.20
 */
const char *
wnck_screen_get_window_manager_name (WnckScreen *screen)
{
  g_return_val_if_fail (WNCK_IS_SCREEN (screen), NULL);

  return screen->priv->wm_name;
}

/**
 * wnck_screen_net_wm_supports:
 * @screen: a #WnckScreen.
 * @atom: a property atom.
 *
 * Gets whether the window manager for @screen supports a certain hint from
 * the <ulink
 * url="http://standards.freedesktop.org/wm-spec/wm-spec-latest.html">Extended
 * Window Manager Hints specification</ulink> (EWMH).
 *
 * When using this function, keep in mind that the window manager can change
 * over time; so you should not use this function in a way that impacts
 * persistent application state. A common bug is that your application can
 * start up before the window manager does when the user logs in, and before
 * the window manager starts wnck_screen_net_wm_supports() will return %FALSE
 * for every property.
 *
 * See also gdk_x11_screen_supports_net_wm_hint() in GDK.
 *
 * Return value: %TRUE if the window manager for @screen supports the @atom
 * hint, %FALSE otherwise.
 */
gboolean
wnck_screen_net_wm_supports (WnckScreen *screen,
                             const char *atom)
{
  g_return_val_if_fail (WNCK_IS_SCREEN (screen), FALSE);

  return gdk_x11_screen_supports_net_wm_hint (_wnck_screen_get_gdk_screen (screen),
                                              gdk_atom_intern (atom, FALSE));
}

/**
 * wnck_screen_get_background_pixmap:
 * @screen: a #WnckScreen.
 *
 * Gets the X window ID of the background pixmap of @screen.
 *
 * Returns: the X window ID of the background pixmap of @screen.
 */
gulong
wnck_screen_get_background_pixmap (WnckScreen *screen)
{
  g_return_val_if_fail (WNCK_IS_SCREEN (screen), None);

  return screen->priv->bg_pixmap;
}

/**
 * wnck_screen_get_width:
 * @screen: a #WnckScreen.
 *
 * Gets the width of @screen.
 *
 * Returns: the width of @screen.
 */
int
wnck_screen_get_width (WnckScreen *screen)
{
  g_return_val_if_fail (WNCK_IS_SCREEN (screen), 0);

  return WidthOfScreen (screen->priv->xscreen);
}

/**
 * wnck_screen_get_height:
 * @screen: a #WnckScreen.
 *
 * Gets the height of @screen.
 *
 * Returns: the height of @screen.
 */
int
wnck_screen_get_height (WnckScreen *screen)
{
  g_return_val_if_fail (WNCK_IS_SCREEN (screen), 0);

  return HeightOfScreen (screen->priv->xscreen);
}

Screen *
_wnck_screen_get_xscreen (WnckScreen *screen)
{
  return screen->priv->xscreen;
}

/**
 * wnck_screen_get_workspace_layout:
 * @screen: a #WnckScreen.
 * @orientation: return location for the orientation used in the #WnckWorkspace
 * layout. See wnck_pager_set_orientation() for more information.
 * @rows: return location for the number of rows in the #WnckWorkspace layout.
 * @columns: return location for the number of columns in the #WnckWorkspace
 * layout.
 * @starting_corner: return location for the starting corner in the
 * #WnckWorkspace layout (i.e. the corner containing the first #WnckWorkspace).
 *
 * Gets the layout of #WnckWorkspace on @screen.
 */
/* TODO: when we are sure about this API, add this function,
 * WnckLayoutOrientation, WnckLayoutCorner and a "layout-changed" signal. But
 * to make it really better, use a WnckScreenLayout struct. We might also want
 * to wait for deprecation of WnckWorkspaceLayout. */
void
_wnck_screen_get_workspace_layout (WnckScreen             *screen,
                                   _WnckLayoutOrientation *orientation,
                                   int                    *rows,
                                   int                    *columns,
                                   _WnckLayoutCorner      *starting_corner)
{
  g_return_if_fail (WNCK_IS_SCREEN (screen));

  if (orientation)
    *orientation = screen->priv->vertical_workspaces ?
                       WNCK_LAYOUT_ORIENTATION_VERTICAL :
                       WNCK_LAYOUT_ORIENTATION_HORIZONTAL;

  if (rows)
    *rows = screen->priv->rows_of_workspaces;

  if (columns)
    *columns = screen->priv->columns_of_workspaces;

  if (starting_corner)
    *starting_corner = screen->priv->starting_corner;
}

/**
 * wnck_screen_try_set_workspace_layout:
 * @screen: a #WnckScreen.
 * @current_token: a token. Use 0 if you do not called
 * wnck_screen_try_set_workspace_layout() before, or if you did not keep the
 * old token.
 * @rows: the number of rows to use for the #WnckWorkspace layout.
 * @columns: the number of columns to use for the #WnckWorkspace layout.
 *
 * Tries to modify the layout of #WnckWorkspace on @screen. To do this, tries
 * to acquire ownership of the layout. If the current process is the owner of
 * the layout, @current_token is used to determine if the caller is the owner
 * (there might be more than one part of the same process trying to set the
 * layout). Since no more than one application should set this property of
 * @screen at a time, setting the layout is not guaranteed to work.
 *
 * If @rows is 0, the actual number of rows will be determined based on
 * @columns and the number of #WnckWorkspace. If @columns is 0, the actual
 * number of columns will be determined based on @rows and the number of
 * #WnckWorkspace. @rows and @columns must not be 0 at the same time.
 *
 * You have to release the ownership of the layout with
 * wnck_screen_release_workspace_layout() when you do not need it anymore.
 *
 * Return value: a token to use for future calls to
 * wnck_screen_try_set_workspace_layout() and to
 * wnck_screen_release_workspace_layout(), or 0 if the layout could not be set.
 */
int
wnck_screen_try_set_workspace_layout (WnckScreen *screen,
                                      int         current_token,
                                      int         rows,
                                      int         columns)
{
  int retval;

  g_return_val_if_fail (WNCK_IS_SCREEN (screen),
                        WNCK_NO_MANAGER_TOKEN);
  g_return_val_if_fail (rows != 0 || columns != 0,
                        WNCK_NO_MANAGER_TOKEN);

  retval = _wnck_try_desktop_layout_manager (screen->priv->xscreen, current_token);

  if (retval != WNCK_NO_MANAGER_TOKEN)
    {
      _wnck_set_desktop_layout (screen->priv->xscreen, rows, columns);
    }

  return retval;
}

/**
 * wnck_screen_release_workspace_layout:
 * @screen: a #WnckScreen.
 * @current_token: the token obtained through
 * wnck_screen_try_set_workspace_layout().
 *
 * Releases the ownership of the layout of #WnckWorkspace on @screen.
 * @current_token is used to verify that the caller is the owner of the layout.
 * If the verification fails, nothing happens.
 */
void
wnck_screen_release_workspace_layout (WnckScreen *screen,
                                      int         current_token)
{
  g_return_if_fail (WNCK_IS_SCREEN (screen));

  _wnck_release_desktop_layout_manager (screen->priv->xscreen,
                                        current_token);

}

/**
 * wnck_screen_get_showing_desktop:
 * @screen: a #WnckScreen.
 *
 * Gets whether @screen is in the "showing the desktop" mode. This mode is
 * changed when a #WnckScreen::showing-desktop-changed signal gets emitted.
 *
 * Return value: %TRUE if @window is fullscreen, %FALSE otherwise.
 *
 * Since: 2.2
 **/
gboolean
wnck_screen_get_showing_desktop (WnckScreen *screen)
{
  g_return_val_if_fail (WNCK_IS_SCREEN (screen), FALSE);

  return screen->priv->showing_desktop;
}

/**
 * wnck_screen_toggle_showing_desktop:
 * @screen: a #WnckScreen.
 * @show: whether to activate the "showing the desktop" mode on @screen.
 *
 * Asks the window manager to set the "showing the desktop" mode on @screen
 * according to @show.
 *
 * Since: 2.2
 **/
void
wnck_screen_toggle_showing_desktop (WnckScreen *screen,
                                    gboolean    show)
{
  g_return_if_fail (WNCK_IS_SCREEN (screen));

  _wnck_toggle_showing_desktop (screen->priv->xscreen,
                                show);
}


/**
 * wnck_screen_move_viewport:
 * @screen: a #WnckScreen.
 * @x: X offset in pixels of viewport.
 * @y: Y offset in pixels of viewport.
 *
 * Asks the window manager to move the viewport of the current #WnckWorkspace
 * on @screen.
 *
 * Since: 2.4
 */
void
wnck_screen_move_viewport (WnckScreen *screen,
                           int         x,
                           int         y)
{
  g_return_if_fail (WNCK_IS_SCREEN (screen));
  g_return_if_fail (x >= 0);
  g_return_if_fail (y >= 0);

  _wnck_change_viewport (screen->priv->xscreen, x, y);
}

#ifdef HAVE_STARTUP_NOTIFICATION
SnDisplay*
_wnck_screen_get_sn_display (WnckScreen *screen)
{
  g_return_val_if_fail (WNCK_IS_SCREEN (screen), NULL);

  return screen->priv->sn_display;
}
#endif /* HAVE_STARTUP_NOTIFICATION */

void
_wnck_screen_change_workspace_name (WnckScreen *screen,
                                    int         number,
                                    const char *name)
{
  int n_spaces;
  char **names;
  int i;

  n_spaces = wnck_screen_get_workspace_count (screen);

  names = g_new0 (char*, n_spaces + 1);

  i = 0;
  while (i < n_spaces)
    {
      if (i == number)
        names[i] = (char*) name;
      else
        {
          WnckWorkspace *workspace;
          workspace = wnck_screen_get_workspace (screen, i);
          if (workspace)
            names[i] = (char*) wnck_workspace_get_name (workspace);
          else
            names[i] = (char*) ""; /* maybe this should be a g_warning() */
        }

      ++i;
    }

  _wnck_set_utf8_list (screen->priv->xscreen,
                       screen->priv->xroot,
                       _wnck_atom_get ("_NET_DESKTOP_NAMES"),
                       names);

  g_free (names);
}

void
_wnck_screen_shutdown_all (void)
{
  int i;
  Display *display;

  if (screens == NULL)
    return;

  display = _wnck_get_default_display ();

  for (i = 0; i < ScreenCount (display); ++i)
    {
      if (screens[i] != NULL) {
        g_object_unref (screens[i]);
        screens[i] = NULL;
      }
    }

  g_free (screens);
  screens = NULL;
}
