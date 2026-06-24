
AM_CPPFLAGS = \
	-I${top_srcdir} \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"xfce4-settings-editor\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfce4-settings-editor

xfce4_settings_editor_built_sources = \
	xfce-settings-marshal.c \
	xfce-settings-marshal.h

xfce4_settings_editor_SOURCES = \
	$(xfce4_settings_editor_built_sources) \
	main.c \
	xfce-settings-cell-renderer.c \
	xfce-settings-cell-renderer.h \
	xfce-settings-editor-box.c \
	xfce-settings-editor-box.h \
	xfce-settings-prop-dialog.c \
	xfce-settings-prop-dialog.h

xfce4_settings_editor_CFLAGS = \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(DBUS_GLIB_CFLAGS) \
	$(PLATFORM_CFLAGS) \
    -DGTK_DISABLE_SINGLE_INCLUDES \
    -DGDK_DISABLE_DEPRECATED \
    -DGTK_DISABLE_DEPRECATED \
    -DGSEAL_ENABLE \
    -DGDK_PIXBUF_DISABLE_DEPRECATED \
    -DG_DISABLE_DEPRECATED

xfce4_settings_editor_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

xfce4_settings_editor_LDADD = \
	$(GTK_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(XFCONF_LIBS)

desktopdir = $(datadir)/applications
desktop_in_files = xfce4-settings-editor.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

#
# Build sources
#
if MAINTAINER_MODE

xfce-settings-marshal.h: xfce-settings-marshal.list Makefile
	$(AM_V_GEN) glib-genmarshal --prefix=_xfce_settings_marshal --internal --header $< > $@

xfce-settings-marshal.c: xfce-settings-marshal.list Makefile
	$(AM_V_GEN) echo "#include \"xfce-settings-marshal.h\"" > $@ \
	&& glib-genmarshal --prefix=_xfce_settings_marshal --body $< >> $@

DISTCLEANFILES = \
	$(xfce4_settings_editor_built_sources) \
	$(desktop_DATA)

BUILT_SOURCES = \
	$(xfce4_settings_editor_built_sources)

else

DISTCLEANFILES = \
	$(desktop_DATA)

endif

EXTRA_DIST = \
	xfce-settings-marshal.list \
	$(desktop_in_files)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
