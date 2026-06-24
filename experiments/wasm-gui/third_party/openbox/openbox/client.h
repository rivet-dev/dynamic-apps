/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   client.h for the Openbox window manager
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

#ifndef __client_h
#define __client_h

#include "misc.h"
#include "mwm.h"
#include "geom.h"
#include "stacking.h"
#include "window.h"
#include "obrender/color.h"

#include <glib.h>
#include <X11/Xlib.h>

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h> /* for pid_t */
#endif

struct _ObFrame;
struct _ObGroup;
struct _ObSessionState;
struct _ObPrompt;

typedef struct _ObClient      ObClient;

/*! Possible window types */
typedef enum
{
    OB_CLIENT_TYPE_DESKTOP, /*!< A desktop (bottom-most window) */
    OB_CLIENT_TYPE_DOCK,    /*!< A dock bar/panel window */
    OB_CLIENT_TYPE_TOOLBAR, /*!< A toolbar window, pulled off an app */
    OB_CLIENT_TYPE_MENU,    /*!< An unpinned menu from an app */
    OB_CLIENT_TYPE_UTILITY, /*!< A small utility window such as a palette */
    OB_CLIENT_TYPE_SPLASH,  /*!< A splash screen window */
    OB_CLIENT_TYPE_DIALOG,  /*!< A dialog window */
    OB_CLIENT_TYPE_NORMAL   /*!< A normal application window */
} ObClientType;

/*! The things the user can do to the client window */
typedef enum
{
    OB_CLIENT_FUNC_RESIZE     = 1 << 0, /*!< Allow user resizing */
    OB_CLIENT_FUNC_MOVE       = 1 << 1, /*!< Allow user moving */
    OB_CLIENT_FUNC_ICONIFY    = 1 << 2, /*!< Allow to be iconified */
    OB_CLIENT_FUNC_MAXIMIZE   = 1 << 3, /*!< Allow to be maximized */
    OB_CLIENT_FUNC_SHADE      = 1 << 4, /*!< Allow to be shaded */
    OB_CLIENT_FUNC_FULLSCREEN = 1 << 5, /*!< Allow to be made fullscreen */
    OB_CLIENT_FUNC_CLOSE      = 1 << 6, /*!< Allow to be closed */
    OB_CLIENT_FUNC_ABOVE      = 1 << 7, /*!< Allow to be put in lower layer */
    OB_CLIENT_FUNC_BELOW      = 1 << 8, /*!< Allow to be put in higher layer */
    OB_CLIENT_FUNC_UNDECORATE = 1 << 9  /*!< Allow to be undecorated */
} ObFunctions;

struct _ObClient
{
    ObWindow obwin;
    Window  window;
    gboolean managed;

    /*! If this client is managing an ObPrompt window, then this is set to the
      prompt */
    struct _ObPrompt *prompt;

    /*! The window's decorations. NULL while the window is being managed! */
    struct _ObFrame *frame;

    /*! The number of unmap events to ignore on the window */
    gint ignore_unmaps;

    /*! The id of the group the window belongs to */
    struct _ObGroup *group;

    /*! Saved session data to apply to this client */
    struct _ObSessionState *session;

    /*! Whether or not the client is a transient window. It may or may not
      have parents when this is true. */
    gboolean transient;
    /*! Whether or not the client is transient for its group */
    gboolean transient_for_group;
    /*! The client which are parents of this client */
    GSList *parents;
    /*! The clients which are transients (children) of this client */
    GSList *transients;
    /*! The desktop on which the window resides (0xffffffff for all
      desktops) */
    guint desktop;

    /*! The startup id for the startup-notification protocol. This will be
      NULL if a startup id is not set. */
    gchar *startup_id;

