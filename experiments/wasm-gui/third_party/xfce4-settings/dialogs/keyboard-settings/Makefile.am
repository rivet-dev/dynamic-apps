AM_CPPFLAGS = \
	-I${top_srcdir} \
	-DDATADIR=\"$(datadir)\" \
	-DBINDIR=\"$(bindir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"xfce4-keyboard-settings\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfce4-keyboard-settings

xfce4_keyboard_settings_SOURCES = \
	main.c \
	xfce-keyboard-settings.c \
	xfce-keyboard-settings.h \
	command-dialog.c \
	command-dialog.h \
	keyboard-dialog_ui.h

xfce4_keyboard_settings_CFLAGS = \
	$(GTK_CFLAGS) \
	$(LIBXKLAVIER_CFLAGS) \
	$(LIBXKLAVIER_0_4_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(LIBXFCE4KBD_PRIVATE_CFLAGS) \
	$(EXO_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(PLATFORM_CFLAGS) \
    -DGTK_DISABLE_SINGLE_INCLUDES \
    -DGDK_DISABLE_DEPRECATED \
    -DGTK_DISABLE_DEPRECATED \
    -DGSEAL_ENABLE \
    -DGDK_PIXBUF_DISABLE_DEPRECATED \
    -DG_DISABLE_DEPRECATED

xfce4_keyboard_settings_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

xfce4_keyboard_settings_LDADD = \
	$(GTK_LIBS) \
	$(LIBXKLAVIER_LIBS) \
	$(LIBXKLAVIER_0_4_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBXFCE4KBD_PRIVATE_LIBS) \
	$(EXO_LIBS) \
	$(XFCONF_LIBS)

if MAINTAINER_MODE

BUILT_SOURCES = \
	keyboard-dialog_ui.h

keyboard-dialog_ui.h: keyboard-dialog.glade
	xdt-csource --static --strip-comments --strip-content --name=keyboard_dialog_ui $< >$@

endif

desktopdir = $(datadir)/applications
desktop_in_files = xfce-keyboard-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(desktop_in_files) \
	keyboard-dialog.glade

DISTCLEANFILES = \
	$(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
