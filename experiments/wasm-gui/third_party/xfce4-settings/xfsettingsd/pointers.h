/*
 *  Copyright (c) 2008 Nick Schermer <nick@xfce.org>
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

#ifndef __POINTERS_H__
#define __POINTERS_H__

typedef struct _XfcePointersHelperClass XfcePointersHelperClass;
typedef struct _XfcePointersHelper      XfcePointersHelper;

#define XFCE_TYPE_POINTERS_HELPER            (xfce_pointers_helper_get_type ())
#define XFCE_POINTERS_HELPER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_POINTERS_HELPER, XfcePointersHelper))
#define XFCE_POINTERS_HELPER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_POINTERS_HELPER, XfcePointersHelperClass))
#define XFCE_IS_POINTERS_HELPER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_POINTERS_HELPER))
#define XFCE_IS_POINTERS_HELPER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_POINTERS_HELPER))
#define XFCE_POINTERS_HELPER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_POINTERS_HELPER, XfcePointersHelperClass))

GType xfce_pointers_helper_get_type (void) G_GNUC_CONST;

#endif /* !__POINTERS_H__ */