    /*! Normal window title */
    gchar *title;
    /*! Window title when iconified */
    gchar *icon_title;
    /*! The title as requested by the client, without any of our own changes */
    gchar *original_title;
    /*! Hostname of machine running the client */
    gchar *client_machine;
    /*! The command used to run the program. Pre-XSMP window identification. */
    gchar *wm_command;
    /*! The PID of the process which owns the window */
    pid_t pid;

    /*! The application that created the window */
    gchar *name;
    /*! The class of the window, can used for grouping */
    gchar *class;
    /*! The specified role of the window, used for identification */
    gchar *role;
    /*! The application that created the window's group. */
    gchar *group_name;
    /*! The class of the window's group, can used for grouping */
    gchar *group_class;
    /*! The session client id for the window. *This can be NULL!* */
    gchar *sm_client_id;

    /*! The type of window (what its function is) */
    ObClientType type;

    /*! Position and size of the window
      This will not always be the actual position of the window on screen, it
      is, rather, the position requested by the client, to which the window's
      gravity is applied.
    */
    Rect area;

    /*! Position of the client window relative to the root window */
    Point root_pos;

    /*! Position and size of the window prior to being maximized */
    Rect pre_max_area;
    /*! Position and size of the window prior to being fullscreened */
    Rect pre_fullscreen_area;
    /*! Remember if the window was maximized before going fullscreen */
    gboolean pre_fullscreen_max_horz,
             pre_fullscreen_max_vert;

    /*! The window's strut
      The strut defines areas of the screen that are marked off-bounds for
      window placement. In theory, where this window exists.
    */
    StrutPartial strut;

    /*! The logical size of the window
      The "logical" size of the window is refers to the user's perception of
      the size of the window, and is the value that should be displayed to the
      user. For example, with xterms, this value it the number of characters
      being displayed in the terminal, instead of the number of pixels.
    */
    Size logical_size;

    /*! Width of the border on the window.
      The window manager will set this to 0 while the window is being managed,
      but needs to restore it afterwards, so it is saved here.
    */
    gint border_width;

    /*! The minimum aspect ratio the client window can be sized to.
      A value of 0 means this is ignored.
    */
    gfloat min_ratio;
    /*! The maximum aspect ratio the client window can be sized to.
      A value of 0 means this is ignored.
    */
    gfloat max_ratio;

    /*! The minimum size of the client window
      If the min is > the max, then the window is not resizable
    */
    Size min_size;
    /*! The maximum size of the client window
      If the min is > the max, then the window is not resizable
    */
    Size max_size;
    /*! The size of increments to resize the client window by */
    Size size_inc;
    /*! The base size of the client window
      This value should be subtracted from the window's actual size when
      displaying its size to the user, or working with its min/max size
    */
    Size base_size;

    /*! Window decoration and functionality hints */
    ObMwmHints mwmhints;

    /*! The client's specified colormap */
    Colormap colormap;

    /*! Where to place the decorated window in relation to the undecorated
      window */
    gint gravity;

    /*! The state of the window, one of WithdrawnState, IconicState, or
      NormalState */
    glong wmstate;

    /*! True if the client supports the delete_window protocol */
    gboolean delete_window;

    /*! Was the window's position requested by the application or the user?
      if by the application, we force it completely onscreen, if by the user
      we only force it if it tries to go completely offscreen, if neither, we
      should place the window ourselves when it first appears */
    guint positioned;

    /*! Was the window's size requested by the application or the user?
      If by the application we don't let it go outside the available area */
    guint sized;

    /*! Can the window receive input focus? */
    gboolean can_focus;
    /*! Notify the window when it receives focus? */
    gboolean focus_notify;

    /*! Will the client respond to pings? */
    gboolean ping;
    /*! Indicates if the client is trying to close but has stopped responding
      to pings */
    gboolean not_responding;
    /*! A prompt shown when you are trying to close a client that is not
      responding.  It asks if you want to kill the client */
    struct _ObPrompt *kill_prompt;
    /*! We tried to close the window with a SIGTERM */
    gint kill_level;

#ifdef SYNC
    /*! The client wants to sync during resizes */
    gboolean sync_request;
    /*! The XSync counter used for synchronizing during resizes */
    guint32 sync_counter;
    /*! The value we're waiting for the counter to reach */
    gulong sync_counter_value;
#endif

