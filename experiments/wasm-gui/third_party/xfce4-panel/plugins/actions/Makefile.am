
AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"libactions\" \
	$(PLATFORM_CPPFLAGS)

plugindir = $(libdir)/xfce4/panel/plugins

plugin_LTLIBRARIES = \
	libactions.la

libactions_built_sources = \
	actions-dialog_ui.h

libactions_la_SOURCES = \
	$(libactions_built_sources) \
	actions.c \
	actions.h

libactions_la_CFLAGS = \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(GIO_CFLAGS) \
	$(PLATFORM_CFLAGS)

libactions_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
	$(PLATFORM_LDFLAGS)

libactions_la_LIBADD = \
	$(top_builddir)/libxfce4panel/libxfce4panel-$(LIBXFCE4PANEL_VERSION_API).la \
	$(top_builddir)/common/libpanel-common.la \
	$(GTK_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(GIO_LIBS) \
	$(XFCONF_LIBS)

libactions_la_DEPENDENCIES = \
	$(top_builddir)/libxfce4panel/libxfce4panel-$(LIBXFCE4PANEL_VERSION_API).la \
	$(top_builddir)/common/libpanel-common.la

#
# .desktop file
#
desktopdir = $(datadir)/xfce4/panel/plugins
desktop_in_files = actions.desktop.in.in
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	actions-dialog.glade \
	$(desktop_in_files)

DISTCLEANFILES = \
	$(desktop_DATA)

if MAINTAINER_MODE
BUILT_SOURCES = \
	$(libactions_built_sources)

DISTCLEANFILES += \
	$(libactions_built_sources)

actions-dialog_ui.h: actions-dialog.glade
	$(AM_V_GEN) xdt-csource --static --strip-comments --strip-content --name=actions_dialog_ui $< >$@
endif

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
