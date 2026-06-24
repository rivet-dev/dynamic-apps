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

/**
 * SECTION:libxfce4ui-config
 * @title: libxfce4ui config
 * @short_description: libxfce4ui config macros
 * @stability: Stable
 * @include: libxfce4ui/libxfce4ui.h
 *
 * Variables and functions to check the Libxfce4ui version.
 **/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxfce4ui/libxfce4ui-config.h>
#include <libxfce4ui/libxfce4ui-alias.h>


/**
 * libxfce4ui_major_version:
 *
 * A constat that evaluates to the major version of libxfce4ui.
 *
 */
const guint libxfce4ui_major_version = LIBXFCE4UI_MAJOR_VERSION;

/**
 * libxfce4ui_minor_version:
 *
 * A constat that evaluates to the minor version of libxfce4ui.
 *
 */
const guint libxfce4ui_minor_version = LIBXFCE4UI_MINOR_VERSION;

/**
 * libxfce4ui_micro_version:
 *
 * A constat that evaluates to the micro version of libxfce4ui.
 *
 */
const guint libxfce4ui_micro_version = LIBXFCE4UI_MICRO_VERSION;



/**
 * libxfce4ui_check_version:
 * @required_major: the required major version.
 * @required_minor: the required minor version.
 * @required_micro: the required micro version.
 *
 * Checks that the <systemitem class="library">libxfce4ui</systemitem> library
 * in use is compatible with the given version. Generally you would pass in
 * the constants #LIBXFCE4UI_MAJOR_VERSION, #LIBXFCE4UI_MINOR_VERSION and
 * #LIBXFCE4UI_MICRO_VERSION as the three arguments to this function; that produces
 * a check that the library in use is compatible with the version of
 * <systemitem class="library">libxfce4ui</systemitem> the extension was
 * compiled against.
 *
 * <example>
 * <title>Checking the runtime version of the Libxfce4ui library</title>
 * <programlisting>
 * const gchar *mismatch;
 * mismatch = libxfce4ui_check_version (LIBXFCE4UI_MAJOR_VERSION,
 *                                      LIBXFCE4UI_MINOR_VERSION,
 *                                      LIBXFCE4UI_MICRO_VERSION);
 * if (G_UNLIKELY (mismatch != NULL))
 *   g_error ("Version mismatch: %<!---->s", mismatch);
 * </programlisting>
 * </example>
 *
 * Return value: %NULL if the library is compatible with the given version,
 *               or a string describing the version mismatch. The returned
 *               string is owned by the library and must not be freed or
 *               modified by the caller.
 **/
const gchar *
libxfce4ui_check_version (guint required_major,
                          guint required_minor,
                          guint required_micro)
{
  if (required_major > LIBXFCE4UI_MAJOR_VERSION)
    return "Libxfce4ui version too old (major mismatch)";
  if (required_major < LIBXFCE4UI_MAJOR_VERSION)
    return "Libxfce4ui version too new (major mismatch)";
  if (required_minor > LIBXFCE4UI_MINOR_VERSION)
    return "Libxfce4ui version too old (minor mismatch)";
  if (required_minor == LIBXFCE4UI_MINOR_VERSION && required_micro > LIBXFCE4UI_MICRO_VERSION)
    return "Libxfce4ui version too old (micro mismatch)";
  return NULL;
}



#define __LIBXFCE4UI_CONFIG_C__
#include <libxfce4ui/libxfce4ui-aliasdef.c>
