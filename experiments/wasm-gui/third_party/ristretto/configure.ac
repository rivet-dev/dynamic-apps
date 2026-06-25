dnl
dnl Copyright (c) 2006-2024
dnl         The Xfce development team. All rights reserved.
dnl
dnl Originally written for Xfce by Benedikt Meurer <benny@xfce.org>
dnl Modified by Stephan Arts <stephan@xfce.org> and Xfce development team.
dnl

dnl ***************************
dnl *** Version information ***
dnl ***************************
XDT_VERSION_INIT([0.13.2])

dnl minimum required versions
m4_define([glib_min_version], [2.56.0])
m4_define([gtk_min_version], [3.22.0])
m4_define([libxfce4ui_min_version], [4.16.0])
m4_define([libxfce4util_min_version], [4.16.0])
m4_define([xfconf_min_version], [4.12.1])
m4_define([libexif_min_version], [0.6.0])
m4_define([cairo_min_version], [1.10.0])
m4_define([libx11_min_version], [1.6.7])

AC_COPYRIGHT([Copyright (c) 2006-2024
        The Xfce development team. All rights reserved.

Written for Xfce by Stephan Arts <stephan@xfce.org>.])

AC_INIT([ristretto], [xdt_version], [https://gitlab.xfce.org/apps/ristretto])
AC_PREREQ([2.69])
AC_CONFIG_MACRO_DIRS([m4])
AC_REVISION([xdt_version_build])

AM_INIT_AUTOMAKE([1.8 no-dist-gzip dist-bzip2 tar-ustar foreign])
AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl check for UNIX variants
AC_USE_SYSTEM_EXTENSIONS
AC_SEARCH_LIBS([strerror],[cposix])
AM_CONDITIONAL([HAVE_CYGWIN], [test "`uname | grep \"CYGWIN\"`" != ""])


dnl check for standard header files
AC_PROG_CC
AC_PROG_SED


dnl Check for i18n support
GETTEXT_PACKAGE="$PACKAGE"
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Name of default gettext domain])
AC_SUBST([GETTEXT_PACKAGE])
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.8])

XDT_CHECK_PACKAGE([GLIB], [glib-2.0], [glib_min_version])
XDT_CHECK_PACKAGE([GTK], [gtk+-3.0], [gtk_min_version])

XDT_CHECK_PACKAGE([LIBXFCE4UI], [libxfce4ui-2], [libxfce4ui_min_version])
XDT_CHECK_PACKAGE([LIBXFCE4UTIL], [libxfce4util-1.0], [libxfce4util_min_version])
XDT_CHECK_PACKAGE([XFCONF], [libxfconf-0], [xfconf_min_version])

XDT_CHECK_PACKAGE([LIBEXIF], [libexif], [libexif_min_version])
XDT_CHECK_PACKAGE([CAIRO], [cairo], [cairo_min_version])

XDT_CHECK_OPTIONAL_PACKAGE([LIBX11], [x11], [libx11_min_version], [libx11], [Libx11 support])

AC_CHECK_HEADERS([magic.h],, [libmagic=false])
AC_CHECK_LIB(magic, [magic_open], [MAGIC_LIBS="-lmagic"],[libmagic=false])
AC_SUBST(MAGIC_LIBS)

LT_LIB_M
AC_SUBST(LIBM)

dnl *********************************************************************
dnl *** Alternative Tumbler service name prefix for Flatpak packaging ***
dnl *********************************************************************
AC_ARG_VAR([TUMBLER_SERVICE_NAME_PREFIX],
           [Alternative prefix to org.freedesktop.thumbnails for Tumbler services])
test -n "$TUMBLER_SERVICE_NAME_PREFIX" || TUMBLER_SERVICE_NAME_PREFIX='org.freedesktop.thumbnails'

AC_SUBST([TUMBLER_SERVICE_PATH_PREFIX],
         [$(printf '%s' $TUMBLER_SERVICE_NAME_PREFIX | $SED -E 's%(^|\.)%/%g')])
AC_DEFINE_UNQUOTED(TUMBLER_SERVICE_NAME_PREFIX, "$TUMBLER_SERVICE_NAME_PREFIX",
                   Name prefix for Tumbler services)
AC_DEFINE_UNQUOTED(TUMBLER_SERVICE_PATH_PREFIX, "$TUMBLER_SERVICE_PATH_PREFIX",
                   Path prefix for Tumbler services)

dnl check for debugging support
XDT_FEATURE_DEBUG([xdt_debug_default])

AC_CONFIG_FILES([
Makefile
po/Makefile.in
src/Makefile
icons/Makefile
icons/16x16/Makefile
icons/24x24/Makefile
icons/32x32/Makefile
icons/48x48/Makefile
icons/64x64/Makefile
icons/96x96/Makefile
icons/128x128/Makefile
icons/scalable/Makefile
])
AC_OUTPUT

echo "----------------------------------------"
echo "Build Configuration for $PACKAGE $VERSION:"
if test "$libmagic" = "false"; then
echo "  libmagic support: no"
else
echo "  libmagic support: yes"
fi
echo ""
echo "The binary will be installed in $prefix/bin"
echo "----------------------------------------"
