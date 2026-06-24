AC_PREREQ(2.62)

m4_define([wnck_major_version], [3])
m4_define([wnck_minor_version], [4])
m4_define([wnck_micro_version], [9])
m4_define([wnck_version],
          [wnck_major_version.wnck_minor_version.wnck_micro_version])

AC_INIT([libwnck], [wnck_version],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=libwnck])

AC_CONFIG_HEADERS(config.h)

AM_INIT_AUTOMAKE([1.11 no-dist-gzip dist-xz tar-ustar])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_CONFIG_MACRO_DIR([m4])

GNOME_MAINTAINER_MODE_DEFINES

dnl libtool versioning for libwnck

dnl increment if the interface has additions, changes, removals.
LIBWNCK_CURRENT=2

dnl increment any time the source changes; set to
dnl  0 if you increment CURRENT
LIBWNCK_REVISION=2

dnl increment if any interfaces have been added; set to 0
dnl  if any interfaces have been changed or removed. removal has
dnl  precedence over adding, so set to 0 if both happened.
LIBWNCK_AGE=2

AC_SUBST(LIBWNCK_CURRENT)
AC_SUBST(LIBWNCK_REVISION)
AC_SUBST(LIBWNCK_AGE)

WNCK_MAJOR_VERSION=wnck_major_version
WNCK_MINOR_VERSION=wnck_minor_version
WNCK_MICRO_VERSION=wnck_micro_version
AC_SUBST(WNCK_MAJOR_VERSION)
AC_SUBST(WNCK_MINOR_VERSION)
AC_SUBST(WNCK_MICRO_VERSION)

LT_PREREQ([2.2.6])
LT_INIT([dlopen disable-static])

IT_PROG_INTLTOOL([0.40.6])

AC_PROG_CC

GNOME_COMPILE_WARNINGS([maximum])

AC_ARG_ENABLE(deprecation_flags,
              [AC_HELP_STRING([--enable-deprecation-flags],
                              [use *_DISABLE_DEPRECATED flags @<:@default=no@:>@])],,
              [enable_deprecation_flags=no])

if test "x$enable_deprecation_flags" = "xyes"; then
   DISABLE_DEPRECATED_CFLAGS=$DISABLE_DEPRECATED
   AC_SUBST(DISABLE_DEPRECATED_CFLAGS)
fi

AC_ARG_ENABLE(tools,
              [AC_HELP_STRING([--enable-tools],
                              [install wnck-based tools @<:@default=yes@:>@])],,
              [enable_tools=yes])
AM_CONDITIONAL(ENABLE_TOOLS, test "x$enable_tools" != "xno")

GETTEXT_PACKAGE=libwnck-3.0
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Package name])

AM_GLIB_GNU_GETTEXT

# AM_GLIB_GNU_GETTEXT above substs $DATADIRNAME
# this is the directory where the *.{mo,gmo} files are installed
wncklocaledir='${prefix}/${DATADIRNAME}/locale'
AC_SUBST(wncklocaledir)

AC_PATH_PROG(PKG_CONFIG, pkg-config)

AC_CHECK_LIBM
AC_SUBST(LIBM)

AC_ARG_ENABLE(startup-notification,
              [AC_HELP_STRING([--enable-startup-notification],
                              [startup notification library @<:@default=auto@:>@])],,
              [enable_startup_notification=auto])

STARTUP_NOTIFICATION_PACKAGE=
if test "$enable_startup_notification" != "no"; then
   STARTUP_NOTIFICATION_REQUIRED=0.4
   PKG_CHECK_MODULES(STARTUP_NOTIFICATION,
      [libstartup-notification-1.0 >= $STARTUP_NOTIFICATION_REQUIRED],
      [have_sn=yes],
      [have_sn=no])

   if test "$have_sn" = "no" -a "$enable_start_notification" = "yes"; then
      AC_MSG_ERROR([startup-notification support required but not found])
   fi

   if test "$have_sn" = "yes"; then
      AC_DEFINE_UNQUOTED(HAVE_STARTUP_NOTIFICATION, 1, [Define if you have libstartup-notification])
      STARTUP_NOTIFICATION_PACKAGE=libstartup-notification-1.0
   fi
