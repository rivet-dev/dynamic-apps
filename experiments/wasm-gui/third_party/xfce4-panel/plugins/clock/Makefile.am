
AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"libclock\" \
	$(PLATFORM_CPPFLAGS)

plugindir = $(libdir)/xfce4/panel/plugins

plugin_LTLIBRARIES = \
	libclock.la

libclock_built_sources = \
	clock-dialog_ui.h

libclock_la_SOURCES = \
	$(libclock_built_sources) \
	clock.c \
	clock.h \
	clock-time.c \
	clock-time.h \
	clock-analog.c \
	clock-analog.h \
	clock-binary.c \
	clock-binary.h \
	clock-digital.c \
	clock-digital.h \
	clock-fuzzy.c \
	clock-fuzzy.h \
	clock-lcd.c \
	clock-lcd.h \
	clock-sleep-monitor.c \
	clock-sleep-monitor.h

libclock_la_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(CAIRO_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(PLATFORM_CFLAGS)

libclock_la_LDFLAGS =	 \
	-avoid-version \
	-module \
	-no-undefined \
	-export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
	$(PLATFORM_LDFLAGS)

libclock_la_LIBADD = \
	$(top_builddir)/libxfce4panel/libxfce4panel-$(LIBXFCE4PANEL_VERSION_API).la \
	$(top_builddir)/common/libpanel-common.la \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(CAIRO_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(XFCONF_LIBS) \
	-lm

libclock_la_DEPENDENCIES = \
	$(top_builddir)/libxfce4panel/libxfce4panel-$(LIBXFCE4PANEL_VERSION_API).la \
	$(top_builddir)/common/libpanel-common.la

#
# .desktop file
#
desktopdir = $(datadir)/xfce4/panel/plugins
desktop_in_files =	clock.desktop.in.in
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	clock-dialog.glade \
	$(desktop_in_files)

DISTCLEANFILES = \
	$(desktop_DATA)

if MAINTAINER_MODE
BUILT_SOURCES = \
	$(libclock_built_sources)

DISTCLEANFILES += \
	$(libclock_built_sources)

clock-dialog_ui.h: clock-dialog.glade
	$(AM_V_GEN) xdt-csource --static --strip-comments --strip-content --name=clock_dialog_ui $< >$@
endif

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