    /*! The window uses shape extension to be non-rectangular? */
    gboolean shaped;
    /*! The window uses shape extension to have non-rectangular input? */
    gboolean shaped_input;

    /*! The window is modal, so it must be processed before any windows it is
      related to can be focused */
    gboolean modal;
    /*! Only the window's titlebar is displayed */
    gboolean shaded;
    /*! The window is iconified */
    gboolean iconic;
    /*! The window is maximized to fill the screen vertically */
    gboolean max_vert;
    /*! The window is maximized to fill the screen horizontally */
    gboolean max_horz;
    /*! The window should not be displayed by pagers */
    gboolean skip_pager;
    /*! The window should not be displayed by taskbars */
    gboolean skip_taskbar;
    /*! The window is a 'fullscreen' window, and should be on top of all
      others */
    gboolean fullscreen;
    /*! The window should be on top of other windows of the same type.
      above takes priority over below. */
    gboolean above;
    /*! The window should be underneath other windows of the same type.
      above takes priority over below. */
    gboolean below;
    /*! Demands attention flag */
    gboolean demands_attention;

    /*! The urgent flag */
    gboolean urgent;

    /*! The layer in which the window will be stacked, windows in lower layers
      are always below windows in higher layers. */
    ObStackingLayer layer;

    /*! A bitmask of values in the ObFrameDecorations enum
      The values in the variable are the decorations that the client wants to
      be displayed around it.
    */
    guint decorations;

    /*! A user option. When this is set to TRUE the client will not ever
      be decorated.
    */
    gboolean undecorated;

    /*! A bitmask of values in the ObFunctions enum
      The values in the variable specify the ways in which the user is allowed
      to modify this window.
    */
    guint functions;

    /* The window's icon, in a variety of shapes and sizes */
    RrImage *icon_set;

    /*! Where the window should iconify to/from */
    Rect icon_geometry;

    /*! A boolean used for algorithms which need to mark clients as visited */
    gboolean visited;
};

extern GList      *client_list;

void client_startup(gboolean reconfig);
void client_shutdown(gboolean reconfig);

typedef void (*ObClientCallback)(ObClient *client, gpointer data);

/* Callback functions */

/*! Get notified when the client is unmanaged */
void client_add_destroy_notify(ObClientCallback func, gpointer data);
void client_remove_destroy_notify(ObClientCallback func);
void client_remove_destroy_notify_data(ObClientCallback func, gpointer data);

/*! Manages a given window
  @param prompt This specifies an ObPrompt which is being managed.  It is
                possible to manage Openbox-owned windows through this.
*/
void client_manage(Window win, struct _ObPrompt *prompt);
/*! Unmanages all managed windows */
void client_unmanage_all(void);
/*! Unmanages a given client */
void client_unmanage(ObClient *client);

/*! This manages a window only so far as is needed to get it's decorations.
   This is used when you want to determine a window's decorations before it
   is mapped. Call client_fake_unmanage() with the returned client when you
   are done with it. */
ObClient *client_fake_manage(Window win);
/*! Free the stuff created by client_fake_manage() */
void client_fake_unmanage(ObClient *self);

/*! Sets the client list on the root window from the client_list */
void client_set_list(void);

/*! Determines if the client should be shown or hidden currently.
  @return TRUE if it should be visible; otherwise, FALSE.
*/
gboolean client_should_show(ObClient *self);

/*! Returns if the window should be treated as a normal window.
  Some windows (desktops, docks, splash screens) have special rules applied
  to them in a number of places regarding focus or user interaction. */
gboolean client_normal(ObClient *self);

/*! Returns if the window is one of an application's helper windows
  (utilty, menu, etc) */
gboolean client_helper(ObClient *self);

