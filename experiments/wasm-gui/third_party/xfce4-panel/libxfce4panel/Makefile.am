DISTCLEANFILES =

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"libxfce4panel\" \
	-DLIBXFCE4PANEL_COMPILATION \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	$(PLATFORM_CPPFLAGS)

lib_LTLIBRARIES = \
	libxfce4panel-2.0.la

libxfce4panel_public_built_sources = \
	libxfce4panel-enum-types.h

libxfce4panel_built_sources = \
	$(libxfce4panel_public_built_sources) \
	libxfce4panel-alias.h \
	libxfce4panel-aliasdef.c \
	libxfce4panel-enum-types.c \
	libxfce4panel-marshal.c \
	libxfce4panel-marshal.h

libxfce4panel_headers = \
	libxfce4panel.h \
	libxfce4panel-config.h \
	libxfce4panel-enums.h \
	xfce-arrow-button.h \
	xfce-panel-convenience.h \
	xfce-panel-macros.h \
	xfce-panel-plugin.h \
	xfce-panel-plugin-provider.h \
	xfce-panel-image.h

libxfce4panel_includedir = \
	$(includedir)/xfce4/libxfce4panel-$(LIBXFCE4PANEL_VERSION_API)/libxfce4panel

libxfce4panel_include_HEADERS = \
	$(libxfce4panel_public_built_sources) \
	$(libxfce4panel_headers)

libxfce4panel_2_0_la_SOURCES = \
	$(libxfce4panel_built_sources) \
	$(libxfce4panel_headers) \
	libxfce4panel-config.c \
	xfce-arrow-button.c \
	xfce-panel-convenience.c \
	xfce-panel-plugin.c \
	xfce-panel-plugin-provider.c \
	xfce-panel-image.c

libxfce4panel_2_0_la_CFLAGS =	 \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(PLATFORM_CFLAGS)

libxfce4panel_2_0_la_LDFLAGS = \
	-no-undefined \
	-export-dynamic \
	-version-info $(LIBXFCE4PANEL_VERINFO) \
	-export-symbols-regex '^[^_].*' \
	$(PLATFORM_LDFLAGS)

