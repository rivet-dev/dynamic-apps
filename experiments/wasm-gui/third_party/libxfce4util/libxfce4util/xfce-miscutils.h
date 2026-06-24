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

#ifndef __XFCE_MISCUTILS_H__
#define __XFCE_MISCUTILS_H__

#include <glib.h>

#include <libxfce4util/xfce-debug.h>

G_BEGIN_DECLS

const gchar* xfce_version_string (void) G_GNUC_PURE;

const gchar* xfce_get_homedir    (void) G_GNUC_PURE;

gchar*       xfce_get_homefile_r (gchar *buffer,
                                  size_t length,
                                  const gchar *format,
                                  ...);

const gchar* xfce_get_userdir    (void) G_GNUC_PURE;

gchar*       xfce_get_userfile_r (gchar *buffer,
                                  size_t length,
                                  const gchar *format,
                                  ...);

#if defined(G_HAVE_ISO_VARARGS)

#define xfce_get_homefile(...)                             \
  (g_build_filename (xfce_get_homedir (), __VA_ARGS__))

#define xfce_get_userfile(...)                             \
  (g_build_filename (xfce_get_userdir (), __VA_ARGS__))

#elif defined(G_HAVE_GNUC_VARARGS)

#define xfce_get_homefile(first_element...)                \
  (g_build_filename (xfce_get_homedir (), ## first_element))

#define xfce_get_userfile(first_element...)                \
  (g_build_filename (xfce_get_userdir (), ## first_element))

#else

static gchar*
xfce_get_homefile (const gchar *first_element, ...)
{
#error "Implement this, if you see this error!"
}

static gchar*
xfce_get_userfile (const gchar *first_element, ...)
{
#error "Implement this, if you see this error!"
}

#endif

gchar* xfce_gethostname                      (void) G_GNUC_MALLOC;

gchar* xfce_expand_variables                 (const gchar  *command,
                                              gchar       **envp) G_GNUC_MALLOC;

void   xfce_append_quoted                    (GString      *string,
                                              const gchar  *unquoted)
  G_GNUC_DEPRECATED_FOR (xfce_g_string_append_quoted());

gchar* xfce_expand_desktop_entry_field_codes (const gchar  *command,
                                              GSList       *uri_list,
                                              const gchar  *icon,
                                              const gchar  *name,
                                              const gchar  *uri,
                                              gboolean      requires_terminal) G_GNUC_MALLOC;

gchar* xfce_unescape_desktop_entry_value     (const gchar  *value) G_GNUC_MALLOC;

G_END_DECLS

#endif /* __XFCE_MISCUTILS_H__ */
