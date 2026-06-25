/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2004-2007 Brian Tarricone, <bjt23@cornell.edu>
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

#ifndef _XFCE_DESKTOP_H_
#define _XFCE_DESKTOP_H_

#include <gtk/gtk.h>
#include <xfconf/xfconf.h>

#include "xfce-backdrop.h"

G_BEGIN_DECLS

#define XFCE_TYPE_DESKTOP              (xfce_desktop_get_type())
#define XFCE_DESKTOP(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), XFCE_TYPE_DESKTOP, XfceDesktop))
#define XFCE_DESKTOP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), XFCE_TYPE_DESKTOP, XfceDesktopClass))
#define XFCE_IS_DESKTOP(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), XFCE_TYPE_DESKTOP))
#define XFCE_IS_DESKTOP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), XFCE_TYPE_DESKTOP))
#define XFCE_DESKTOP_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), XFCE_TYPE_DESKTOP, XfceDesktopClass))

typedef struct _XfceDesktop XfceDesktop;
typedef struct _XfceDesktopClass XfceDesktopClass;
typedef struct _XfceDesktopPrivate XfceDesktopPrivate;

typedef void (*SessionLogoutFunc)();

typedef enum
{
    XFCE_DESKTOP_ICON_STYLE_NONE = 0,
    XFCE_DESKTOP_ICON_STYLE_WINDOWS,
    XFCE_DESKTOP_ICON_STYLE_FILES,
} XfceDesktopIconStyle;

struct _XfceDesktop
{
    GtkWindow window;

    /*< private >*/
    XfceDesktopPrivate *priv;
};

struct _XfceDesktopClass
{
    GtkWindowClass parent_class;
};

GType xfce_desktop_get_type                     (void) G_GNUC_CONST;

GtkWidget *xfce_desktop_new(GdkScreen *gscreen,
                            XfconfChannel *channel,
                            const gchar *property_prefix);

gint xfce_desktop_get_n_monitors(XfceDesktop *desktop);

void xfce_desktop_set_icon_style(XfceDesktop *desktop,
                                 XfceDesktopIconStyle style);
XfceDesktopIconStyle xfce_desktop_get_icon_style(XfceDesktop *desktop);

void xfce_desktop_set_icon_size(XfceDesktop *desktop,
                                guint icon_size);

void xfce_desktop_set_primary(XfceDesktop *desktop,
                              gboolean primary);

void xfce_desktop_set_use_icon_font_size(XfceDesktop *desktop,
                                         gboolean use_system);
void xfce_desktop_set_icon_font_size(XfceDesktop *desktop,
                                     guint font_size_points);

void xfce_desktop_set_center_text(XfceDesktop *desktop,
                                  gboolean center_text);

void xfce_desktop_set_session_logout_func(XfceDesktop *desktop,
                                          SessionLogoutFunc logout_func);

void xfce_desktop_freeze_updates(XfceDesktop *desktop);
void xfce_desktop_thaw_updates(XfceDesktop *desktop);


void xfce_desktop_popup_root_menu(XfceDesktop *desktop,
                                  guint button,
                                  guint activate_time);
void xfce_desktop_popup_secondary_root_menu(XfceDesktop *desktop,
                                            guint button,
                                            guint activate_time);

void xfce_desktop_refresh(XfceDesktop *desktop,
                          gboolean advance_wallpaper,
                          gboolean all_monitors);

void xfce_desktop_arrange_icons(XfceDesktop *desktop);

gboolean xfce_desktop_get_cycle_backdrop(XfceDesktop *desktop);

G_END_DECLS

#endif
