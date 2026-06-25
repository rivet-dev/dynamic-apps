AM_CPPFLAGS = \
	-I${top_srcdir}

noinst_LTLIBRARIES = \
	libxfce-notifyd-common.la

libxfce_notifyd_common_la_SOURCES = \
	xfce-notify-common.c \
	xfce-notify-common.h \
	xfce-notify-enum-types.c \
	xfce-notify-enum-types.h \
	xfce-notify-log-types.h \
	xfce-notify-log-types.c \
	xfce-notify-log-gbus.c \
	xfce-notify-log-gbus.h \
	xfce-notify-log-util.c \
	xfce-notify-log-util.h

libxfce_notifyd_common_la_CFLAGS = \
	$(LIBXFCE4UI_CFLAGS) \
	$(GTK3_CFLAGS) \
	$(GIO_UNIX_CFLAGS) \
	$(SQLITE_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(PLATFORM_CFLAGS)

libxfce_notifyd_common_la_LIBADD = \
	$(LIBXFCE4UI_LIBS) \
	$(GTK3_LIBS) \
	$(GIO_UNIX_LIBS) \
	$(XFCONF_LIBS) \
	$(SQLITE_LIBS)

libxfce_notifyd_common_la_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

enum_headers = \
	xfce-notify-common.h

dist_noinst_DATA = \
	xfce-notify-log-dbus.xml

BUILT_SOURCES = \
	xfce-notify-enum-types.c \
	xfce-notify-enum-types.h \
	xfce-notify-log-gbus.c \
	xfce-notify-log-gbus.h

CLEANFILES = \
	xfce-notify-enum-types.c \
	xfce-notify-enum-types.h \
	xfce-notify-log-gbus.c \
	xfce-notify-log-gbus.h

xfce-notify-log-gbus.h xfce-notify-log-gbus.c: $(srcdir)/xfce-notify-log-dbus.xml Makefile
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
		--interface-prefix org.xfce.Notifyd.Log \
		--c-namespace Xfce \
		--generate-c-code xfce-notify-log-gbus \
		--glib-max-allowed $(GLIB_REQUIRED_VERSION) \
		$<

xfce-notify-enum-types.h: $(enum_headers) Makefile
	$(AM_V_GEN) $(GLIB_MKENUMS) \
	      --fhead "#ifndef __XFCE_NOTIFY_ENUM_TYPES_H__\n#define __XFCE_NOTIFY_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n\n" \
	      --fprod "/* enumerations from \"@filename@\" */\n" \
	      --vhead "GType @enum_name@_get_type(void) G_GNUC_CONST;\n#define XFCE_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n\n" \
	      --ftail "G_END_DECLS\n\n#endif /* !__XFCE_NOTIFY_ENUM_TYPES_H__ */" \
	      $(addprefix $(srcdir)/,$(enum_headers)) > $@ || { rm -f $@; exit 1; }

xfce-notify-enum-types.c: $(enum_headers) Makefile
	$(AM_V_GEN) $(GLIB_MKENUMS) \
	      --fhead "#include \"xfce-notify-common.h\"\n#include \"xfce-notify-enum-types.h\"" \
	      --fprod "\n/* enumerations from \"@filename@\" */" \
	      --vhead "GType\n@enum_name@_get_type(void)\n{\n    static GType type = 0;\n    if (type == 0) {\n        static const G@Type@Value values[] = {"\
	      --vprod "            { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
	      --vtail "            { 0, NULL, NULL }\n        };\n       type = g_@type@_register_static(\"@EnumName@\", values);\n    }\n    return type;\n}\n\n" \
	      $(addprefix $(srcdir)/,$(enum_headers)) > $@ || { rm -f $@; exit 1; }