else
   have_sn=no
fi
AC_SUBST(STARTUP_NOTIFICATION_PACKAGE)

PKG_CHECK_MODULES(XLIB, x11,
    X11_PACKAGE=x11,
    [X11_PACKAGE=
     AC_PATH_XTRA
     if test "x$no_x" = xyes; then
       AC_MSG_ERROR("no (requires X development libraries)")
     else
       XLIB_LIBS="$X_PRE_LIBS $X_LIBS -lX11 $X_EXTRA_LIBS"
       XLIB_CFLAGS=$X_CFLAGS
     fi])
AC_SUBST(X11_PACKAGE)

AC_MSG_CHECKING([if building with X Resource Information Extension Library])
if $PKG_CONFIG xres; then
  AC_MSG_RESULT([yes])
  have_xres=yes
  AC_DEFINE_UNQUOTED(HAVE_XRES, 1, [Define if you have the XRes library])
  XRES_PACKAGE=xres
else
  AC_CHECK_LIB(XRes, XResQueryExtension,          
               [XLIB_LIBS="$XLIB_LIBS -lXRes -lXext"
                have_xres=yes
                AC_DEFINE_UNQUOTED(HAVE_XRES, 1, [Define if you have the XRes library])
                AC_MSG_RESULT([yes])],
               [have_xres=no
                AC_MSG_RESULT([no])],
               $XLIB_LIBS -lXext)
  XRES_PACKAGE=
fi
AC_SUBST(XRES_PACKAGE)
AC_SUBST(XLIB_CFLAGS)
AC_SUBST(XLIB_LIBS)

PKG_CHECK_MODULES([LIBWNCK],[
	gtk+-3.0 >= 3.4
	glib-2.0 >= 2.32
	gobject-2.0 >= 2.13.0
	$STARTUP_NOTIFICATION_PACKAGE
	$XRES_PACKAGE
])

AC_PATH_PROG(GDK_PIXBUF_CSOURCE, gdk-pixbuf-csource, no)

if test x"$GDK_PIXBUF_CSOURCE" = xno; then
  AC_MSG_ERROR([gdk-pixbuf-csource executable not found in your path - should be installed with GTK])
fi

AC_SUBST(GDK_PIXBUF_CSOURCE)

##################################################
# Checks for gtk-doc and docbook-tools
##################################################
GTK_DOC_CHECK([1.9])

GOBJECT_INTROSPECTION_CHECK([0.6.14])

AC_OUTPUT([
Makefile
doc/Makefile
po/Makefile.in
libwnck/Makefile
libwnck/version.h
libwnck-3.0.pc
libwnck-3.0-uninstalled.pc
])

dnl ---------------------------------------------------------------------------
dnl - Show summary
dnl ---------------------------------------------------------------------------

echo "
              libwnck $VERSION
              `echo libwnck $VERSION | sed "s/./=/g"`

        prefix:                       ${prefix}
        exec_prefix:                  ${exec_prefix}
        libdir:                       ${libdir}
        bindir:                       ${bindir}
        sbindir:                      ${sbindir}
        sysconfdir:                   ${sysconfdir}
        localstatedir:                ${localstatedir}
        datadir:                      ${datadir}
        source code location:         ${srcdir}
        compiler:                     ${CC}
        cflags:                       ${CFLAGS}
        Maintainer mode:              ${USE_MAINTAINER_MODE}
        Use *_DISABLE_DEPRECATED:     ${enable_deprecation_flags}

        Startup notification support: ${have_sn}
        XRes support:                 ${have_xres}
        Build introspection support:  ${found_introspection}
        Build gtk-doc documentation:  ${enable_gtk_doc}
        Install wnck-based tools:     ${enable_tools}

"
