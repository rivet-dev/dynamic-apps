dnl Copyright (c) 2004-2006
dnl         The Thunar development team. All rights reserved.
dnl
dnl Written for Thunar by Benedikt Meurer <benny@xfce.org>.
dnl



dnl # BM_THUNAR_PLUGIN_APR()
dnl #
dnl # Check whether the "Advanced Properties" plugin
dnl # should be built and installed.
dnl #
AC_DEFUN([BM_THUNAR_PLUGIN_APR],
[
AC_ARG_ENABLE([apr-plugin], [AS_HELP_STRING([--disable-apr-plugin], [Don't build the thunar-apr plugin, see plugins/thunar-apr/README.md])],
  [ac_bm_thunar_plugin_apr=$enableval], [ac_bm_thunar_plugin_apr=yes])
AC_MSG_CHECKING([whether to build the thunar-apr plugin])
AM_CONDITIONAL([THUNAR_PLUGIN_APR], [test x"$ac_bm_thunar_plugin_apr" = x"yes"])
AC_MSG_RESULT([$ac_bm_thunar_plugin_apr])

dnl Check for libexif (for the "Image" properties page)
XDT_CHECK_OPTIONAL_PACKAGE([EXIF], [libexif], [0.6.0], [exif], [Exif support])
])



dnl # BM_THUNAR_PLUGIN_SBR()
dnl #
dnl # Check whether the "Simple Builtin Renamers" plugin
dnl # should be built and installed.
dnl #
AC_DEFUN([BM_THUNAR_PLUGIN_SBR],
[
AC_ARG_ENABLE([sbr-plugin], AS_HELP_STRING([--disable-sbr-plugin], [Don't build the thunar-sbr plugin, see plugins/thunar-sbr/README.md]),
  [ac_bm_thunar_plugin_sbr=$enableval], [ac_bm_thunar_plugin_sbr=yes])
AC_MSG_CHECKING([whether to build the thunar-sbr plugin])
AM_CONDITIONAL([THUNAR_PLUGIN_SBR], [test x"$ac_bm_thunar_plugin_sbr" = x"yes"])
AC_MSG_RESULT([$ac_bm_thunar_plugin_sbr])

dnl Check for PCRE2 (for the "Search & Replace" renamer)
XDT_CHECK_OPTIONAL_PACKAGE([PCRE2], [libpcre2-8], [10.0], [pcre2], [Regular expression support])
])



dnl # BM_THUNAR_PLUGIN_TPA()
dnl #
dnl # Check whether the "Trash Panel Applet" plugin should
dnl # be built and installed (this is actually a plugin
dnl # for the Xfce panel, not for Thunar).
dnl #
AC_DEFUN([BM_THUNAR_PLUGIN_TPA],
[
AC_ARG_ENABLE([tpa-plugin], AS_HELP_STRING([--disable-tpa-plugin], [Don't build the thunar-tpa plugin, see plugins/thunar-tpa/README.md]),
  [ac_bm_thunar_plugin_tpa=$enableval], [ac_bm_thunar_plugin_tpa=yes])
if test x"$ac_bm_thunar_plugin_tpa" = x"yes"; then
  XDT_CHECK_PACKAGE([LIBXFCE4PANEL], [libxfce4panel-2.0], [4.12.0],
  [
    ac_bm_thunar_plugin_tpa=yes
  ],
  [
    dnl # Cannot build thunar-tpa if xfce4-panel is not installed
    ac_bm_thunar_plugin_tpa=no
  ])
else
  ac_bm_thunar_plugin_tpa=no
fi
AC_MSG_CHECKING([whether to build the thunar-tpa plugin])
AM_CONDITIONAL([THUNAR_PLUGIN_TPA], [test x"$ac_bm_thunar_plugin_tpa" = x"yes"])
AC_MSG_RESULT([$ac_bm_thunar_plugin_tpa])
])



dnl # BM_THUNAR_PLUGIN_UCA()
dnl #
dnl # Check whether the "User Customizable Actions" plugin
dnl # should be built and installed.
dnl #
AC_DEFUN([BM_THUNAR_PLUGIN_UCA],
[
AC_ARG_ENABLE([uca-plugin], AS_HELP_STRING([--disable-uca-plugin], [Don't build the thunar-uca plugin, see plugins/thunar-uca/README.md]),
  [ac_bm_thunar_plugin_uca=$enableval], [ac_bm_thunar_plugin_uca=yes])
AC_MSG_CHECKING([whether to build the thunar-uca plugin])
AM_CONDITIONAL([THUNAR_PLUGIN_UCA], [test x"$ac_bm_thunar_plugin_uca" = x"yes"])
AC_MSG_RESULT([$ac_bm_thunar_plugin_uca])
])

dnl # BM_THUNAR_PLUGIN_WALLPAPER()
dnl #
dnl # Check whether the "Wallpaper" plugin
dnl # should be built and installed.
dnl #
AC_DEFUN([BM_THUNAR_PLUGIN_WALLPAPER],
[
AC_ARG_ENABLE([wallpaper-plugin], AS_HELP_STRING([--disable-wallpaper-plugin], [Don't build the thunar-wallpaper plugin, see plugins/thunar-wallpaper/README.md]),
  [ac_bm_thunar_plugin_wallpaper=$enableval], [ac_bm_thunar_plugin_wallpaper=yes])
AC_MSG_CHECKING([whether to build the thunar-wallpaper plugin])
AM_CONDITIONAL([THUNAR_PLUGIN_WALLPAPER], [test x"$ac_bm_thunar_plugin_wallpaper" = x"yes"])
AC_MSG_RESULT([$ac_bm_thunar_plugin_wallpaper])
if test x"$ac_bm_thunar_plugin_wallpaper" = x"yes"; then
	AC_CHECK_PROG([xfconf_query_found], [xfconf-query], [yes], [no])
	if test x"$xfconf_query_found" = x"no"; then
		echo "***"
		echo "*** xfconf-query was not found on your system."
		echo "*** The wallpaper won't work without it installed."
		echo "***"
	fi
fi
])
