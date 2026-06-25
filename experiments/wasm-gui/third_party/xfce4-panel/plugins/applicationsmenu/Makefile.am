
AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"libapplicationsmenu\" \
	$(PLATFORM_CPPFLAGS)

plugindir = $(libdir)/xfce4/panel/plugins

plugin_LTLIBRARIES = \
	libapplicationsmenu.la

libapplicationsmenu_built_sources = \
	applicationsmenu-dialog_ui.h

libapplicationsmenu_la_SOURCES = \
	$(libapplicationsmenu_built_sources) \
	applicationsmenu.c \
	applicationsmenu.h

libapplicationsmenu_la_CFLAGS = \
	$(GTK_CFLAGS) \
	$(EXO_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(GARCON_CFLAGS) \
	$(GARCON_GTK3_CFLAGS) \
	$(PLATFORM_CFLAGS)

libapplicationsmenu_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
	$(PLATFORM_LDFLAGS)

libapplicationsmenu_la_LIBADD = \
	$(top_builddir)/libxfce4panel/libxfce4panel-$(LIBXFCE4PANEL_VERSION_API).la \
	$(top_builddir)/common/libpanel-common.la \
	$(GTK_LIBS) \
	$(EXO_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(GARCON_LIBS) \
	$(GARCON_GTK3_LIBS) \
	$(XFCONF_LIBS)

libapplicationsmenu_la_DEPENDENCIES = \
	$(top_builddir)/libxfce4panel/libxfce4panel-$(LIBXFCE4PANEL_VERSION_API).la \
	$(top_builddir)/common/libpanel-common.la

#
# xfce4-popup-applicationsmenu script
#
bin_SCRIPTS = \
	xfce4-popup-applicationsmenu

xfce4-popup-applicationsmenu: xfce4-popup-applicationsmenu.sh Makefile
	$(AM_V_GEN) $(SED) -e "s,\@bindir\@,$(bindir),g" \
	-e "s,\@localedir\@,$(localedir),g" $< >$@

#
# .desktop file
#
desktopdir = $(datadir)/xfce4/panel/plugins
desktop_in_files = applicationsmenu.desktop.in.in
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	applicationsmenu-dialog.glade \
	xfce4-popup-applicationsmenu.sh \
	$(desktop_in_files)

DISTCLEANFILES = \
	$(desktop_DATA) \
	xfce4-popup-applicationsmenu

if MAINTAINER_MODE
BUILT_SOURCES = \
	$(libapplicationsmenu_built_sources)

DISTCLEANFILES += \
	$(libapplicationsmenu_built_sources)

applicationsmenu-dialog_ui.h: applicationsmenu-dialog.glade
	$(AM_V_GEN) xdt-csource --static --strip-comments --strip-content --name=applicationsmenu_dialog_ui $< >$@
endif

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
