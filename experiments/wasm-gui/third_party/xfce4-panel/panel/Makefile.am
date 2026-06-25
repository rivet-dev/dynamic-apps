
AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"xfce4-panel\" \
	-DWNCK_I_KNOW_THIS_IS_UNSTABLE \
	-DDATADIR=\"$(datadir)/xfce4\" \
	-DLIBDIR=\"$(libdir)/xfce4\" \
	-DHELPERDIR=\"$(HELPER_PATH_PREFIX)/xfce4/panel\" \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	-DDBUS_API_SUBJECT_TO_CHANGE \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfce4-panel

xfce4_panel_built_sources = \
	panel-gdbus-exported-service.h \
	panel-gdbus-exported-service.c \
	panel-marshal.c \
	panel-marshal.h \
	panel-plugin-external-wrapper-exported.h \
	panel-plugin-external-wrapper-exported.c \
	panel-preferences-dialog-ui.h

xfce4_panel_SOURCES = \
	$(xfce4_panel_built_sources) \
	main.c \
	panel-application.c \
	panel-application.h \
	panel-base-window.c \
	panel-base-window.h \
	panel-dbus-service.c \
	panel-dbus-service.h \
	panel-dbus-client.c \
	panel-dbus-client.h \
	panel-dialogs.c \
	panel-dialogs.h \
	panel-item-dialog.c \
	panel-item-dialog.h \
	panel-itembar.c \
	panel-itembar.h \
	panel-module.c \
	panel-module.h \
	panel-module-factory.c \
	panel-module-factory.h \
	panel-plugin-external.c \
	panel-plugin-external.h \
	panel-plugin-external-wrapper.c \
	panel-plugin-external-wrapper.h \
	panel-preferences-dialog.c \
	panel-preferences-dialog.h \
	panel-tic-tac-toe.c \
	panel-tic-tac-toe.h \
	panel-window.c \
	panel-window.h

xfce4_panel_CFLAGS = \
	$(GTK_CFLAGS) \
	$(GMODULE_CFLAGS) \
	$(GIO_UNIX_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(LIBWNCK_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(LIBX11_CFLAGS) \
	$(PLATFORM_CFLAGS)

xfce4_panel_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

xfce4_panel_LDADD = \
	$(top_builddir)/libxfce4panel/libxfce4panel-$(LIBXFCE4PANEL_VERSION_API).la \
	$(top_builddir)/common/libpanel-common.la \
	$(GTK_LIBS) \
	$(GMODULE_LIBS) \
	$(GIO_UNIX_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBWNCK_LIBS) \
	$(XFCONF_LIBS) \
	$(LIBX11_LIBS) \
	-lm

xfce4_panel_DEPENDENCIES = \
	$(top_builddir)/libxfce4panel/libxfce4panel-$(LIBXFCE4PANEL_VERSION_API).la \
	$(top_builddir)/common/libpanel-common.la

if MAINTAINER_MODE

panel-marshal.h: panel-marshal.list Makefile
	$(AM_V_GEN) glib-genmarshal --prefix=_panel_marshal --internal --header $< > $@

panel-marshal.c: panel-marshal.list Makefile
	$(AM_V_GEN) echo "#include <panel/panel-marshal.h>" > $@ \
	&& glib-genmarshal --prefix=_panel_marshal --body $< >> $@

panel-gdbus-exported-service.h:
panel-gdbus-exported-service.c: panel-dbus-service-infos.xml Makefile
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
	--interface-prefix org.xfce.Panel \
	--c-namespace XfcePanel   \
	--generate-c-code panel-gdbus-exported-service \
	$(srcdir)/panel-dbus-service-infos.xml

panel-plugin-external-wrapper-exported.h:
panel-plugin-external-wrapper-exported.c: panel-plugin-external-wrapper-infos.xml Makefile
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
	--interface-prefix org.xfce.Panel.Wrapper \
	--c-namespace XfcePanelPluginWrapper   \
	--generate-c-code panel-plugin-external-wrapper-exported \
	$(srcdir)/panel-plugin-external-wrapper-infos.xml

panel-preferences-dialog-ui.h: $(srcdir)/panel-preferences-dialog.glade Makefile
	$(AM_V_GEN) xdt-csource --static --strip-comments --strip-content --name=panel_preferences_dialog_ui $< >$@

#panel-marshal.h: $(srcdir)/panel-marshal.list Makefile
#	$(AM_V_GEN) glib-genmarshal --header --prefix=panel_marshal $< > $@
#
#panel-marshal.c: $(srcdir)/panel-marshal.list Makefile
#	$(AM_V_GEN) echo "#include <panel/panel-marshal.h>" > $@ \
#	&& glib-genmarshal --body --prefix=panel_marshal $< >> $@

DISTCLEANFILES = \
	$(xfce4_panel_built_sources)

BUILT_SOURCES = \
	$(xfce4_panel_built_sources)

endif

EXTRA_DIST = \
	panel-dbus-service-infos.xml \
	panel-plugin-external-wrapper-infos.xml \
	panel-preferences-dialog.glade \
	panel-marshal.list

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
