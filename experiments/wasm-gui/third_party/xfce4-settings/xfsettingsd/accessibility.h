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

#ifndef __ACCESSIBILITY_H__
#define __ACCESSIBILITY_H__

typedef struct _XfceAccessibilityHelperClass XfceAccessibilityHelperClass;
typedef struct _XfceAccessibilityHelper      XfceAccessibilityHelper;

#define XFCE_TYPE_ACCESSIBILITY_HELPER            (xfce_accessibility_helper_get_type ())
#define XFCE_ACCESSIBILITY_HELPER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_ACCESSIBILITY_HELPER, XfceAccessibilityHelper))
#define XFCE_ACCESSIBILITY_HELPER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_ACCESSIBILITY_HELPER, XfceAccessibilityHelperClass))
#define XFCE_IS_ACCESSIBILITY_HELPER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_ACCESSIBILITY_HELPER))
#define XFCE_IS_ACCESSIBILITY_HELPER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_ACCESSIBILITY_HELPER))
#define XFCE_ACCESSIBILITY_HELPER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_ACCESSIBILITY_HELPER, XfceAccessibilityHelperClass))

GType xfce_accessibility_helper_get_type (void) G_GNUC_CONST;

#endif /* !__ACCESSIBILITY_H__ */
