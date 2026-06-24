/*-
 * Copyright (c) 2004 os-cillation e.K.
 *
 * Written by Benedikt Meurer <benny@xfce.org>.
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <exo/exo-config.h>
#include <exo/exo-alias.h>

/**
 * SECTION: exo-config
 * @title: Version Information
 * @short_description: Variables and macros to check the exo version
 * @include: exo/exo.h
 *
 * Exo provides version information, primarily useful in configure for
 * builds that have a configure script. Applications may use it to
 * check if a certain feature is available in the version of libexo
 * they are being built against or being linked with.
 **/

/**
 * exo_major_version:
 *
 * The major version number of the exo library (e.g. in exo 1.2.3 this is 1).
 *
 * This variable is in the library, and therefore represents the exo
 * library you have linked against. Contrast with the #EXO_MAJOR_VERSION
 * macro, which represents the major version of the libexo headers you
 * have included.
 **/
const guint exo_major_version = EXO_MAJOR_VERSION;

/**
 * exo_minor_version:
 *
 * The minor version number of the exo library (e.g. in exo 1.2.3 this is 2).
 *
 * This variable is in the library, and therefore represents the exo
 * library you have linked against. Contrast with the #EXO_MINOR_VERSION
 * macro, which represents the minor version of the libexo headers you
 * have included.
 **/
const guint exo_minor_version = EXO_MINOR_VERSION;

/**
 * exo_micro_version:
 *
 * The micro version number of the exo library (e.g. in exo 1.2.3 this is 3).
 *
 * This variable is in the library, and therefore represents the exo
 * library you have linked against. Contrast with the #EXO_MICRO_VERSION
 * macro, which represents the micro version of the libexo headers you
 * have included.
 **/
const guint exo_micro_version = EXO_MICRO_VERSION;



/**
 * exo_check_version:
 * @required_major : the required major version.
 * @required_minor : the required minor version.
 * @required_micro : the required micro version.
 *
 * Checks that the <systemitem class="library">exo</systemitem> library
 * in use is compatible with the given version. Generally you would pass in
 * the constants #EXO_MAJOR_VERSION, #EXO_MINOR_VERSION and #EXO_MICRO_VERSION
 * as the three arguments to this function; that produces
 * a check that the library in use is compatible with the version of
 * <systemitem class="library">exo</systemitem> the application was
 * compiled against.
 *
 * <example>
 * <title>Checking the runtime version of the exo library</title>
 * <programlisting>
 * const gchar *mismatch;
 * mismatch = exo_check_version (EXO_VERSION_MAJOR,
 *                               EXO_VERSION_MINOR,
 *                               EXO_VERSION_MICRO);
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
 * Since: 0.3.1
 **/
const gchar*
exo_check_version (guint required_major,
                   guint required_minor,
                   guint required_micro)
{
  return NULL;
}



#define __EXO_CONFIG_C__
#include <exo/exo-aliasdef.c>
