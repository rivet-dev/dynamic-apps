AM_CPPFLAGS =								\
	-I$(top_builddir)						\
	-I$(top_srcdir)

sbin_PROGRAMS =								\
	xfce4-kiosk-query

xfce4_kiosk_query_SOURCES =						\
	main.c

xfce4_kiosk_query_CFLAGS =						\
	$(GLIB_CFLAGS)

xfce4_kiosk_query_DEPENDENCIES =					\
	$(top_builddir)/libxfce4util/libxfce4util.la

xfce4_kiosk_query_LDFLAGS =						\
	$(top_builddir)/libxfce4util/libxfce4util.la