libxfce4panel_2_0_la_LIBADD = \
	$(GTK_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	-lm

#
# Pkg-config file
#
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libxfce4panel-$(LIBXFCE4PANEL_VERSION_API).pc

#
# Build sources
#
if MAINTAINER_MODE

libxfce4panel-marshal.h: libxfce4panel-marshal.list Makefile
	$(AM_V_GEN) glib-genmarshal --prefix=_libxfce4panel_marshal --internal --header $< > $@

libxfce4panel-marshal.c: libxfce4panel-marshal.list Makefile
	$(AM_V_GEN) echo "#include <libxfce4panel/libxfce4panel-marshal.h>" > $@ \
	&& glib-genmarshal --prefix=_libxfce4panel_marshal --body $< >> $@

libxfce4panel-alias.h: make-libxfce4panel-alias.pl libxfce4panel.symbols
	$(AM_V_GEN) $(PERL) $(srcdir)/make-libxfce4panel-alias.pl $(srcdir)/libxfce4panel.symbols > $@

libxfce4panel-aliasdef.c: make-libxfce4panel-alias.pl libxfce4panel.symbols
	$(AM_V_GEN) $(PERL) $(srcdir)/make-libxfce4panel-alias.pl -def < $(srcdir)/libxfce4panel.symbols > $@

libxfce4panel-enum-types.h: $(libxfce4panel_headers) Makefile
	$(AM_V_GEN) ( cd $(srcdir) && glib-mkenums \
		--fhead "#ifndef __LIBXFCE4PANEL_ENUM_TYPES_H__\n#define __LIBXFCE4PANEL_ENUM_TYPES_H__\n#include <glib-object.h>\nG_BEGIN_DECLS\n" \
		--fprod "/* enumerations from \"@filename@\" */\n" \
		--vhead "GType @enum_name@_get_type (void) G_GNUC_CONST;\n#define XFCE_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
		--ftail "G_END_DECLS\n\n#endif /* !__LIBXFCE4PANEL_ENUM_TYPES_H__ */" \
		$(libxfce4panel_headers) ) > $@

libxfce4panel-enum-types.c:  $(libxfce4panel_headers) Makefile
	$(AM_V_GEN) ( cd $(srcdir) && glib-mkenums \
		--fhead "#include <libxfce4panel/libxfce4panel-enums.h>\n#include <libxfce4panel/libxfce4panel-enum-types.h>\n#include <libxfce4panel/libxfce4panel-alias.h>" \
		--fprod "\n/* enumerations from \"@filename@\" */" \
		--vhead "GType\n@enum_name@_get_type (void)\n{\n\tstatic GType type = 0;\n\tif (type == 0) {\n\tstatic const G@Type@Value values[] = {"\
		--vprod "\t{ @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
		--vtail "\t{ 0, NULL, NULL }\n\t};\n\ttype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n\treturn type;\n}\n" \
		--ftail "\n#define __LIBXFCE4PANEL_ENUM_TYPES_C__\n#include <libxfce4panel/libxfce4panel-aliasdef.c>\n" \
		$(libxfce4panel_headers) ) > $@

DISTCLEANFILES += \
	$(libxfce4panel_built_sources)

BUILT_SOURCES = \
	$(libxfce4panel_built_sources)

CLEANFILES = \
	actual-abi \
	expected-abi

endif
# endif MAINTAINER_MODE


# GObject Introspection
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ENV = CC="$(CC)"
INTROSPECTION_SCANNER_ARGS = \
	--add-include-path=$(srcdir) \
	--add-include-path=$(prefix)/share/gir-1.0 \
	--warn-all \
	--identifier-prefix=Xfce \
	--identifier-prefix=xfce \
	--identifier-prefix=LIBXFCE4PANEL \
	--identifier-prefix=libxfce4panel \
	--identifier-prefix=Plugin \
	--c-include=libxfce4panel/libxfce4panel.h
INTROSPECTION_COMPILER_ARGS = \
	--includedir=$(srcdir) \
	--includedir=$(prefix)/share/gir-1.0 \
	--includedir=.

if HAVE_INTROSPECTION
introspection_sources = $(libxfce4panel_2_0_la_SOURCES)

Libxfce4panel-2.0.gir: libxfce4panel-2.0.la Makefile
Libxfce4panel_2_0_gir_INCLUDES = GObject-2.0 GLib-2.0 Gio-2.0 Gtk-3.0 Libxfce4util-1.0
Libxfce4panel_2_0_gir_CFLAGS = $(AM_CPPFLAGS)
Libxfce4panel_2_0_gir_LIBS = libxfce4panel-2.0.la
Libxfce4panel_2_0_gir_FILES = $(introspection_sources)
Libxfce4panel_2_0_gir_EXPORT_PACKAGES = libxfce4panel-2.0
INTROSPECTION_GIRS += Libxfce4panel-2.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

DISTCLEANFILES += $(gir_DATA) $(typelib_DATA)
endif
# endif HAVE_INTROSPECTION

EXTRA_DIST = \
	abicheck.sh \
	libxfce4panel.symbols \
	libxfce4panel-2.0.deps \
	Libxfce4panel-2.0.metadata \
	libxfce4panel-alias.h \
	libxfce4panel-aliasdef.c \
	libxfce4panel-marshal.list \
	make-libxfce4panel-alias.pl

if ENABLE_VAPIGEN
-include $(VAPIGEN_MAKEFILE)

libxfce4panel-2.0.vapi: Libxfce4panel-2.0.gir libxfce4panel-2.0.deps

VAPIGEN_VAPIS = libxfce4panel-2.0.vapi
VAPIGEN_GIRDIRS = $(prefix)/share/gir-1.0

##libxfce4panel_2_0_vapi_DEPS = libxfce4panel-2.0 GObject-2.0 GLib-2.0 Gtk-3.0 libxfce4util-1.0
libxfce4panel_2_0_vapi_METADATADIRS = .
libxfce4panel_2_0_vapi_FILES = Libxfce4panel-2.0.gir

vapidir = $(datadir)/vala/vapi
vapi_DATA = $(VAPIGEN_VAPIS) $(VAPIGEN_VAPIS:.vapi=.deps)
endif

if HAVE_GNUC_VISIBILITY
TESTS =	abicheck.sh
endif

# required for gtk-doc
dist-hook: all

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
