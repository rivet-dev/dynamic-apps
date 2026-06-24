/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   obt/prop.h for the Openbox window manager
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

#ifndef __obt_prop_h
#define __obt_prop_h

#include <X11/Xlib.h>
#include <glib.h>

G_BEGIN_DECLS

typedef enum {
    /* types */
    OBT_PROP_CARDINAL, /*!< The atom which represents the Cardinal data type */
    OBT_PROP_WINDOW, /*!< The atom which represents window ids */
    OBT_PROP_PIXMAP, /*!< The atom which represents pixmap ids */
    OBT_PROP_ATOM, /*!< The atom which represents atom values */
    OBT_PROP_STRING, /*!< The atom which represents latin1 strings */
    OBT_PROP_COMPOUND_TEXT, /*!< The atom which represents locale-encoded
                              strings */
    OBT_PROP_UTF8_STRING, /*!< The atom which represents utf8-encoded strings*/

    /* selection stuff */
    OBT_PROP_MANAGER,

    /* window hints */
    OBT_PROP_WM_COLORMAP_WINDOWS,
    OBT_PROP_WM_PROTOCOLS,
    OBT_PROP_WM_STATE,
    OBT_PROP_WM_DELETE_WINDOW,
    OBT_PROP_WM_TAKE_FOCUS,
    OBT_PROP_WM_CHANGE_STATE,
    OBT_PROP_WM_NAME,
    OBT_PROP_WM_ICON_NAME,
    OBT_PROP_WM_CLASS,
    OBT_PROP_WM_WINDOW_ROLE,
    OBT_PROP_WM_CLIENT_MACHINE,
    OBT_PROP_WM_COMMAND,
    OBT_PROP_WM_CLIENT_LEADER,
    OBT_PROP_WM_TRANSIENT_FOR,
    OBT_PROP_MOTIF_WM_HINTS,
    OBT_PROP_MOTIF_WM_INFO,

    /* SM atoms */
    OBT_PROP_SM_CLIENT_ID,

    /* NETWM atoms */

    /* Atoms that are used inside messages - these don't go in net_supported */

    OBT_PROP_NET_WM_MOVERESIZE_SIZE_TOPLEFT,
    OBT_PROP_NET_WM_MOVERESIZE_SIZE_TOP,
    OBT_PROP_NET_WM_MOVERESIZE_SIZE_TOPRIGHT,
    OBT_PROP_NET_WM_MOVERESIZE_SIZE_RIGHT,
    OBT_PROP_NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT,
    OBT_PROP_NET_WM_MOVERESIZE_SIZE_BOTTOM,
    OBT_PROP_NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT,
    OBT_PROP_NET_WM_MOVERESIZE_SIZE_LEFT,
    OBT_PROP_NET_WM_MOVERESIZE_MOVE,
    OBT_PROP_NET_WM_MOVERESIZE_SIZE_KEYBOARD,
    OBT_PROP_NET_WM_MOVERESIZE_MOVE_KEYBOARD,
    OBT_PROP_NET_WM_MOVERESIZE_CANCEL,

    OBT_PROP_NET_WM_STATE_ADD,
    OBT_PROP_NET_WM_STATE_REMOVE,
    OBT_PROP_NET_WM_STATE_TOGGLE,

    OBT_PROP_NET_WM_ORIENTATION_HORZ,
    OBT_PROP_NET_WM_ORIENTATION_VERT,
    OBT_PROP_NET_WM_TOPLEFT,
    OBT_PROP_NET_WM_TOPRIGHT,
    OBT_PROP_NET_WM_BOTTOMRIGHT,
    OBT_PROP_NET_WM_BOTTOMLEFT,

    OBT_PROP_NET_WM_WINDOW_TYPE_POPUP_MENU,

    OBT_PROP_PRIVATE_PADDING1,
    OBT_PROP_PRIVATE_PADDING2,
    OBT_PROP_PRIVATE_PADDING3,
    OBT_PROP_PRIVATE_PADDING4,
    OBT_PROP_PRIVATE_PADDING5,
    OBT_PROP_PRIVATE_PADDING6,
    OBT_PROP_PRIVATE_PADDING7,
    OBT_PROP_PRIVATE_PADDING8,
    OBT_PROP_PRIVATE_PADDING9,
    OBT_PROP_PRIVATE_PADDING10,
    OBT_PROP_PRIVATE_PADDING11,
    OBT_PROP_PRIVATE_PADDING12,

    /* Everything below here must go in net_supported on the root window */

    /* root window properties */
    OBT_PROP_NET_SUPPORTED,
    OBT_PROP_NET_CLIENT_LIST,
    OBT_PROP_NET_CLIENT_LIST_STACKING,
    OBT_PROP_NET_NUMBER_OF_DESKTOPS,
    OBT_PROP_NET_DESKTOP_GEOMETRY,
    OBT_PROP_NET_DESKTOP_VIEWPORT,
    OBT_PROP_NET_CURRENT_DESKTOP,
    OBT_PROP_NET_DESKTOP_NAMES,
    OBT_PROP_NET_ACTIVE_WINDOW,
/*    Atom net_restack_window;*/
    OBT_PROP_NET_WORKAREA,
    OBT_PROP_NET_SUPPORTING_WM_CHECK,
    OBT_PROP_NET_DESKTOP_LAYOUT,
    OBT_PROP_NET_SHOWING_DESKTOP,

    /* root window messages */
    OBT_PROP_NET_CLOSE_WINDOW,
    OBT_PROP_NET_WM_MOVERESIZE,
    OBT_PROP_NET_MOVERESIZE_WINDOW,
    OBT_PROP_NET_REQUEST_FRAME_EXTENTS,
    OBT_PROP_NET_RESTACK_WINDOW,

    /* helpful hints to apps that aren't used for anything */
    OBT_PROP_NET_WM_FULL_PLACEMENT,

    /* startup-notification extension */
    OBT_PROP_NET_STARTUP_ID,

    /* application window properties */
    OBT_PROP_NET_WM_NAME,
    OBT_PROP_NET_WM_VISIBLE_NAME,
    OBT_PROP_NET_WM_ICON_NAME,
    OBT_PROP_NET_WM_VISIBLE_ICON_NAME,
    OBT_PROP_NET_WM_DESKTOP,
    OBT_PROP_NET_WM_WINDOW_TYPE,
    OBT_PROP_NET_WM_STATE,
    OBT_PROP_NET_WM_STRUT,
    OBT_PROP_NET_WM_STRUT_PARTIAL,
    OBT_PROP_NET_WM_ICON,
    OBT_PROP_NET_WM_ICON_GEOMETRY,
    OBT_PROP_NET_WM_PID,
    OBT_PROP_NET_WM_ALLOWED_ACTIONS,
    OBT_PROP_NET_WM_WINDOW_OPACITY,
    OBT_PROP_NET_WM_USER_TIME,
/*  OBT_PROP_NET_WM_USER_TIME_WINDOW, */
    OBT_PROP_NET_FRAME_EXTENTS,

    /* application protocols */
    OBT_PROP_NET_WM_PING,
#ifdef SYNC
    OBT_PROP_NET_WM_SYNC_REQUEST,
    OBT_PROP_NET_WM_SYNC_REQUEST_COUNTER,
#endif

    OBT_PROP_NET_WM_WINDOW_TYPE_DESKTOP,
    OBT_PROP_NET_WM_WINDOW_TYPE_DOCK,
    OBT_PROP_NET_WM_WINDOW_TYPE_TOOLBAR,
    OBT_PROP_NET_WM_WINDOW_TYPE_MENU,
    OBT_PROP_NET_WM_WINDOW_TYPE_UTILITY,
    OBT_PROP_NET_WM_WINDOW_TYPE_SPLASH,
    OBT_PROP_NET_WM_WINDOW_TYPE_DIALOG,
    OBT_PROP_NET_WM_WINDOW_TYPE_NORMAL,

    OBT_PROP_NET_WM_ACTION_MOVE,
    OBT_PROP_NET_WM_ACTION_RESIZE,
    OBT_PROP_NET_WM_ACTION_MINIMIZE,
    OBT_PROP_NET_WM_ACTION_SHADE,
/*    OBT_PROP_NET_WM_ACTION_STICK,*/
    OBT_PROP_NET_WM_ACTION_MAXIMIZE_HORZ,
    OBT_PROP_NET_WM_ACTION_MAXIMIZE_VERT,
    OBT_PROP_NET_WM_ACTION_FULLSCREEN,
    OBT_PROP_NET_WM_ACTION_CHANGE_DESKTOP,
    OBT_PROP_NET_WM_ACTION_CLOSE,
    OBT_PROP_NET_WM_ACTION_ABOVE,
    OBT_PROP_NET_WM_ACTION_BELOW,

    OBT_PROP_NET_WM_STATE_MODAL,
/*    OBT_PROP_NET_WM_STATE_STICKY,*/
    OBT_PROP_NET_WM_STATE_MAXIMIZED_VERT,
    OBT_PROP_NET_WM_STATE_MAXIMIZED_HORZ,
    OBT_PROP_NET_WM_STATE_SHADED,
    OBT_PROP_NET_WM_STATE_SKIP_TASKBAR,
    OBT_PROP_NET_WM_STATE_SKIP_PAGER,
    OBT_PROP_NET_WM_STATE_HIDDEN,
    OBT_PROP_NET_WM_STATE_FULLSCREEN,
    OBT_PROP_NET_WM_STATE_ABOVE,
    OBT_PROP_NET_WM_STATE_BELOW,
    OBT_PROP_NET_WM_STATE_DEMANDS_ATTENTION,

    /* KDE atoms */

    OBT_PROP_KDE_WM_CHANGE_STATE,
    OBT_PROP_KDE_NET_WM_FRAME_STRUT,
    OBT_PROP_KDE_NET_WM_WINDOW_TYPE_OVERRIDE,

/*
    OBT_PROP_ROOTPMAPID,
    OBT_PROP_ESETROOTID,
*/

    /* Openbox specific atoms */

    OBT_PROP_OB_WM_ACTION_UNDECORATE,
    OBT_PROP_OB_WM_STATE_UNDECORATED,
    OBT_PROP_OPENBOX_PID, /* this is depreecated in favour of ob_control */
    OBT_PROP_OB_THEME,
    OBT_PROP_OB_CONFIG_FILE,
    OBT_PROP_OB_CONTROL,
    OBT_PROP_OB_VERSION,
    OBT_PROP_OB_APP_ROLE,
    OBT_PROP_OB_APP_TITLE,
    OBT_PROP_OB_APP_NAME,
    OBT_PROP_OB_APP_CLASS,
    OBT_PROP_OB_APP_GROUP_NAME,
    OBT_PROP_OB_APP_GROUP_CLASS,
    OBT_PROP_OB_APP_TYPE,

    OBT_PROP_NUM_ATOMS
} ObtPropAtom;