/*! Returns true if the window occupies space in the monitor conceptually, or
  false if it does not and its presence should be ignored when possible. */
gboolean client_occupies_space(ObClient *self);

/*! Return if the client is a type which should be given focus from mouse
  presses on the *client* window. This doesn't affect clicking on the
  decorations. This doesn't count for focus cycling, different rules apply to
  that. */
gboolean client_mouse_focusable(ObClient *self);

/*! Return if the client is a type which should be given focus from the
  mouse entering the window. This doesn't count for focus cycling, different
  rules apply to that. */
gboolean client_enter_focusable(ObClient *self);

/* Returns if the window is focused */
gboolean client_focused(ObClient *self);

/*! When the client is resized but not moved, figure out the new position
  for it based on its gravity:
  http://standards.freedesktop.org/wm-spec/wm-spec-1.4.html#id2512541
*/
void client_gravity_resize_w(ObClient *self, gint *x, gint oldw, gint neww);

/*! When the client is resized but not moved, figure out the new position
  for it based on its gravity:
  http://standards.freedesktop.org/wm-spec/wm-spec-1.4.html#id2512541
*/
void client_gravity_resize_h(ObClient *self, gint *y, gint oldh, gint newh);

/*! Convert a position/size from a given gravity to the client's true gravity,
  when the client is only resizing (the reference point doesn't move)
 */
void client_convert_gravity_resize(ObClient *self, gint gravity,
                                   gint *x, gint *y,
                                   gint w, gint h);

#define client_move(self, x, y) \
  client_configure(self, x, y, self->area.width, self->area.height, TRUE, TRUE,\
                   FALSE)
#define client_resize(self, w, h) \
  client_configure(self, self->area.x, self->area.y, w, h, TRUE, TRUE, FALSE)
#define client_move_resize(self, x, y, w, h) \
  client_configure(self, x, y, w, h, TRUE, TRUE, FALSE)
#define client_reconfigure(self, force) \
  client_configure(self, ((ObClient*)self)->area.x, ((ObClient*)self)->area.y, \
                   ((ObClient*)self)->area.width, \
                   ((ObClient*)self)->area.height, FALSE, TRUE, force)

/*! Figure out where a window will end up and what size it will be if you
  told it to move/resize to these coordinates.

  These values are what client_configure will give the window.

  @param x The x coordiante of the new position for the client.
  @param y The y coordiante of the new position for the client.
  @param w The width component of the new size for the client.
  @param h The height component of the new size for the client.
  @param logicalw Returns the width component of the new logical width.
                  This value is only returned when the new w or h calculated
                  differ from the ones passed in.
  @param logicalh Returns the height component of the new logical height.
                  This value is only returned when the new w or h calculated
                  differ from the ones passed in.
  @param user Specifies whether this is a user-requested change or a
              program requested change. For program requested changes, the
              constraints are not checked.
*/
void client_try_configure(ObClient *self, gint *x, gint *y, gint *w, gint *h,
                          gint *logicalw, gint *logicalh,
                          gboolean user);

/*! Move and/or resize the window.
  This also maintains things like the client's minsize, and size increments.
  @param x The x coordiante of the new position for the client.
  @param y The y coordiante of the new position for the client.
  @param w The width component of the new size for the client.
  @param h The height component of the new size for the client.
  @param user Specifies whether this is a user-requested change or a
              program requested change. For program requested changes, the
              constraints are not checked.
  @param final If user is true, then this should specify if this is a final
               configuration. e.g. Final should be FALSE if doing an
               interactive move/resize, and then be TRUE for the last call
               only.
  @param force_reply Send a ConfigureNotify to the client regardless of if
                     the position/size changed.
*/
void client_configure(ObClient *self, gint x, gint y, gint w, gint h,
                      gboolean user, gboolean final, gboolean force_reply);

