if MOUSEPAD_PLUGIN_TEST

mousepad_plugindir = \
	$(libdir)/mousepad/plugins

mousepad_plugin_LTLIBRARIES = \
	libmousepad-plugin-test.la

libmousepad_plugin_test_la_SOURCES = \
	test-plugin.c \
	test-plugin.h \
	mousepad-plugin-test.c

libmousepad_plugin_test_la_CFLAGS = \
	-I$(top_builddir) \
	-I$(top_builddir)/plugins \
	-I$(top_srcdir) \
	-I$(top_srcdir)/plugins \
	-DG_LOG_DOMAIN=\"mousepad-plugin-test\" \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GTKSOURCEVIEW_CFLAGS) \
	$(PLATFORM_CFLAGS) \
	$(PLATFORM_CPPFLAGS)

libmousepad_plugin_test_la_LDFLAGS = \
	-avoid-version \
	-export-dynamic \
	-module \
	$(PLATFORM_LDFLAGS)

libmousepad_plugin_test_la_LIBADD = \
	$(top_builddir)/mousepad/libmousepad.la \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(GTKSOURCEVIEW_LIBS)

libmousepad_plugin_test_la_DEPENDENCIES = \
	$(top_builddir)/mousepad/libmousepad.la

endif
