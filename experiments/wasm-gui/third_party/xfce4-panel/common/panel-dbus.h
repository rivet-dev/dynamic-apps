/*
 * Copyright (C) 2008-2010 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __PANEL_DBUS_H__
#define __PANEL_DBUS_H__

/* panel dbus names */
#define PANEL_DBUS_NAME              "org.xfce.Panel"
#define PANEL_DBUS_PATH              "/org/xfce/Panel"
#define PANEL_DBUS_INTERFACE         PANEL_DBUS_NAME
#define PANEL_DBUS_WRAPPER_PATH      PANEL_DBUS_PATH "/Wrapper/%d"
#define PANEL_DBUS_WRAPPER_INTERFACE PANEL_DBUS_INTERFACE ".Wrapper"

enum
{
  DBUS_SET_TYPE,
  DBUS_SET_VALUE
};

#endif /* !__PANEL_DBUS_H__ */
