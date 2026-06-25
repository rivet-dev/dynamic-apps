
AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"libpager\" \
	-DWNCK_I_KNOW_THIS_IS_UNSTABLE \
	$(PLATFORM_CPPFLAGS)

plugindir = $(libdir)/xfce4/panel/plugins

plugin_LTLIBRARIES = \
	libpager.la

libpager_built_sources = \
	pager-dialog_ui.h

libpager_la_SOURCES = \
	$(libpager_built_sources) \
	pager.c \
	pager.h \
	pager-buttons.h \
	pager-buttons.c

libpager_la_CFLAGS = \
	$(GTK_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBWNCK_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(PLATFORM_CFLAGS)

libpager_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
	$(PLATFORM_LDFLAGS)

libpager_la_LIBADD = \
	$(top_builddir)/libxfce4panel/libxfce4panel-$(LIBXFCE4PANEL_VERSION_API).la \
	$(top_builddir)/common/libpanel-common.la \
	$(GTK_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBWNCK_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(XFCONF_LIBS)

libpager_la_DEPENDENCIES = \
	$(top_builddir)/libxfce4panel/libxfce4panel-$(LIBXFCE4PANEL_VERSION_API).la \
	$(top_builddir)/common/libpanel-common.la

#
# .desktop file
#
desktopdir = $(datadir)/xfce4/panel/plugins
desktop_in_files = pager.desktop.in.in
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	pager-dialog.glade \
	$(desktop_in_files)

DISTCLEANFILES = \
	$(desktop_DATA)

if MAINTAINER_MODE
BUILT_SOURCES = \
	$(libpager_built_sources)

DISTCLEANFILES += \
	$(libpager_built_sources)

pager-dialog_ui.h: pager-dialog.glade
	$(AM_V_GEN) xdt-csource --static --strip-comments --strip-content --name=pager_dialog_ui $< >$@
endif

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
