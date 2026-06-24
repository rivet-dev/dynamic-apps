SUBDIRS = \
	helpers

AM_CPPFLAGS = \
	-I${top_srcdir} \
	-DBINDIR=\"$(bindir)\" \
	-DDATADIR=\"$(datadir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"xfce4-mime-settings\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfce4-mime-settings	\
	xfce4-mime-helper

xfce4_mime_helper_built_sources = \
	xfce-mime-helper-enum-types.c \
	xfce-mime-helper-enum-types.h

xfce_mime_helper_headers = \
	xfce-mime-helper.h

xfce4_mime_settings_SOURCES = \
	$(xfce4_mime_helper_built_sources) \
	$(xfce_mime_helper_headers) \
	main.c \
	xfce-mime-helper-utils.c \
	xfce-mime-helper-utils.h \
	xfce-mime-helper.c \
	xfce-mime-helper-chooser.c \
	xfce-mime-helper-chooser.h \
	xfce-mime-chooser.c \
	xfce-mime-chooser.h \
	xfce-mime-window.c \
	xfce-mime-window.h

xfce4_mime_settings_CFLAGS = \
	$(GTK_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GIO_UNIX_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(EXO_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(PLATFORM_CFLAGS) \
    -DGTK_DISABLE_SINGLE_INCLUDES \
    -DGDK_DISABLE_DEPRECATED \
    -DGTK_DISABLE_DEPRECATED \
    -DGSEAL_ENABLE \
    -DGDK_PIXBUF_DISABLE_DEPRECATED \
    -DG_DISABLE_DEPRECATED

xfce4_mime_settings_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

xfce4_mime_settings_LDADD = \
	$(GTK_LIBS) \
	$(GIO_LIBS) \
	$(GIO_UNIX_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(EXO_LIBS) \
	$(XFCONF_LIBS)

xfce4_mime_helperdir = $(HELPER_PATH_PREFIX)/xfce4

xfce4_mime_helper_SOURCES = \
	$(xfce4_mime_helper_built_sources) \
	$(xfce4_mime_helper_headers) \
	xfce-mime-helper-chooser.c \
	xfce-mime-helper-chooser.h \
	xfce-mime-helper-launcher-dialog.c \
	xfce-mime-helper-launcher-dialog.h \
	xfce-mime-helper-utils.c \
	xfce-mime-helper-utils.h \
	xfce-mime-helper.c \
	xfce-mime-helper-main.c

xfce4_mime_helper_CFLAGS = \
	$(GTK3_CFLAGS) \
	$(GIO_UNIX_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
    $(EXO_CFLAGS)

xfce4_mime_helper_LDADD = \
	$(GTK3_LIBS) \
	$(GIO_UNIX_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(EXO_LIBS)

desktopdir = $(datadir)/applications
desktop_in_files = \
	xfce4-mime-settings.desktop.in \
	xfce4-file-manager.desktop.in \
	xfce4-mail-reader.desktop.in \
	xfce4-terminal-emulator.desktop.in \
	xfce4-web-browser.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

if MAINTAINER_MODE
DISTCLEANFILES = \
	stamp-xfce-mime-helper-enum-types.h \
	$(xfce4_mime_helper_built_sources)

BUILT_SOURCES = \
	$(xfce4_mime_helper_built_sources)

xfce-mime-helper-enum-types.h: stamp-xfce-mime-helper-enum-types.h
	@true
stamp-xfce-mime-helper-enum-types.h: $(xfce_mime_helper_headers) Makefile
	$(AM_V_GEN) ( cd $(srcdir) && glib-mkenums \
		--fhead "#ifndef __XFCE_MIME_HELPER_ENUM_TYPES_H__\n#define __XFCE_MIME_HELPER_ENUM_TYPES_H__\n#include <exo/exo.h>\nG_BEGIN_DECLS\n" \
		--fprod "/* enumerations from \"@filename@\" */\n" \
		--vhead "GType @enum_name@_get_type (void) G_GNUC_CONST;\n#define XFCE_MIME_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
		--ftail "G_END_DECLS\n\n#endif /* !__XFCE_MIME_HELPER_ENUM_TYPES_H__ */" \
		$(xfce_mime_helper_headers) ) >> xgen-eeth \
	&& (cmp -s xgen-eeth xfce-mime-helper-enum-types.h || cp xgen-eeth xfce-mime-helper-enum-types.h ) \
	&& rm -f xgen-eeth \
	&& echo timestamp > $(@F)

xfce-mime-helper-enum-types.c: $(xfce_mime_helper_headers) Makefile
	$(AM_V_GEN) ( cd $(srcdir) && glib-mkenums \
		--fhead "#undef GTK_DISABLE_DEPRECATED\n#define GTK_ENABLE_BROKEN\n#include <dialogs/mime-settings/xfce-mime-helper.h>\n\n#include \"xfce-mime-helper-enum-types.h\"" \
		--fprod "\n/* enumerations from \"@filename@\" */" \
		--vhead "GType\n@enum_name@_get_type (void)\n{\n\tstatic GType type = 0;\n\tif (type == 0) {\n\tstatic const G@Type@Value values[] = {"\
		--vprod "\t{ @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
		--vtail "\t{ 0, NULL, NULL }\n\t};\n\ttype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n\treturn type;\n}\n" \
		$(xfce_mime_helper_headers) ) >> xgen-eetc \
	&& cp xgen-eetc xfce-mime-helper-enum-types.c \
	&& rm -f xgen-eetc
else
DISTCLEANFILES = \
	$(desktop_DATA)
endif

EXTRA_DIST = \
	$(desktop_in_files)