Atom obt_prop_atom(ObtPropAtom a);

typedef enum {
    /*! STRING is latin1 encoded.  It cannot contain control characters except
       for tab and line-feed. */
    OBT_PROP_TEXT_STRING = 1,
    /*! STRING text restricted to characters in the X Portable Character
      Set, which is a subset of latin1.
      http://static.cray-cyber.org/Documentation/NEC_SX_R10_1/G1AE02E/CHAP1.HTML
    */
    OBT_PROP_TEXT_STRING_XPCS = 2,
    /*! STRING text restricted to not allow any control characters to be
      present. */
    OBT_PROP_TEXT_STRING_NO_CC = 3,
    /* COMPOUND_TEXT is encoded in the current locale setting. */
    OBT_PROP_TEXT_COMPOUND_TEXT = 4,
    /* UTF8_STRING is encoded as utf-8. */
    OBT_PROP_TEXT_UTF8_STRING = 5,
} ObtPropTextType;

gboolean obt_prop_get32(Window win, Atom prop, Atom type, guint32 *ret);
gboolean obt_prop_get_array32(Window win, Atom prop, Atom type, guint32 **ret,
                              guint *nret);

gboolean obt_prop_get_text(Window win, Atom prop, ObtPropTextType type,
                           gchar **ret);
