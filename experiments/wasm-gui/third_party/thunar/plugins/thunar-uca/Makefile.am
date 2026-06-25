AM_CPPFLAGS =								\
	-I$(top_builddir)						\
	-I$(top_builddir)/plugins					\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/plugins						\
	-DBINDIR=\"$(bindir)\"						\
	-DDATADIR=\"$(datadir)\"					\
	-DG_LOG_DOMAIN=\"thunar-uca\"					\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	$(PLATFORM_CPPFLAGS)

extensionsdir = $(libdir)/thunarx-$(THUNARX_VERSION_API)
extensions_LTLIBRARIES =						\
	thunar-uca.la

thunar_uca_la_SOURCES =							\
	thunar-uca-chooser.c						\
	thunar-uca-chooser.h						\
	thunar-uca-context.c						\
	thunar-uca-context.h						\
	thunar-uca-editor.c						\
	thunar-uca-editor.h						\
	thunar-uca-model.c						\
	thunar-uca-model.h						\
	thunar-uca-plugin.c						\
	thunar-uca-private.c						\
	thunar-uca-private.h						\
	thunar-uca-provider.c						\
	thunar-uca-provider.h						\
	thunar-uca.gresource.c

thunar_uca_la_CFLAGS =							\
	$(EXO_CFLAGS)							\
	$(LIBXFCE4UTIL_CFLAGS)						\
	$(LIBXFCE4UI_CFLAGS)						\
	$(LIBXFCE4KBD_PRIVATE_CFLAGS) 			    \
	$(PLATFORM_CFLAGS)

thunar_uca_la_LDFLAGS =							\
	-avoid-version							\
	-export-dynamic							\
	-no-undefined							\
	-export-symbols-regex "^thunar_extension_(shutdown|initialize|list_types)" \
	-module								\
	$(PLATFORM_LDFLAGS)

thunar_uca_la_LIBADD =							\
	$(top_builddir)/thunarx/libthunarx-$(THUNARX_VERSION_API).la	\
	$(EXO_LIBS)							\
	$(LIBXFCE4UTIL_LIBS)						\
	$(LIBXFCE4UI_LIBS)							\
	$(LIBXFCE4KBD_PRIVATE_LIBS)

thunar_uca_la_DEPENDENCIES =						\
	$(top_builddir)/thunarx/libthunarx-$(THUNARX_VERSION_API).la

thunar_uca_templates =							\
	thunar-uca-ui.xml

GLIB_COMPILE_RESOURCES = $(shell $(PKG_CONFIG) --variable=glib_compile_resources gio-2.0)

resource_files = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/thunar-uca.gresource.xml)

thunar-uca.gresource.c: thunar-uca.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< \
		--target=$@ --sourcedir=$(srcdir) --generate-source

defaultsdir = $(sysconfdir)/xdg/Thunar
defaults_in_files = uca.xml.in
defaults_DATA = $(defaults_in_files:.xml.in=.xml)
@INTLTOOL_XML_RULE@

EXTRA_DIST =								\
	README.md								\
	thunar-uca.gresource.xml					\
	thunar-uca-editor.ui						\
	thunar-uca-chooser.ui						\
	$(defaults_in_files)

CLEANFILES =								\
	$(defaults_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
