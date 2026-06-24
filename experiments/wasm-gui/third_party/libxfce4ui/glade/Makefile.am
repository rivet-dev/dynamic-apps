AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"libxfce4ui-glade\" \
	$(PLATFORM_CPPFLAGS)

SUBDIRS = \
	icons

if HAVE_GLADEUI2
##
## Glade module
##
glademoduledir = \
	$(GLADEUI2_MODULEDIR)

glademodule_LTLIBRARIES = \
	libxfce4uiglade2.la

libxfce4uiglade2_la_SOURCES  = \
	libxfce4ui-glade.c

libxfce4uiglade2_la_CFLAGS = \
	$(GLADEUI2_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(PLATFORM_CFLAGS)

libxfce4uiglade2_la_LDFLAGS = \
	-no-undefined \
	-avoid-version \
	-module \
	$(PLATFORM_LDFLAGS)

libxfce4uiglade2_la_LIBADD = \
	$(top_builddir)/libxfce4ui/libxfce4ui-2.la \
	$(LIBXFCE4UTIL_LIBS) \
	$(GLADEUI2_LIBS)

##
## Glade catalog xml file
##
gladecatalogdir = \
	$(GLADEUI2_CATALOGDIR)
gladecatalog_DATA = \
	libxfce4ui-2.xml
@INTLTOOL_XML_NOMERGE_RULE@
endif

CLEANFILES = \
	libxfce4ui-2.xml

EXTRA_DIST = \
	libxfce4ui-2.xml.in

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