/*! Finds coordinates to keep a client on the screen.
  @param self The client
  @param x The x coord of the client, may be changed.
  @param y The y coord of the client, may be changed.
  @param w The width of the client.
  @param w The height of the client.
  @param rude Be rude about it. If false, it is only moved if it is entirely
              not visible. If true, then make sure the window is inside the
              struts if possible.
  @return true if the client was moved to be on-screen; false if not.
*/
gboolean client_find_onscreen(ObClient *self, gint *x, gint *y, gint w, gint h,
                              gboolean rude);

/*! Moves a client so that it is on screen if it is entirely out of the
  viewable screen.
  @param self The client to move
  @param rude Be rude about it. If false, it is only moved if it is entirely
              not visible. If true, then make sure the window is inside the
              struts if possible.
*/
void client_move_onscreen(ObClient *self, gboolean rude);

/*! dir is either North, South, East or West. It can't be, for example,
  Northwest */
void client_find_edge_directional(ObClient *self, ObDirection dir,
                                  gint my_head, gint my_tail,
                                  gint my_edge_start, gint my_edge_size,
                                  gint *dest, gboolean *near_edge);
void client_find_move_directional(ObClient *self, ObDirection dir,
                                  gint *x, gint *y);

typedef enum {
    CLIENT_RESIZE_GROW,
    CLIENT_RESIZE_GROW_IF_NOT_ON_EDGE,
    CLIENT_RESIZE_SHRINK,
} ObClientDirectionalResizeType;

/*! Moves the client area passed in to grow/shrink the given edge. */
void client_find_resize_directional(ObClient *self,
                                    ObDirection side,
                                    ObClientDirectionalResizeType resize_type,
                                    gint *x, gint *y, gint *w, gint *h);

/*! Fullscreen's or unfullscreen's the client window
  @param fs true if the window should be made fullscreen; false if it should
            be returned to normal state.
*/
void client_fullscreen(ObClient *self, gboolean fs);

/*! Determine if the window, using the given client-area, would be considered
  as an "oldschool fullscreen" window, that is, if it is filling a whole
  monitor. */
gboolean client_is_oldfullscreen(const ObClient *self, const Rect *area);

/*! Iconifies or uniconifies the client window
  @param iconic true if the window should be iconified; false if it should be
                restored.
  @param curdesk If iconic is FALSE, then this determines if the window will
                 be uniconified to the current viewable desktop (true) or to
                 its previous desktop (false)
*/
void client_iconify(ObClient *self, gboolean iconic, gboolean curdesk,
                    gboolean hide_animation);

/*! Maximize or unmaximize the client window
  @param max true if the window should be maximized; false if it should be
             returned to normal size.
  @param dir 0 to set both horz and vert, 1 to set horz, 2 to set vert.
*/
void client_maximize(ObClient *self, gboolean max, gint dir);

/*! Shades or unshades the client window
  @param shade true if the window should be shaded; false if it should be
               unshaded.
*/
void client_shade(ObClient *self, gboolean shade);

/*! Set a client window to have decorations or not */
void client_set_undecorated(ObClient *self, gboolean undecorated);

/*! Hilite the window to make the user notice it */
void client_hilite(ObClient *self, gboolean hilite);

/*! Request the client to close its window */
void client_close(ObClient *self);

/*! Kill the client off violently */
void client_kill(ObClient *self);

/*! Sends the window to the specified desktop
  @param donthide If TRUE, the window will not be shown/hidden after its
                  desktop has been changed. Generally this should be FALSE.
  @param dontraise If TRUE, the window will not be raised. Generally this should
                   be FALSE.
*/
void client_set_desktop(ObClient *self, guint target, gboolean donthide,
                        gboolean dontraise);

/*! Show the client if it should be shown. Returns if the window is shown. */
gboolean client_show(ObClient *self);

/*! Show the client if it should be shown. Returns if the window is hidden. */
gboolean client_hide(ObClient *self);

