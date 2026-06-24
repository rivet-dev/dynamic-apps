/*
 * Copyright (c) 2007 The Xfce Development Team
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
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#if !defined (_LIBXFCE4UI_INSIDE_LIBXFCE4UI_H) && !defined (LIBXFCE4UI_COMPILATION)
#error "Only <libxfce4ui/libxfce4ui.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef __LIBXFCE4UI_CONFIG_H__
#define __LIBXFCE4UI_CONFIG_H__

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * LIBXFCE4UI_MAJOR_VERSION:
 *
 * A macro that evaluates to the major version of libxfce4ui, in a format
 * that can be used by the C pre-processor.
 *
 */
#define LIBXFCE4UI_MAJOR_VERSION (4)

/**
 * LIBXFCE4UI_MINOR_VERSION:
 *
 * A macro that evaluates to the minor version of libxfce4ui, in a format
 * that can be used by the C pre-processor.
 *
 */
#define LIBXFCE4UI_MINOR_VERSION (18)

/**
 * LIBXFCE4UI_MICRO_VERSION:
 *
 * A macro that evaluates to the micro version of libxfce4ui, in a format
 * that can be used by the C pre-processor.
 *
 */
#define LIBXFCE4UI_MICRO_VERSION (6)

/**
 * LIBXFCE4UI_CHECK_VERSION:
 * @major : The major version of the library, i.e. 4
 * @minor : The minor version of the library, i.e. 18
 * @micro : The micro release of the library, i.e. 6
 *
 * A macro to compare libxfce4ui versions to determine if a particular
 * function is available.
 *
 */
#define LIBXFCE4UI_CHECK_VERSION(major,minor,micro) \
  (LIBXFCE4UI_MAJOR_VERSION > (major) \
   || (LIBXFCE4UI_MAJOR_VERSION == (major) \
       && LIBXFCE4UI_MINOR_VERSION > (minor)) \
   || (LIBXFCE4UI_MAJOR_VERSION == (major) \
       && LIBXFCE4UI_MINOR_VERSION == (minor) \
       && LIBXFCE4UI_MICRO_VERSION >= (micro)))

extern const guint libxfce4ui_major_version;
extern const guint libxfce4ui_minor_version;
extern const guint libxfce4ui_micro_version;

const gchar *libxfce4ui_check_version (guint required_major,
                                       guint required_minor,
                                       guint required_micro);

G_END_DECLS

#endif /* !__LIBXFCE4UI_CONFIG_H__ */
