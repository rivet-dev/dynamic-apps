dnl ***************************
dnl *** Version information ***
dnl ***************************
XDT_VERSION_INIT([0.6.1])

dnl minimum required versions
m4_define([glib_min_version], [2.56.2])
m4_define([gtk_min_version], [3.22.0])
m4_define([gtksourceview3_min_version], [3.24.0])
m4_define([gtksourceview4_min_version], [4.0.0])
m4_define([polkit_min_version], [0.102])

dnl ***************************
dnl *** Initialize autoconf ***
dnl ***************************
AC_COPYRIGHT([Copyright (C) 2005-2023
        The Mousepad developers. All rights reserved.])
AC_INIT([Mousepad], [xdt_version], [https://gitlab.xfce.org/apps/mousepad], [mousepad])
AC_PREREQ([2.69])
AC_CONFIG_MACRO_DIRS([m4])
AC_CANONICAL_TARGET()
AC_REVISION([xdt_version_build])

dnl ***************************
dnl *** Initialize automake ***
dnl ***************************
AM_INIT_AUTOMAKE([1.8 no-dist-gzip dist-bzip2 tar-ustar foreign])
AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE()
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl *******************************
dnl *** Check for UNIX variants ***
dnl *******************************
AC_USE_SYSTEM_EXTENSIONS
AC_SEARCH_LIBS([strerror],[cposix])

dnl ********************************
dnl *** Check for basic programs ***
dnl ********************************
AC_PROG_CC()
AM_PROG_CC_C_O()
LT_PATH_LD([])
AC_PROG_INSTALL()
AC_PROG_SED

dnl **************************
dnl *** Initialize libtool ***
dnl **************************
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

dnl **************************************
dnl *** Substitute version information ***
dnl **************************************
MOUSEPAD_VERSION_MAJOR=xdt_version_major()
MOUSEPAD_VERSION_MINOR=xdt_version_minor()
MOUSEPAD_VERSION_MICRO=xdt_version_micro()
AC_SUBST([MOUSEPAD_VERSION_MAJOR])
AC_SUBST([MOUSEPAD_VERSION_MINOR])
AC_SUBST([MOUSEPAD_VERSION_MICRO])

dnl ************************************
dnl *** Check for standard libraries ***
dnl ************************************
AC_SEARCH_LIBS([round], [m], [], [
  AC_MSG_ERROR([unable to find the round() function in math library])
])
AC_CHECK_FUNCS([realpath bind_textdomain_codeset])

dnl **********************************
dnl *** Check for standard headers ***
dnl **********************************
AC_CHECK_HEADERS([errno.h libintl.h locale.h math.h stdio.h stdlib.h string.h])

dnl ******************************
dnl *** Check for i18n support ***
dnl ******************************
GETTEXT_PACKAGE="$PACKAGE"
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Name of default gettext domain])
AC_SUBST([GETTEXT_PACKAGE])
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.8])

dnl ***********************************
dnl *** Check for required packages ***
dnl ***********************************
XDT_CHECK_PACKAGE([GLIB], [glib-2.0], [glib_min_version])
XDT_CHECK_PACKAGE([GMODULE], [gmodule-2.0], [glib_min_version])
XDT_CHECK_PACKAGE([GTK], [gtk+-3.0], [gtk_min_version])

dnl *******************************
dnl *** GtkSourceView 4 support ***
dnl *******************************
AC_ARG_ENABLE([gtksourceview4],
              [AS_HELP_STRING([--enable-gtksourceview4],
                              [Build against GtkSourceView 4 instead of GtkSourceView 3])],
              [enable_gtksourceview4=$enableval],
              [enable_gtksourceview4=auto])
AS_IF([test "x$enable_gtksourceview4" = "xyes"],
      [PKG_CHECK_MODULES([GTKSOURCEVIEW], [gtksourceview-4 >= gtksourceview4_min_version])])
AS_IF([test "x$enable_gtksourceview4" = "xauto"],
      [PKG_CHECK_MODULES([GTKSOURCEVIEW], [gtksourceview-4 >= gtksourceview4_min_version],
                         [enable_gtksourceview4=yes], [enable_gtksourceview4=no])])
AS_IF([test "x$enable_gtksourceview4" = "xno"],
      [PKG_CHECK_MODULES([GTKSOURCEVIEW], [gtksourceview-3.0 >= gtksourceview3_min_version])])

dnl **********************
dnl *** Polkit support ***
dnl **********************
XDT_CHECK_OPTIONAL_PACKAGE([POLKIT], [polkit-gobject-1], [polkit_min_version], [polkit], [Polkit support])

dnl ************************************
dnl *** Enable support for GSettings ***
dnl ************************************
GLIB_GSETTINGS

AC_ARG_ENABLE([keyfile-settings],
              [AS_HELP_STRING([--enable-keyfile-settings],
                              [Use the GSettings keyfile backend rather than the default])],
              [enable_keyfile_settings="yes"],
              [enable_keyfile_settings="default"])

AM_CONDITIONAL([WITH_KEYFILE_SETTINGS],
               [test "x$enable_keyfile_settings" = "xyes"])

dnl *************************
dnl *** Check for plugins ***
dnl *************************
MOUSEPAD_PLUGIN_GSPELL()
MOUSEPAD_PLUGIN_SHORTCUTS()
MOUSEPAD_PLUGIN_TEST()

dnl ***********************************
dnl *** Check for debugging support ***
dnl ***********************************
XDT_FEATURE_DEBUG([xdt_debug_default])

dnl **************************************
dnl *** Check for linker optimizations ***
dnl **************************************
XDT_FEATURE_LINKER_OPTS()

dnl *********************************
dnl *** Substitute platform flags ***
dnl *********************************
AC_MSG_CHECKING([PLATFORM_CPPFLAGS])
AC_MSG_RESULT([$PLATFORM_CPPFLAGS])
AC_SUBST([PLATFORM_CPPFLAGS])
AC_MSG_CHECKING([PLATFORM_CFLAGS])
AC_MSG_RESULT([$PLATFORM_CFLAGS])
AC_SUBST([PLATFORM_CFLAGS])
AC_MSG_CHECKING([PLATFORM_LDFLAGS])
AC_MSG_RESULT([$PLATFORM_LDFLAGS])
AC_SUBST([PLATFORM_LDFLAGS])

AC_CONFIG_FILES([
Makefile
mousepad/Makefile
plugins/Makefile
plugins/gspell-plugin/Makefile
plugins/shortcuts-plugin/Makefile
plugins/test-plugin/Makefile
po/Makefile.in
icons/Makefile
icons/16x16/Makefile
icons/48x48/Makefile
icons/128x128/Makefile
icons/scalable/Makefile
])
AC_OUTPUT

dnl ***************************
dnl *** Print configuration ***
dnl ***************************
echo
echo "Build Configuration:"
echo
echo "* Debug Support:                 $enable_debug"
echo "* Use keyfile backend:           $enable_keyfile_settings"
echo "* Build against GtkSourceView 4: $enable_gtksourceview4"
echo "* Polkit support:                ${POLKIT_FOUND:-no}"
echo
echo "Plugins:"
echo
echo "* gspell:                        $ac_mousepad_plugin_gspell"
echo "* shortcuts:                     $ac_mousepad_plugin_shortcuts"
echo "* test:                          $ac_mousepad_plugin_test"
echo
