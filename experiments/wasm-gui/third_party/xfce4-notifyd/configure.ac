dnl configure.ac
dnl
dnl xfce4-notifyd - server-side implementation of the fd.o notifications spec
dnl
dnl Copyright (c) 2008 Brian Tarricone <brian@tarricone.org>
dnl Copyright (c) 2019 Simon Steinbeiss <simon@xfce.org>
dnl

m4_define([libx11_minimum_version], [1.6.7])
m4_define([glib_minimum_version], [2.68])
m4_define([gtk_minimum_version], [3.22])
m4_define([canberra_minimum_version], [0.30])
m4_define([sqlite_minimum_version], [3.34])
m4_define([dbus_minimum_version], [1.0])
m4_define([systemd_minimum_version], [245])

dnl version info
XDT_VERSION_INIT([0.9.6])

dnl init autoconf
AC_INIT([xfce4-notifyd], [xdt_version],
        [https://gitlab.xfce.org/apps/xfce4-notifyd],
        [xfce4-notifyd])
AC_PREREQ([2.59c])
AC_CONFIG_SRCDIR([xfce4-notifyd/main.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl init automake
AM_INIT_AUTOMAKE([1.11 dist-bzip2 tar-ustar no-dist-gzip subdir-objects foreign])
AM_MAINTAINER_MODE()

dnl set helper path prefix
AC_ARG_WITH([helper-path-prefix],
            [AS_HELP_STRING([--with-helper-path-prefix=PATH],[Path prefix under which helper executables will be installed (default: $libdir)])],
            [HELPER_PATH_PREFIX="$withval"],
            [HELPER_PATH_PREFIX="$libdir"])
AC_SUBST([HELPER_PATH_PREFIX])

dnl check for UNIX variants
AC_USE_SYSTEM_EXTENSIONS()

dnl check for basic programs
AC_PROG_CC()
AM_PROG_CC_C_O()
LT_PATH_LD([])
AC_PROG_INSTALL()
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

dnl check for standard header files
AC_CHECK_HEADERS([errno.h fcntl.h  grp.h signal.h stdlib.h string.h \
                  sys/stat.h sys/time.h sys/types.h sys/wait.h \
                  unistd.h math.h])

dnl Check for i18n support
GETTEXT_PACKAGE="$PACKAGE"
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Name of default gettext domain])
AC_SUBST([GETTEXT_PACKAGE])
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.20])

dnl check for required packages
XDT_CHECK_PACKAGE([GIO], [gio-2.0], [glib_minimum_version])
XDT_CHECK_PACKAGE([GIO_UNIX], [gio-unix-2.0], [glib_minimum_version])
XDT_CHECK_PACKAGE([GLIB], [glib-2.0], [glib_minimum_version])
XDT_CHECK_PACKAGE([GTK], [gtk+-3.0], [gtk_minimum_version])
XDT_CHECK_PACKAGE([LIBNOTIFY], [libnotify], [0.7.0])
XDT_CHECK_PACKAGE([LIBXFCE4PANEL], [libxfce4panel-2.0], [4.14.0])
XDT_CHECK_PACKAGE([LIBXFCE4UI], [libxfce4ui-2], [4.12.0])
XDT_CHECK_PACKAGE([LIBXFCE4UTIL], [libxfce4util-1.0], [4.12.0])
XDT_CHECK_PACKAGE([XFCONF], [libxfconf-0], [4.10.0])
XDT_CHECK_PACKAGE([SQLITE], [sqlite3], [sqlite_minimum_version])
XDT_CHECK_PACKAGE([DBUS], [dbus-1], [dbus_minimum_version])

XDT_CHECK_OPTIONAL_FEATURE([X11],
                           [x11],
                           [
                               XDT_FEATURE_DEPENDENCY([LIBX11], [x11], [libx11_minimum_version])
                               XDT_FEATURE_DEPENDENCY([GDK_X11], [gdk-x11-3.0], [gtk_minimum_version])
                           ],
                           [the X11 windowing system])

XDT_CHECK_OPTIONAL_FEATURE([WAYLAND],
                           [wayland],
                           [
                               XDT_FEATURE_DEPENDENCY([GDK_WAYLAND], [gdk-wayland-3.0], [gtk_minimum_version])
                               XDT_FEATURE_DEPENDENCY([GTK_LAYER_SHELL], [gtk-layer-shell-0], [0.7.0])
                           ],
                           [the Wayland windowing system])

XDT_CHECK_OPTIONAL_FEATURE([SOUND],
                           [sound],
                           [XDT_FEATURE_DEPENDENCY([CANBERRA_GTK], [libcanberra-gtk3], [canberra_minimum_version])],
                           [notification sound])

