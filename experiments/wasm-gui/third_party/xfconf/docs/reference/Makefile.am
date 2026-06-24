## Process this file with automake to produce Makefile.in

# The name of the module.
DOC_MODULE=xfconf

# The top-level SGML file.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.xml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="XFCONF_DISABLE_DEPRECATED"

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR= \
	$(top_srcdir)/common \
	$(top_srcdir)/xfconf \
	$(top_srcdir)/xfconfd

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--xml-mode --output-format=xml

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=

# Used for dependencies
HFILE_GLOB =
CFILE_GLOB =

# Header files to ignore when scanning
IGNORE_HFILES= \
	xfconf-gdbus-bindings.h \
	xfconf-cache.h \
	xfconf-private.h \
	xfconf-backend-factory.h \
	xfconf-backend-perchannel-xml.h \
	xfconf-daemon.h \
	xfconf-lifecycle-manager.h \
	xfconf-locking-utils.h

# Extra files to add when scanning (relative to $srcdir)
EXTRA_HFILES =

# Images to copy into HTML directory
HTML_IMAGES = 

# Extra SGML files that are included by DOC_MAIN_SGML_FILE
content_files =

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
GTKDOC_CFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir)

GTKDOC_LIBS = \
	$(top_builddir)/xfconf/libxfconf-$(LIBXFCONF_VERSION_API).la

include $(top_srcdir)/gtk-doc.make

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
