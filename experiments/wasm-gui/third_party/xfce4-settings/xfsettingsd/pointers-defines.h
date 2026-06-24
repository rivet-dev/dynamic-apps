/*
 *  Copyright (c) 2011 Nick Schermer <nick@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/XI.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XIproto.h>

#ifndef __POINTERS_DEFINES_H__
#define __POINTERS_DEFINES_H__

/* Xi 1.4 is required */
#define MIN_XI_VERS_MAJOR 1
#define MIN_XI_VERS_MINOR 4

/* test if the required version of inputproto (1.4.2) is available */
#undef DEVICE_HOTPLUGGING
#ifdef XI_Add_DevicePresenceNotify_Major
#  if XI_Add_DevicePresenceNotify_Major >= 1 && defined (DeviceRemoved)
#    define DEVICE_HOTPLUGGING
#  else
#    undef DEVICE_HOTPLUGGING
#  endif
#endif

/* test if device properties are available */
#undef DEVICE_PROPERTIES
#ifdef XI_Add_DeviceProperties_Major
#  define DEVICE_PROPERTIES
#endif

#ifndef IsXExtensionPointer
#define IsXExtensionPointer 4
#endif

#endif /* !__POINTERS_DEFINES_H__ */
