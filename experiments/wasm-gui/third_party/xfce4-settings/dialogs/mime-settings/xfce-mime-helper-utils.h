/*-
 * Copyright (c) 2003-2006 Benedikt Meurer <benny@xfce.org>.
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __XFCE_MIME_HELPER_UTILS_H__
#define __XFCE_MIME_HELPER_UTILS_H__

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include "xfce-mime-helper.h"

G_BEGIN_DECLS

gboolean xfce_mime_helper_category_from_string (const gchar       *string,
                                          XfceMimeHelperCategory *category_return);
gchar   *xfce_mime_helper_category_to_string   (XfceMimeHelperCategory  category) G_GNUC_MALLOC;

G_END_DECLS

#endif /* !__XFCE_MIME_HELPER_UTILS_H__ */
