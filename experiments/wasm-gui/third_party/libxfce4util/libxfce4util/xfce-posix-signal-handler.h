/*
 * Copyright (c) 2007 Brian Tarricone <bjt23@cornell.edu>
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 */

#if !defined(_LIBXFCE4UTIL_INSIDE_LIBXFCE4UTIL_H) && !defined(LIBXFCE4UTIL_COMPILATION)
#error "Only <libxfce4util/libxfce4util.h> can be included directly, this file may disappear or change contents"
#endif

#ifndef __XFCE_POSIX_SIGNAL_HANDLER_H__
#define __XFCE_POSIX_SIGNAL_HANDLER_H__

#include <glib.h>

G_BEGIN_DECLS

/**
 * XfcePosixSignalHandler:
 * @signal: The signal that was caught.
 * @user_data: The @user_data parameter passed when the handler was registered.
 */

typedef void (*XfcePosixSignalHandler)(gint signal, gpointer user_data);

gboolean xfce_posix_signal_handler_init(GError **error);
void xfce_posix_signal_handler_shutdown(void);

gboolean xfce_posix_signal_handler_set_handler(gint signal,
                                               XfcePosixSignalHandler handler,
                                               gpointer user_data,
                                               GError **error);
void xfce_posix_signal_handler_restore_handler(gint signal);

G_END_DECLS

#endif  /* __XFCE_POSIX_SIGNAL_HANDLER_H__ */
