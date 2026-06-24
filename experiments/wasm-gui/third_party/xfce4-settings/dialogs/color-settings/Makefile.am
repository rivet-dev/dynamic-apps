AM_CPPFLAGS = \
	-I${top_srcdir} \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"xfce4-color-settings\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfce4-color-settings

xfce4_color_settings_SOURCES = \
	main.c \
	color-device.c \
	color-device.h \
	color-profile.c \
	color-profile.h \
	color-dialog_ui.h

xfce4_color_settings_CFLAGS = \
	$(GTK_CFLAGS) \
	$(COLORD_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(PLATFORM_CFLAGS) \
    -DGTK_DISABLE_SINGLE_INCLUDES \
    -DGDK_DISABLE_DEPRECATED \
    -DGTK_DISABLE_DEPRECATED \
    -DGSEAL_ENABLE \
    -DGDK_PIXBUF_DISABLE_DEPRECATED \
    -DG_DISABLE_DEPRECATED

xfce4_color_settings_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

xfce4_color_settings_LDADD = \
	$(GTK_LIBS) \
	$(COLORD_LIBS) \
	$(LIBXFCE4UI_LIBS)

if MAINTAINER_MODE

BUILT_SOURCES = \
	color-dialog_ui.h

color-dialog_ui.h: color-dialog.glade
	xdt-csource --static --strip-comments --strip-content --name=color_dialog_ui $< >$@

endif

desktopdir = $(datadir)/applications
desktop_in_files = xfce4-color-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(desktop_in_files) \
	color-dialog.glade

DISTCLEANFILES = \
	$(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
