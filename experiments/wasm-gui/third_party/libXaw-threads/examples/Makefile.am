if ENABLE_UNIT_TESTS
check_PROGRAMS = list menu pane repeater scrollbar simple strip toggle viewport xawhisto

TESTS=$(check_PROGRAMS)

AM_CFLAGS = $(CWARNFLAGS) $(XAW_CFLAGS)
AM_CPPFLAGS = \
    -I$(top_srcdir)/include \
    -I$(top_builddir)/include \
    -I$(top_builddir)/include/X11
LDADD= $(top_builddir)/src/libXaw7.la $(XAW7_LIBS)

TESTS_ENVIRONMENT = $(MALLOC_DEBUG_ENV)

endif ENABLE_UNIT_TESTS
