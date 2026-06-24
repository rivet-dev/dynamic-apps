# Name of the module
DOC_MODULE=garcon

# Top-level SGML file
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.xml

SCAN_OPTIONS=

# Extra options to pass to gtkdoc-scangobj
SCANGOBJ_OPTIONS=

# Directory containing the source code
DOC_SOURCE_DIR=$(top_srcdir)/garcon $(top_srcdir)/garcon-gtk

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--xml-mode --output-format=xml

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=

# Used for dependencies
HFILE_GLOB=
CFILE_GLOB=

# Extra files to add when scanning (relative to $srcdir)
EXTRA_HFILES=

# Images to copy into HTML directory
HTML_IMAGES =

# Extra SGML files that are included by DOC_MAIN_SGML_FILE
content_files =

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
GTKDOC_CFLAGS =	 							\
	-I$(top_srcdir) 						\
	-I$(top_builddir) 						\
	$(GIO_CFLAGS)							\
	$(GLIB_CFLAGS)							\
	$(GOBJECT_CFLAGS)						\
	$(GTHREAD_CFLAGS)						\
	$(GTK_CFLAGS)							\
	$(LIBXFCE4UTIL_CFLAGS)

GTKDOC_LIBS =								\
	$(GIO_LIBS)							\
	$(GLIB_LIBS)							\
	$(GOBJECT_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GTK_CFLAGS)							\
	$(LIBXFCE4UTIL_LIBS)						\
	$(top_builddir)/garcon/libgarcon-$(GARCON_VERSION_API).la \
	$(top_builddir)/garcon-gtk/libgarcon-gtk3-1.la

include $(top_srcdir)/gtk-doc.make
