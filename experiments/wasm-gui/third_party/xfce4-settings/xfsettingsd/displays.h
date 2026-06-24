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

#ifndef __DISPLAYS_H__
#define __DISPLAYS_H__

typedef struct _XfceDisplaysHelperClass XfceDisplaysHelperClass;
typedef struct _XfceDisplaysHelper      XfceDisplaysHelper;

#define XFCE_TYPE_DISPLAYS_HELPER            (xfce_displays_helper_get_type ())
#define XFCE_DISPLAYS_HELPER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_DISPLAYS_HELPER, XfceDisplaysHelper))
#define XFCE_DISPLAYS_HELPER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_DISPLAYS_HELPER, XfceDisplaysHelperClass))
#define XFCE_IS_DISPLAYS_HELPER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_DISPLAYS_HELPER))
#define XFCE_IS_DISPLAYS_HELPER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_DISPLAYS_HELPER))
#define XFCE_DISPLAYS_HELPER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_DISPLAYS_HELPER, XfceDisplaysHelperClass))

GType xfce_displays_helper_get_type (void) G_GNUC_CONST;

#endif /* !__DISPLAYS_H__ */
