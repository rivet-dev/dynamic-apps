AM_CPPFLAGS =								\
	-I$(top_builddir)						\
	-I$(top_builddir)/plugins					\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/plugins						\
	-DG_LOG_DOMAIN=\"thunar-sbr\"					\
	$(PLATFORM_CPPFLAGS)

extensionsdir = $(libdir)/thunarx-$(THUNARX_VERSION_API)
extensions_LTLIBRARIES =						\
	thunar-sbr.la

thunar_sbr_la_SOURCES =							\
	thunar-sbr-case-renamer.c					\
	thunar-sbr-case-renamer.h					\
	thunar-sbr-date-renamer.c					\
	thunar-sbr-date-renamer.h					\
	thunar-sbr-enum-types.c						\
	thunar-sbr-enum-types.h						\
	thunar-sbr-insert-renamer.c					\
	thunar-sbr-insert-renamer.h					\
	thunar-sbr-number-renamer.c					\
	thunar-sbr-number-renamer.h					\
	thunar-sbr-plugin.c						\
	thunar-sbr-provider.c						\
	thunar-sbr-provider.h						\
	thunar-sbr-remove-renamer.c					\
	thunar-sbr-remove-renamer.h					\
	thunar-sbr-replace-renamer.c					\
	thunar-sbr-replace-renamer.h

thunar_sbr_la_CFLAGS =							\
	$(EXIF_CFLAGS)							\
	$(EXO_CFLAGS)							\
	$(GLIB_CFLAGS)							\
	$(PCRE2_CFLAGS)							\
	$(PLATFORM_CFLAGS)

thunar_sbr_la_LDFLAGS =							\
	-avoid-version							\
	-export-dynamic							\
	-no-undefined							\
	-export-symbols-regex "^thunar_extension_(shutdown|initialize|list_types)" \
	-module								\
	$(PLATFORM_LDFLAGS)

thunar_sbr_la_LIBADD =							\
	$(top_builddir)/thunarx/libthunarx-$(THUNARX_VERSION_API).la	\
	$(EXIF_LIBS)							\
	$(EXO_LIBS)							\
	$(GLIB_LIBS)							\
	$(PCRE2_LIBS)

thunar_sbr_la_DEPENDENCIES =						\
	$(top_builddir)/thunarx/libthunarx-$(THUNARX_VERSION_API).la

EXTRA_DIST =								\
	README.md

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
