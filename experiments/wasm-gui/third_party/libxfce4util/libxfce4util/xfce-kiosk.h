/* $Id$ */
/*-
 * Copyright (c) 2003-2006 Benedikt Meurer <benny@xfce.org>
 * All rights reserved.
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

#ifndef __XFCE_KIOSK_H__
#define __XFCE_KIOSK_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define XFCE_TYPE_KIOSK xfce_kiosk_get_type ()
G_DECLARE_FINAL_TYPE (XfceKiosk, xfce_kiosk, XFCE, KIOSK, GObject)

XfceKiosk *xfce_kiosk_new   (const gchar     *module) G_GNUC_MALLOC;
gboolean   xfce_kiosk_query (const XfceKiosk *kiosk,
                             const gchar     *capability);
void       xfce_kiosk_free  (XfceKiosk       *kiosk);

G_END_DECLS

#endif /* !__XFCE_KIOSK_H__ */
