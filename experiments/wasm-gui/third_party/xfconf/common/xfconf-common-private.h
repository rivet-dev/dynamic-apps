/*
 *  xfconf
 *
 *  Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; version 2
 *  of the License ONLY.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __XFCONF_COMMON_PRIVATE_H__
#define __XFCONF_COMMON_PRIVATE_H__

#include <glib.h>

enum _XfconfError;

gboolean _xfconf_error_from_dbus_error_name(const gchar *error_name,
                                            enum _XfconfError *xfconf_error);

#endif  /* __XFCONF_COMMON_PRIVATE_H__ */
