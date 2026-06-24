
AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"xfce4-about\" \
	-DDATADIR=\"$(datadir)/xfce4\" \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfce4-about

xfce4_about_built_sources = \
	about-dialog-ui.h

xfce4_about_SOURCES = \
	$(xfce4_about_built_sources) \
	contributors.h \
	main.c

if ENABLE_GLIBTOP

xfce4_about_SOURCES += \
	system-info.c \
	system-info.h

endif

xfce4_about_CFLAGS = \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(GTK3_CFLAGS) \
	$(GLIBTOP_CFLAGS) \
	$(EPOXY_CFLAGS) \
	$(GUDEV_CFLAGS) \
	$(LIBX11_CFLAGS) \
	$(PLATFORM_CFLAGS)

xfce4_about_LDADD = \
	$(top_builddir)/libxfce4ui/libxfce4ui-2.la \
	$(LIBXFCE4UTIL_LIBS) \
	$(GLIBTOP_LIBS) \
	$(EPOXY_LIBS) \
	$(GUDEV_LIBS) \
	$(LIBX11_LIBS) \
	$(LIBX11_LDFLAGS) \
	$(GTK3_LIBS)

xfce4_about_DEPENDENCIES = \
	$(top_builddir)/libxfce4ui/libxfce4ui-2.la

desktopdir = $(datadir)/applications
desktop_in_files = xfce4-about.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

DISTCLEANFILES = \
	$(desktop_DATA)

if MAINTAINER_MODE

about-dialog-ui.h: $(srcdir)/xfce4-about-dialog.glade Makefile
	$(AM_V_GEN) xdt-csource --static --strip-comments --strip-content --name=xfce_about_dialog_ui $< >$@

BUILT_SOURCES = \
	$(xfce4_about_built_sources)

DISTCLEANFILES += \
	$(xfce4_about_built_sources)

endif

EXTRA_DIST = \
	$(desktop_in_files) \
	xfce4-about-dialog.glade
