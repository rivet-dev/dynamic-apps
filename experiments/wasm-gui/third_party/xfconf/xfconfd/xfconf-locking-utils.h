/*
 *  xfconf
 *
 *  Copyright (c) 2007 Brian Tarricone <bjt23@cornell.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __XFCONF_UTIL_H__
#define __XFCONF_UTIL_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define  XFCONF_DBUS_TYPE_G_DOUBLE_ARRAY  (dbus_g_type_get_collection("GArray", G_TYPE_DOUBLE))

G_GNUC_INTERNAL gboolean xfconf_user_is_in_list(const gchar *list);

G_END_DECLS

#endif  /* __XFCONF_UTIL_H__ */
