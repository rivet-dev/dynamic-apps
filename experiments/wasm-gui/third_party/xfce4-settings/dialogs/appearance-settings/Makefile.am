AM_CPPFLAGS = \
	-I${top_srcdir} \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DHELPERDIR=\"$(HELPER_PATH_PREFIX)/xfce4/settings\" \
	-DG_LOG_DOMAIN=\"xfce4-appearance-settings\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfce4-appearance-settings

xfce4_appearance_settings_SOURCES = \
	main.c \
	appearance-dialog_ui.h

xfce4_appearance_settings_CFLAGS = \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(PLATFORM_CFLAGS) \
    -DGTK_DISABLE_SINGLE_INCLUDES \
    -DGDK_DISABLE_DEPRECATED \
    -DGTK_DISABLE_DEPRECATED \
    -DGSEAL_ENABLE \
    -DGDK_PIXBUF_DISABLE_DEPRECATED \
    -DG_DISABLE_DEPRECATED

xfce4_appearance_settings_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

xfce4_appearance_settings_LDADD = \
	$(GTK_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(XFCONF_LIBS)

xfce4_appearance_scriptsdir = $(HELPER_PATH_PREFIX)/xfce4/settings
xfce4_appearance_scripts_SCRIPTS = appearance-install-theme

if MAINTAINER_MODE

BUILT_SOURCES = \
	appearance-dialog_ui.h

appearance-dialog_ui.h: appearance-dialog.glade
	xdt-csource --static --strip-comments --strip-content --name=appearance_dialog_ui $< >$@

endif

desktopdir = $(datadir)/applications
desktop_in_files = xfce-ui-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(desktop_in_files) \
	appearance-install-theme \
	appearance-dialog.glade

DISTCLEANFILES = \
	$(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
