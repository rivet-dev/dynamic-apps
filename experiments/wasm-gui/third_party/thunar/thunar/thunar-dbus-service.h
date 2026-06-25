/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __THUNAR_DBUS_SERVICE_H__
#define __THUNAR_DBUS_SERVICE_H__

#include <exo/exo.h>

G_BEGIN_DECLS;

typedef struct _ThunarDBusServiceClass ThunarDBusServiceClass;
typedef struct _ThunarDBusService      ThunarDBusService;

#define THUNAR_TYPE_DBUS_SERVICE             (thunar_dbus_service_get_type ())
#define THUNAR_DBUS_SERVICE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_DBUS_SERVICE, ThunarDBusService))
#define THUNAR_DBUS_SERVICE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_DBUS_SERVICE, ThunarDBusServiceClass))
#define THUNAR_IS_DBUS_SERVICE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_DBUS_SERVICE))
#define THUNAR_IS_DBUS_SERVICE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_DBUS_BRIGDE))
#define THUNAR_DBUS_SERVICE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_DBUS_SERVICE, ThunarDBusServicetClass))

GType    thunar_dbus_service_get_type       (void) G_GNUC_CONST;

gboolean thunar_dbus_service_export_on_connection (ThunarDBusService *service,
                                                   GDBusConnection   *connection,
                                                   GError           **error);

G_END_DECLS;

#endif /* !__THUNAR_DBUS_SERVICE_H__ */
