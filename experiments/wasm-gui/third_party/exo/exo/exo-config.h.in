/*-
 * Copyright (c) 2004-2006 os-cillation e.K.
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

#if !defined (EXO_INSIDE_EXO_H) && !defined (EXO_COMPILATION)
#error "Only <exo/exo.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef __EXO_CONFIG_H__
#define __EXO_CONFIG_H__

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * EXO_MAJOR_VERSION:
 *
 * Like #exo_major_version, but from the headers used at application
 * compile time, rather than from the library linked against at
 * application run time.
 **/
#define EXO_MAJOR_VERSION @LIBEXO_VERSION_MAJOR@

/**
 * EXO_MINOR_VERSION:
 *
 * Like #exo_minor_version, but from the headers used at application
 * compile time, rather than from the library linked against at
 * application run time.
 **/
#define EXO_MINOR_VERSION @LIBEXO_VERSION_MINOR@

/**
 * EXO_MICRO_VERSION:
 *
 * Like #exo_micro_version, but from the headers used at application
 * compile time, rather than from the library linked against at
 * application run time.
 **/
#define EXO_MICRO_VERSION @LIBEXO_VERSION_MICRO@

/**
 * EXO_CHECK_VERSION:
 * @major: major version (e.g. 1 for version 1.2.3)
 * @minor: minor version (e.g. 2 for version 1.2.3)
 * @micro: micro version (e.g. 3 for version 1.2.3)
 *
 * Checks the exo version.
 *
 * Returns: %TRUE if the version of the exo header files is equal or
 *          better than the passed-in version.
 **/
#define EXO_CHECK_VERSION(major, minor, micro) \
  (EXO_MAJOR_VERSION > (major) \
   || (EXO_MAJOR_VERSION == (major) \
       && EXO_MINOR_VERSION > (minor)) \
   || (EXO_MAJOR_VERSION == (major) \
       && EXO_MINOR_VERSION == (minor) \
       && EXO_MICRO_VERSION >= (micro)))

extern const guint exo_major_version;
extern const guint exo_minor_version;
extern const guint exo_micro_version;

const gchar *exo_check_version (guint required_major,
                                guint required_minor,
                                guint required_micro);

/* verify that G_GNUC_NULL_TERMINATED is defined */
#if !defined(G_GNUC_NULL_TERMINATED)
#if __GNUC__ >= 4
#define G_GNUC_NULL_TERMINATED __attribute__((__sentinel__))
#else
#define G_GNUC_NULL_TERMINATED
#endif /* __GNUC__ */
#endif /* !defined(G_GNUC_NULL_TERMINATED) */

/* verify that G_GNUC_WARN_UNUSED_RESULT is defined */
#if !defined(G_GNUC_WARN_UNUSED_RESULT)
#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#define G_GNUC_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define G_GNUC_WARN_UNUSED_RESULT
#endif /* __GNUC__ */
#endif /* !defined(G_GNUC_WARN_UNUSED_RESULT) */

/* shorter macros for the GParamSpecs with static strings */
#define EXO_PARAM_READABLE  (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)
#define EXO_PARAM_WRITABLE  (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS)
#define EXO_PARAM_READWRITE (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)

G_END_DECLS

#endif /* !__EXO_CONFIG_H__ */
