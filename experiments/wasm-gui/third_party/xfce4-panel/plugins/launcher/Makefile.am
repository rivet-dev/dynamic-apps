
AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"liblauncher\" \
	-DSN_API_NOT_YET_FROZEN \
	$(PLATFORM_CPPFLAGS)

plugindir = $(libdir)/xfce4/panel/plugins

plugin_LTLIBRARIES = \
	liblauncher.la

liblauncher_built_sources = \
	launcher-dialog_ui.h

liblauncher_la_SOURCES = \
	$(liblauncher_built_sources) \
	launcher.c \
	launcher.h \
	launcher-dialog.c \
	launcher-dialog.h

liblauncher_la_CFLAGS = \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(GARCON_CFLAGS) \
	$(GARCON_GTK3_CFLAGS) \
	$(GIO_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(PLATFORM_CFLAGS)

liblauncher_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
	$(PLATFORM_LDFLAGS)

liblauncher_la_LIBADD = \
	$(top_builddir)/libxfce4panel/libxfce4panel-$(LIBXFCE4PANEL_VERSION_API).la \
	$(top_builddir)/common/libpanel-common.la \
	$(GTK_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(GARCON_LIBS) \
	$(GARCON_GTK3_LIBS) \
	$(GIO_LIBS) \
	$(XFCONF_LIBS)

liblauncher_la_DEPENDENCIES = \
	$(top_builddir)/libxfce4panel/libxfce4panel-$(LIBXFCE4PANEL_VERSION_API).la \
	$(top_builddir)/common/libpanel-common.la

#
# .desktop file
#
desktopdir = $(datadir)/xfce4/panel/plugins
desktop_in_files = launcher.desktop.in.in
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	launcher-dialog.glade \
	$(desktop_in_files)

DISTCLEANFILES = \
	$(desktop_DATA)

if MAINTAINER_MODE
BUILT_SOURCES = \
	$(liblauncher_built_sources)

DISTCLEANFILES += \
	$(libclock_built_sources)

launcher-dialog_ui.h: launcher-dialog.glade
	$(AM_V_GEN) xdt-csource --static --strip-comments --strip-content --name=launcher_dialog_ui $< >$@
endif

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
