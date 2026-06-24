## Process this file with automake to produce Makefile.in

bin_PROGRAMS = lxpanel lxpanelctl

lxpanel_CPPFLAGS = \
	-I$(top_srcdir) \
	-DPACKAGE_LIB_DIR=\""$(libdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)/lxpanel"\" \
	-DPACKAGE_UI_DIR=\""$(datadir)/lxpanel/ui"\" \
	-DPACKAGE_BIN_DIR=\""$(bindir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	$(PACKAGE_CFLAGS) \
	$(KEYBINDER_CFLAGS) \
	$(G_CAST_CHECKS)

BUILTIN_PLUGINS = $(top_builddir)/plugins/libbuiltin_plugins.a

if ENABLE_MENU_CACHE
MENU_SOURCES = menu-policy.c
endif

pkglib_LTLIBRARIES = liblxpanel.la

liblxpanel_la_CPPFLAGS = $(lxpanel_CPPFLAGS)
liblxpanel_la_SOURCES = \
	misc.c \
	configurator.c \
	dbg.c \
	ev.c \
	icon-grid.c \
	panel.c \
	panel-plugin-move.c \
	plugin.c \
	conf.c \
	space.c \
	input-button.c

liblxpanel_la_LDFLAGS = \
	-no-undefined \
	-version-info 0:0:0

liblxpanel_la_LIBADD = \
	$(PACKAGE_LIBS) \
	$(KEYBINDER_LIBS) \
	$(X11_LIBS)

lxpanel_includedir = $(includedir)/lxpanel
lxpanel_include_HEADERS = \
	plugin.h \
	panel.h \
	misc.h \
	icon-grid.h \
	conf.h

lxpanel_SOURCES = \
	icon-grid-old.c \
	bg.c \
	gtk-run.c \
	main.c \
	$(MENU_SOURCES)

if BUILD_PLUGIN_LOADER
DYNAMIC_FLAGS = -export-dynamic
endif
lxpanel_LDFLAGS = $(DYNAMIC_FLAGS)

lxpanel_LDADD = \
		liblxpanel.la \
		$(BUILTIN_PLUGINS) \
		$(PACKAGE_LIBS) \
		$(KEYBINDER_LIBS) \
		$(X11_LIBS)

lxpanelctl_SOURCES = lxpanelctl.c lxpanelctl.h
lxpanelctl_LDADD = $(X11_LIBS)

EXTRA_DIST = \
	bg.h \
	dbg.h \
	ev.h \
	menu-policy.h \
	icon-grid-old.h \
	gtk-compat.h \
	space.h \
	private.h

$(top_builddir)/plugins/libbuiltin_plugins.a: $(lxpanel_include_HEADERS) builtin-plugins-hook

# re-check local modules so it is refreshed for lxpanel binary
builtin-plugins-hook:
	@cd $(top_builddir)/plugins && $(MAKE) libbuiltin_plugins.a

.PHONY: builtin-plugins-hook