gboolean obt_prop_get_array_text(Window win, Atom prop,
                                 ObtPropTextType type,
                                 gchar ***ret);

void obt_prop_set32(Window win, Atom prop, Atom type, gulong val);
void obt_prop_set_array32(Window win, Atom prop, Atom type, gulong *val,
                          guint num);
void obt_prop_set_text(Window win, Atom prop, const gchar *str);
void obt_prop_set_array_text(Window win, Atom prop, const gchar *const *strs);

void obt_prop_erase(Window win, Atom prop);

void obt_prop_message(gint screen, Window about, Atom messagetype,
                      glong data0, glong data1, glong data2, glong data3,
                      glong data4, glong mask);
void obt_prop_message_to(Window to, Window about, Atom messagetype,
                         glong data0, glong data1, glong data2, glong data3,
                         glong data4, glong mask);

#define OBT_PROP_ATOM(prop) obt_prop_atom(OBT_PROP_##prop)

#define OBT_PROP_GET32(win, prop, type, ret) \
    (obt_prop_get32(win, OBT_PROP_ATOM(prop), OBT_PROP_ATOM(type), ret))
#define OBT_PROP_GETA32(win, prop, type, ret, nret) \
    (obt_prop_get_array32(win, OBT_PROP_ATOM(prop), OBT_PROP_ATOM(type), \
                          ret, nret))
