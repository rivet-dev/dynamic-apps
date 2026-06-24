/* $Id$ */
/*-
 * Copyright (c) 2003-2006 Benedikt Meurer <benny@xfce.org>
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 */

#if !defined(_LIBXFCE4UTIL_INSIDE_LIBXFCE4UTIL_H) && !defined(LIBXFCE4UTIL_COMPILATION)
#error "Only <libxfce4util/libxfce4util.h> can be included directly, this file may disappear or change contents"
#endif

#ifndef __XFCE_LICENSE_H__
#define __XFCE_LICENSE_H__

G_BEGIN_DECLS

/**
 * XfceLicenseTextType:
 * @XFCE_LICENSE_TEXT_BSD  : the BSD License.
 * @XFCE_LICENSE_TEXT_GPL  : the GNU General Public License.
 * @XFCE_LICENSE_TEXT_LGPL : the GNU Lesser General Public License.
 *
 * The license text to return from xfce_get_license_text().
 **/
typedef enum /*< enum >*/
{
  XFCE_LICENSE_TEXT_BSD,
  XFCE_LICENSE_TEXT_GPL,
  XFCE_LICENSE_TEXT_LGPL,
} XfceLicenseTextType;

const gchar *xfce_get_license_text (XfceLicenseTextType license_type) G_GNUC_PURE;

#define XFCE_LICENSE_BSD  (xfce_get_license_text (XFCE_LICENSE_TEXT_BSD))
#define XFCE_LICENSE_GPL  (xfce_get_license_text (XFCE_LICENSE_TEXT_GPL))
#define XFCE_LICENSE_LGPL (xfce_get_license_text (XFCE_LICENSE_TEXT_LGPL))

G_END_DECLS

#endif /* !__XFCE_LICENSE_H__ */
