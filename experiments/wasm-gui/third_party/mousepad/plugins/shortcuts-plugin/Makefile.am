if MOUSEPAD_PLUGIN_SHORTCUTS

mousepad_plugindir = \
	$(libdir)/mousepad/plugins

mousepad_plugin_LTLIBRARIES = \
	libmousepad-plugin-shortcuts.la

libmousepad_plugin_shortcuts_la_SOURCES = \
	shortcuts-plugin.c \
	shortcuts-plugin.h \
	mousepad-plugin-shortcuts.c

libmousepad_plugin_shortcuts_la_CFLAGS = \
	-I$(top_builddir) \
	-I$(top_builddir)/plugins \
	-I$(top_srcdir) \
	-I$(top_srcdir)/plugins \
	-DG_LOG_DOMAIN=\"mousepad-plugin-shortcuts\" \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GTKSOURCEVIEW_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(LIBXFCE4KBD_PRIVATE_CFLAGS) \
	$(PLATFORM_CFLAGS) \
	$(PLATFORM_CPPFLAGS)

libmousepad_plugin_shortcuts_la_LDFLAGS = \
	-avoid-version \
	-export-dynamic \
	-module \
	$(PLATFORM_LDFLAGS)

libmousepad_plugin_shortcuts_la_LIBADD = \
	$(top_builddir)/mousepad/libmousepad.la \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(GTKSOURCEVIEW_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBXFCE4KBD_PRIVATE_LIBS)

libmousepad_plugin_shortcuts_la_DEPENDENCIES = \
	$(top_builddir)/mousepad/libmousepad.la

endif
