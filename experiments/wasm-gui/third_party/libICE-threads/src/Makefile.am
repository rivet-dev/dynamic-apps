lib_LTLIBRARIES=libICE.la

AM_CPPFLAGS = -I${top_srcdir}/include

AM_CFLAGS = \
	$(ICE_CFLAGS) \
	$(CWARNFLAGS)

libICE_la_LDFLAGS = -version-number 6:3:0 -no-undefined

libICE_la_LIBADD = $(ICE_LIBS)

libICE_la_SOURCES = \
	ICElibint.h \
	accept.c \
	authutil.c \
	connect.c \
	error.c \
	getauth.c \
	globals.h \
	iceauth.c \
	icetrans.c \
	listen.c \
	listenwk.c \
	locking.c \
	misc.c \
	ping.c \
	process.c \
	protosetup.c \
	register.c \
	replywait.c \
	setauth.c \
	shutdown.c \
	watch.c

iceincludedir=$(includedir)/X11/ICE
iceinclude_HEADERS=\
	$(top_srcdir)/include/X11/ICE/ICE.h \
	$(top_srcdir)/include/X11/ICE/ICEconn.h \
	$(top_srcdir)/include/X11/ICE/ICElib.h \
	$(top_srcdir)/include/X11/ICE/ICEmsg.h \
	$(top_srcdir)/include/X11/ICE/ICEproto.h \
	$(top_srcdir)/include/X11/ICE/ICEutil.h

if LINT
ALL_LINT_FLAGS=$(LINT_FLAGS) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
		$(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS)

lint:
	$(LINT) $(ALL_LINT_FLAGS) $(libICE_la_SOURCES) $(XINERAMA_LIBS)
endif LINT
