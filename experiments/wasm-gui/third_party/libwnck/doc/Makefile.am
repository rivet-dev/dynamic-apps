## Process this file with automake to produce Makefile.in

# The name of the module.
DOC_MODULE=libwnck
DOC_MODULE_VERSION=3.0

# The top-level SGML file.
DOC_MAIN_SGML_FILE=libwnck-docs.sgml

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=../libwnck

# Extra options to pass to gtkdoc-scangobj
SCANGOBJ_OPTIONS=--nogtkinit

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="WNCK_DISABLE_DEPRECATED"

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--sgml-mode --output-format=xml

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=

# Used for dependencies
HFILE_GLOB=$(top_srcdir)/libwnck/*.h
CFILE_GLOB=$(top_srcdir)/libwnck/*.c

# Header files to ignore when scanning
IGNORE_HFILES=

EXTRA_HFILES = 

# Images to copy into HTML directory
HTML_IMAGES =

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files = example-force-update.c example-lazy-initialization.c

# Other files to distribute
extra_files =

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
GTKDOC_CFLAGS = -I$(top_builddir) -I$(top_srcdir) $(LIBWNCK_CFLAGS) -DWNCK_I_KNOW_THIS_IS_UNSTABLE -DWNCK_COMPILATION
GTKDOC_LIBS = $(top_builddir)/libwnck/libwnck-3.la $(LIBWNCK_LIBS)

include $(top_srcdir)/gtk-doc.make

CLEANFILES += gsf-scan.*

.PHONY : dist-hook-local

-include $(top_srcdir)/git.mk
