# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:

AM_CPPFLAGS =								\
	-I$(top_builddir)						\
	-I$(top_srcdir)							\
	-DBINDIR=\"$(bindir)\"						\
	-DDATADIR=\"$(datadir)\"					\
	-DEXO_DISABLE_DEPRECATED					\
	-DG_LOG_DOMAIN=\"thunar\"					\
	-DLIBDIR=\"$(libdir)\"						\
	-DHELPERDIR=\"$(HELPER_PATH_PREFIX)\"				\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	-DTHUNAR_VERSION_API=\"$(THUNAR_VERSION_API)\"			\
	-DSN_API_NOT_YET_FROZEN						\
	-DG_UDEV_API_IS_SUBJECT_TO_CHANGE				\
	$(PLATFORM_CPPFLAGS)

bin_SCRIPTS =								\
	thunar-settings

bin_PROGRAMS =								\
	thunar

thunar_built_sources =							\
	thunar-marshal.c							\
	thunar-marshal.h							\
	thunar-dbus-freedesktop-interfaces.h		\
	thunar-dbus-freedesktop-interfaces.c		\
	thunar-dbus-service-infos.h					\
	thunar-dbus-service-infos.c					\
	thunar-thumbnailer-proxy.c					\
	thunar-thumbnailer-proxy.h					\
	thunar-thumbnail-cache-proxy.h				\
	thunar-thumbnail-cache-proxy.c				\
	thunar-resources.h							\
	thunar-resources.c


thunar_SOURCES =							\
	$(thunar_include_HEADERS)					\
	$(thunar_built_sources)						\
	$(thunar_dbus_sources)						\
	main.c								\
	thunar-abstract-dialog.c					\
	thunar-abstract-dialog.h					\
	thunar-abstract-icon-view.c					\
	thunar-abstract-icon-view.h					\
	thunar-action-manager.c						\
	thunar-action-manager.h						\
	thunar-application.c						\
	thunar-application.h						\
	thunar-browser.c						\
	thunar-browser.h						\
	thunar-chooser-button.c						\
	thunar-chooser-button.h						\
	thunar-chooser-dialog.c						\
	thunar-chooser-dialog.h						\
	thunar-chooser-model.c						\
	thunar-chooser-model.h						\
	thunar-clipboard-manager.c					\
	thunar-clipboard-manager.h					\
	thunar-column-editor.c						\
	thunar-column-editor.h						\
	thunar-column-model.c						\
	thunar-column-model.h						\
	thunar-compact-view.c						\
	thunar-compact-view.h						\
	thunar-component.c						\
	thunar-component.h						\
	thunar-dbus-service.c						\
	thunar-dbus-service.h						\
	thunar-deep-count-job.h						\
	thunar-deep-count-job.c						\
	thunar-details-view.c						\
	thunar-details-view.h						\
	thunar-dialogs.c						\
	thunar-dialogs.h						\
	thunar-device.c							\
	thunar-device.h							\
	thunar-device-monitor.c						\
	thunar-device-monitor.h						\
	thunar-dnd.c							\
	thunar-dnd.h							\
	thunar-emblem-chooser.c						\
	thunar-emblem-chooser.h						\
	thunar-enum-types.c						\
	thunar-enum-types.h						\
	thunar-file.c							\
	thunar-file.h							\
	thunar-file-monitor.c						\
	thunar-file-monitor.h						\
	thunar-folder.c							\
	thunar-folder.h							\
	thunar-gdk-extensions.c						\
	thunar-gdk-extensions.h						\
	thunar-gio-extensions.c						\
	thunar-gio-extensions.h						\
	thunar-gobject-extensions.c					\
	thunar-gobject-extensions.h					\
	thunar-gtk-extensions.c						\
	thunar-gtk-extensions.h						\
	thunar-history.c						\
	thunar-history.h						\
	thunar-ice.c							\
	thunar-ice.h							\
	thunar-icon-factory.c						\
	thunar-icon-factory.h						\
	thunar-icon-renderer.c						\
	thunar-icon-renderer.h						\
	thunar-text-renderer.c						\
	thunar-text-renderer.h						\
	thunar-icon-view.c						\
	thunar-icon-view.h						\
	thunar-image.c							\
	thunar-image.h							\
	thunar-io-jobs.c						\
	thunar-io-jobs.h						\
	thunar-io-jobs-util.c						\
	thunar-io-jobs-util.h						\
	thunar-io-scan-directory.c					\
	thunar-io-scan-directory.h					\
	thunar-job.c							\
	thunar-job.h							\
	thunar-job-operation.c						\
	thunar-job-operation.h						\
	thunar-job-operation-history.c					\
	thunar-job-operation-history.h					\
	thunar-list-model.c						\
	thunar-list-model.h						\
	thunar-location-bar.c						\
	thunar-location-bar.h						\
	thunar-location-button.c					\
	thunar-location-button.h					\
	thunar-location-buttons.c					\
	thunar-location-buttons.h					\
	thunar-location-entry.c						\
	thunar-location-entry.h						\
	thunar-menu.c							\
	thunar-menu.h							\
	thunar-notify.c							\
	thunar-notify.h							\
	thunar-navigator.c						\
	thunar-navigator.h						\
	thunar-pango-extensions.c					\
	thunar-pango-extensions.h					\
	thunar-path-entry.c						\
	thunar-path-entry.h						\
	thunar-permissions-chooser.c					\
	thunar-permissions-chooser.h					\
	thunar-preferences-dialog.c					\
	thunar-preferences-dialog.h					\
	thunar-preferences.c						\
	thunar-preferences.h						\
	thunar-private.h						\
	thunar-progress-dialog.c					\
	thunar-progress-dialog.h					\
	thunar-progress-view.c						\
	thunar-progress-view.h						\
	thunar-properties-dialog.c					\
	thunar-properties-dialog.h					\
	thunar-renamer-dialog.c						\
	thunar-renamer-dialog.h						\
	thunar-renamer-model.c						\
	thunar-renamer-model.h						\
	thunar-renamer-pair.c						\
	thunar-renamer-pair.h						\
	thunar-renamer-progress.c					\
	thunar-renamer-progress.h					\
	thunar-sendto-model.c						\
	thunar-sendto-model.h						\
	thunar-session-client.c						\
	thunar-session-client.h						\
	thunar-shortcuts-icon-renderer.c				\
	thunar-shortcuts-icon-renderer.h				\
	thunar-shortcuts-model.c					\
	thunar-shortcuts-model.h					\
	thunar-shortcuts-pane.c						\
	thunar-shortcuts-pane.h						\
	thunar-shortcuts-view.c						\
	thunar-shortcuts-view.h						\
	thunar-side-pane.c						\
	thunar-side-pane.h						\
	thunar-simple-job.c						\
	thunar-simple-job.h						\
	thunar-size-label.c						\
	thunar-size-label.h						\
	thunar-standard-view.c						\
	thunar-standard-view.h						\
	thunar-statusbar.c						\
	thunar-statusbar.h						\
	thunar-toolbar-editor.c						\
    thunar-toolbar-editor.h						\
	thunar-thumbnail-cache.c					\
	thunar-thumbnail-cache.h					\
	thunar-thumbnailer.c						\
	thunar-thumbnailer.h						\
	thunar-transfer-job.c						\
	thunar-transfer-job.h						\
	thunar-tree-model.c						\
	thunar-tree-model.h						\
	thunar-tree-pane.c						\
	thunar-tree-pane.h						\
	thunar-tree-view.c						\
	thunar-tree-view.h						\
	thunar-user.c							\
	thunar-user.h							\
	thunar-util.c							\
	thunar-util.h							\
	thunar-view.c							\
	thunar-view.h							\
	thunar-window.c							\
	thunar-window.h

