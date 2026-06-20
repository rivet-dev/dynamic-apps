
#  Copyright 2005 Red Hat, Inc.
#
#  Permission to use, copy, modify, distribute, and sell this software and its
#  documentation for any purpose is hereby granted without fee, provided that
#  the above copyright notice appear in all copies and that both that
#  copyright notice and this permission notice appear in supporting
#  documentation, and that the name of Red Hat not be used in
#  advertising or publicity pertaining to distribution of the software without
#  specific, written prior permission.  Red Hat makes no
#  representations about the suitability of this software for any purpose.  It
#  is provided "as is" without express or implied warranty.
#
#  RED HAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
#  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
#  EVENT SHALL RED HAT BE LIABLE FOR ANY SPECIAL, INDIRECT OR
#  CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
#  DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
#  TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
#  PERFORMANCE OF THIS SOFTWARE.
#

# Initialize Autoconf
AC_PREREQ([2.60])
AC_INIT([libXtst], [1.2.5],
	[https://gitlab.freedesktop.org/xorg/lib/libxtst/-/issues], [libXtst])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])

# Initialize Automake
AM_INIT_AUTOMAKE([foreign dist-xz])

# Initialize libtool
LT_INIT

# Require xorg-macros minimum of 1.12 for DocBook external references
m4_ifndef([XORG_MACROS_VERSION],
          [m4_fatal([must install xorg-macros 1.12 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.12)
XORG_DEFAULT_OPTIONS
XORG_ENABLE_SPECS
XORG_WITH_XMLTO(0.0.22)
XORG_WITH_FOP
XORG_WITH_XSLTPROC
XORG_CHECK_SGML_DOCTOOLS(1.8)

# Obtain compiler/linker options for dependencies
PKG_CHECK_MODULES(XTST, [x11 >= 1.6] [xext >= 1.0.99.4] xi [recordproto >= 1.13.99.1] [xextproto >= 7.0.99.3] inputproto)

# Determine if the source for man pages is available
# It may already be present (tarball) or can be generated using xmlto
AM_CONDITIONAL([INSTALL_MANPAGES],
	[test -f "$srcdir/man/XTestQueryExtension.man" || test "x$have_xmlto" = xyes])

# Allow checking code with lint, sparse, etc.
XORG_WITH_LINT
XORG_LINT_LIBRARY([Xtst])
LINT_FLAGS="${LINT_FLAGS} ${XTST_CFLAGS}"

AC_CONFIG_FILES([Makefile
		man/Makefile
		src/Makefile
		specs/Makefile
		xtst.pc])
AC_OUTPUT
