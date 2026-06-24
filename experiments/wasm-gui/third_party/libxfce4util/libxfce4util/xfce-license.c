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

/**
 * SECTION: xfce-license
 * @title: Software Licenses
 * @short_description: Software Licenses
 *
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxfce4util/libxfce4util-private.h>
#include <libxfce4util/libxfce4util-alias.h>

#ifdef __SUNPRO_C
#pragma align 4 (xfce_builtin_license_BSD)
#endif
#ifdef __GNUC__
static const char xfce_builtin_license_BSD[] __attribute__ ((__aligned__ (4))) =
#else
static const char xfce_builtin_license_BSD[] =
#endif
  N_ (" Redistribution and use in source and binary forms, with or without\n"
      " modification, are permitted provided that the following conditions\n"
      " are met:\n"
      "\n"
      " 1. Redistributions of source code must retain the above copyright\n"
      "    notice, this list of conditions and the following disclaimer.\n"
      " 2. Redistributions in binary form must reproduce the above copyright\n"
      "    notice, this list of conditions and the following disclaimer in the\n"
      "    documentation and/or other materials provided with the distribution.\n"
      "\n"
      " THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR\n"
      " IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES\n"
      " OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.\n"
      " IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,\n"
      " INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT\n"
      " NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,\n"
      " DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY\n"
      " THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n"
      " (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF\n"
      " THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n");

#ifdef __SUNPRO_C
#pragma align 4 (xfce_builtin_license_GPL)
#endif
#ifdef __GNUC__
static const char xfce_builtin_license_GPL[] __attribute__ ((__aligned__ (4))) =
#else
static const char xfce_builtin_license_GPL[] =
#endif
  N_ ("This program is free software; you can redistribute it and/or modify it\n"
      "under the terms of the GNU General Public License as published by the Free\n"
      "Software Foundation; either version 2 of the License, or (at your option)\n"
      "any later version.\n"
      "\n"
      "This program is distributed in the hope that it will be useful, but WITHOUT\n"
      "ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or\n"
      "FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for\n"
      "more details.\n"
      "\n"
      "You should have received a copy of the GNU General Public License along with\n"
      "this program; if not, write to the Free Software Foundation, Inc., 51 Franklin\n"
      "Street, Fifth Floor, Boston, MA 02110-1301, USA.\n");

#ifdef __SUNPRO_C
#pragma align 4 (xfce_builtin_license_LGPL)
#endif
#ifdef __GNUC__
static const char xfce_builtin_license_LGPL[] __attribute__ ((__aligned__ (4))) =
#else
static const char xfce_builtin_license_LGPL[] =
#endif
  N_ ("This library is free software; you can redistribute it and/or\n"
      "modify it under the terms of the GNU Library General Public\n"
      "License as published by the Free Software Foundation; either\n"
      "version 2 of the License, or (at your option) any later version.\n"
      "\n"
      "This library is distributed in the hope that it will be useful,\n"
      "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
      "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n"
      "Library General Public License for more details.\n"
      "\n"
      "You should have received a copy of the GNU Library General Public\n"
      "License along with this library; if not, write to the \n"
      "Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, \n"
      "Boston, MA 02110-1301  USA.\n");



/**
 * xfce_get_license_text:
 * @license_type : an #XfceLicenseTextType.
 *
 * Returns the text of the software license specified in the
 * @license_type translated to the current language. If no
 * translation is available, the english license text will
 * be returned.
 *
 * Return value: the license text for @license_type.
 **/
const gchar*
xfce_get_license_text (XfceLicenseTextType license_type)
{
  /* make sure to initialize the i18n support */
  _xfce_i18n_init ();

  /* return the appropriate license */
  switch (license_type)
    {
    case XFCE_LICENSE_TEXT_BSD: return _(xfce_builtin_license_BSD);
    case XFCE_LICENSE_TEXT_GPL: return _(xfce_builtin_license_GPL);
    default:                    return _(xfce_builtin_license_LGPL);
    }
}



#define __XFCE_LICENSE_C__
#include <libxfce4util/libxfce4util-aliasdef.c>
