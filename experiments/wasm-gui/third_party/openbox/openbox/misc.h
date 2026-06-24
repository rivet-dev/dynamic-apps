/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   misc.h for the Openbox window manager
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

#ifndef __ob__misc_h
#define __ob__misc_h

/*! The alpha value to use for icons of iconified windows in various places
  like the focus cycle popup and client list menus.
  Give iconic windows 7/16 alpha. A little under 50%.
 */
#define OB_ICONIC_ALPHA 0x70

typedef enum
{
    OB_CURSOR_NONE,
    OB_CURSOR_POINTER,
    OB_CURSOR_BUSYPOINTER,
    OB_CURSOR_BUSY,
    OB_CURSOR_MOVE,
    OB_CURSOR_NORTH,
    OB_CURSOR_NORTHEAST,
    OB_CURSOR_EAST,
    OB_CURSOR_SOUTHEAST,
    OB_CURSOR_SOUTH,
    OB_CURSOR_SOUTHWEST,
    OB_CURSOR_WEST,
    OB_CURSOR_NORTHWEST,
    OB_NUM_CURSORS
} ObCursor;

typedef enum
{
    OB_STATE_STARTING,
    OB_STATE_RUNNING,
    OB_STATE_EXITING,
    OB_STATE_RECONFIGURING
} ObState;

typedef enum
{
    OB_DIRECTION_NORTH,
    OB_DIRECTION_NORTHEAST,
    OB_DIRECTION_EAST,
    OB_DIRECTION_SOUTHEAST,
    OB_DIRECTION_SOUTH,
    OB_DIRECTION_SOUTHWEST,
    OB_DIRECTION_WEST,
    OB_DIRECTION_NORTHWEST
} ObDirection;

typedef enum
{
    OB_ORIENTATION_HORZ,
    OB_ORIENTATION_VERT
} ObOrientation;

typedef enum
{
    OB_CORNER_TOPLEFT,
    OB_CORNER_TOPRIGHT,
    OB_CORNER_BOTTOMLEFT,
    OB_CORNER_BOTTOMRIGHT
} ObCorner;

typedef enum {
    OB_MOUSE_ACTION_PRESS,
    OB_MOUSE_ACTION_RELEASE,
    OB_MOUSE_ACTION_CLICK,
    OB_MOUSE_ACTION_DOUBLE_CLICK,
    OB_MOUSE_ACTION_MOTION,
    OB_NUM_MOUSE_ACTIONS
} ObMouseAction;

typedef enum {
    OB_USER_ACTION_NONE, /* being fired from inside another action and such */
    OB_USER_ACTION_KEYBOARD_KEY,
    OB_USER_ACTION_MOUSE_PRESS,
    OB_USER_ACTION_MOUSE_RELEASE,
    OB_USER_ACTION_MOUSE_CLICK,
    OB_USER_ACTION_MOUSE_DOUBLE_CLICK,
    OB_USER_ACTION_MOUSE_MOTION,
    OB_USER_ACTION_MENU_SELECTION,
    OB_NUM_USER_ACTIONS
} ObUserAction;

#endif
