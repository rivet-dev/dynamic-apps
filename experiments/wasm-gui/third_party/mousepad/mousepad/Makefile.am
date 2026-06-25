AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-DBINDIR=\"$(bindir)\" \
	-DDATADIR=\"$(datadir)\" \
	-DLIBDIR=\"$(libdir)\" \
	-DG_LOG_DOMAIN=\"Mousepad\" \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	-DGDK_PIXBUF_DISABLE_SINGLE_INCLUDES \
	-DGTK_DISABLE_SINGLE_INCLUDES \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	mousepad

lib_LTLIBRARIES = \
	libmousepad.la

mousepad_built_sources = \
	mousepad-marshal.c \
	mousepad-marshal.h

libmousepad_la_SOURCES = \
	$(mousepad_built_sources) \
	mousepad-application.c \
	mousepad-application.h \
	mousepad-close-button.c \
	mousepad-close-button.h \
	mousepad-dialogs.c \
	mousepad-dialogs.h \
	mousepad-document.c \
	mousepad-document.h \
	mousepad-encoding.c \
	mousepad-encoding.h \
	mousepad-encoding-dialog.c \
	mousepad-encoding-dialog.h \
	mousepad-file.c \
	mousepad-file.h \
	mousepad-history.c \
	mousepad-history.h \
	mousepad-plugin.c \
	mousepad-plugin.h \
	mousepad-plugin-provider.c \
	mousepad-plugin-provider.h \
	mousepad-prefs-dialog.c \
	mousepad-prefs-dialog.h \
	mousepad-print.c \
	mousepad-print.h \
	mousepad-private.h \
	mousepad-replace-dialog.c \
	mousepad-replace-dialog.h \
	mousepad-resources.c \
	mousepad-search-bar.c \
	mousepad-search-bar.h \
	mousepad-settings.c \
	mousepad-settings.h \
	mousepad-settings-store.c \
	mousepad-settings-store.h \
	mousepad-statusbar.c \
	mousepad-statusbar.h \
	mousepad-util.c \
	mousepad-util.h \
	mousepad-view.c \
	mousepad-view.h \
	mousepad-window.c \
	mousepad-window.h

libmousepad_la_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GMODULE_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GTKSOURCEVIEW_CFLAGS) \
	$(PLATFORM_CFLAGS) \
	-DMOUSEPAD_GSETTINGS_SCHEMA_DIR=\""$(datadir)/glib-2.0/schemas"\" \
	-DMOUSEPAD_PLUGIN_DIRECTORY=\"$(libdir)/mousepad/plugins\"

libmousepad_la_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

libmousepad_la_LIBADD = \
	$(GLIB_LIBS) \
	$(GMODULE_LIBS) \
	$(GTK_LIBS) \
	$(GTKSOURCEVIEW_LIBS)

if WITH_KEYFILE_SETTINGS
libmousepad_la_CFLAGS += \
	-DMOUSEPAD_SETTINGS_KEYFILE_BACKEND=1
endif

mousepad_SOURCES = \
	main.c
mousepad_CFLAGS = \
	$(AM_CPPFLAGS) \
	$(libmousepad_la_CFLAGS)
mousepad_LDFLAGS = \
	$(libmousepad_la_LDFLAGS) \
	$(libmousepad_la_LIBADD)
mousepad_LDADD = \
	libmousepad.la
mousepad_DEPENDENCIES = \
	libmousepad.la

if MAINTAINER_MODE
DISTCLEANFILES = \
	$(mousepad_built_sources) \
	mousepad-resources.c

BUILT_SOURCES = \
	$(mousepad_built_sources) \
	mousepad-resources.c

mousepad_resource_deps = $(shell glib-compile-resources --generate-dependencies --sourcedir $(srcdir)/resources --sourcedir $(builddir)/resources $(srcdir)/resources/mousepad.gresource.xml)

mousepad_resources_c_deps = $(srcdir)/resources/mousepad.gresource.xml $(mousepad_resource_deps)

mousepad-resources.c: $(mousepad_resources_c_deps)
	$(AM_V_GEN) glib-compile-resources \
		--target="$@" \
		--sourcedir="$(srcdir)/resources" \
		--sourcedir="$(builddir)/resources" \
		--generate-source "$(srcdir)/resources/mousepad.gresource.xml"

mousepad-marshal.h: mousepad-marshal.list Makefile
	$(AM_V_GEN) glib-genmarshal --header --prefix=_mousepad_marshal $< > $@

mousepad-marshal.c: mousepad-marshal.list Makefile
	$(AM_V_GEN) echo "#include <mousepad/mousepad-marshal.h>" > $@ \
	&& glib-genmarshal --body --prefix=_mousepad_marshal $< >> $@
endif

EXTRA_DIST = \
	$(mousepad_resource_deps) \
	resources/mousepad.gresource.xml \
	mousepad-marshal.list \
	org.xfce.mousepad.gschema.xml

gsettings_SCHEMAS = org.xfce.mousepad.gschema.xml
@GSETTINGS_RULES@

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
