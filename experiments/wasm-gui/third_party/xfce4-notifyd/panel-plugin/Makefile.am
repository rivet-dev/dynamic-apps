AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"xfce4-notification-plugin\" \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	$(PLATFORM_CPPFLAGS)

#
# Notification plugin
#
plugin_LTLIBRARIES = \
	libnotification-plugin.la

plugindir = \
	$(libdir)/xfce4/panel/plugins

libnotification_plugin_la_SOURCES = \
	notification-plugin.c \
	notification-plugin.h \
	notification-plugin-dialogs.c \
	notification-plugin-dialogs.h \
	notification-plugin-log.c \
	notification-plugin-log.h \
	notification-plugin-settings-ui.c \
	notification-plugin-settings-ui.h

libnotification_plugin_la_CFLAGS = \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(LIBXFCE4PANEL_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(PLATFORM_CFLAGS)

libnotification_plugin_la_LDFLAGS = \
       -avoid-version \
       -module \
       -no-undefined \
       -export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
       $(PLATFORM_LDFLAGS)

libnotification_plugin_la_LIBADD = \
	$(top_builddir)/common/libxfce-notifyd-common.la \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBXFCE4PANEL_LIBS)	\
	$(XFCONF_LIBS)

desktopdir = $(datadir)/xfce4/panel/plugins
desktop_in_files = notification-plugin.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

glade_files = \
	notification-plugin-settings.glade

built_sources = \
	notification-plugin-settings-ui.c \
	notification-plugin-settings-ui.h

dist_noinst_DATA =	\
	notification-plugin-settings-ui.gresource.xml \
	$(glade_files) \
	$(desktop_in_files)

BUILT_SOURCES = \
	$(built_sources)

CLEANFILES = \
	$(built_sources) \
	$(desktop_DATA)

%.desktop: $(srcdir)/%.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

%-ui.h: $(srcdir)/%-ui.gresource.xml $(glade_files) Makefile
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header --manual-register $<

%-ui.c: $(srcdir)/%-ui.gresource.xml $(glade_files) Makefile
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source --manual-register $<

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
