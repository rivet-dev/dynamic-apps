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

#ifndef __PANEL_DBUS_SERVICE_H__
#define __PANEL_DBUS_SERVICE_H__

#include <glib.h>
#include <common/panel-dbus.h>

G_BEGIN_DECLS

typedef struct _PanelDBusServiceClass PanelDBusServiceClass;
typedef struct _PanelDBusService      PanelDBusService;

#define PANEL_TYPE_DBUS_SERVICE            (panel_dbus_service_get_type ())
#define PANEL_DBUS_SERVICE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PANEL_TYPE_DBUS_SERVICE, PanelDBusService))
#define PANEL_DBUS_CLASS_SERVICE(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), PANEL_TYPE_DBUS_SERVICE, PanelDBusServiceClass))
#define PANEL_IS_DBUS_SERVICE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PANEL_TYPE_DBUS_SERVICE))
#define PANEL_IS_DBUS_SERVICE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), PANEL_TYPE_DBUS_SERVICE))
#define PANEL_DBUS_SERVICE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), PANEL_TYPE_DBUS_SERVICE, PanelDBusServiceClass))



GType               panel_dbus_service_get_type    (void) G_GNUC_CONST;

PanelDBusService   *panel_dbus_service_get         (void);

void                panel_dbus_service_exit_panel  (gboolean          restart);

gboolean            panel_dbus_service_get_restart (void);

G_END_DECLS

#endif /* !__PANEL_DBUS_SERVICE_H__ */