#define OBT_PROP_GETS(win, prop, ret) \
    (obt_prop_get_text(win, OBT_PROP_ATOM(prop), 0, ret))
#define OBT_PROP_GETSS(win, prop, ret) \
    (obt_prop_get_array_text(win, OBT_PROP_ATOM(prop), 0, ret))

#define OBT_PROP_GETS_TYPE(win, prop, type, ret) \
    (obt_prop_get_text(win, OBT_PROP_ATOM(prop), OBT_PROP_TEXT_##type, ret))
#define OBT_PROP_GETSS_TYPE(win, prop, type, ret) \
    (obt_prop_get_array_text(win, OBT_PROP_ATOM(prop), \
                             OBT_PROP_TEXT_##type, ret))

#define OBT_PROP_GETS_UTF8(win, prop, ret) \
    OBT_PROP_GETS_TYPE(win, prop, UTF8_STRING, ret)
#define OBT_PROP_GETSS_UTF8(win, prop, ret) \
    OBT_PROP_GETSS_TYPE(win, prop, UTF8_STRING, ret)
#define OBT_PROP_GETS_XPCS(win, prop, ret) \
    OBT_PROP_GETS_TYPE(win, prop, STRING_XPCS, ret)

#define OBT_PROP_SET32(win, prop, type, val) \
    (obt_prop_set32(win, OBT_PROP_ATOM(prop), OBT_PROP_ATOM(type), val))
#define OBT_PROP_SETA32(win, prop, type, val, num) \
    (obt_prop_set_array32(win, OBT_PROP_ATOM(prop), OBT_PROP_ATOM(type), \
                          val, num))
#define OBT_PROP_SETS(win, prop, val) \
    (obt_prop_set_text(win, OBT_PROP_ATOM(prop), val))
#define OBT_PROP_SETSS(win, prop, strs) \
    (obt_prop_set_array_text(win, OBT_PROP_ATOM(prop), strs))

#define OBT_PROP_ERASE(win, prop) (obt_prop_erase(win, OBT_PROP_ATOM(prop)))

#define OBT_PROP_MSG(screen, about, msgtype, data0, data1, data2, data3, \
                     data4) \
    (obt_prop_message(screen, about, OBT_PROP_ATOM(msgtype), \
                      data0, data1, data2, data3, data4, \
                      SubstructureNotifyMask | SubstructureRedirectMask))

#define OBT_PROP_MSG_TO(to, about, msgtype, data0, data1, data2, data3, \
                        data4, mask) \
    (obt_prop_message_to(to, about, OBT_PROP_ATOM(msgtype), \
                         data0, data1, data2, data3, data4, mask))

G_END_DECLS

#endif /* __obt_prop_h */
