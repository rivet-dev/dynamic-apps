lib_LTLIBRARIES=libSM.la

AM_CPPFLAGS = -I${top_srcdir}/include
AM_CFLAGS = $(CWARNFLAGS) $(SM_CFLAGS)

if WITH_LIBUUID
AM_CFLAGS += $(LIBUUID_CFLAGS) -DHAVE_LIBUUID
endif

libSM_la_LDFLAGS = -version-number 6:0:1 -no-undefined

libSM_la_LIBADD = $(SM_LIBS)

if WITH_LIBUUID
libSM_la_LIBADD += $(LIBUUID_LIBS)
endif

libSM_la_SOURCES = \
	SMlibint.h \
	sm_client.c \
	sm_error.c \
	sm_genid.c \
	sm_manager.c \
	sm_misc.c \
	sm_process.c

EXTRA_DIST = sm_auth.c

smincludedir=$(includedir)/X11/SM
sminclude_HEADERS=\
	$(top_srcdir)/include/X11/SM/SM.h \
	$(top_srcdir)/include/X11/SM/SMlib.h \
	$(top_srcdir)/include/X11/SM/SMproto.h
