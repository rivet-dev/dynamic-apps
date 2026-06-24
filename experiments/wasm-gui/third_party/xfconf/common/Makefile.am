
AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DLIBXFCONF_COMPILATION \
	-DG_LOG_DOMAIN=\"xfconf-common\" \
	$(PLATFORM_CPPFLAGS)

noinst_LTLIBRARIES = \
	libxfconf-common.la \
	libxfconf-gvaluefuncs.la \
	libxfconf-dbus.la

libxfconf_common_la_SOURCES = \
	xfconf-errors.c \
	xfconf-marshal.c \
	xfconf-marshal.h

libxfconf_common_la_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(PLATFORM_CFLAGS)

libxfconf_common_la_LDFLAGS = \
	$(PLATFORM_LDFLAGS)

libxfconf_common_la_LIBADD = \
	$(GLIB_LIBS)

libxfconf_built_sources = \
	xfconf-alias.h \
	xfconf-aliasdef.c

libxfconf_gvaluefuncs_la_SOURCES = \
	xfconf-gvaluefuncs.c \
	xfconf-gvaluefuncs.h

libxfconf_gvaluefuncs_la_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GIO_UNIX_CFLAGS) \
	$(PLATFORM_CFLAGS)

libxfconf_gvaluefuncs_la_LDFLAGS = \
	$(PLATFORM_LDFLAGS)

libxfconf_gvaluefuncs_la_LIBADD = \
	$(GLIB_LIBS) \
	$(GIO_UNIX_LIBS)


libxfconf_dbus_la_SOURCES = \
	xfconf-gdbus-bindings.c \
	xfconf-gdbus-bindings.h

libxfconf_dbus_la_CFLAGS = $(GIO_UNIX_CFLAGS)

libxfconf_dbus_la_LIBADD = $(GIO_UNIX_LIBS)
	

BUILT_SOURCES = \
	xfconf-marshal.c \
	xfconf-marshal.h \
	xfconf-gdbus-bindings.c \
	xfconf-gdbus-bindings.h

xfconf-dbus.xml: xfconf-dbus.xml.in
	$(SED) -e "s,\@XFCONF_SERVICE_NAME_PREFIX\@,$(XFCONF_SERVICE_NAME_PREFIX),g" \
	       -e "s,\@XFCONF_SERVICE_PATH_PREFIX\@,$(XFCONF_SERVICE_PATH_PREFIX),g" < $< > $@

xfconf-marshal.h: $(srcdir)/xfconf-marshal.list Makefile
	$(AM_V_GEN) glib-genmarshal --prefix=_xfconf_marshal $(srcdir)/xfconf-marshal.list --header > xfconf-marshal.h
xfconf-marshal.c: $(srcdir)/xfconf-marshal.list Makefile
	$(AM_V_GEN) echo '#include "xfconf-marshal.h"' > xfconf-marshal.c \
	&& glib-genmarshal --prefix=_xfconf_marshal $(srcdir)/xfconf-marshal.list --body >> xfconf-marshal.c

xfconf-gdbus-bindings.h:
xfconf-gdbus-bindings.c: xfconf-dbus.xml Makefile
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
	--interface-prefix @XFCONF_SERVICE_NAME_PREFIX@.Xfconf \
	--annotate "@XFCONF_SERVICE_NAME_PREFIX@.Xfconf" "org.gtk.GDBus.C.Name" Exported \
	--c-namespace Xfconf   \
	--generate-c-code xfconf-gdbus-bindings \
	$<

CLEANFILES = \
	actual-abi \
	expected-abi

DISTCLEANFILES = \
	$(libxfconf_built_sources) \
	$(BUILT_SOURCES) \
	xfconf-dbus.xml

BUILT_SOURCES += \
	$(libxfconf_built_sources)

xfconf-alias.h: make-exo-alias.pl $(top_srcdir)/xfconf/xfconf.symbols
	$(AM_V_GEN) $(PERL) $(srcdir)/make-exo-alias.pl <$(top_srcdir)/xfconf/xfconf.symbols >xfconf-alias.h

xfconf-aliasdef.c: make-exo-alias.pl $(top_srcdir)/xfconf/xfconf.symbols
	$(AM_V_GEN) $(PERL) $(srcdir)/make-exo-alias.pl -def <$(top_srcdir)/xfconf/xfconf.symbols >xfconf-aliasdef.c


EXTRA_DIST = \
	make-exo-alias.pl \
	xfconf-alias.h \
	xfconf-aliasdef.c \
	xfconf-common-private.h \
	xfconf-dbus.xml.in \
	xfconf-marshal.list \
	xfconf-types.c

# required for make distcheck
dist-hook: all
