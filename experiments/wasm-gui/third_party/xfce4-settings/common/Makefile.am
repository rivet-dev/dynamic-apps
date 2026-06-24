AM_CPPFLAGS = \
	-I${top_srcdir}	\
	-DPNP_IDS=\"$(PNP_IDS)\" \
	$(PLATFORM_CPPFLAGS)

noinst_LTLIBRARIES = \
	libxfce4-settings.la

libxfce4_settings_la_SOURCES = \
	display-name.c \
	display-profiles.c \
	display-profiles.h \
	edid-parse.c \
	edid.h \
	xfce-randr.c \
	xfce-randr.h

libxfce4_settings_la_CFLAGS = \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(XRANDR_CFLAGS) \
	$(LIBX11_CFLAGS) \
	$(PLATFORM_CFLAGS)

libxfce4_settings_la_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

libxfce4_settings_la_LIBADD = \
	$(GTK_LIBS) \
	$(XFCONF_LIBS) \
	$(LIBX11_LIBS) -lm \
	$(XRANDR_LIBS) \
	$(GLIB_LIBS)
