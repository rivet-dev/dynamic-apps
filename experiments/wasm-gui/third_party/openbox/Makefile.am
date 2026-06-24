SUBDIRS = m4 po

docxbmdir       = $(docdir)/xbm
themedir        = $(datadir)/themes
localedir       = $(datadir)/locale
configdir       = $(sysconfdir)/xdg
rcdir           = $(configdir)/openbox
xsessionsdir    = $(datadir)/xsessions
gnomesessiondir = $(datadir)/gnome-session/sessions
gnomewmfilesdir = $(datadir)/gnome/wm-properties
pkgconfigdir    = $(libdir)/pkgconfig
obtpubincludedir= $(includedir)/openbox/@OBT_VERSION@/obt
rrpubincludedir = $(includedir)/openbox/@RR_VERSION@/obrender
pixmapdir       = $(datadir)/pixmaps
xsddir          = $(datadir)/openbox
appsdir         = $(datadir)/applications

theme = Clearlooks

AUTOMAKE_OPTIONS = subdir-objects foreign

ACLOCAL_AMFLAGS = -I m4

INCLUDES = -I.

check_PROGRAMS = \
	obrender/rendertest

lib_LTLIBRARIES = \
	obt/libobt.la \
	obrender/libobrender.la

bin_PROGRAMS = \
	openbox/openbox \
	tools/gdm-control/gdm-control \
	tools/gnome-panel-control/gnome-panel-control \
	tools/obxprop/obxprop

noinst_PROGRAMS = \
	obt/obt_unittests

nodist_bin_SCRIPTS = \
	data/xsession/openbox-session \
	data/xsession/openbox-gnome-session \
	data/xsession/openbox-kde-session

dist_rc_SCRIPTS = \
	data/environment

nodist_rc_SCRIPTS = \
	data/autostart/autostart

dist_libexec_SCRIPTS = \
	data/autostart/openbox-xdg-autostart

nodist_libexec_SCRIPTS = \
	data/autostart/openbox-autostart


## obrender ##

obrender_rendertest_CPPFLAGS = \
	$(PANGO_CFLAGS) \
	$(GLIB_CFLAGS) \
	-DG_LOG_DOMAIN=\"RenderTest\"
obrender_rendertest_LDADD = \
	obt/libobt.la \
	obrender/libobrender.la \
	$(GLIB_LIBS) \
	$(PANGO_LIBS) \
	$(XML_LIBS) \
	$(X_LIBS)
obrender_rendertest_SOURCES = obrender/test.c

