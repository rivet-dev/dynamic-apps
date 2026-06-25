
AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"libdirectorymenu\" \
	$(PLATFORM_CPPFLAGS)

plugindir = $(libdir)/xfce4/panel/plugins

plugin_LTLIBRARIES = \
	libdirectorymenu.la

libdirectorymenu_built_sources = \
	directorymenu-dialog_ui.h

libdirectorymenu_la_SOURCES = \
	$(libdirectorymenu_built_sources) \
	directorymenu.c \
	directorymenu.h

libdirectorymenu_la_CFLAGS = \
	$(GTK_CFLAGS) \
	$(EXO_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GIO_UNIX_CFLAGS) \
	$(PLATFORM_CFLAGS)

libdirectorymenu_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
	$(PLATFORM_LDFLAGS)

libdirectorymenu_la_LIBADD = \
	$(top_builddir)/libxfce4panel/libxfce4panel-$(LIBXFCE4PANEL_VERSION_API).la \
	$(top_builddir)/common/libpanel-common.la \
	$(GTK_LIBS) \
	$(EXO_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(GIO_LIBS) \
	$(GIO_UNIX_LIBS) \
	$(XFCONF_LIBS)

libdirectorymenu_la_DEPENDENCIES = \
	$(top_builddir)/libxfce4panel/libxfce4panel-$(LIBXFCE4PANEL_VERSION_API).la \
	$(top_builddir)/common/libpanel-common.la

#
# xfce4-popup-directorymenu script
#
bin_SCRIPTS = \
	xfce4-popup-directorymenu

xfce4-popup-directorymenu: xfce4-popup-directorymenu.sh Makefile
	$(AM_V_GEN) $(SED) -e "s,\@bindir\@,$(bindir),g" \
	-e "s,\@localedir\@,$(localedir),g" $< >$@

#
# .desktop file
#
desktopdir = $(datadir)/xfce4/panel/plugins
desktop_in_files = directorymenu.desktop.in.in
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	directorymenu-dialog.glade \
	xfce4-popup-directorymenu.sh \
	$(desktop_in_files)

DISTCLEANFILES = \
	$(desktop_DATA) \
	xfce4-popup-directorymenu

if MAINTAINER_MODE
BUILT_SOURCES = \
	$(libdirectorymenu_built_sources)

DISTCLEANFILES += \
	$(libdirectorymenu_built_sources)

directorymenu-dialog_ui.h: directorymenu-dialog.glade
	$(AM_V_GEN) xdt-csource --static --strip-comments --strip-content --name=directorymenu_dialog_ui $< >$@
endif

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
