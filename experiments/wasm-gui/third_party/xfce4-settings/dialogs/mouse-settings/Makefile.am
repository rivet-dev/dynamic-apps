AM_CPPFLAGS = \
	-I${top_srcdir} \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"xfce4-mouse-settings\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfce4-mouse-settings

xfce4_mouse_settings_SOURCES = \
	main.c \
	mouse-dialog_ui.h

xfce4_mouse_settings_CFLAGS = \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(XI_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(LIBX11_CFLAGS) \
	$(LIBINPUT_CFLAGS) \
	$(PLATFORM_CFLAGS) \
    -DGTK_DISABLE_SINGLE_INCLUDES \
    -DGDK_DISABLE_DEPRECATED \
    -DGTK_DISABLE_DEPRECATED \
    -DGSEAL_ENABLE \
    -DGDK_PIXBUF_DISABLE_DEPRECATED \
    -DG_DISABLE_DEPRECATED

xfce4_mouse_settings_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

xfce4_mouse_settings_LDADD = \
	$(GTK_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(XFCONF_LIBS) \
	$(XI_LIBS) \
	$(LIBX11_LIBS) \
	$(LIBINPUT_LIBS) \
	-lm

if HAVE_XCURSOR
xfce4_mouse_settings_CFLAGS += \
	$(XCURSOR_CFLAGS)

xfce4_mouse_settings_LDADD += \
	$(XCURSOR_LIBS)
endif

if MAINTAINER_MODE

BUILT_SOURCES = \
	mouse-dialog_ui.h

mouse-dialog_ui.h: mouse-dialog.glade
	xdt-csource --static --strip-comments --strip-content --name=mouse_dialog_ui $< >$@

endif

desktopdir = $(datadir)/applications
desktop_in_files = xfce-mouse-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(desktop_in_files) \
	mouse-dialog.glade

DISTCLEANFILES = \
	$(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
