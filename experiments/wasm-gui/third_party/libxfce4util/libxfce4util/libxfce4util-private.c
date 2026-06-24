/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include <libxfce4util/libxfce4util-private.h>
#include <libxfce4util/libxfce4util-alias.h>



/**
 * _xfce_i18n_init:
 *
 * Initializes the libxfce4util i18n support. We don't call
 * xfce_textdomain() here because we don't want to become the
 * default domain for future gettext() calls (which is what
 * textdomain does), because then we break apps that make
 * gettext calls afterwards.
 **/
void
_xfce_i18n_init (void)
{
  static gboolean inited = FALSE;

  if (G_UNLIKELY (!inited))
    {
      inited = TRUE;

      /* bind the text domain for the package to the given directory */
      bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);

      /* setup the encoding for the package */
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
      bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif
    }
}



#define __LIBXFCE4UTIL_PRIVATE_C__
#include <libxfce4util/libxfce4util-aliasdef.c>
