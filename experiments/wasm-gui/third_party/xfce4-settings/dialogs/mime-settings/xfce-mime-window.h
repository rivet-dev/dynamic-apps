/*
 * Copyright (C) 2012 Nick Schermer <nick@xfce.org>
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

#ifndef __XFCE_MIME_WINDOW_H__
#define __XFCE_MIME_WINDOW_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _XfceMimeWindowClass XfceMimeWindowClass;
typedef struct _XfceMimeWindow      XfceMimeWindow;

#define XFCE_TYPE_MIME_WINDOW            (xfce_mime_window_get_type ())
#define XFCE_MIME_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_MIME_WINDOW, XfceMimeWindow))
#define XFCE_MIME_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_MIME_WINDOW, XfceMimeWindowClass))
#define XFCE_IS_MIME_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_MIME_WINDOW))
#define XFCE_IS_MIME_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_MIME_WINDOW))
#define XFCE_MIME_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_MIME_WINDOW, XfceMimeWindowClass))

GType           xfce_mime_window_get_type      (void) G_GNUC_CONST;

XfceMimeWindow *xfce_mime_window_new           (void) G_GNUC_MALLOC;
GtkWidget      *xfce_mime_window_create_dialog (XfceMimeWindow *settings);
GtkWidget      *xfce_mime_window_create_plug   (XfceMimeWindow *settings,
                                                gint            socket_id);

G_END_DECLS

#endif /* !__XFCE_MIME_WINDOW_H__ */