/*! Show the client if it should be shown, and hide it if it should be
  hidden. This is for example, when switching desktops.
*/
void client_showhide(ObClient *self);

/*! Validate client, by making sure no Destroy or Unmap events exist in
  the event queue for the window.
  @return true if the client is valid; false if the client has already
          been unmapped/destroyed, and so is invalid.
*/
gboolean client_validate(ObClient *self);

/*! Sets the wm_state to the specified value */
void client_set_wm_state(ObClient *self, glong state);

/*! Adjusts the window's net_state
  This should not be called as part of the window mapping process! It is for
  use when updating the state post-mapping.<br>
  client_apply_startup_state is used to do the same things during the mapping
  process.
*/
void client_set_state(ObClient *self, Atom action, glong data1, glong data2);

/* Given a ObClient, find the client that focus would actually be sent to if
   you wanted to give focus to the specified ObClient. Will return the same
   ObClient passed to it or another ObClient if appropriate. */
ObClient *client_focus_target(ObClient *self);

/*! Returns what client_focus would return if passed the same client, but
  without focusing it or modifying the focus order lists. */
gboolean client_can_focus(ObClient *self);

/*! Attempt to focus the client window */
gboolean client_focus(ObClient *self);

/*! Activates the client for use, focusing, uniconifying it, etc. To be used
  when the user deliberately selects a window for use.
  @param desktop If true, and the window is on another desktop, it will still
                 be activated.
  @param here If true, and the window is on another desktop, it will be moved
              to the current desktop, otherwise the desktop will switch to
              where the window is.
  @param raise If true, the client is brought to the front.
  @param unshade If true, the client is unshaded (if it is shaded)
  @param user If true, then a user action is what requested the activation;
              otherwise, it means an application requested it on its own
*/
void client_activate(ObClient *self, gboolean desktop, gboolean here,
                     gboolean raise, gboolean unshade, gboolean user);

/*! Bring all of its helper windows to its desktop. These are the utility and
  stuff windows. */
void client_bring_helper_windows(ObClient *self);

/*! Bring all of its modal windows to its desktop. */
void client_bring_modal_windows(ObClient *self);

/*! Calculates the stacking layer for the client window */
void client_calc_layer(ObClient *self);

/*! Updates the window's transient status, and any parents of it */
void client_update_transient_for(ObClient *self);
/*! Update the protocols that the window supports and adjusts things if they
  change */
void client_update_protocols(ObClient *self);
#ifdef SYNC
/*! Updates the window's sync request counter for resizes */
void client_update_sync_request_counter(ObClient *self);
#endif
/*! Updates the window's colormap */
void client_update_colormap(ObClient *self, Colormap colormap);
/*! Updates the requested opacity for the window from the client. */
void client_update_opacity(ObClient *self);
/*! Updates the WMNormalHints and adjusts things if they change */
void client_update_normal_hints(ObClient *self);

/*! Updates the WMHints and adjusts things if they change
  @param initstate Whether to read the initial_state property from the
                   WMHints. This should only be used during the mapping
                   process.
*/
void client_update_wmhints(ObClient *self);
/*! Updates the window's title and icon title */
void client_update_title(ObClient *self);
/*! Updates the strut for the client */
void client_update_strut(ObClient *self);
/*! Updates the window's icons */
void client_update_icons(ObClient *self);
/*! Updates the window's icon geometry (where to iconify to/from) */
void client_update_icon_geometry(ObClient *self);

/*! Helper function to convert the ->type member to string representation */
const gchar *client_type_to_string(ObClient *self);

/*! Set up what decor should be shown on the window and what functions should
  be allowed (ObClient::decorations and ObClient::functions).
  This also updates the NET_WM_ALLOWED_ACTIONS hint.
  @param reconfig When TRUE, the window will be reconfigured to show the
         changes
*/
void client_setup_decor_and_functions(ObClient *self, gboolean reconfig);

