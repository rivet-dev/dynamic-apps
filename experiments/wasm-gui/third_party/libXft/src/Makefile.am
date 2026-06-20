AM_CFLAGS = $(FONTCONFIG_CFLAGS) $(FREETYPE_CFLAGS) $(XRENDER_CFLAGS) \
            $(CWARNFLAGS) -I$(top_builddir)/include/X11/Xft

lib_LTLIBRARIES = libXft.la

libXft_la_SOURCES = xftint.h \
                    xftcolor.c \
                    xftcore.c \
                    xftdbg.c \
                    xftdpy.c \
                    xftdraw.c \
                    xftextent.c \
                    xftfont.c \
                    xftfreetype.c \
                    xftglyphs.c \
                    xftinit.c \
                    xftlist.c \
                    xftname.c \
                    xftrender.c \
                    xftstr.c \
                    xftswap.c \
                    xftxlfd.c

libXft_la_LIBADD = @FONTCONFIG_LIBS@ @FREETYPE_LIBS@ @XRENDER_LIBS@

# -version-number requires libtool >= 1.5
libXft_la_LDFLAGS = -version-number $(XFT_LT_VERSION) -no-undefined

libXftincludedir = $(includedir)/X11/Xft
libXftinclude_HEADERS = \
	$(top_builddir)/include/X11/Xft/Xft.h		\
	$(top_srcdir)/include/X11/Xft/XftCompat.h
