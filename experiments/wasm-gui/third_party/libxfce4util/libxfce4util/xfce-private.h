/* $Id$ */
/*-
 * Copyright (c) 2003-2007 Benedikt Meurer <benny@xfce.org>
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

#ifndef __XFCE_PRIVATE_H__
#define __XFCE_PRIVATE_H__

#include <glib.h>

G_BEGIN_DECLS

/* support macros for debugging */
#ifndef NDEBUG
#define _xfce_assert(expr)                  g_assert (expr)
#define _xfce_assert_not_reached()          g_assert_not_reached ()
#define _xfce_return_if_fail(expr)          g_return_if_fail (expr)
#define _xfce_return_val_if_fail(expr, val) g_return_val_if_fail (expr, (val))
#else
#define _xfce_assert(expr)                  G_STMT_START{ (void)0; }G_STMT_END
#define _xfce_assert_not_reached()          G_STMT_START{ (void)0; }G_STMT_END
#define _xfce_return_if_fail(expr)          G_STMT_START{ (void)0; }G_STMT_END
#define _xfce_return_val_if_fail(expr, val) G_STMT_START{ (void)0; }G_STMT_END
#endif

G_END_DECLS

#endif /* !__XFCE_RC_H__ */

