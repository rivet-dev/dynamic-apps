bin_PROGRAMS = xfdesktop-settings

xfdesktop_settings_SOURCES = \
	xfdesktop-settings-appearance-frame-ui.h \
	xfdesktop-settings-ui.h \
	main.c

xfdesktop_settings_CFLAGS =						\
	-I$(top_srcdir)								\
	-I$(top_srcdir)/common						\
	-I$(top_srcdir)/src 						\
	$(LIBEXO_CFLAGS)							\
	$(LIBXFCE4UI_CFLAGS)						\
	$(XFCONF_CFLAGS)							\
	$(GTHREAD_CFLAGS)							\
	$(LIBX11_CFLAGS)							\
	$(LIBWNCK_CFLAGS) 							\
	-DG_LOG_DOMAIN=\"xfdesktop-settings\"		\
	-DEXO_API_SUBJECT_TO_CHANGE					\
	-DWNCK_I_KNOW_THIS_IS_UNSTABLE 				\
	-DLOCALEDIR=\"$(localedir)\"				\
	-DDATADIR=\"$(datadir)\"					\
	-DBINDIR=\"$(bindir)\"

xfdesktop_settings_LDADD =						\
	$(top_builddir)/common/libxfdesktop.la		\
	$(XFCONF_LIBS)								\
	$(LIBXFCE4UI_LIBS)							\
	$(LIBEXO_LIBS)								\
	$(GTHREAD_LIBS)								\
	$(LIBWNCK_LIBS) 							\
	$(LIBX11_LIBS)								\
	$(GTK_LIBS)

desktop_in_files = xfce-backdrop-settings.desktop.in
desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

if MAINTAINER_MODE

BUILT_SOURCES = \
	xfdesktop-settings-appearance-frame-ui.h \
	xfdesktop-settings-ui.h

SUFFIXES: .ui

%-ui.h: $(srcdir)/%-ui.glade Makefile
	$(AM_V_GEN) xdt-csource --static --name=`basename $< .glade | $(SED) -e 's/-/_/g'`  $< >$@

endif

EXTRA_DIST = \
	xfdesktop-settings-ui.glade \
	xfdesktop-settings-appearance-frame-ui.glade \
	$(desktop_in_files)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(desktop_DATA)