obrender_libobrender_la_CPPFLAGS = \
	$(X_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(XML_CFLAGS) \
	$(PANGO_CFLAGS) \
	$(IMLIB2_CFLAGS) \
	$(LIBRSVG_CFLAGS) \
	-DG_LOG_DOMAIN=\"ObRender\" \
	-DDEFAULT_THEME=\"$(theme)\"
obrender_libobrender_la_LDFLAGS = \
	-version-info $(RR_CURRENT):$(RR_REVISION):$(RR_AGE)
obrender_libobrender_la_LIBADD = \
	obt/libobt.la \
	$(X_LIBS) \
	$(PANGO_LIBS) \
	$(GLIB_LIBS) \
	$(IMLIB2_LIBS) \
	$(LIBRSVG_LIBS) \
	$(XML_LIBS)
obrender_libobrender_la_SOURCES = \
	gettext.h \
	obrender/button.c \
	obrender/color.h \
	obrender/color.c \
	obrender/font.h \
	obrender/font.c \
	obrender/geom.h \
	obrender/gradient.h \
	obrender/gradient.c \
	obrender/icon.h \
	obrender/image.h \
	obrender/image.c \
	obrender/imagecache.h \
	obrender/imagecache.c \
	obrender/instance.h \
	obrender/instance.c \
	obrender/mask.h \
	obrender/mask.c \
	obrender/render.h \
	obrender/render.c \
	obrender/theme.h \
	obrender/theme.c

## obt ##

obt_libobt_la_CPPFLAGS = \
	$(X_CFLAGS) \
	$(XINERAMA_CFLAGS) \
	$(XKB_CFLAGS) \
	$(XRANDR_CFLAGS) \
	$(XSHAPE_CFLAGS) \
	$(XSYNC_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(XML_CFLAGS) \
	-DG_LOG_DOMAIN=\"Obt\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DDATADIR=\"$(datadir)\" \
	-DCONFIGDIR=\"$(configdir)\"
obt_libobt_la_LDFLAGS = \
	-version-info $(OBT_CURRENT):$(OBT_REVISION):$(OBT_AGE)
obt_libobt_la_LIBADD = \
	$(X_LIBS) \
	$(XINERAMA_LIBS) \
	$(XKB_LIBS) \
	$(XRANDR_LIBS) \
	$(XSHAPE_LIBS) \
	$(XSYNC_LIBS) \
	$(GLIB_LIBS) \
	$(XML_LIBS)
obt_libobt_la_SOURCES = \
	obt/bsearch.h \
	obt/display.h \
	obt/display.c \
	obt/internal.h \
	obt/keyboard.h \
	obt/keyboard.c \
	obt/xml.h \
	obt/xml.c \
	obt/ddparse.h \
	obt/ddparse.c \
	obt/link.h \
	obt/link.c \
	obt/paths.h \
	obt/paths.c \
	obt/prop.h \
	obt/prop.c \
	obt/signal.h \
	obt/signal.c \
	obt/util.h \
	obt/xqueue.h \
	obt/xqueue.c

## openbox ##

openbox_openbox_CPPFLAGS = \
	$(SM_CFLAGS) \
	$(X_CFLAGS) \
	$(XCURSOR_CFLAGS) \
	$(SM_CFLAGS) \
	$(PANGO_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(LIBSN_CFLAGS) \
	$(XML_CFLAGS) \
	-DLOCALEDIR=\"$(localedir)\" \
	-DDATADIR=\"$(datadir)\" \
	-DCONFIGDIR=\"$(configdir)\" \
	-DG_LOG_DOMAIN=\"Openbox\"
openbox_openbox_LDADD = \
	$(XINERAMA_LIBS) \
	$(XRANDR_LIBS) \
	$(SM_LIBS) \
	$(GLIB_LIBS) \
	$(X_LIBS) \
	$(XCURSOR_LIBS) \
	$(LIBSN_LIBS) \
	$(XML_LIBS) \
	$(EFENCE_LIBS) \
	$(LIBINTL) \
	obrender/libobrender.la \
	obt/libobt.la
openbox_openbox_LDFLAGS = -export-dynamic
openbox_openbox_SOURCES = \
	gettext.h \
	openbox/actions/all.c \
	openbox/actions/all.h \
	openbox/actions/addremovedesktop.c \
	openbox/actions/breakchroot.c \
	openbox/actions/close.c \
	openbox/actions/cyclewindows.c \
	openbox/actions/debug.c \
	openbox/actions/decorations.c \
	openbox/actions/desktop.c \
	openbox/actions/dock.c \
	openbox/actions/dockautohide.c \
	openbox/actions/directionalwindows.c \
	openbox/actions/execute.c \
	openbox/actions/exit.c \
	openbox/actions/focus.c \
	openbox/actions/focustobottom.c \
	openbox/actions/fullscreen.c \
	openbox/actions/growtoedge.c \
	openbox/actions/iconify.c \
	openbox/actions/if.c \
	openbox/actions/kill.c \
	openbox/actions/layer.c \
	openbox/actions/lower.c \
	openbox/actions/maximize.c \
	openbox/actions/move.c \
	openbox/actions/moverelative.c \
	openbox/actions/moveresizeto.c \
	openbox/actions/movetoedge.c \
	openbox/actions/omnipresent.c \
	openbox/actions/raise.c \
	openbox/actions/raiselower.c \
	openbox/actions/reconfigure.c \
	openbox/actions/resize.c \
	openbox/actions/resizerelative.c \
	openbox/actions/restart.c \
	openbox/actions/shade.c \
	openbox/actions/shadelowerraise.c \
	openbox/actions/showdesktop.c \
	openbox/actions/showmenu.c \
	openbox/actions/unfocus.c \
	openbox/actions.c \
	openbox/actions.h \
	openbox/client.c \
	openbox/client.h \
	openbox/client_list_menu.c \
	openbox/client_list_menu.h \
	openbox/client_list_combined_menu.c \
	openbox/client_list_combined_menu.h \
	openbox/client_menu.c \
	openbox/client_menu.h \
	openbox/config.c \
	openbox/config.h \
	openbox/debug.c \
	openbox/debug.h \
	openbox/dock.c \
	openbox/dock.h \
	openbox/event.c \
	openbox/event.h \
	openbox/focus.c \
	openbox/focus.h \
	openbox/focus_cycle.c \
	openbox/focus_cycle.h \
	openbox/focus_cycle_indicator.c \
	openbox/focus_cycle_indicator.h \
	openbox/focus_cycle_popup.c \
	openbox/focus_cycle_popup.h \
	openbox/frame.c \
	openbox/frame.h \
	openbox/framerender.c \
	openbox/framerender.h \
	openbox/geom.h \
	openbox/grab.c \
	openbox/grab.h \
	openbox/group.c \
	openbox/group.h \
	openbox/keyboard.c \
	openbox/keyboard.h \
	openbox/keytree.c \
	openbox/keytree.h \
	openbox/menuframe.c \
	openbox/menuframe.h \
	openbox/menu.c \
	openbox/menu.h \
	openbox/misc.h \
	openbox/mouse.c \
	openbox/mouse.h \
	openbox/moveresize.c \
	openbox/moveresize.h \
	openbox/mwm.h \
	openbox/openbox.c \
	openbox/openbox.h \
	openbox/ping.c \
	openbox/ping.h \
	openbox/place.c \
	openbox/place.h \
	openbox/place_overlap.c \
	openbox/place_overlap.h \
	openbox/prompt.c \
	openbox/prompt.h \
	openbox/popup.c \
	openbox/popup.h \
	openbox/resist.c \
	openbox/resist.h \
	openbox/screen.c \
	openbox/screen.h \
	openbox/session.c \
	openbox/session.h \
	openbox/stacking.c \
	openbox/stacking.h \
	openbox/startupnotify.c \
	openbox/startupnotify.h \
	openbox/translate.c \
	openbox/translate.h \
	openbox/window.c \
	openbox/window.h

## obt_unittests ##

obt_obt_unittests_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-DLOCALEDIR=\"$(localedir)\" \
	-DDATADIR=\"$(datadir)\" \
	-DCONFIGDIR=\"$(configdir)\" \
	-DG_LOG_DOMAIN=\"Obt-Unittests\"
obt_obt_unittests_LDADD = \
	$(GLIB_LIBS) \
	obt/libobt.la
obt_obt_unittests_LDFLAGS = -export-dynamic
obt_obt_unittests_SOURCES = \
	obt/unittest_base.h \
	obt/unittest_base.c \
	obt/bsearch_unittest.c

## gnome-panel-control ##

tools_gnome_panel_control_gnome_panel_control_CPPFLAGS = \
	$(X_CFLAGS)
tools_gnome_panel_control_gnome_panel_control_LDADD = \
	$(X_LIBS)
tools_gnome_panel_control_gnome_panel_control_SOURCES = \
	tools/gnome-panel-control/gnome-panel-control.c

## obxprop ##

tools_obxprop_obxprop_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(X_CFLAGS)
tools_obxprop_obxprop_LDADD = \
	$(GLIB_LIBS) \
	$(X_LIBS)
tools_obxprop_obxprop_SOURCES = \
	tools/obxprop/obxprop.c

## gdm-control ##

tools_gdm_control_gdm_control_CPPFLAGS = \
	$(XAUTH_CFLAGS) \
	$(X_CFLAGS) \
	$(GLIB_CFLAGS)
tools_gdm_control_gdm_control_LDADD = \
	$(XAUTH_LIBS) \
	$(X_LIBS) \
	$(GLIB_LIBS)
tools_gdm_control_gdm_control_SOURCES = \
	tools/gdm-control/gdm-control.c


## default button masks ##
dist_docxbm_DATA = \
        data/xbm/bullet.xbm \
        data/xbm/close.xbm \
        data/xbm/desk_toggled.xbm \
        data/xbm/desk.xbm \
        data/xbm/iconify.xbm \
        data/xbm/max_toggled.xbm \
        data/xbm/max.xbm \
        data/xbm/shade_toggled.xbm \
        data/xbm/shade.xbm


## themes ##

clearlooks_themedir = $(themedir)/Clearlooks/openbox-3
dist_clearlooks_theme_DATA= \
	themes/Clearlooks/openbox-3/themerc

clearlooks34_themedir = $(themedir)/Clearlooks-3.4/openbox-3
dist_clearlooks34_theme_DATA= \
	themes/Clearlooks-3.4/openbox-3/themerc

clearlooksolive_themedir = $(themedir)/Clearlooks-Olive/openbox-3
dist_clearlooksolive_theme_DATA= \
	themes/Clearlooks-Olive/openbox-3/themerc

mikachu_themedir = $(themedir)/Mikachu/openbox-3
dist_mikachu_theme_DATA= \
	themes/Mikachu/openbox-3/bullet.xbm \
	themes/Mikachu/openbox-3/close.xbm \
	themes/Mikachu/openbox-3/desk.xbm \
	themes/Mikachu/openbox-3/iconify.xbm \
	themes/Mikachu/openbox-3/max.xbm \
	themes/Mikachu/openbox-3/themerc


natura_themedir = $(themedir)/Natura/openbox-3
dist_natura_theme_DATA= \
	themes/Natura/openbox-3/close_hover.xbm \
	themes/Natura/openbox-3/close.xbm \
	themes/Natura/openbox-3/desk_toggled.xbm \
	themes/Natura/openbox-3/desk_hover.xbm \
	themes/Natura/openbox-3/desk.xbm \
	themes/Natura/openbox-3/iconify_hover.xbm \
	themes/Natura/openbox-3/iconify.xbm \
	themes/Natura/openbox-3/max_hover.xbm \
	themes/Natura/openbox-3/max_toggled.xbm \
	themes/Natura/openbox-3/max.xbm \
	themes/Natura/openbox-3/shade.xbm \
	themes/Natura/openbox-3/shade_hover.xbm \
	themes/Natura/openbox-3/themerc

artwizboxed_themedir = $(themedir)/Artwiz-boxed/openbox-3
dist_artwizboxed_theme_DATA= \
	themes/Artwiz-boxed/openbox-3/themerc

bear2_themedir = $(themedir)/Bear2/openbox-3
dist_bear2_theme_DATA= \
	themes/Bear2/openbox-3/close_pressed.xbm \
	themes/Bear2/openbox-3/close.xbm \
	themes/Bear2/openbox-3/desk_toggled.xbm \
	themes/Bear2/openbox-3/desk.xbm \
	themes/Bear2/openbox-3/iconify_pressed.xbm \
	themes/Bear2/openbox-3/iconify.xbm \
	themes/Bear2/openbox-3/max_pressed.xbm \
	themes/Bear2/openbox-3/max_toggled.xbm \
	themes/Bear2/openbox-3/max.xbm \
	themes/Bear2/openbox-3/shade_pressed.xbm \
	themes/Bear2/openbox-3/shade.xbm \
	themes/Bear2/openbox-3/themerc

orang_themedir = $(themedir)/Orang/openbox-3
dist_orang_theme_DATA= \
	themes/Orang/openbox-3/themerc

onyx_themedir = $(themedir)/Onyx/openbox-3
dist_onyx_theme_DATA= \
	themes/Onyx/openbox-3/themerc

onyxcitrus_themedir = $(themedir)/Onyx-Citrus/openbox-3
dist_onyxcitrus_theme_DATA= \
	themes/Onyx-Citrus/openbox-3/themerc

syscrash_themedir = $(themedir)/Syscrash/openbox-3
dist_syscrash_theme_DATA= \
	themes/Syscrash/openbox-3/max_disabled.xbm \
	themes/Syscrash/openbox-3/max_pressed.xbm \
	themes/Syscrash/openbox-3/max_toggled.xbm \
	themes/Syscrash/openbox-3/max.xbm \
	themes/Syscrash/openbox-3/themerc

## public headers ##

rrpubinclude_HEADERS = \
	obrender/color.h \
	obrender/font.h \
	obrender/geom.h \
	obrender/gradient.h \
	obrender/image.h \
	obrender/instance.h \
	obrender/mask.h \
	obrender/render.h \
	obrender/theme.h \
	obrender/version.h

obtpubinclude_HEADERS = \
	obt/link.h \
	obt/display.h \
	obt/keyboard.h \
	obt/xml.h \
	obt/paths.h \
	obt/prop.h \
	obt/signal.h \
	obt/util.h \
	obt/version.h \
	obt/xqueue.h

nodist_pkgconfig_DATA = \
	obrender/obrender-3.5.pc \
	obt/obt-3.5.pc

## data ##

dist_apps_DATA = \
	data/openbox.desktop

dist_pixmap_DATA = \
	data/openbox.png

dist_rc_DATA = \
	data/rc.xml \
	data/menu.xml

edit = $(SED) \
	-e 's!@version\@!$(VERSION)!' \
	-e 's!@configdir\@!$(configdir)!' \
	-e 's!@rcdir\@!$(rcdir)!' \
	-e 's!@libexecdir\@!$(libexecdir)!' \
	-e 's!@bindir\@!$(bindir)!'

data/autostart/autostart: $(top_srcdir)/data/autostart/autostart.in Makefile
	@echo make: creating $@
	@test -d $(shell dirname $(top_builddir)/$@) || \
	  mkdir $(shell dirname $(top_builddir)/$@)
	@$(edit) $< >$(top_builddir)/$@

data/autostart/openbox-autostart: $(top_srcdir)/data/autostart/openbox-autostart.in Makefile
	@echo make: creating $@
	@test -d $(shell dirname $(top_builddir)/$@) || \
	  mkdir $(shell dirname $(top_builddir)/$@)
	@$(edit) $< >$(top_builddir)/$@

%.desktop: %.desktop.in Makefile
	@echo make: creating $@
	@$(edit) $< >$@	

%-session: %-session.in Makefile
	@echo make: creating $@
	@$(edit) $< >$@	

%.1.in: %.1.sgml
	@echo make: creating $@
	@docbook-to-man $< >$@

%.1: %.1.in Makefile
	@echo make: creating $@
	@$(edit) $< >$@	

dist_gnomewmfiles_DATA = \
	data/gnome-wm-properties/openbox.desktop

nodist_xsessions_DATA = \
	data/xsession/openbox.desktop \
	data/xsession/openbox-gnome.desktop \
	data/xsession/openbox-kde.desktop

dist_gnomesession_DATA = \
	data/gnome-session/openbox-gnome.session \
	data/gnome-session/openbox-gnome-fallback.session

dist_noinst_DATA = \
	data/rc.xsd \
	data/menu.xsd \
	data/autostart/autostart.in \
	data/autostart/openbox-autostart.in \
	data/xsession/openbox.desktop.in \
	data/xsession/openbox-gnome.desktop.in \
	data/xsession/openbox-kde.desktop.in \
	data/xsession/openbox-session.in \
	data/xsession/openbox-gnome-session.in \
	data/xsession/openbox-kde-session.in \
	doc/openbox.1.sgml \
	doc/openbox.1.in \
	doc/openbox-session.1.sgml \
	doc/openbox-session.1.in \
	doc/openbox-gnome-session.1.sgml \
	doc/openbox-gnome-session.1.in \
	doc/openbox-kde-session.1.sgml \
	doc/openbox-kde-session.1.in \
	doc/obxprop.1.sgml \
	doc/obxprop.1.in \
	obrender/version.h.in \
	obrender/obrender-3.5.pc.in \
	obt/obt-3.5.pc.in \
	obt/version.h.in \
	tools/themeupdate/themeupdate.py \
	tests/hideshow.py \
	tests/Makefile \
	tests/aspect.c \
	tests/fullscreen.c \
	tests/grav.c \
	tests/grouptran.c \
	tests/icons.c \
	tests/modal2.c \
	tests/modal3.c \
	tests/modal.c \
	tests/noresize.c \
	tests/override.c \
	tests/positioned.c \
	tests/strut.c \
	tests/title.c \
	tests/urgent.c

dist_doc_DATA = \
	COMPLIANCE \
	README \
	AUTHORS \
	CHANGELOG \
	COPYING \
	data/rc.xsd \
	data/menu.xsd \
	doc/rc-mouse-focus.xml

nodist_man_MANS = \
	doc/openbox.1 \
	doc/openbox-session.1 \
	doc/openbox-gnome-session.1 \
	doc/openbox-kde-session.1 \
	doc/obxprop.1

EXTRA_DIST = \
	config.rpath

# make clean doesn't delete these for some reason, even though they are
# built by make
CLEANFILES = \
	$(nodist_man_MANS) \
	$(nodist_bin_SCRIPTS) \
	$(nodist_xsessions_DATA) \
	$(nodist_rc_SCRIPTS) \
	$(nodist_libexec_SCRIPTS)

#doc:
#       $(MAKE) -$(MAKEFLAGS) -C doc/doxygen doc

distclean-local:
	for d in . m4 po obrender parser obt openbox; do \
		for p in core core.* gmon.out *\~ *.orig *.rej .\#*; do \
			rm -f "$$d/$$p"; \
		done \
	done

.PHONY: doc
