
bin_PROGRAMS = \
	xfce4-notifyd-config

xfce4_notifyd_config_SOURCES = \
	$(top_srcdir)/common/xfce-notify-common.h \
	main.c \
	xfce-notify-log-viewer.c \
	xfce-notify-log-viewer.h \
	xfce-notify-config-ui.h \
	xfce-notify-config-ui.c

xfce4_notifyd_config_CFLAGS = \
	-DG_LOG_DOMAIN=\"xfce4-notifyd-config\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-I$(top_srcdir) \
	-I$(builddir) \
	$(LIBNOTIFY_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GIO_UNIX_CFLAGS) \
	$(GTK_CFLAGS) \
	$(SQLITE_CFLAGS)

xfce4_notifyd_config_LDADD = \
	$(top_builddir)/common/libxfce-notifyd-common.la \
	$(LIBXFCE4UTIL_LIBS) \
	$(XFCONF_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(GIO_LIBS) \
	$(GIO_UNIX_LIBS) \
	$(GTK_LIBS) \
	$(SQLITE_LIBS) \
	$(LIBNOTIFY_LIBS)

dist_man1_MANS = \
	xfce4-notifyd-config.1

desktopdir = $(datadir)/applications
desktop_in_files = xfce4-notifyd-config.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

glade_files = \
	xfce4-notifyd-config.glade \
	xfce4-notifyd-config-known-app.glade

built_xfce4_notifyd_config = \
	xfce-notify-config-ui.h \
	xfce-notify-config-ui.c

dist_noinst_DATA = \
	xfce-notify-config-ui.gresource.xml \
	$(glade_files) \
	$(desktop_in_files)

BUILT_SOURCES = \
	$(built_xfce4_notifyd_config)

CLEANFILES = \
	$(built_xfce4_notifyd_config) \
	$(desktop_DATA)

%.desktop: $(srcdir)/%.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

%-ui.h: $(srcdir)/%-ui.gresource.xml $(glade_files) Makefile
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header --manual-register $<

%-ui.c: $(srcdir)/%-ui.gresource.xml $(glade_files) Makefile
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source --manual-register $<
