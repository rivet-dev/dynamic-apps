noinst_LTLIBRARIES = libxfdesktop.la

libxfdesktop_built_sources = \
	xfdesktop-marshal.c \
	xfdesktop-marshal.h \
	tumbler.c \
	tumbler.h

libxfdesktop_la_SOURCES = \
	$(libxfdesktop_built_sources) \
	xfdesktop-common.c \
	xfdesktop-common.h \
	xfdesktop-thumbnailer.c \
	xfdesktop-thumbnailer.h

libxfdesktop_la_CFLAGS = \
	-I$(top_srcdir)/src \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GIO_UNIX_CFLAGS)


if MAINTAINER_MODE

xfdesktop-marshal.h: xfdesktop-marshal.list Makefile
	$(AM_V_GEN) glib-genmarshal --prefix=xfdesktop_marshal --internal --header $< > $@

xfdesktop-marshal.c: xfdesktop-marshal.list Makefile
	$(AM_V_GEN) echo '#include "xfdesktop-marshal.h"' > $@ \
	&& glib-genmarshal --prefix=xfdesktop_marshal --body $< >> $@

tumbler.c tumbler.h : $(srcdir)/tumbler-service-dbus.xml Makefile.am
	gdbus-codegen \
		--c-namespace=Tumbler \
		--interface-prefix=org.freedesktop.thumbnails. \
		--generate-c-code=tumbler \
		$(srcdir)/tumbler-service-dbus.xml

DISTCLEANFILES = \
	$(libxfdesktop_built_sources)

BUILT_SOURCES = \
	$(libxfdesktop_built_sources)

endif

EXTRA_DIST = \
	xfdesktop-marshal.list
