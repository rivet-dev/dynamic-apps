# Process this file with automake to produce Makefile.in
# 
# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:

# The name of the module.
DOC_MODULE=libxfce4util

# The top-level SGML file.
DOC_MAIN_SGML_FILE=libxfce4util-docs.xml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="XFCE_DISABLE_DEPRECATED"

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=$(top_srcdir)/libxfce4util

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--xml-mode --output-format=xml

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

include $(top_srcdir)/gtk-doc.make
