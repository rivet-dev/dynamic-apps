AM_CPPFLAGS =								\
	-I$(top_builddir)						\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/examples					\
	-DG_LOG_DOMAIN=\"TexOpenTerminal\"				\
	$(PLATFORM_CPPFLAGS)

extensionsdir = $(libdir)/thunarx-$(THUNARX_VERSION_API)
extensions_LTLIBRARIES =						\
	tex-open-terminal.la

tex_open_terminal_la_SOURCES =						\
	tex-open-terminal-plugin.c					\
	tex-open-terminal.c						\
	tex-open-terminal.h

tex_open_terminal_la_CFLAGS =						\
	$(GTK_CFLAGS)							\
	$(LIBXFCE4UI_CFLAGS)						\
	$(PLATFORM_CFLAGS)

tex_open_terminal_la_DEPENDENCIES =					\
	$(top_builddir)/thunarx/libthunarx-$(THUNARX_VERSION_API).la

tex_open_terminal_la_LIBADD =						\
	$(top_builddir)/thunarx/libthunarx-$(THUNARX_VERSION_API).la	\
	$(LIBXFCE4UI_LIBS)						\
	$(GTK_LIBS)

tex_open_terminal_la_LDFLAGS =						\
	-avoid-version							\
	-export-dynamic							\
	-export-symbols-regex="^thunar_extension_(shutdown|initialize|list_types)" \
	-module								\
	$(PLATFORM_LDFLAGS)

EXTRA_DIST =								\
	README.md

# Don't install/uninstall examples by default
install:
uninstall:

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
