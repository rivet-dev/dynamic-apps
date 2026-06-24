/* window object */
/* vim: set sw=2 et: */

/*
 * Copyright (C) 2001 Havoc Pennington
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

#if !defined (__LIBWNCK_H_INSIDE__) && !defined (WNCK_COMPILATION)
#error "Only <libwnck/libwnck.h> can be included directly."
#endif

#ifndef WNCK_WINDOW_H
#define WNCK_WINDOW_H

#ifndef WNCK_I_KNOW_THIS_IS_UNSTABLE
#error "libwnck should only be used if you understand that it's subject to frequent change, and is not supported as a fixed API/ABI or as part of the platform"
#endif

#include <glib-object.h>
#include <libwnck/screen.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

/**
 * WnckWindowState:
 * @WNCK_WINDOW_STATE_MINIMIZED: the window is minimized.
 * @WNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY: the window is horizontically
 * maximized.
 * @WNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY: the window is vertically maximized.
 * @WNCK_WINDOW_STATE_SHADED: the window is shaded.
 * @WNCK_WINDOW_STATE_SKIP_PAGER: the window should not be included on pagers.
 * @WNCK_WINDOW_STATE_SKIP_TASKLIST: the window should not be included on
 * tasklists.
 * @WNCK_WINDOW_STATE_STICKY: the window is sticky (see
 * wnck_window_is_sticky()).
 * @WNCK_WINDOW_STATE_HIDDEN: the window is not visible on its #WnckWorkspace
 * and viewport (when minimized, for example).
 * @WNCK_WINDOW_STATE_FULLSCREEN: the window is fullscreen.
 * @WNCK_WINDOW_STATE_DEMANDS_ATTENTION: the window needs attention (because
 * the window requested activation but the window manager refused it, for
 * example).
 * @WNCK_WINDOW_STATE_URGENT: the window requires a response from the user.
 * @WNCK_WINDOW_STATE_ABOVE: the window is above other windows (see
 * wnck_window_make_above()).
 * @WNCK_WINDOW_STATE_BELOW: the window is below other windows (see
 * wnck_window_make_below()).
 *
 * Type used as a bitmask to describe the state of a #WnckWindow.
 */
typedef enum
{
  WNCK_WINDOW_STATE_MINIMIZED              = 1 << 0,
  WNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY = 1 << 1,
  WNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY   = 1 << 2,
  WNCK_WINDOW_STATE_SHADED                 = 1 << 3,
  WNCK_WINDOW_STATE_SKIP_PAGER             = 1 << 4,
  WNCK_WINDOW_STATE_SKIP_TASKLIST          = 1 << 5,
  WNCK_WINDOW_STATE_STICKY                 = 1 << 6,
  WNCK_WINDOW_STATE_HIDDEN                 = 1 << 7,
  WNCK_WINDOW_STATE_FULLSCREEN             = 1 << 8,
  WNCK_WINDOW_STATE_DEMANDS_ATTENTION      = 1 << 9,
  WNCK_WINDOW_STATE_URGENT                 = 1 << 10,
  WNCK_WINDOW_STATE_ABOVE                  = 1 << 11,
  WNCK_WINDOW_STATE_BELOW                  = 1 << 12
} WnckWindowState;

