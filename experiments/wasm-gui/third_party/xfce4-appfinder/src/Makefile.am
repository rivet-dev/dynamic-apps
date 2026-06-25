# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"xfce4-appfinder\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = xfce4-appfinder

xfce4_appfinder_built_sources = \
	appfinder-preferences-ui.h

xfce4_appfinder_SOURCES = \
	$(xfce4_appfinder_built_sources) \
	appfinder-actions.c \
	appfinder-actions.h \
	appfinder-category-model.c \
	appfinder-category-model.h \
	appfinder-gdbus.c \
	appfinder-gdbus.h \
	appfinder-model.c \
	appfinder-model.h \
	appfinder-preferences.c \
	appfinder-preferences.h \
	appfinder-private.h \
	appfinder-window.c \
	appfinder-window.h \
	main.c

xfce4_appfinder_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTHREAD_CFLAGS) \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(GARCON_CFLAGS) \
	$(GIO_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(PLATFORM_CFLAGS)

xfce4_appfinder_LDADD = \
	$(GLIB_LIBS) \
	$(GTHREAD_LIBS) \
	$(GTK_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(GARCON_LIBS) \
	$(XFCONF_LIBS) \
	$(GIO_LIBS)

xfce4_appfinder_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

# install symlink to 'xfrun4'
install-exec-hook:
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	-( cd $(DESTDIR)$(bindir) ; test -x xfrun4 || ln -sf xfce4-appfinder xfrun4 )

if MAINTAINER_MODE

appfinder-preferences-ui.h: $(srcdir)/appfinder-preferences.glade Makefile
	$(AM_V_GEN) xdt-csource --static --strip-comments --strip-content --name=appfinder_preferences_ui $< >$@

DISTCLEANFILES = \
	$(xfce4_appfinder_built_sources)

BUILT_SOURCES = \
	$(xfce4_appfinder_built_sources)

endif

EXTRA_DIST = \
	appfinder-preferences.glade