/*! Sets the window's type and transient flag */
void client_get_type_and_transientness(ObClient *self);
/*! Gets the motif wm hints */
void client_get_mwm_hints(ObClient *self);

/*! Returns a client's icon set, or its parents (recursively) if it doesn't
  have one
*/
RrImage* client_icon(ObClient *self);

/*! Return TRUE if the client is transient for some other window. Return
  FALSE if it's not transient or there is no window for it to be
  transient for */
gboolean client_has_parent(ObClient *self);

/*! Return TRUE if the client has some transient children, and FALSE otherwise.
*/
gboolean client_has_children(ObClient *self);

/*! Searches a client's immediate parents for a focused window. The function
  does not check for the passed client, only for *ONE LEVEL* of its parents.
  If no focused parent is found, NULL is returned.
*/
ObClient *client_search_focus_parent(ObClient *self);

/*! Searches a client's parents for a focused window. The function
  does not check for the passed client, but searches through all of its
  parents. If no focused parent is found, NULL is returned.
*/
ObClient *client_search_focus_parent_full(ObClient *self);

/*! Searches a client's transients for a focused window. The function does not
  check for the passed client, only for its transients.
  If no focused transient is found, NULL is returned.
*/
ObClient *client_search_focus_tree(ObClient *self);

/*! Searches a client's transient tree for a focused window. The function
  searches up the tree and down other branches as well as the passed client's.
  If no focused client is found, NULL is returned.
*/
ObClient *client_search_focus_tree_full(ObClient *self);

/*! Searches a client's group and each member's transients for a focused
  window.  This doesn't go up the window's transient tree at all. If no
  focused client is found, NULL is returned. */
ObClient *client_search_focus_group_full(ObClient *self);

/*! Return a modal child of the client window that can be focused.
    @return A modal child of the client window that can be focused, or 0 if
            none was found.
*/
ObClient *client_search_modal_child(ObClient *self);

/*! Returns a list of top-level windows which this is a transient for.
  It will only contain more than 1 element if the client is transient for its
  group.
*/
GSList *client_search_all_top_parents(ObClient *self);

/*! Returns a list of top-level windows which this is a transient for, and
  which are in the same layer as this client.
  It will only contain more than 1 element if the client is transient for its
  group.
*/
GSList *client_search_all_top_parents_layer(ObClient *self);

/*! Returns the client's parent when it is transient for a direct window
  rather than a group. If it has no parents, or is transient for the
  group, this returns null */
ObClient *client_direct_parent(ObClient *self);

/*! Returns a window's top level parent. This only counts direct parents,
  not groups if it is transient for its group.
*/
ObClient *client_search_top_direct_parent(ObClient *self);

/*! Is one client a direct child of another (i.e. not through the group.)
  This checks more than one level, so there may be another direct child in
  between */
gboolean client_is_direct_child(ObClient *parent, ObClient *child);

/*! Search for a parent of a client. This only searches up *ONE LEVEL*, and
  returns the searched for parent if it is a parent, or NULL if not. */
ObClient *client_search_parent(ObClient *self, ObClient *search);

/*! Search for a transient of a client. The transient is returned if it is one,
  NULL is returned if the given search is not a transient of the client. */
ObClient *client_search_transient(ObClient *self, ObClient *search);

/*! Set a client window to be above/below other clients.
  @layer < 0 indicates the client should be placed below other clients.<br />
         = 0 indicates the client should be placed with other clients.<br />
         > 0 indicates the client should be placed above other clients.
*/
void client_set_layer(ObClient *self, gint layer);

guint client_monitor(ObClient *self);

ObClient* client_under_pointer(void);

gboolean client_has_group_siblings(ObClient *self);

/*! Returns TRUE if the client has a transient child, a parent, or a
  group member.  Returns FALSE otherwise.
*/
gboolean client_has_relative(ObClient *self);

/*! Returns TRUE if the client is running on the same machine as Openbox */
gboolean client_on_localhost(ObClient *self);

#endif
