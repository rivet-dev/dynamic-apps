dnl  Copyright 2005 Red Hat, Inc.
dnl
dnl  Permission to use, copy, modify, distribute, and sell this software and its
dnl  documentation for any purpose is hereby granted without fee, provided that
dnl  the above copyright notice appear in all copies and that both that
dnl  copyright notice and this permission notice appear in supporting
dnl  documentation, and that the name of Red Hat not be used in
dnl  advertising or publicity pertaining to distribution of the software without
dnl  specific, written prior permission.  Red Hat makes no
dnl  representations about the suitability of this software for any purpose.  It
dnl  is provided "as is" without express or implied warranty.
dnl
dnl  RED HAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
dnl  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
dnl  EVENT SHALL RED HAT BE LIABLE FOR ANY SPECIAL, INDIRECT OR
dnl  CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
dnl  DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
dnl  TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
dnl  PERFORMANCE OF THIS SOFTWARE.
dnl
dnl Process this file with autoconf to create configure.

# Initialize Autoconf
AC_PREREQ([2.60])
AC_INIT([twm], [1.0.12],
        [https://gitlab.freedesktop.org/xorg/app/twm/issues], [twm])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([config.h])
AC_USE_SYSTEM_EXTENSIONS

# Initialize Automake
AM_INIT_AUTOMAKE([foreign dist-xz])

# Require X.Org macros 1.8 or later for MAN_SUBSTS set by XORG_MANPAGE_SECTIONS
m4_ifndef([XORG_MACROS_VERSION],
          [m4_fatal([must install xorg-macros 1.8 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.8)
XORG_DEFAULT_OPTIONS

AC_PROG_YACC
AC_PATH_PROG([YACC_INST], $YACC)
if test ! -f "$srcdir/gram.c"; then
   if test -z "$YACC_INST"; then
      AC_MSG_ERROR([yacc not found - unable to compile gram.y])
   fi
fi
AC_PROG_LEX

AC_CHECK_FUNCS([mkstemp])

# Checks for pkg-config packages
PKG_CHECK_MODULES([TWM], [x11 xext xt xmu ice sm xproto >= 7.0.17])
PKG_CHECK_MODULES([XRANDR], [xrandr], [have_xrandr=yes], [have_xrandr=no])
if test "$have_xrandr" = yes; then
	AC_DEFINE([HAVE_XRANDR], [1], [Define to 1 if you have the xrandr headers/libraries])
fi

AC_CONFIG_FILES([Makefile
		src/Makefile
		man/Makefile])
AC_OUTPUT
