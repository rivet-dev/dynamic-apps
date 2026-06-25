/*
 * Copyright (C) 2011 Nick Schermer <nick@xfce.org>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __XFCE_APPFINDER_WINDOW_H__
#define __XFCE_APPFINDER_WINDOW_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _XfceAppfinderWindowClass XfceAppfinderWindowClass;
typedef struct _XfceAppfinderWindow      XfceAppfinderWindow;

#define XFCE_TYPE_APPFINDER_WINDOW            (xfce_appfinder_window_get_type ())
#define XFCE_APPFINDER_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_APPFINDER_WINDOW, XfceAppfinderWindow))
#define XFCE_APPFINDER_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_APPFINDER_WINDOW, XfceAppfinderWindowClass))
#define XFCE_IS_APPFINDER_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_APPFINDER_WINDOW))
#define XFCE_IS_APPFINDER_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_APPFINDER_WINDOW))
#define XFCE_APPFINDER_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_APPFINDER_WINDOW, XfceAppfinderWindowClass))

GType      xfce_appfinder_window_get_type     (void) G_GNUC_CONST;

void       xfce_appfinder_window_set_expanded (XfceAppfinderWindow *window,
                                               gboolean             expanded);

G_END_DECLS

#endif /* !__XFCE_APPFINDER_WINDOW_H__ */

