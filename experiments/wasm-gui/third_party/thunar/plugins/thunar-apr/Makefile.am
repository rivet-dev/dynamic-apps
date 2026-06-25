AM_CPPFLAGS =								\
	-I$(top_builddir)						\
	-I$(top_builddir)/plugins					\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/plugins						\
	-DG_LOG_DOMAIN=\"thunar-apr\"					\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	$(PLATFORM_CPPFLAGS)

extensionsdir = $(libdir)/thunarx-$(THUNARX_VERSION_API)
extensions_LTLIBRARIES =						\
	thunar-apr.la

thunar_apr_la_SOURCES =							\
	thunar-apr-abstract-page.c					\
	thunar-apr-abstract-page.h					\
	thunar-apr-desktop-page.c					\
	thunar-apr-desktop-page.h					\
	thunar-apr-image-page.c						\
	thunar-apr-image-page.h						\
	thunar-apr-plugin.c						\
	thunar-apr-private.c						\
	thunar-apr-private.h						\
	thunar-apr-provider.c						\
	thunar-apr-provider.h

thunar_apr_la_CFLAGS =							\
	$(EXIF_CFLAGS)							\
	$(EXO_CFLAGS)							\
	$(LIBXFCE4UTIL_CFLAGS)						\
	$(LIBXFCE4UI_CFLAGS)						\
	$(GLIB_CFLAGS)							\
	$(GTK_CFLAGS)							\
	$(PLATFORM_CFLAGS)

thunar_apr_la_LDFLAGS =							\
	-avoid-version							\
	-export-dynamic							\
	-no-undefined							\
	-export-symbols-regex "^thunar_extension_(shutdown|initialize|list_types)" \
	-module								\
	$(PLATFORM_LDFLAGS)

thunar_apr_la_LIBADD =							\
	$(top_builddir)/thunarx/libthunarx-$(THUNARX_VERSION_API).la	\
	$(EXIF_LIBS)							\
	$(EXO_LIBS)							\
	$(LIBXFCE4UTIL_LIBS)						\
	$(LIBXFCE4UI_LIBS)							\
	$(GLIB_LIBS)							\
	$(GTK_LIBS)

thunar_apr_la_DEPENDENCIES =						\
	$(top_builddir)/thunarx/libthunarx-$(THUNARX_VERSION_API).la


EXTRA_DIST =								\
	README.md

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
