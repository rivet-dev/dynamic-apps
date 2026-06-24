
AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DLIBXFCONF_COMPILATION \
	-DG_LOG_DOMAIN=\"xfconf\" \
	$(PLATFORM_CPPFLAGS)

lib_LTLIBRARIES = libxfconf-0.la

libxfconfincludedir = $(includedir)/xfce4/xfconf-$(LIBXFCONF_VERSION_API)/xfconf
libxfconfinclude_HEADERS = \
	xfconf-binding.h \
	xfconf-channel.h \
	xfconf-errors.h \
	xfconf-types.h \
	xfconf.h

libxfconf_0_la_SOURCES = \
	$(libxfconfinclude_HEADERS) \
	xfconf-binding.c \
	xfconf-cache.c \
	xfconf-cache.h \
	xfconf-channel.c \
	xfconf-private.h \
	xfconf.c \
	$(top_srcdir)/common/xfconf-types.c

libxfconf_0_la_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GIO_UNIX_CFLAGS) \
	$(GTHREAD_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(PLATFORM_CFLAGS)

libxfconf_0_la_LDFLAGS = \
	-export-dynamic \
	-version-info $(LIBXFCONF_VERINFO) \
	-export-symbols-regex "^[^_].*" \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

libxfconf_0_la_LIBADD = \
	$(top_builddir)/common/libxfconf-common.la \
	$(top_builddir)/common/libxfconf-gvaluefuncs.la \
	$(top_builddir)/common/libxfconf-dbus.la \
	$(GIO_LIBS) \
	$(GIO_UNIX_LIBS) \
	$(GLIB_LIBS) \
	$(GTHREAD_LIBS) \
	$(LIBXFCE4UTIL_LIBS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libxfconf-0.pc


EXTRA_DIST = \
	abicheck.sh \
	xfconf.symbols \
	libxfconf-0.deps

# required for gtk-doc
dist-hook: all

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ENV = CC="$(CC)"
INTROSPECTION_SCANNER_ARGS = \
	--add-include-path=$(srcdir) \
	--warn-all \
	-DLIBXFCONF_COMPILATION \
	--symbol-prefix=xfconf_g_ \
	--symbol-prefix=xfconf_get_ \
	--symbol-prefix=xfconf_ \
	--c-include=xfconf/xfconf.h
INTROSPECTION_COMPILER_ARGS = \
	--includedir=$(srcdir) \
	--includedir=.

if HAVE_INTROSPECTION
introspection_sources = $(filter-out %private.h %cache.h, $(libxfconf_0_la_SOURCES))

Xfconf-0.gir: libxfconf-0.la Makefile
Xfconf_0_gir_INCLUDES = GObject-2.0 GLib-2.0 Gio-2.0
Xfconf_0_gir_CFLAGS = $(AM_CPPFLAGS)
Xfconf_0_gir_LIBS = libxfconf-0.la
Xfconf_0_gir_FILES = $(introspection_sources)
Xfconf_0_gir_EXPORT_PACKAGES = libxfconf-0
INTROSPECTION_GIRS += Xfconf-0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

DISTCLEANFILES = $(gir_DATA) $(typelib_DATA)
endif

if ENABLE_VAPIGEN
-include $(VAPIGEN_MAKEFILE)

libxfconf-0.vapi: Xfconf-0.gir libxfconf-0.deps

VAPIGEN_VAPIS = libxfconf-0.vapi
VAPIGEN_GIRDIRS = $(prefix)/share/gir-1.0

libxfconf_0_vapi_DEPS = gobject-2.0 glib-2.0 gio-2.0
libxfconf_0_vapi_FILES = Xfconf-0.gir

vapidir = $(datadir)/vala/vapi
vapi_DATA = $(VAPIGEN_VAPIS) $(VAPIGEN_VAPIS:.vapi=.deps)
endif

if HAVE_GNUC_VISIBILITY
TESTS = abicheck.sh
endif
