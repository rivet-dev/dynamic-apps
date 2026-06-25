
AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"libwindowmenu\" \
	-DWNCK_I_KNOW_THIS_IS_UNSTABLE \
	$(PLATFORM_CPPFLAGS)

plugindir = $(libdir)/xfce4/panel/plugins

plugin_LTLIBRARIES = \
	libwindowmenu.la

libwindowmenu_built_sources = \
	windowmenu-dialog_ui.h

libwindowmenu_la_SOURCES = \
	$(libwindowmenu_built_sources) \
	windowmenu.c \
	windowmenu.h

libwindowmenu_la_CFLAGS = \
	$(GTK_CFLAGS) \
	$(EXO_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(LIBWNCK_CFLAGS) \
	$(PLATFORM_CFLAGS)

libwindowmenu_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
	$(PLATFORM_LDFLAGS)

libwindowmenu_la_LIBADD = \
	$(top_builddir)/libxfce4panel/libxfce4panel-$(LIBXFCE4PANEL_VERSION_API).la \
	$(top_builddir)/common/libpanel-common.la \
	$(GTK_LIBS) \
	$(EXO_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBWNCK_LIBS) \
	$(XFCONF_LIBS)

libwindowmenu_la_DEPENDENCIES = \
	$(top_builddir)/libxfce4panel/libxfce4panel-$(LIBXFCE4PANEL_VERSION_API).la \
	$(top_builddir)/common/libpanel-common.la

#
# xfce4-popup-windowmenu script
#
bin_SCRIPTS = \
	xfce4-popup-windowmenu

xfce4-popup-windowmenu: xfce4-popup-windowmenu.sh Makefile
	$(AM_V_GEN) $(SED) -e "s,\@bindir\@,$(bindir),g" \
	-e "s,\@localedir\@,$(localedir),g" $< >$@

#
# .desktop file
#
desktopdir = $(datadir)/xfce4/panel/plugins
desktop_in_files = windowmenu.desktop.in.in
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	windowmenu-dialog.glade \
	xfce4-popup-windowmenu.sh \
	$(desktop_in_files)

DISTCLEANFILES = \
	$(desktop_DATA) \
	xfce4-popup-windowmenu

if MAINTAINER_MODE
BUILT_SOURCES = \
	$(libwindowmenu_built_sources)

DISTCLEANFILES += \
	$(libwindowmenu_built_sources)

windowmenu-dialog_ui.h: windowmenu-dialog.glade
	$(AM_V_GEN) xdt-csource --static --strip-comments --strip-content --name=windowmenu_dialog_ui $< >$@
endif

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
