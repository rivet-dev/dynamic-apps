lib_LTLIBRARIES = libXtst.la

libXtst_la_SOURCES = \
         XRecord.c \
         XTest.c

libXtst_la_LIBADD = @XTST_LIBS@

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/include/X11/extensions

AM_CFLAGS = \
	$(XTST_CFLAGS) \
	$(CWARNFLAGS)

libXtst_la_LDFLAGS = -version-number 6:1:0 -no-undefined

libXtstincludedir = $(includedir)/X11/extensions
libXtstinclude_HEADERS = $(top_srcdir)/include/X11/extensions/XTest.h \
                         $(top_srcdir)/include/X11/extensions/record.h

if LINT
ALL_LINT_FLAGS=$(LINT_FLAGS) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
	       $(AM_CPPFLAGS) $(CPPFLAGS)

lint:
	$(LINT) $(ALL_LINT_FLAGS) $(libXtst_la_SOURCES) $(XTST_LIBS)
endif LINT
