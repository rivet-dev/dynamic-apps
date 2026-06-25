bin_PROGRAMS = ristretto

ristretto_SOURCES = \
	app_menu_item.c app_menu_item.h \
	file.c file.h \
	gnome_wallpaper_manager.c gnome_wallpaper_manager.h \
	icon_bar.c icon_bar.h \
	image_list.c image_list.h \
	image_viewer.c image_viewer.h \
	main.c \
	main_window.c main_window.h \
	main_window_ui.h \
	marshal.c marshal.h \
	mime_db.c mime_db.h \
	monitor_chooser.c monitor_chooser.h \
	preferences_dialog.h preferences_dialog.c \
	print.c print.h \
	privacy_dialog.h privacy_dialog.c \
	properties_dialog.h properties_dialog.c \
	settings.c settings.h \
	thumbnailer.c thumbnailer.h \
	tumbler.c tumbler.h \
	util.c util.h \
	wallpaper_manager.c wallpaper_manager.h \
	xfce_wallpaper_manager.c xfce_wallpaper_manager.h

ristretto_CFLAGS = \
	$(GTK_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTHREAD_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GIO_UNIX_CFLAGS) \
	$(LIBEXIF_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(EXO_CFLAGS) \
	$(CAIRO_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(LIBX11_CFLAGS) \
	-DDOCDIR=\"$(DESTDIR)$(docdir)\" \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\"

ristretto_LDFLAGS = \
	$(LIBX11_LDFLAGS)

ristretto_LDADD = \
	$(GTK_LIBS) \
	$(GLIB_LIBS) \
	$(GTHREAD_LIBS) \
	$(GIO_LIBS) \
	$(GIO_UNIX_LIBS) \
	$(LIBEXIF_LIBS) \
	$(XFCONF_LIBS) \
	$(EXO_LIBS) \
	$(CAIRO_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBX11_LIBS) \
	$(LIBM) \
	$(MAGIC_LIBS)

AM_CPPFLAGS = \
	-I${top_srcdir}

DISTCLEANFILES =
ristretto_built_sources =
BUILT_SOURCES =

if MAINTAINER_MODE

ristretto_built_sources += \
    marshal.h \
    marshal.c

BUILT_SOURCES += \
	main_window_ui.h

main_window_ui.h: main_window_ui.xml
	xdt-csource --strip-comments --strip-content --static --name=main_window_ui $< > $@

marshal.h: stamp-marshal.h
	@true

stamp-marshal.h: marshal.list Makefile
	$(AM_V_GEN) ( \
		cd $(srcdir) \
		&& echo "#ifndef __RSTTO_MARSHAL_H__" > xgen-tmh \
		&& echo "#define __RSTTO_MARSHAL_H__" >> xgen-tmh \
		&& ( glib-genmarshal \
			--prefix=_rstto_marshal \
			--header marshal.list ) >> xgen-tmh \
		&& echo "#endif /* !__RSTTO_MARSHAL_H__ */" >> xgen-tmh \
		&& ( cmp -s xgen-tmh marshal.h || cp xgen-tmh marshal.h ) \
		&& rm -f xgen-tmh \
		&& echo timestamp > $(@F) \
	)

marshal.c: marshal.list Makefile
	$(AM_V_GEN) ( \
		cd $(srcdir) \
		&& echo "#include \"marshal.h\"" > xgen-tmc \
		&& ( glib-genmarshal \
			--prefix=_rstto_marshal \
			--body marshal.list ) >> xgen-tmc \
		&& cp xgen-tmc marshal.c \
		&& rm -f xgen-tmc \
	)

DISTCLEANFILES += \
	stamp-marshal.h

endif

ristretto_built_sources += \
    tumbler.c \
    tumbler.h

BUILT_SOURCES += \
    $(ristretto_built_sources)

tumbler-service-dbus.xml: tumbler-service-dbus.xml.in
	$(SED) -e "s,\@TUMBLER_SERVICE_NAME_PREFIX\@,$(TUMBLER_SERVICE_NAME_PREFIX),g" \
	       -e "s,\@TUMBLER_SERVICE_PATH_PREFIX\@,$(TUMBLER_SERVICE_PATH_PREFIX),g" < $< > $@

tumbler.c tumbler.h : tumbler-service-dbus.xml Makefile.am
	gdbus-codegen \
		--c-namespace=Tumbler \
		--interface-prefix=@TUMBLER_SERVICE_NAME_PREFIX@. \
		--generate-c-code=tumbler \
		tumbler-service-dbus.xml

DISTCLEANFILES += \
	$(ristretto_built_sources) \
	tumbler-service-dbus.xml

EXTRA_DIST = \
	main_window_ui.xml \
	tumbler-service-dbus.xml.in
