## Process this file with automake to produce Makefile.in

# The name of the module.
DOC_MODULE=exo

# Uncomment for versioned docs and specify the version of the module, e.g. '2'.
DOC_MODULE_VERSION=$(LIBEXO_VERSION_API)

# The top-level SGML file.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.xml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="EXO_DISABLE_DEPRECATED"

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=$(top_srcdir)

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--output-format=xml --xml-mode

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=

# Used for dependencies
HFILE_GLOB=
CFILE_GLOB=

# Header files to ignore when scanning
IGNORE_HFILES=

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
	$(GTK_CFLAGS)                                                  \
	$(LIBXFCE4UTIL_CFLAGS)

GTKDOC_LIBS =  								\
	$(top_builddir)/exo/libexo-$(LIBEXO_VERSION_API).la	\
	$(GTK_LIBS)

include $(top_srcdir)/gtk-doc.make

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
