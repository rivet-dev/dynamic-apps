/*
 *  Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
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
 *
 *  XKB Extension code taken from the original mcs-keyboard-plugin written
 *  by Olivier Fourdan.
 */

#include <X11/extensions/XInput.h>

#ifndef __KEYBOARDS_H__
#define __KEYBOARDS_H__

typedef struct _XfceKeyboardsHelperClass XfceKeyboardsHelperClass;
typedef struct _XfceKeyboardsHelper      XfceKeyboardsHelper;

#define XFCE_TYPE_KEYBOARDS_HELPER            (xfce_keyboards_helper_get_type ())
#define XFCE_KEYBOARDS_HELPER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_KEYBOARDS_HELPER, XfceKeyboardsHelper))
#define XFCE_KEYBOARDS_HELPER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_KEYBOARDS_HELPER, XfceKeyboardsHelperClass))
#define XFCE_IS_KEYBOARDS_HELPER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_KEYBOARDS_HELPER))
#define XFCE_IS_KEYBOARDS_HELPER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_KEYBOARDS_HELPER))
#define XFCE_KEYBOARDS_HELPER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_KEYBOARDS_HELPER, XfceKeyboardsHelperClass))

/* test if the required version of inputproto (1.4.2) is available */
#undef DEVICE_HOTPLUGGING
#ifdef XI_Add_DevicePresenceNotify_Major
#  if XI_Add_DevicePresenceNotify_Major >= 1 && defined (DeviceRemoved)
#    define DEVICE_HOTPLUGGING
#  else
#    undef DEVICE_HOTPLUGGING
#  endif
#endif

GType xfce_keyboards_helper_get_type (void) G_GNUC_CONST;

#endif /* !__KEYBOARDS_H__ */