noinst_LIBRARIES = thunar.a
thunar_a_SOURCES =
thunar_a_LIBADD = 							\
	$(filter %.o, $(thunar_SOURCES:%.c=thunar-%.o))			\
	$(wildcard $(top_builddir)/thunarx/.libs/*.o)

thunar_CFLAGS =								\
	$(EXO_CFLAGS)							\
	$(GIO_CFLAGS)							\
	$(GTHREAD_CFLAGS)						\
	$(GUDEV_CFLAGS)							\
	$(LIBNOTIFY_CFLAGS)						\
	$(LIBSM_CFLAGS)							\
	$(LIBXFCE4UI_CFLAGS)						\
	$(LIBXFCE4UTIL_CFLAGS)						\
	$(LIBXFCE4KBD_PRIVATE_CFLAGS) \
	$(XFCONF_CFLAGS)						\
	$(PANGO_CFLAGS)							\
	$(PLATFORM_CFLAGS)

thunar_LDFLAGS =							\
	-no-undefined							\
	$(LIBSM_LDFLAGS)						\
	$(PLATFORM_LDFLAGS)

thunar_LDADD =								\
	$(top_builddir)/thunarx/libthunarx-$(THUNARX_VERSION_API).la	\
	$(EXO_LIBS)							\
	$(GIO_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GUDEV_LIBS)							\
	$(LIBNOTIFY_LIBS)						\
	$(LIBSM_LIBS)							\
	$(LIBXFCE4UI_LIBS)						\
	$(LIBXFCE4UTIL_LIBS)						\
	$(LIBXFCE4KBD_PRIVATE_LIBS) \
	$(XFCONF_LIBS)							\
	$(PANGO_LIBS)

thunar_DEPENDENCIES =							\
	$(top_builddir)/thunarx/libthunarx-$(THUNARX_VERSION_API).la

if HAVE_GIO_UNIX
thunar_CFLAGS +=							\
	$(GIO_UNIX_CFLAGS)

thunar_LDADD +=								\
	$(GIO_UNIX_LIBS)
endif

desktopdir = $(datadir)/applications
desktop_in_files = thunar-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

# install symlink to 'thunar'
install-exec-hook:
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	-( cd $(DESTDIR)$(bindir) ; test -x Thunar || ln -sf thunar Thunar )

uninstall-local:
	-( cd $(DESTDIR)$(bindir) ; test -h Thunar && rm -f Thunar )

clean-local:
	rm -f *.core core core.* *.o *.a

DISTCLEANFILES =							\
	thunar-settings.desktop

if MAINTAINER_MODE
CLEANFILES =								\
	xgen-tmc							\
	xgen-tmh

DISTCLEANFILES +=							\
	$(thunar_built_sources)						\
	stamp-thunar-marshal.h

BUILT_SOURCES =								\
	$(thunar_built_sources)

thunar-dbus-freedesktop-interfaces.h thunar-dbus-freedesktop-interfaces.c: $(srcdir)/thunar-dbus-freedesktop-interfaces.xml Makefile
	$(AM_V_GEN) gdbus-codegen --c-namespace Thunar --generate-c-code=thunar-dbus-freedesktop-interfaces $(srcdir)/thunar-dbus-freedesktop-interfaces.xml

thunar-dbus-service-infos.h thunar-dbus-service-infos.c: $(srcdir)/thunar-dbus-service-infos.xml Makefile
	$(AM_V_GEN) gdbus-codegen --c-namespace Thunar --generate-c-code=thunar-dbus-service-infos $(srcdir)/thunar-dbus-service-infos.xml

thunar-thumbnailer-proxy.h thunar-thumbnailer-proxy.c: $(srcdir)/thunar-thumbnailer-dbus.xml Makefile
	$(AM_V_GEN) gdbus-codegen --c-namespace Thunar --generate-c-code thunar-thumbnailer-proxy $(srcdir)/thunar-thumbnailer-dbus.xml

thunar-thumbnail-cache-proxy.h thunar-thumbnail-cache-proxy.c: $(srcdir)/thunar-thumbnail-cache-dbus.xml Makefile
	$(AM_V_GEN) gdbus-codegen --c-namespace Thunar --generate-c-code thunar-thumbnail-cache-proxy $(srcdir)/thunar-thumbnail-cache-dbus.xml

thunar-marshal.h: stamp-thunar-marshal.h
	@true

stamp-thunar-marshal.h: thunar-marshal.list Makefile
	$(AM_V_GEN) ( \
		cd $(srcdir) \
		&& echo "#ifndef __THUNAR_MARSHAL_H__" > xgen-tmh \
		&& echo "#define __THUNAR_MARSHAL_H__" >> xgen-tmh \
		&& ( glib-genmarshal \
			--prefix=_thunar_marshal \
			--header thunar-marshal.list ) >> xgen-tmh \
		&& echo "#endif /* !__THUNAR_MARSHAL_H__ */" >> xgen-tmh \
		&& ( cmp -s xgen-tmh thunar-marshal.h || cp xgen-tmh thunar-marshal.h ) \
		&& rm -f xgen-tmh \
		&& echo timestamp > $(@F) \
	)

thunar-marshal.c: thunar-marshal.list Makefile
	$(AM_V_GEN) ( \
		cd $(srcdir) \
		&& echo "#include <thunar/thunar-marshal.h>" > xgen-tmc \
		&& ( glib-genmarshal \
			--prefix=_thunar_marshal \
			--body thunar-marshal.list ) >> xgen-tmc \
		&& cp xgen-tmc thunar-marshal.c \
		&& rm -f xgen-tmc \
	)

thunar-resources.c: thunar.gresource.xml
	glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name thunar $(srcdir)/thunar.gresource.xml

thunar-resources.h: thunar.gresource.xml
	glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name thunar $(srcdir)/thunar.gresource.xml
endif

EXTRA_DIST =								\
	thunar-dbus-freedesktop-interfaces.xml	\
	thunar-dbus-service-infos.xml			\
	thunar-marshal.list						\
	thunar-thumbnail-frame.png				\
	thunar-settings							\
	thunar.gresource.xml					\
	$(desktop_in_files)

dist-hook: thunar.a