/**
 * WnckWindowActions:
 * @WNCK_WINDOW_ACTION_MOVE: the window may be moved around the screen. 
 * @WNCK_WINDOW_ACTION_RESIZE: the window may be resized.
 * @WNCK_WINDOW_ACTION_SHADE: the window may be shaded.
 * @WNCK_WINDOW_ACTION_STICK: the window may be sticked.
 * @WNCK_WINDOW_ACTION_MAXIMIZE_HORIZONTALLY: the window may be maximized
 * horizontally.
 * @WNCK_WINDOW_ACTION_MAXIMIZE_VERTICALLY: the window may be maximized
 * vertically.
 * @WNCK_WINDOW_ACTION_CHANGE_WORKSPACE: the window may be moved between
 * workspaces, or (un)pinned.
 * @WNCK_WINDOW_ACTION_CLOSE: the window may be closed.
 * @WNCK_WINDOW_ACTION_UNMAXIMIZE_HORIZONTALLY: the window may be unmaximized
 * horizontally.
 * @WNCK_WINDOW_ACTION_UNMAXIMIZE_VERTICALLY: the window may be maximized
 * vertically.
 * @WNCK_WINDOW_ACTION_UNSHADE: the window may be unshaded.
 * @WNCK_WINDOW_ACTION_UNSTICK: the window may be unsticked.
 * @WNCK_WINDOW_ACTION_MINIMIZE: the window may be minimized.
 * @WNCK_WINDOW_ACTION_UNMINIMIZE: the window may be unminimized.
 * @WNCK_WINDOW_ACTION_MAXIMIZE: the window may be maximized.
 * @WNCK_WINDOW_ACTION_UNMAXIMIZE: the window may be unmaximized.
 * @WNCK_WINDOW_ACTION_FULLSCREEN: the window may be brought to fullscreen.
 * @WNCK_WINDOW_ACTION_ABOVE: the window may be made above other windows.
 * @WNCK_WINDOW_ACTION_BELOW: the window may be made below other windows.
 *
 * Type used as a bitmask to describe the actions that can be done for a
 * #WnckWindow.
 */
typedef enum
{
  WNCK_WINDOW_ACTION_MOVE                    = 1 << 0,
  WNCK_WINDOW_ACTION_RESIZE                  = 1 << 1,
  WNCK_WINDOW_ACTION_SHADE                   = 1 << 2,
  WNCK_WINDOW_ACTION_STICK                   = 1 << 3,
  WNCK_WINDOW_ACTION_MAXIMIZE_HORIZONTALLY   = 1 << 4,
  WNCK_WINDOW_ACTION_MAXIMIZE_VERTICALLY     = 1 << 5,
  WNCK_WINDOW_ACTION_CHANGE_WORKSPACE        = 1 << 6, /* includes pin/unpin */
  WNCK_WINDOW_ACTION_CLOSE                   = 1 << 7,
  WNCK_WINDOW_ACTION_UNMAXIMIZE_HORIZONTALLY = 1 << 8,
  WNCK_WINDOW_ACTION_UNMAXIMIZE_VERTICALLY   = 1 << 9,
  WNCK_WINDOW_ACTION_UNSHADE                 = 1 << 10,
  WNCK_WINDOW_ACTION_UNSTICK                 = 1 << 11,
  WNCK_WINDOW_ACTION_MINIMIZE                = 1 << 12,
  WNCK_WINDOW_ACTION_UNMINIMIZE              = 1 << 13,
  WNCK_WINDOW_ACTION_MAXIMIZE                = 1 << 14,
  WNCK_WINDOW_ACTION_UNMAXIMIZE              = 1 << 15,
  WNCK_WINDOW_ACTION_FULLSCREEN              = 1 << 16,
  WNCK_WINDOW_ACTION_ABOVE                   = 1 << 17,
  WNCK_WINDOW_ACTION_BELOW                   = 1 << 18
} WnckWindowActions;

/**
 * WnckWindowType:
 * @WNCK_WINDOW_NORMAL: the window is a normal window.
 * @WNCK_WINDOW_DESKTOP: the window is a desktop.
 * @WNCK_WINDOW_DOCK: the window is a dock or a panel.
 * @WNCK_WINDOW_DIALOG: the window is a dialog window.
 * @WNCK_WINDOW_TOOLBAR: the window is a tearoff toolbar.
 * @WNCK_WINDOW_MENU: the window is a tearoff menu.
 * @WNCK_WINDOW_UTILITY: the window is a small persistent utility window, such
 * as a palette or toolbox.
 * @WNCK_WINDOW_SPLASHSCREEN: the window is a splash screen displayed as an
 * application is starting up.
 *
 * Type describing the semantic type of a #WnckWindow.
 */