XDT_CHECK_PACKAGE_BINARY([GDBUS_CODEGEN], [gio-2.0], [gdbus_codegen], [gdbus-codegen])
XDT_CHECK_PACKAGE_BINARY([GLIB_COMPILE_RESOURCES], [gio-2.0], [glib_compile_resources], [glib-compile-resources])
XDT_CHECK_PACKAGE_BINARY([GLIB_GENMARSHAL], [glib-2.0], [glib_genmarshal], [glib-genmarshal])
XDT_CHECK_PACKAGE_BINARY([GLIB_MKENUMS], [glib-2.0], [glib_mkenums], [glib-mkenums])

dnl d-bus activation files
AC_ARG_ENABLE([dbus-start-daemon],
              [AS_HELP_STRING([--disable-dbus-start-daemon],
                              [Do not install the D-Bus auto-activation files (default=enabled)])],
              [use_dbus_start_daemon=$enableval], [use_dbus_start_daemon=yes])
AM_CONDITIONAL([USE_DBUS_START_DAEMON],
               [test "x$use_dbus_start_daemon" = "xyes"])

AC_ARG_WITH([dbus-service-dir],
            [AS_HELP_STRING([--with-dbus-service-dir=DIR],
                            [Specifies the directory where D-Bus service activation files should be installed (default=${datadir}/dbus-1/services)])],
            [dbus_service_dir=$withval], [dbus_service_dir="${datadir}/dbus-1/services"])
if test x"$use_dbus_start_daemon" = x"yes" -a x"$dbus_service_dir" = x"auto"; then
    dbus_service_dir="`$PKG_CONFIG --variable=session_bus_services_dir dbus-1`"
fi
DBUS_SERVICE_DIR="$dbus_service_dir"
AC_SUBST([DBUS_SERVICE_DIR])

dnl systemd service files
XDT_CHECK_OPTIONAL_PACKAGE([SYSTEMD], [systemd], [systemd_minimum_version], [systemd], [systemd])
if test x"$enable_systemd" != x"no"; then
    enable_systemd=yes
fi
AM_CONDITIONAL([USE_SYSTEMD], [test x"$enable_systemd" = x"yes"])

dnl note that the default does not use ${libdir}, as that is often something
dnl like ${prefix}/lib/x86_64-linux-gnu, which is generally not correct for
dnl where systemd keeps unit files.
AC_ARG_WITH([systemd-user-service-dir],
            [AS_HELP_STRING([--with-systemd-user-service-dir=DIR],
                            [Specifies the directory where user bus services should be installed (default=${prefix}/lib/systemd/user)])],
            [systemd_user_service_dir=$withval], [systemd_user_service_dir="${prefix}/lib/systemd/user"])
if test x"$enable_systemd" != x"no"; then
    if test x"$systemd_user_service_dir" = x"auto"; then
        if test x"$SYSTEMD_FOUND" != x"yes"; then
            AC_MSG_ERROR([systemd was not found, but --systemd-user-service-dir=auto was passed])
        fi
        systemd_user_service_dir="`$PKG_CONFIG --variable=systemd_user_unit_dir systemd`"
    fi
    SYSTEMD_USER_SERVICE_DIR="$systemd_user_service_dir"
    AC_SUBST([SYSTEMD_USER_SERVICE_DIR])
fi

dnl XDG autostart files
AC_ARG_ENABLE([xdg-autostart],
              [AS_HELP_STRING([--disable-xdg-autostart],
                              [Do not install an XDG autostart file to start xfce4-notifyd when the desktop session starts (default=enabled)])],
              [use_xdg_autostart=$enableval], [use_xdg_autostart=yes])
AM_CONDITIONAL([USE_XDG_AUTOSTART],
               [test x"$use_xdg_autostart" = x"yes"])

dnl check for debugging support
XDT_FEATURE_DEBUG([xdt_debug_default])

AC_CONFIG_FILES(
[
    Makefile
    icons/Makefile
    icons/16x16/Makefile
    icons/48x48/Makefile
    icons/128x128/Makefile
    icons/scalable/Makefile
    icons/symbolic/Makefile
    common/Makefile
    panel-plugin/Makefile
    po/Makefile.in
    themes/Makefile
    tests/Makefile
    xfce4-notifyd/Makefile
    xfce4-notifyd-config/Makefile
])
AC_OUTPUT

dnl ***************************
dnl *** Print configuration ***
dnl ***************************
echo
echo "Build Configuration:"
echo
echo "* Installation prefix:       $libdir"
echo "* Debug Support:             $enable_debug"
echo "* X11 Support:               $ENABLE_X11"
echo "* Wayland Support:           $ENABLE_WAYLAND"
echo "* Sound support:             $ENABLE_SOUND"
echo "* Startup methods:"
echo "  - D-Bus activation:        $use_dbus_start_daemon ($DBUS_SERVICE_DIR)"
echo "  - Systemd:                 $enable_systemd ($SYSTEMD_USER_SERVICE_DIR)"
echo "  - XDG autostart:           $use_xdg_autostart ($sysconfdir/xdg/autostart)"
echo
