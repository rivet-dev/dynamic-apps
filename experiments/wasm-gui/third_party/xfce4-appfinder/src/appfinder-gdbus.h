/*
 * Copyright (C) 2013 Nick Schermer <nick@xfce.org>
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

#ifndef __XFCE_APPFINDER_GDBUS_H__
#define __XFCE_APPFINDER_GDBUS_H__

G_BEGIN_DECLS

#include <gio/gio.h>

gboolean appfinder_gdbus_service     (GError      **error);

gboolean appfinder_gdbus_quit        (GError      **error);

gboolean appfinder_gdbus_open_window (gboolean      expanded,
                                      const gchar  *startup_id,
                                      GError      **error);

G_END_DECLS

#endif /* !__XFCE_APPFINDER_GDBUS_H__ */
