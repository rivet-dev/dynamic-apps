/*
 * Copyright (C) 2006-2010 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *               2006-2008 Jim Huang <jserv.tw@gmail.com>
 *               2008-2009 Fred Chien <fred@lxde.org>
 *               2009 Marty Jack <martyj19@comcast.net>
 *               2011-2013 Henry Gebhardt <hsggebhardt@gmail.com>
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

#ifndef __MISC_H__
#define __MISC_H__ 1

#include <X11/Xatom.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <stdio.h>
#include <libfm/fm.h>

#include "panel.h"

G_BEGIN_DECLS

extern Atom a_UTF8_STRING;
extern Atom a_XROOTPMAP_ID;

extern Atom a_WM_STATE;
extern Atom a_WM_DELETE_WINDOW;
extern Atom a_WM_PROTOCOLS;
extern Atom a_NET_WORKAREA;
extern Atom a_NET_CLIENT_LIST;
extern Atom a_NET_CLIENT_LIST_STACKING;
extern Atom a_NET_NUMBER_OF_DESKTOPS;
extern Atom a_NET_CURRENT_DESKTOP;
extern Atom a_NET_DESKTOP_VIEWPORT;
extern Atom a_NET_DESKTOP_NAMES;
extern Atom a_NET_ACTIVE_WINDOW;
extern Atom a_NET_CLOSE_WINDOW;
extern Atom a_NET_SHOWING_DESKTOP;
extern Atom a_NET_SUPPORTED;
extern Atom a_NET_WM_STATE;
extern Atom a_NET_WM_STATE_SKIP_TASKBAR;
extern Atom a_NET_WM_STATE_SKIP_PAGER;
extern Atom a_NET_WM_STATE_STICKY;
extern Atom a_NET_WM_STATE_HIDDEN;
extern Atom a_NET_WM_STATE_SHADED;

#define a_NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define a_NET_WM_STATE_ADD           1    /* add/set property */
#define a_NET_WM_STATE_TOGGLE        2    /* toggle property  */

extern Atom a_NET_WM_WINDOW_TYPE;
extern Atom a_NET_WM_WINDOW_TYPE_DESKTOP;
extern Atom a_NET_WM_WINDOW_TYPE_DOCK;
extern Atom a_NET_WM_WINDOW_TYPE_TOOLBAR;
extern Atom a_NET_WM_WINDOW_TYPE_MENU;
extern Atom a_NET_WM_WINDOW_TYPE_UTILITY;
extern Atom a_NET_WM_WINDOW_TYPE_SPLASH;
extern Atom a_NET_WM_WINDOW_TYPE_DIALOG;
extern Atom a_NET_WM_WINDOW_TYPE_NORMAL;

extern Atom a_NET_WM_DESKTOP;
extern Atom a_NET_WM_NAME;
extern Atom a_NET_WM_VISIBLE_NAME;
extern Atom a_NET_WM_STRUT;
extern Atom a_NET_WM_STRUT_PARTIAL;
extern Atom a_NET_WM_ICON;
extern Atom a_KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR;

extern Atom a_NET_SYSTEM_TRAY_OPCODE;
extern Atom a_NET_SYSTEM_TRAY_MESSAGE_DATA;
extern Atom a_NET_SYSTEM_TRAY_ORIENTATION;
extern Atom a_MANAGER;

extern Atom a_LXPANEL_CMD; /* for private client message */

/* Decoded value of WM_STATE property. */
typedef struct {
    unsigned int modal : 1;
    unsigned int sticky : 1;
    unsigned int maximized_vert : 1;
    unsigned int maximized_horz : 1;
    unsigned int shaded : 1;
    unsigned int skip_taskbar : 1;
    unsigned int skip_pager : 1;
    unsigned int hidden : 1;
    unsigned int fullscreen : 1;
    unsigned int above : 1;
    unsigned int below : 1;
} NetWMState;

/* Decoded value of _NET_WM_WINDOW_TYPE property. */
typedef struct {
    unsigned int desktop : 1;
    unsigned int dock : 1;
    unsigned int toolbar : 1;
    unsigned int menu : 1;
    unsigned int utility : 1;
    unsigned int splash : 1;
    unsigned int dialog : 1;
    unsigned int normal : 1;
} NetWMWindowType;

void Xclimsgx(Screen *screen, Window win, Atom type, long l0, long l1, long l2, long l3, long l4);
void Xclimsgwm(Window win, Atom type, Atom arg);
void *get_xaproperty (Window win, Atom prop, Atom type, int *nitems);
char *get_textproperty(Window win, Atom prop);
void *get_utf8_property(Window win, Atom atom);
char **get_utf8_property_list(Window win, Atom atom, int *count);

void resolve_atoms();
//Window Select_Window(Display *dpy);
int get_net_number_of_desktops();
int get_net_current_desktop ();
int get_net_wm_desktop(Window win);
int get_wm_state (Window win);
void get_net_wm_state(Window win, NetWMState *nws);
void get_net_wm_window_type(Window win, NetWMWindowType *nwwt);
GPid get_net_wm_pid(Window win);

