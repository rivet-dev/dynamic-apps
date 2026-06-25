/*
 * Copyright (C) 2009-2010 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxfce4panel/libxfce4panel-config.h>
#include <libxfce4panel/libxfce4panel-alias.h>



/**
 * SECTION: config
 * @title: Version Information
 * @short_description: Information about the panel version in use.
 * @include: libxfce4panel/libxfce4panel.h
 *
 * The panel library provides version information, which could be used
 * by plugins to handle new API.
 **/



/**
 * libxfce4panel_major_version:
 *
 * The major version number of the libxfce4panel library (e.g. in
 * version 4.8.0 this is 4).
 *
 * This variable is in the library, so represents the
 * libxfce4panel library you have linked against. Contrast with the
 * #LIBXFCE4PANEL_MAJOR_VERSION macro, which represents the major
 * version of the libxfce4panel headers you have included.
 *
 * Since: 4.8
 **/
const guint libxfce4panel_major_version = LIBXFCE4PANEL_MAJOR_VERSION;



/**
 * libxfce4panel_minor_version:
 *
 * The minor version number of the libxfce4panel library (e.g. in
 * version 4.8.0 this is 8).
 *
 * This variable is in the library, so represents the
 * libxfce4panel library you have linked against. Contrast with the
 * #LIBXFCE4PANEL_MINOR_VERSION macro, which represents the minor
 * version of the libxfce4panel headers you have included.
 *
 * Since: 4.8
 **/
const guint libxfce4panel_minor_version = LIBXFCE4PANEL_MINOR_VERSION;



/**
 * libxfce4panel_micro_version:
 *
 * The micro version number of the libxfce4panel library (e.g. in
 * version 4.8.0 this is 0).
 *
 * This variable is in the library, so represents the
 * libxfce4panel library you have linked against. Contrast with the
 * #LIBXFCE4PANEL_MICRO_VERSION macro, which represents the micro
 * version of the libxfce4panel headers you have included.
 *
 * Since: 4.8
 **/
const guint libxfce4panel_micro_version = LIBXFCE4PANEL_MICRO_VERSION;



/**
 * libxfce4panel_check_version:
 * @required_major: the required major version.
 * @required_minor: the required minor version.
 * @required_micro: the required micro version.
 *
 * Checks that the libxfce4panel library in use is compatible with
 * the given version. Generally you would pass in the constants
 * #LIBXFCE4PANEL_MAJOR_VERSION, #LIBXFCE4PANEL_MINOR_VERSION and
 * #LIBXFCE4PANEL_MICRO_VERSION as the three arguments to this
 * function; that produces a check that the library in use is
 * compatible with the version of libxfce4panel the extension was
 * compiled against.
 *
 * <example>
 * <title>Checking the runtime version of the Libxfce4panel library</title>
 * <programlisting>
 * const gchar *mismatch;
 * mismatch = libxfce4panel_check_version (LIBXFCE4PANEL_MAJOR_VERSION,
 *                                      LIBXFCE4PANEL_MINOR_VERSION,
 *                                      LIBXFCE4PANEL_MICRO_VERSION);
 * if (G_UNLIKELY (mismatch != NULL))
 *   g_error ("Version mismatch: %<!---->s", mismatch);
 * </programlisting>
 * </example>
 *
 * Returns: %NULL if the library is compatible with the given version,
 *          or a string describing the version mismatch. The returned
 *          string is owned by the library and must not be freed or
 *          modified by the caller.
 *
 * Since: 4.8
 **/
const gchar *
libxfce4panel_check_version (guint required_major,
                             guint required_minor,
                             guint required_micro)
{
  if (required_major > LIBXFCE4PANEL_MAJOR_VERSION)
    return "Xfce Panel version too old (major mismatch)";
  if (required_major < LIBXFCE4PANEL_MAJOR_VERSION)
    return "Xfce Panel version too new (major mismatch)";
  if (required_minor > LIBXFCE4PANEL_MINOR_VERSION)
    return "Xfce Panel version too old (minor mismatch)";
  if (required_minor == LIBXFCE4PANEL_MINOR_VERSION
      && required_micro > LIBXFCE4PANEL_MICRO_VERSION)
    return "Xfce Panel version too old (micro mismatch)";
  return NULL;
}



#define __LIBXFCE4PANEL_CONFIG_C__
#include <libxfce4panel/libxfce4panel-aliasdef.c>
