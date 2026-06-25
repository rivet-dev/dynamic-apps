
AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"xfce4-panel-migrate\" \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	-DXDGCONFIGDIR=\"$(sysconfdir)/xdg\" \
	$(PLATFORM_CPPFLAGS)

confdir = $(sysconfdir)/xdg/xfce4/panel
conf_DATA = \
	default.xml

migratedir = $(HELPER_PATH_PREFIX)/xfce4/panel

migrate_PROGRAMS = \
	migrate

migrate_SOURCES = \
	main.c \
	migrate-config.c \
	migrate-config.h \
	migrate-default.c \
	migrate-default.h

migrate_CFLAGS = \
	$(GTK_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(PLATFORM_CFLAGS)

migrate_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

migrate_LDADD = \
	$(top_builddir)/libxfce4panel/libxfce4panel-$(LIBXFCE4PANEL_VERSION_API).la \
	$(GTK_LIBS) \
	$(XFCONF_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS)

migrate_DEPENDENCIES = \
	$(top_builddir)/libxfce4panel/libxfce4panel-$(LIBXFCE4PANEL_VERSION_API).la

EXTRA_DIST = \
	default.xml

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