typedef enum
{
  WNCK_WINDOW_NORMAL,       /* document/app window */
  WNCK_WINDOW_DESKTOP,      /* desktop background */
  WNCK_WINDOW_DOCK,         /* panel */
  WNCK_WINDOW_DIALOG,       /* dialog */
  WNCK_WINDOW_TOOLBAR,      /* tearoff toolbar */
  WNCK_WINDOW_MENU,         /* tearoff menu */
  WNCK_WINDOW_UTILITY,      /* palette/toolbox window */
  WNCK_WINDOW_SPLASHSCREEN  /* splash screen */
} WnckWindowType;

/**
 * WnckWindowGravity:
 * @WNCK_WINDOW_GRAVITY_CURRENT: keep the current gravity point.
 * @WNCK_WINDOW_GRAVITY_NORTHWEST: use the left top corner of the frame window
 * as gravity point.
 * @WNCK_WINDOW_GRAVITY_NORTH: use the center of the frame window's top side as
 * gravity point.
 * @WNCK_WINDOW_GRAVITY_NORTHEAST: use the right top corner of the frame window
 * as gravity point.
 * @WNCK_WINDOW_GRAVITY_WEST: use the center of the frame window's left side as
 * gravity point.
 * @WNCK_WINDOW_GRAVITY_CENTER: use the center of the frame window as gravity
 * point.
 * @WNCK_WINDOW_GRAVITY_EAST: use the center of the frame window's right side
 * as gravity point.
 * @WNCK_WINDOW_GRAVITY_SOUTHWEST: use the left bottom corner of the frame
 * window as gravity point.
 * @WNCK_WINDOW_GRAVITY_SOUTH: use the center of the frame window's bottom side
 * as gravity point.
 * @WNCK_WINDOW_GRAVITY_SOUTHEAST: use the right bottom corner of the frame
 * window as gravity point.
 * @WNCK_WINDOW_GRAVITY_STATIC: use the left top corner of the client window as
 * gravity point.
 *
 * Flag used when changing the geometry of a #WnckWindow. This is the gravity
 * point to use as a reference for the new position.
 *
 * Since: 2.16
 */
typedef enum
{
  WNCK_WINDOW_GRAVITY_CURRENT   = 0,
  WNCK_WINDOW_GRAVITY_NORTHWEST = 1,
  WNCK_WINDOW_GRAVITY_NORTH     = 2,
  WNCK_WINDOW_GRAVITY_NORTHEAST = 3,
  WNCK_WINDOW_GRAVITY_WEST      = 4,
  WNCK_WINDOW_GRAVITY_CENTER    = 5,
  WNCK_WINDOW_GRAVITY_EAST      = 6,
  WNCK_WINDOW_GRAVITY_SOUTHWEST = 7,
  WNCK_WINDOW_GRAVITY_SOUTH     = 8,
  WNCK_WINDOW_GRAVITY_SOUTHEAST = 9,
  WNCK_WINDOW_GRAVITY_STATIC    = 10
} WnckWindowGravity;

/**
 * WnckWindowMoveResizeMask:
 * @WNCK_WINDOW_CHANGE_X: X coordinate of the window should be changed.
 * @WNCK_WINDOW_CHANGE_Y: Y coordinate of the window should be changed.
 * @WNCK_WINDOW_CHANGE_WIDTH: width of the window should be changed.
 * @WNCK_WINDOW_CHANGE_HEIGHT: height of the window should be changed.
 *
 * Flag used as a bitmask when changing the geometry of a #WnckWindow. This
 * indicates which part of the geometry should be changed.
 *
 * Since: 2.16
 */
typedef enum
{
  WNCK_WINDOW_CHANGE_X      = 1 << 0,
  WNCK_WINDOW_CHANGE_Y      = 1 << 1,
  WNCK_WINDOW_CHANGE_WIDTH  = 1 << 2,
  WNCK_WINDOW_CHANGE_HEIGHT = 1 << 3
} WnckWindowMoveResizeMask;

