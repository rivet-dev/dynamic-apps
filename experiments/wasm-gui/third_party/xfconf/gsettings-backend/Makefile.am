giomodule_LTLIBRARIES= libxfconfgsettingsbackend.la

giomoduledir = $(GIO_MODULE_DIR)

libxfconfgsettingsbackend_la_SOURCES =		\
	xfconf-giomodule.c			\
	xfconf-gsettings-backend.c		\
	xfconf-gsettings-backend.h

libxfconfgsettingsbackend_la_CFLAGS =		\
	-I$(top_srcdir) 			\
	-I$(top_builddir) 			\
	$(GIO_CFLAGS)				\
	$(GMODULE_CFLAGS)

libxfconfgsettingsbackend_la_LDFLAGS =		\
	-export-dynamic				\
	-avoid-version				\
	-module 				\
	-export-symbols-regex "^[^_].*"

libxfconfgsettingsbackend_la_LIBADD =		\
	$(top_builddir)/xfconf/libxfconf-0.la 	\
	$(top_builddir)/common/libxfconf-gvaluefuncs.la\
	$(GIO_LDFLAGS)				\
	$(GMODULE_LDFLAGS)

