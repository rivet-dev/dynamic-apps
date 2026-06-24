/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   screen.h for the Openbox window manager
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

#ifndef __screen_h
#define __screen_h

#include "misc.h"
#include "geom.h"

struct _ObClient;

#define DESKTOP_ALL (0xffffffff)

typedef enum {
    SCREEN_SHOW_DESKTOP_NO,
    SCREEN_SHOW_DESKTOP_UNTIL_WINDOW,
    SCREEN_SHOW_DESKTOP_UNTIL_TOGGLE
} ObScreenShowDestopMode;

/*! The number of available desktops */
extern guint screen_num_desktops;
/*! The number of virtual "xinerama" screens/heads */
extern guint screen_num_monitors;
/*! The current desktop */
extern guint screen_desktop;
/*! The desktop which was last visible */
extern guint screen_last_desktop;
/*! Are we in showing-desktop mode? */
extern ObScreenShowDestopMode screen_show_desktop_mode;
/*! The support window also used for focus and stacking */
extern Window screen_support_win;
/*! The last time at which the user changed desktops */
extern Time screen_desktop_user_time;

typedef struct ObDesktopLayout {
    ObOrientation orientation;
    ObCorner start_corner;
    guint rows;
    guint columns;
} ObDesktopLayout;
extern ObDesktopLayout screen_desktop_layout;

/*! An array of gchar*'s which are desktop names in UTF-8 format */
extern gchar **screen_desktop_names;

/*! Take over the screen, set the basic hints on it claming it as ours */
gboolean screen_annex(void);

/*! Once the screen is ours, set up its initial state */
void screen_startup(gboolean reconfig);
/*! Free resources */
void screen_shutdown(gboolean reconfig);

/*! Figure out the new size of the screen and adjust stuff for it */
void screen_resize(void);

/*! Change the number of available desktops */
void screen_set_num_desktops(guint num);
/*! Change the current desktop */
void screen_set_desktop(guint num, gboolean dofocus);
/*! Add a new desktop either at the end or inserted at the current desktop */
void screen_add_desktop(gboolean current);
/*! Remove a desktop, either at the end or the current desktop */
void screen_remove_desktop(gboolean current);

guint screen_find_desktop(guint from, ObDirection dir,
                          gboolean wrap, gboolean linear);

/*! Show the desktop popup/notification
  @permanent If TRUE, the popup will stay on the screen until you call
             screen_hide_desktop_popup().  Otherwise it will hide after a
             delay.
 */
void screen_show_desktop_popup(guint d, gboolean permanent);
/*! Hide it */
void screen_hide_desktop_popup(void);

/*! Shows and focuses the desktop and hides all the client windows, or
  returns to the normal state, showing client windows.
  @param If show_only is non-NULL, then only that client is shown (assuming
         show is FALSE (restoring from show-desktop mode), and the rest are
         iconified.
*/
void screen_show_desktop(ObScreenShowDestopMode show_mode,
                         struct _ObClient *show_only);

/*! Returns true if showing desktop mode is enabled. */
gboolean screen_showing_desktop();

/*! Updates the desktop layout from the root property if available */
void screen_update_layout(void);

/*! Get desktop names from the root window property */
void screen_update_desktop_names(void);

/*! Installs or uninstalls a colormap for a client. If client is NULL, then
  it handles the root colormap. */
void screen_install_colormap(struct _ObClient *client, gboolean install);

void screen_update_areas(void);

const Rect* screen_physical_area_all_monitors(void);

/*! Returns a Rect which is owned by the screen code and should not be freed */
const Rect* screen_physical_area_monitor(guint head);

/*! Returns the monitor which contains the active window, or the one
  containing the pointer otherwise. */
guint screen_monitor_active(void);

/*! Returns a Rect which is owned by the screen code and should not be freed */
const Rect* screen_physical_area_active(void);

/*! Returns the primary monitor, as specified by the config.
  @fixed If TRUE, then this will always return a fixed monitor, otherwise
         it may change based on where focus is, or other heuristics.
 */
guint screen_monitor_primary(gboolean fixed);

/*! Returns physical area for the primary monitor, as specified by the config.
  @fixed If TRUE, then this will always use a fixed monitor as primary,
         otherwise it may change based on where focus is, or other heuristics.
         See screen_monitor_primary().
  @return A Rect which is owned by the screen code and should not be freed
*/
const Rect* screen_physical_area_primary(gboolean fixed);

/* doesn't include struts which the search area is already outside of when
   'search' is not NULL */
#define SCREEN_AREA_ALL_MONITORS ((unsigned)-1)
#define SCREEN_AREA_ONE_MONITOR  ((unsigned)-2)

/*! @param head is the number of the head or one of SCREEN_AREA_ALL_MONITORS,
           SCREEN_AREA_ONE_MONITOR
    @param search NULL or the whole monitor(s)
    @return A Rect allocated with g_slice_new()
 */
Rect* screen_area(guint desktop, guint head, Rect *search);

gboolean screen_physical_area_monitor_contains(guint head, Rect *search);

/*! Determines which physical monitor a rectangle is on by calculating the
    area of the part of the rectable on each monitor.  The number of the
    monitor containing the greatest area of the rectangle is returned.
*/
guint screen_find_monitor(const Rect *search);

/*! Finds the monitor which contains the point @x, @y */
guint screen_find_monitor_point(guint x, guint y);

/*! Sets the root cursor. This function decides which cursor to use, but you
  gotta call it to let it know it should change. */
void screen_set_root_cursor(void);

/*! Gives back the pointer's position in x and y. Returns TRUE if the pointer
  is on this screen and FALSE if it is on another screen. */
gboolean screen_pointer_pos(gint *x, gint *y);

/*! Returns the monitor which contains the pointer device */
guint screen_monitor_pointer(void);

/*! Compare the desktop for two windows to see if they are considered on the
  same desktop.
  Windows that are on "all desktops" are treated like they are only on the
  current desktop, so they are only in one place at a time.
  @return TRUE if they are on the same desktop, FALSE otherwise.
*/
gboolean screen_compare_desktops(guint a, guint b);

/*! Resolve a gravity point into absolute coordinates.
 * width and height are the size of the object being placed, used for
 * aligning to right/bottom edges of the area. */
void screen_apply_gravity_point(gint *x, gint *y, gint width, gint height,
                                const GravityPoint *position, const Rect *area);
#endif