#define WNCK_TYPE_WINDOW              (wnck_window_get_type ())
#define WNCK_WINDOW(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), WNCK_TYPE_WINDOW, WnckWindow))
#define WNCK_WINDOW_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), WNCK_TYPE_WINDOW, WnckWindowClass))
#define WNCK_IS_WINDOW(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), WNCK_TYPE_WINDOW))
#define WNCK_IS_WINDOW_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), WNCK_TYPE_WINDOW))
#define WNCK_WINDOW_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), WNCK_TYPE_WINDOW, WnckWindowClass))

typedef struct _WnckWindowClass   WnckWindowClass;
typedef struct _WnckWindowPrivate WnckWindowPrivate;

/**
 * WnckWindow:
 *
 * The #WnckWindow struct contains only private fields and should not be
 * directly accessed.
 */
struct _WnckWindow
{
  GObject parent_instance;

  WnckWindowPrivate *priv;
};

struct _WnckWindowClass
{
  GObjectClass parent_class;

  /* window name or icon name changed */
  void (* name_changed) (WnckWindow *window);

  /* minimized, maximized, sticky, skip pager, skip task, shaded
   * may have changed
   */
  void (* state_changed) (WnckWindow     *window,
                          WnckWindowState changed_mask,
                          WnckWindowState new_state);

  /* Changed workspace or pinned/unpinned state */
  void (* workspace_changed) (WnckWindow *window);

  /* Changed icon */
  void (* icon_changed)      (WnckWindow *window);

  /* Changed actions */
  void (* actions_changed)   (WnckWindow       *window,
                              WnckWindowActions changed_mask,
                              WnckWindowActions new_actions);

  /* Changed size/position */
  void (* geometry_changed)  (WnckWindow       *window);

  /* Changed class group/instance name */
  void (* class_changed)     (WnckWindow       *window);

  /* Changed role */
  void (* role_changed)      (WnckWindow       *window);

  /* Padding for future expansion */
  void (* pad1) (void);
  void (* pad2) (void);
};

GType wnck_window_get_type (void) G_GNUC_CONST;

WnckWindow* wnck_window_get (gulong xwindow);

WnckScreen* wnck_window_get_screen    (WnckWindow *window);

gboolean    wnck_window_has_name      (WnckWindow *window);
const char* wnck_window_get_name      (WnckWindow *window);
gboolean    wnck_window_has_icon_name (WnckWindow *window);
const char* wnck_window_get_icon_name (WnckWindow *window);

WnckApplication* wnck_window_get_application  (WnckWindow *window);
WnckWindow*      wnck_window_get_transient    (WnckWindow *window);
gulong           wnck_window_get_group_leader (WnckWindow *window);
gulong           wnck_window_get_xid          (WnckWindow *window);

WnckClassGroup *wnck_window_get_class_group   (WnckWindow *window);

const char* wnck_window_get_class_group_name    (WnckWindow *window);
const char* wnck_window_get_class_instance_name (WnckWindow *window);

const char* wnck_window_get_session_id        (WnckWindow *window);
const char* wnck_window_get_session_id_utf8   (WnckWindow *window);
const char* wnck_window_get_role              (WnckWindow *window);
int         wnck_window_get_pid               (WnckWindow *window);
gint        wnck_window_get_sort_order        (WnckWindow *window);
void        wnck_window_set_sort_order        (WnckWindow *window,
						gint order);

WnckWindowType wnck_window_get_window_type    (WnckWindow *window);
void           wnck_window_set_window_type    (WnckWindow *window,
                                               WnckWindowType wintype);

gboolean wnck_window_is_minimized              (WnckWindow *window);
gboolean wnck_window_is_maximized_horizontally (WnckWindow *window);
gboolean wnck_window_is_maximized_vertically   (WnckWindow *window);
gboolean wnck_window_is_maximized              (WnckWindow *window);
gboolean wnck_window_is_shaded                 (WnckWindow *window);
gboolean wnck_window_is_above                  (WnckWindow *window);
gboolean wnck_window_is_below                  (WnckWindow *window);
gboolean wnck_window_is_skip_pager             (WnckWindow *window);
gboolean wnck_window_is_skip_tasklist          (WnckWindow *window);
gboolean wnck_window_is_fullscreen             (WnckWindow *window);
gboolean wnck_window_is_sticky                 (WnckWindow *window);
gboolean wnck_window_needs_attention           (WnckWindow *window);
gboolean wnck_window_or_transient_needs_attention (WnckWindow *window);

