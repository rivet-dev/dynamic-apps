# The name of the module.
DOC_MODULE=thunar

# The top-level SGML file.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.xml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="EXO_DISABLE_DEPRECATED" --rebuild-sections --rebuild-types

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=$(top_srcdir)/thunar

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--xml-mode --output-format=xml

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=

# Used for dependencies
HFILE_GLOB=$(top_srcdir)/thunar/*.h
CFILE_GLOB=$(top_srcdir)/thunar/*.c

# Header files to ignore when scanning
IGNORE_HFILES=							\
	thunar-private.h					\
	thunar-dbus-freedesktop-interfaces.h			\
	thunar-dbus-service-infos.h				\
	thunar-marshal.h					\
	stamp-thunar-marshal.h					\
	thunar-thumbnailer-proxy.h				\
	thunar-thumbnail-cache-proxy.h				\
	thunar-resources.h


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
	-I$(top_srcdir)/thunar			\
	-I$(top_builddir) 					\
	$(GTK_CFLAGS)								\
	$(GTK_DOC_EXTRA_CFLAGS)			\
	$(EXO_CFLAGS)								\
	$(GIO_CFLAGS)								\
	$(GTHREAD_CFLAGS)						\
	$(GUDEV_CFLAGS)							\
	$(LIBNOTIFY_CFLAGS)						\
	$(LIBSM_CFLAGS)							\
	$(LIBXFCE4UI_CFLAGS)						\
	$(LIBXFCE4KBD_PRIVATE_CFLAGS) 					\
	$(XFCONF_CFLAGS)						\
	$(PANGO_CFLAGS)							\
	$(PLATFORM_CFLAGS)

GTKDOC_LIBS =  								\
	$(GTK_LIBS)							\
	$(top_builddir)/thunarx/libthunarx-$(THUNARX_VERSION_API).la	\
	$(top_builddir)/thunar/thunar.a					\
	$(EXO_LIBS)							\
	$(GIO_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GMODULE_LIBS)							\
	$(GUDEV_LIBS)							\
	$(LIBNOTIFY_LIBS)						\
	$(LIBSM_LIBS)							\
	$(LIBXFCE4UI_LIBS)						\
	$(LIBXFCE4KBD_PRIVATE_LIBS)					\
	$(XFCONF_LIBS)							\
	$(PANGO_LIBS)

include $(top_srcdir)/gtk-doc.make

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