/**
 * panel_handle_x_error
 * @d: X display
 * @ev: X error event
 *
 * Prints X error message to stderr if logging was enabled.
 */
extern int panel_handle_x_error(Display * d, XErrorEvent * ev);
extern int panel_handle_x_error_swallow_BadWindow_BadDrawable(Display * d, XErrorEvent * ev);

/**
 * expand_tilda
 * @file: file path
 *
 * If first char of @file is a tilda (~) then replaces it with content of
 * environment variable HOME.
 *
 * Returns: (transfer full): new allocated path string.
 */
gchar *expand_tilda(const gchar *file);

/**
 * gcolor2rgb24
 * @color: a color structure
 *
 * Creates integer @color representation which can be easily converted
 * into string using printf conversion "#%06x".
 *
 * Returns: a 32-bit integer.
 */
guint32 gcolor2rgb24(GdkColor *color);

/**
 * lxpanel_button_new_for_icon
 * @panel: panel instance
 * @name: icon name or path
 * @color: (allow-none): hilight color for icon
 * @label: (allow-none): optional label for button
 *
 * Creates new #GtkEventBox button which will follow theme and icon size
 * changes on @panel. If icon name is not found in theme then fallback
 * name "application-x-executable" will be used. Button is flagged to be
 * displayed.
 *
 * Returns: (transfer full): a new #GtkEventBox widget.
 */
extern GtkWidget *lxpanel_button_new_for_icon(LXPanel *panel, const gchar *name, GdkColor *color, const gchar *label);
extern GtkWidget *lxpanel_button_new_for_fm_icon(LXPanel *panel, FmIcon *icon, GdkColor *color, const gchar *label);

/**
 * lxpanel_button_set_icon
 * @btn: a button instance
 * @name: icon name or path
 * @size: new icon size
 *
 * Updates icon in the button created with lxpanel_button_new_for_icon()
 * or lxpanel_button_new_for_fm_icon() before. If @size > 0 then stop
 * following panel icon size and use this fixed size, if @size is 0 then
 * do no changes on icons size.
 */
extern void lxpanel_button_set_icon(GtkWidget* btn, const gchar *name, gint size);
extern void lxpanel_button_update_icon(GtkWidget* btn, FmIcon *icon, gint size);

/**
 * lxpanel_button_set_label
 * @btn: a button instance
 * @label: new label text
 *
 * Changes text of label created with lxpanel_button_new_for_icon(),
 * lxpanel_button_new_for_fm_icon(), or lxpanel_button_compose(). Does
 * nothing if those API were called with NULL passed as label.
 *
 * Returns: %TRUE in case of success.
 *
 * Since: 0.8.0
 */
extern gboolean lxpanel_button_set_label(GtkWidget *btn, const char *label);

/**
 * lxpanel_button_compose
 * @event_box: a widget to add image and label
 * @image: an image widget
 * @color: (allow-none): hilight color for icon
 * @label: (allow-none): optional label for button
 *
 * Composes button similarly to lxpanel_button_new_for_icon() but using
 * existing container @event_box and @image. The @image should be created
 * using lxpanel_image_new_for_icon() or lxpanel_image_new_for_fm_icon(),
 * and it can be updated later using lxpanel_button_set_icon() API.
 *
 * Returns: (transfer none): @event_box.
 *
 * Since: 0.8.0
 */
extern GtkWidget *lxpanel_button_compose(GtkWidget *event_box, GtkWidget *image,
                                         GdkColor *color, const gchar *label);

/**
 * lxpanel_image_new_for_icon
 * @panel: (allow-none): panel instance
 * @name: icon name or image path
 * @height: image size
 * @fallback: (allow-none): fallback image name or path
 *
 * Creates new #GtkImage which will follow theme if @name is a themed
 * icon name. If @height is -1 then created image will also follow icon
 * size changes on @panel. If icon not found in theme and @fallback is
 * not %NULL then it will be used to load icon or image. Otherwise the
 * "application-x-executable" will be used as fallback.
 *
 * Returns: (transfer full): a new #GtkImage widget.
 *
 * Since: 0.8.0
 */
extern GtkWidget *lxpanel_image_new_for_icon(LXPanel *panel, const gchar *name,
                                             gint height, const gchar *fallback);
extern GtkWidget *lxpanel_image_new_for_fm_icon(LXPanel *panel, FmIcon *icon,
                                                gint height, const gchar *fallback);

/**
 * lxpanel_image_change_icon
 * @img: an image
 * @name: icon name or image path
 * @fallback: (allow-none): fallback image name or path
 *
 * Changes an image @img created by lxpanel_image_new_for_icon() to use
 * new icon @name and @fallback.
 *
 * Returns: %TRUE.
 *
 * Since: 0.8.0
 */
extern gboolean lxpanel_image_change_icon(GtkWidget *img, const gchar *name,
                                          const char *fallback);

G_END_DECLS

#endif
