
AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"libpanel-common\" \
	-DLIBXFCE4PANEL_COMPILATION \
	-DHELPDIR=\"$(docdir)/html\" \
	$(PLATFORM_CPPFLAGS)

noinst_LTLIBRARIES = \
	libpanel-common.la

libpanel_common_la_SOURCES = \
	panel-debug.c \
	panel-debug.h \
	panel-utils.c \
	panel-utils.h \
	panel-xfconf.c \
	panel-xfconf.h

libpanel_common_la_CFLAGS = \
	$(XFCONF_CFLAGS) \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(PLATFORM_CFLAGS)

libpanel_common_la_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

libpanel_common_la_LIBADD = \
	$(XFCONF_LIBS) \
	$(GTK_LIBS) \
	$(LIBXFCE4UI_LIBS)

EXTRA_DIST = \
	panel-dbus.h \
	panel-private.h

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
