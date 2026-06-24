AC_PREREQ([2.54])
AC_INIT([openbox], [3.6.1], [http://bugzilla.icculus.org])
AC_CONFIG_SRCDIR([openbox/openbox.c])

AM_INIT_AUTOMAKE([foreign])
AM_SILENT_RULES([yes])

OB_VERSION=$PACKAGE_VERSION
AC_SUBST(OB_VERSION)

dnl Making releases:
dnl RR_ is for obrender, OBT_ is for obt, keep major/minor in sync,
dnl treat the rest separately.
dnl
dnl if MAJOR or MINOR version changes, be sure to change AC_INIT above to match.
dnl the other variables aren't related to the user visible versions, but are used
dnl to version the .so files.
dnl if no backward compat is broken, it's just a preference if you want to bump
dnl the AC_INIT micro version or minor version. if the AC_INIT micro version is
dnl 0, don't write it out.
dnl
dnl How to process .so versioning,
dnl   RR_MICRO_VERSION += 1;
dnl   RR_INTERFACE_AGE += 1;
dnl   RR_BINARY_AGE += 1;
dnl if any functions have been added, set RR_INTERFACE_AGE to 0.
dnl if backwards compatibility has been broken,
dnl set RR_BINARY_AGE and RR_INTERFACE_AGE to 0.
dnl if you set anything to 0, you should also bump the MINOR version.
dnl
RR_MAJOR_VERSION=3
RR_MINOR_VERSION=6
RR_MICRO_VERSION=32
RR_INTERFACE_AGE=0
RR_BINARY_AGE=0
RR_VERSION=$RR_MAJOR_VERSION.$RR_MINOR_VERSION

OBT_MAJOR_VERSION=3
OBT_MINOR_VERSION=6
OBT_MICRO_VERSION=4
OBT_INTERFACE_AGE=2
OBT_BINARY_AGE=2
OBT_VERSION=$OBT_MAJOR_VERSION.$OBT_MINOR_VERSION

AC_SUBST(RR_MAJOR_VERSION)
AC_SUBST(RR_MINOR_VERSION)
AC_SUBST(RR_MICRO_VERSION)
AC_SUBST(RR_INTERFACE_AGE)
AC_SUBST(RR_BINARY_AGE)
AC_SUBST(RR_VERSION)
AC_SUBST(OBT_MAJOR_VERSION)
AC_SUBST(OBT_MINOR_VERSION)
AC_SUBST(OBT_MICRO_VERSION)
AC_SUBST(OBT_INTERFACE_AGE)
AC_SUBST(OBT_BINARY_AGE)
AC_SUBST(OBT_VERSION)

dnl Libtool versioning
RR_RELEASE=$RR_MAJOR_VERSION.$RR_MINOR_VERSION
RR_CURRENT=`expr $RR_MICRO_VERSION - $RR_INTERFACE_AGE`
RR_REVISION=$RR_INTERFACE_AGE
RR_AGE=`expr $RR_BINARY_AGE - $RR_INTERFACE_AGE`
RR_CURRENT_MINUS_AGE=`expr $RR_CURRENT - $RR_AGE`

OBT_RELEASE=$OBT_MAJOR_VERSION.$OBT_MINOR_VERSION
OBT_CURRENT=`expr $OBT_MICRO_VERSION - $OBT_INTERFACE_AGE`
OBT_REVISION=$OBT_INTERFACE_AGE
OBT_AGE=`expr $OBT_BINARY_AGE - $OBT_INTERFACE_AGE`
OBT_CURRENT_MINUS_AGE=`expr $OBT_CURRENT - $OBT_AGE`

AC_SUBST(RR_RELEASE)
AC_SUBST(RR_CURRENT)
AC_SUBST(RR_REVISION)
AC_SUBST(RR_AGE)
AC_SUBST(RR_CURRENT_MINUS_AGE)
AC_SUBST(OBT_RELEASE)
AC_SUBST(OBT_CURRENT)
AC_SUBST(OBT_REVISION)
AC_SUBST(OBT_AGE)
AC_SUBST(OBT_CURRENT_MINUS_AGE)

AC_PREFIX_DEFAULT([/usr/local])
test "$prefix" = "NONE" && prefix=$ac_default_prefix

dnl Determine build target
OB_DEBUG
dnl Pick compiler specific/build target flags, and set $CVS
AM_PROG_CC_C_O
OB_COMPILER_FLAGS
AC_C_CONST
AC_C_INLINE

AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)
LIBTOOL="$LIBTOOL --silent"

AC_PROG_INSTALL

AM_GNU_GETTEXT_VERSION(0.15)
AM_GNU_GETTEXT([external])

AC_CHECK_HEADERS(ctype.h dirent.h errno.h fcntl.h grp.h locale.h pwd.h)
AC_CHECK_HEADERS(signal.h string.h stdio.h stdlib.h unistd.h sys/stat.h)
AC_CHECK_HEADERS(sys/select.h sys/socket.h sys/time.h sys/types.h sys/wait.h)

AC_PATH_PROG([SED], [sed], [no])
if test "$SED" = "no"; then
  AC_MSG_ERROR([The program "sed" is not available. This program is required to build Openbox.])
fi

AC_PATH_PROG([DIRNAME], [dirname], [no])
if test "$DIRNAME" = "no"; then
  AC_MSG_ERROR([The program "dirname" is not available. This program is required to build Openbox.])
fi

PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.14.0])
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

