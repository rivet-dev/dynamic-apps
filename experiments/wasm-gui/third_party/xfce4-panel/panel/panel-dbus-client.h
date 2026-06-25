/*
 * Copyright (C) 2008-2010 Nick Schermer <nick@xfce.org>
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

#ifndef __PANEL_DBUS_CLIENT_H__
#define __PANEL_DBUS_CLIENT_H__

#include <glib.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

gboolean  panel_dbus_client_display_preferences_dialog (guint         active,
                                                        guint         socket_id,
                                                        GError      **error);

gboolean  panel_dbus_client_display_items_dialog       (guint         active,
                                                        GError      **error);

gboolean  panel_dbus_client_save                       (GError      **error);

gboolean  panel_dbus_client_add_new_item               (const gchar  *plugin_name,
                                                        gchar       **arguments,
                                                        GError      **error);

gboolean  panel_dbus_client_plugin_event               (const gchar  *plugin_event,
                                                        gboolean     *return_succeed,
                                                        GError      **error);

gboolean  panel_dbus_client_terminate                  (gboolean      restart,
                                                        GError      **error);

G_END_DECLS

#endif /* !__PANEL_DBUS_CLIENT_H__ */

