dnl configure.ac
dnl
dnl xfce4-screenshooter
dnl

XDT_VERSION_INIT([1.11.1])

AC_INIT([xfce4-screenshooter], [xdt_version], [https://gitlab.xfce.org/apps/xfce4-screenshooter], [xfce4-screenshooter])
AC_PREREQ([2.69])
AC_REVISION([xdt_version_build])
AC_CONFIG_MACRO_DIRS([m4])
dnl ***************************
dnl *** Initialize automake ***
dnl ***************************
AM_INIT_AUTOMAKE([1.8 dist-bzip2 tar-ustar foreign])
AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE()
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl ********************************
dnl *** Check for basic programs ***
dnl ********************************
AC_PROG_CC()
AC_PROG_CC_C_O()
AC_PROG_LD()
AC_PROG_INSTALL()
AM_PROG_CC_C_O()

dnl **************************
dnl *** Initialize libtool ***
dnl **************************
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

dnl **********************************
dnl *** Check for standard headers ***
dnl **********************************
AC_HEADER_STDC

dnl ***********************************
dnl *** Check for required packages ***
dnl ***********************************
XDT_CHECK_PACKAGE([LIBXFCE4PANEL], [libxfce4panel-2.0], [4.16.0])
XDT_CHECK_PACKAGE([LIBXFCE4UTIL], [libxfce4util-1.0], [4.16.0])
XDT_CHECK_PACKAGE([LIBXFCE4UI], [libxfce4ui-2], [4.16.0])
XDT_CHECK_PACKAGE([GTHREAD], [gthread-2.0], [2.42.0])
XDT_CHECK_PACKAGE([GTK], [gtk+-3.0], [3.24.0])
XDT_CHECK_PACKAGE([GDK], [gdk-3.0], [3.24.0])
XDT_CHECK_PACKAGE([GLIB], [glib-2.0], [2.66.0])
XDT_CHECK_PACKAGE([EXO], [exo-2], [0.12.0])
XDT_CHECK_PACKAGE([PANGO], [pango], [1.44.0])
XDT_CHECK_PACKAGE([XFCONF], [libxfconf-0], [4.16.0])

dnl ***********************************
dnl *** Check for optional packages ***
dnl ***********************************
XDT_CHECK_OPTIONAL_FEATURE([X11],
                           [x11],
                           [
                             XDT_FEATURE_DEPENDENCY([LIBX11], [x11], [1.6.7])
                             XDT_FEATURE_DEPENDENCY([GDKX11], [gdk-x11-3.0], [3.24.0])
                             XDT_FEATURE_DEPENDENCY([XINPUT2], [xi], [1.7.8])
                             XDT_FEATURE_DEPENDENCY([LIBXEXT], [xext], [1.0.0])
                             XDT_FEATURE_DEPENDENCY([XFIXES], [xfixes], [4.0.0])
                           ],
                           [the X11 windowing system])
XDT_CHECK_OPTIONAL_FEATURE([WAYLAND],
                           [wayland],
                           [
                             XDT_FEATURE_DEPENDENCY([GDK_WAYLAND], [gdk-wayland-3.0], [3.24.0])
                             XDT_FEATURE_DEPENDENCY([WAYLAND_SCANNER], [wayland-scanner], [1.15.0])
                             XDT_FEATURE_DEPENDENCY([WAYLAND_CLIENT], [wayland-client], [1.15.0])
                           ],
                           [the Wayland windowing system])
if test x"$ENABLE_X11" != x"yes" -a x"$ENABLE_WAYLAND" != x"yes"; then
  AC_MSG_ERROR([Either both X11 and Wayland support was disabled, or required dependencies are missing. One of the two must be enabled.])
fi

if test x"$ENABLE_X11" != x"yes"; then
  dnl Do not check for optional X11 packages if X11 is disabled
  enable_libxtst=no
fi
XDT_CHECK_OPTIONAL_PACKAGE([LIBXTST], [xtst], [libxtst_min_version], [libxtst], [Libxtst support])

if test x"$ENABLE_WAYLAND" = x"yes"; then
  WLR_PROTOCOLS_PKGDATADIR=`$PKG_CONFIG --variable=pkgdatadir wlr-protocols 2>/dev/null || true`
  if test x"$WLR_PROTOCOLS_PKGDATADIR" = x""; then
    WLR_PROTOCOLS_PKGDATADIR="$ac_abs_confdir/protocols/wlr-protocols"
    if test "$(echo "$WLR_PROTOCOLS_PKGDATADIR/"*)" = "$WLR_PROTOCOLS_PKGDATADIR/*"; then
      AC_MSG_ERROR([$WLR_PROTOCOLS_PKGDATADIR seems empty, did you clone the wlr-protocols submodule?])
    fi
  fi
  AC_MSG_CHECKING([for wlr-protocols])
  AC_MSG_RESULT([$WLR_PROTOCOLS_PKGDATADIR])
  AC_SUBST([WLR_PROTOCOLS_PKGDATADIR])
fi

dnl ******************************
dnl *** Check for i18n support ***
dnl ******************************
GETTEXT_PACKAGE="$PACKAGE"
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Name of default gettext domain])
AC_SUBST([GETTEXT_PACKAGE])

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.8])

dnl **************************
dnl *** Check for help2man ***
dnl **************************
AC_CHECK_PROGS([HELP2MAN], [help2man])
if ! test -z "$HELP2MAN"; then
AM_CONDITIONAL([HAVE_HELP2MAN], true)
else
AM_CONDITIONAL([HAVE_HELP2MAN], false)
fi

dnl Check for debugging support
XDT_FEATURE_DEBUG([xdt_debug_default])

AC_OUTPUT([
Makefile
po/Makefile.in
])

dnl ***************************
dnl *** Print configuration ***
dnl ***************************

echo ""
echo "Build Configuration:"
echo ""

echo "  * XFIXES support:                $XFIXES_FOUND"
echo "  * Debugging support:             $enable_debug"
echo "  * X11 Support:                   ${ENABLE_X11:-no}"
echo "  * Wayland Support:               ${ENABLE_WAYLAND:-no}"

echo ""
