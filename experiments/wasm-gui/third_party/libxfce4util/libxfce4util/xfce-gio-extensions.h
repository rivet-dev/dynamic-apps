/*-
 * Copyright (c) 2021 Yongha Hwang <mshrimp@sogang.ac.kr>
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
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#if !defined(_LIBXFCE4UTIL_INSIDE_LIBXFCE4UTIL_H) && !defined(LIBXFCE4UTIL_COMPILATION)
#error "Only <libxfce4util/libxfce4util.h> can be included directly, this file may disappear or change contents"
#endif

#ifndef __XFCE_GIO_EXTENSIONS_H__
#define __XFCE_GIO_EXTENSIONS_H__

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

gboolean      xfce_g_file_metadata_is_supported   (GFile        *file);
gchar        *xfce_g_file_create_checksum         (GFile        *file,
                                                   GCancellable *cancellable,
                                                   GError      **error);
gboolean      xfce_g_file_set_trusted             (GFile        *file,
                                                   gboolean      is_trusted,
                                                   GCancellable *cancellable,
                                                   GError      **error);
gboolean      xfce_g_file_is_trusted              (GFile        *file,
                                                   GCancellable *cancellable,
                                                   GError      **error);

G_END_DECLS

#endif /*__XFCE_GIO_EXTENSIONS_H__*/