void wnck_window_set_skip_pager    (WnckWindow *window,
                                    gboolean skip);
void wnck_window_set_skip_tasklist (WnckWindow *window,
                                    gboolean skip);
void wnck_window_set_fullscreen (WnckWindow *window,
                                 gboolean fullscreen);

void wnck_window_close                   (WnckWindow *window,
                                          guint32     timestamp);
void wnck_window_minimize                (WnckWindow *window);
void wnck_window_unminimize              (WnckWindow *window,
                                          guint32     timestamp);
void wnck_window_maximize                (WnckWindow *window);
void wnck_window_unmaximize              (WnckWindow *window);
void wnck_window_maximize_horizontally   (WnckWindow *window);
void wnck_window_unmaximize_horizontally (WnckWindow *window);
void wnck_window_maximize_vertically     (WnckWindow *window);
void wnck_window_unmaximize_vertically   (WnckWindow *window);
void wnck_window_shade                   (WnckWindow *window);
void wnck_window_unshade                 (WnckWindow *window);
void wnck_window_make_above              (WnckWindow *window);
void wnck_window_unmake_above            (WnckWindow *window);
void wnck_window_make_below              (WnckWindow *window);
void wnck_window_unmake_below            (WnckWindow *window);
void wnck_window_stick                   (WnckWindow *window);
void wnck_window_unstick                 (WnckWindow *window);
void wnck_window_keyboard_move           (WnckWindow *window);
void wnck_window_keyboard_size           (WnckWindow *window);

WnckWorkspace* wnck_window_get_workspace     (WnckWindow    *window);
void           wnck_window_move_to_workspace (WnckWindow    *window,
                                              WnckWorkspace *space);

/* pinned = on all workspaces */
gboolean wnck_window_is_pinned (WnckWindow *window);
void     wnck_window_pin       (WnckWindow *window);
void     wnck_window_unpin     (WnckWindow *window);

void     wnck_window_activate  (WnckWindow *window,
                                guint32     timestamp);
gboolean wnck_window_is_active (WnckWindow *window);
gboolean wnck_window_is_most_recently_activated (WnckWindow *window);
void     wnck_window_activate_transient (WnckWindow *window,
                                         guint32     timestamp);
gboolean wnck_window_transient_is_most_recently_activated (WnckWindow *window);

GdkPixbuf* wnck_window_get_icon      (WnckWindow *window);
GdkPixbuf* wnck_window_get_mini_icon (WnckWindow *window);

gboolean wnck_window_get_icon_is_fallback (WnckWindow *window);

void wnck_window_set_icon_geometry        (WnckWindow *window,
					   int         x,
					   int         y,
					   int         width,
					   int         height);

WnckWindowActions wnck_window_get_actions (WnckWindow *window);
WnckWindowState   wnck_window_get_state   (WnckWindow *window);

void wnck_window_get_client_window_geometry (WnckWindow *window,
                                             int        *xp,
                                             int        *yp,
                                             int        *widthp,
                                             int        *heightp);
void wnck_window_get_geometry (WnckWindow *window,
                               int        *xp,
                               int        *yp,
                               int        *widthp,
                               int        *heightp);
void wnck_window_set_geometry (WnckWindow               *window,
                               WnckWindowGravity         gravity,
                               WnckWindowMoveResizeMask  geometry_mask,
                               int                       x,
                               int                       y,
                               int                       width,
                               int                       height);

gboolean wnck_window_is_visible_on_workspace (WnckWindow    *window,
                                              WnckWorkspace *workspace);
gboolean wnck_window_is_on_workspace         (WnckWindow    *window,
                                              WnckWorkspace *workspace);
gboolean wnck_window_is_in_viewport          (WnckWindow    *window,
                                              WnckWorkspace *workspace);

G_END_DECLS

#endif /* WNCK_WINDOW_H */
