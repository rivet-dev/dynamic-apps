/*
 * Copyright (c) 2002      Anders Carlsson <andersca@gnu.org>
 * Copyright (c) 2003-2006 Vincent Untz
 * Copyright (c) 2008      Red Hat, Inc.
 * Copyright (c) 2009-2010 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __SYSTRAY_SOCKET_H__
#define __SYSTRAY_SOCKET_H__

#include <gtk/gtk.h>
#include <gtk/gtkx.h>

typedef struct _SystraySocketClass SystraySocketClass;
typedef struct _SystraySocket      SystraySocket;

#define XFCE_TYPE_SYSTRAY_SOCKET            (systray_socket_get_type ())
#define XFCE_SYSTRAY_SOCKET(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_SYSTRAY_SOCKET, SystraySocket))
#define XFCE_SYSTRAY_SOCKET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_SYSTRAY_SOCKET, SystraySocketClass))
#define XFCE_IS_SYSTRAY_SOCKET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_SYSTRAY_SOCKET))
#define XFCE_IS_SYSTRAY_SOCKET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_SYSTRAY_SOCKET))
#define XFCE_SYSTRAY_SOCKET_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_SYSTRAY_SOCKET, SystraySocketClass))

GType            systray_socket_get_type      (void) G_GNUC_CONST;

void             systray_socket_register_type (GTypeModule     *type_module);

GtkWidget       *systray_socket_new           (GdkScreen       *screen,
                                               Window           window) G_GNUC_MALLOC;

void             systray_socket_force_redraw  (SystraySocket   *socket);

gboolean         systray_socket_is_composited (SystraySocket   *socket);

const gchar     *systray_socket_get_name      (SystraySocket   *socket);

Window          *systray_socket_get_window    (SystraySocket   *socket);

gboolean         systray_socket_get_hidden    (SystraySocket   *socket);

void             systray_socket_set_hidden    (SystraySocket   *socket,
                                               gboolean         hidden);

#endif /* !__SYSTRAY_SOCKET_H__ */
