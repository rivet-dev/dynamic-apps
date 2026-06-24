AM_CPPFLAGS = \
	-I${top_srcdir} \
	-DMENUFILE=\"$(sysconfdir)/xdg/menus/xfce-settings-manager.menu\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"xfce4-settings-manager\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfce4-settings-manager

xfce4_settings_manager_SOURCES = \
	main.c \
	xfce-settings-manager-dialog.c \
	xfce-settings-manager-dialog.h

xfce4_settings_manager_CFLAGS = \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(GARCON_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(EXO_CFLAGS) \
	$(PLATFORM_CFLAGS) \
    -DGTK_DISABLE_SINGLE_INCLUDES \
    -DGSEAL_ENABLE

xfce4_settings_manager_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

xfce4_settings_manager_LDADD = \
	$(GTK_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(XFCONF_LIBS) \
	$(EXO_LIBS) \
	$(GARCON_LIBS)

menudir = $(sysconfdir)/xdg/menus
menu_DATA = xfce-settings-manager.menu

desktopdir = $(datadir)/applications
desktop_in_files = xfce-settings-manager.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(desktop_in_files) \
	$(menu_DATA)

DISTCLEANFILES = \
	$(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