PKG_CHECK_MODULES(PANGO, [pango >= 1.8.0 pangoxft >= 1.8.0])
AC_SUBST(PANGO_CFLAGS)
AC_SUBST(PANGO_LIBS)

PKG_CHECK_MODULES(XML, [libxml-2.0 >= 2.6.0])
AC_SUBST(XML_CFLAGS)
AC_SUBST(XML_LIBS)

AC_ARG_ENABLE(startup-notification,
  AC_HELP_STRING(
    [--disable-startup-notification],
    [disable the startup notification library. [default=enabled]]
  ),
  [enable_sn=$enableval],
  [enable_sn=yes]
)

if test "$enable_sn" = yes; then
PKG_CHECK_MODULES(LIBSN, [libstartup-notification-1.0 >= 0.8],
  [
    AC_DEFINE(USE_LIBSN, [1], [Use startup-notification])
    AC_SUBST(LIBSN_CFLAGS)
    AC_SUBST(LIBSN_LIBS)
    sn_found=yes
  ],
  [
    sn_found=no
  ]
)
else
  sn_found=no
fi

AC_ARG_ENABLE(xcursor,
  AC_HELP_STRING(
    [--disable-xcursor],
    [disable use of the X Cursor library. [default=enabled]]
  ),
  [enable_xcursor=$enableval],
  [enable_xcursor=yes]
)

if test "$enable_xcursor" = yes; then
PKG_CHECK_MODULES(XCURSOR, [xcursor],
  [
    AC_DEFINE(USE_XCURSOR, [1], [Use X Cursor library])
    AC_SUBST(XCURSOR_CFLAGS)
    AC_SUBST(XCURSOR_LIBS)
    xcursor_found=yes
  ],
  [
    xcursor_found=no
  ]
)
else
  xcursor_found=no
fi

AC_ARG_ENABLE(imlib2,
  AC_HELP_STRING(
    [--disable-imlib2],
    [disable use of Imlib2 image library for loading icons. [default=enabled]]
  ),
  [enable_imlib2=$enableval],
  [enable_imlib2=yes]
)

if test "$enable_imlib2" = yes; then
PKG_CHECK_MODULES(IMLIB2, [imlib2],
  [
    AC_DEFINE(USE_IMLIB2, [1], [Use Imlib2 image library])
    AC_SUBST(IMLIB2_CFLAGS)
    AC_SUBST(IMLIB2_LIBS)
    # export it for the pkg-config file
    PKG_CONFIG_IMLIB=imlib2
    AC_SUBST(PKG_CONFIG_IMLIB)
    imlib2_found=yes
  ],
  [
    imlib2_found=no
  ]
)
else
  imlib2_found=no
fi

AM_CONDITIONAL(USE_IMLIB2, [test $imlib2_found = yes])

AC_ARG_ENABLE(librsvg,
  AC_HELP_STRING(
    [--disable-librsvg],
    [disable use of SVG image files for loading icons. [default=enabled]]
  ),
  [enable_librsvg=$enableval],
  [enable_librsvg=yes]
)

if test "$enable_librsvg" = yes; then
PKG_CHECK_MODULES(LIBRSVG, [librsvg-2.0],
  [
    AC_DEFINE(USE_LIBRSVG, [1], [Use SVG image files])
    AC_SUBST(LIBRSVG_CFLAGS)
    AC_SUBST(LIBRSVG_LIBS)
    # export it for the pkg-config file
    PKG_CONFIG_LIBRSVG=librsvg-2.0
    AC_SUBST(PKG_CONFIG_LIBRSVG)
    librsvg_found=yes
  ],
  [
    librsvg_found=no
  ]
)
else
  librsvg_found=no
fi

AM_CONDITIONAL(USE_LIBRSVG, [test $librsvg_found = yes])

dnl Check for session management
X11_SM

#EFENCE_LIBS=-lefence
EFENCE_LIBS=""
AC_SUBST(EFENCE_LIBS)

dnl Check for X11 extensions
X11_EXT_XKB
X11_EXT_XRANDR
X11_EXT_SHAPE
X11_EXT_XINERAMA
X11_EXT_SYNC
X11_EXT_AUTH

AC_CONFIG_FILES([
  Makefile
  m4/Makefile
  po/Makefile.in
  obrender/obrender-3.5.pc
  obt/obt-3.5.pc
  obrender/version.h
  obt/version.h
  version.h
])
AC_CONFIG_COMMANDS([doc],
                   [test -d doc || mkdir doc])
AC_CONFIG_COMMANDS([data],
                   [test -d data || mkdir data])
AC_CONFIG_COMMANDS([data/xsession],
                   [test -d data/xsession || mkdir data/xsession])
AC_OUTPUT

AC_MSG_RESULT
AC_MSG_RESULT([Compiling with these options:
               Startup Notification... $sn_found
               X Cursor Library... $xcursor_found
               Session Management... $SM
               Imlib2 Library... $imlib2_found
               SVG Support (librsvg)... $librsvg_found
               ])
AC_MSG_RESULT([configure complete, now type "make"])
