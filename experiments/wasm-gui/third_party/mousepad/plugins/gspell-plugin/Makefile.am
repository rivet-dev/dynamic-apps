if MOUSEPAD_PLUGIN_GSPELL

mousepad_plugindir = \
	$(libdir)/mousepad/plugins

mousepad_plugin_LTLIBRARIES = \
	libmousepad-plugin-gspell.la

libmousepad_plugin_gspell_la_SOURCES = \
	gspell-plugin.c \
	gspell-plugin.h \
	mousepad-plugin-gspell.c

libmousepad_plugin_gspell_la_CFLAGS = \
	-I$(top_builddir) \
	-I$(top_builddir)/plugins \
	-I$(top_srcdir) \
	-I$(top_srcdir)/plugins \
	-DG_LOG_DOMAIN=\"mousepad-plugin-gspell\" \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GTKSOURCEVIEW_CFLAGS) \
	$(GSPELL_CFLAGS) \
	$(PLATFORM_CFLAGS) \
	$(PLATFORM_CPPFLAGS)

libmousepad_plugin_gspell_la_LDFLAGS = \
	-avoid-version \
	-export-dynamic \
	-module \
	$(PLATFORM_LDFLAGS)

libmousepad_plugin_gspell_la_LIBADD = \
	$(top_builddir)/mousepad/libmousepad.la \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(GTKSOURCEVIEW_LIBS) \
	$(GSPELL_LIBS)

libmousepad_plugin_gspell_la_DEPENDENCIES = \
	$(top_builddir)/mousepad/libmousepad.la

gsettings_SCHEMAS = org.xfce.mousepad.plugins.gspell.gschema.xml

EXTRA_DIST = \
	$(gsettings_SCHEMAS)

@GSETTINGS_RULES@

endif
