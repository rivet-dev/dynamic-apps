## Process this file with automake to produce Makefile.in

## common flags for all plugins
AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS) \
	$(G_CAST_CHECKS)

AM_LDFLAGS = \
	-module -avoid-version -shared -export-dynamic \
	-no-undefined -rpath $(libdir)/lxpanel/plugins \
	$(top_builddir)/src/liblxpanel.la \
	$(PACKAGE_LIBS)

BUILT_SOURCES = $(top_builddir)/src/liblxpanel.la

$(top_builddir)/src/liblxpanel.la:
	cd $(top_builddir)/src && $(MAKE) liblxpanel.la

## built-in plugins
noinst_LIBRARIES = libbuiltin_plugins.a

libbuiltin_plugins_a_CFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)/lxpanel"\" \
	-DPACKAGE_UI_DIR=\""$(datadir)/lxpanel/ui"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

if ENABLE_MENU_CACHE
MENU_SOURCES = \
	menu.c
endif

PLUGINS_SOURCES = \
	dclock.c \
	dirmenu.c \
	launchtaskbar.c \
	task-button.c \
	launch-button.c \
	pager.c \
	separator.c \
	tray.c \
	wincmd.c \
	$(MENU_SOURCES)

libbuiltin_plugins_a_SOURCES = \
	$(PLUGINS_SOURCES)

libbuiltin_plugins_a_LIBADD =


## loadable plugins
if BUILD_PLUGIN_LOADER
DYNAMIC_PLUGINS = \
	$(PLUGINS_LIST)
endif

pkglib_LTLIBRARIES = $(DYNAMIC_PLUGINS)

EXTRA_LTLIBRARIES = \
	batt.la \
	cpu.la \
	cpufreq.la \
	deskno.la \
	indicator.la \
	kbled.la \
	monitors.la \
	netstat.la \
	netstatus.la \
	thermal.la \
	volume.la \
	weather.la \
	xkb.la

pkglibdir = $(libdir)/lxpanel/plugins

# batt
batt_la_SOURCES = \
	batt/batt.c \
	batt/batt_sys.c
batt_la_CFLAGS = -I$(srcdir)/batt

# cpu
cpu_la_SOURCES = cpu/cpu.c

# cpufreq
cpufreq_la_SOURCES = cpufreq/cpufreq.c

# deskno
deskno_la_SOURCES = deskno/deskno.c

# indicator
indicator_la_SOURCES = indicator/indicator.c
indicator_la_CFLAGS = \
	-DINDICATOR_DIR=\""$(INDICATORDIR)"\" \
	-DINDICATOR_ICONS_DIR=\""$(INDICATORICONSDIR)"\" \
	$(APPLET_CFLAGS)
indicator_la_LIBADD = $(APPLET_LIBS)

# kbled
kbled_la_SOURCES = kbled/kbled.c
kbled_la_LIBADD = $(X11_LIBS)

# monitors
monitors_la_SOURCES = monitors/monitors.c

# netstat
netstat_la_SOURCES = \
	netstat/netstat.c \
	netstat/devproc.c \
	netstat/statusicon.c \
	netstat/wireless.c \
	netstat/lxnm_client.c \
	netstat/passwd_gui.c
netstat_la_CFLAGS = -I$(srcdir)/netstat
netstat_la_LIBADD = -liw

# netstatus
netstatus_la_SOURCES = \
	netstatus/netstatus.c \
	netstatus/netstatus-dialog.c \
	netstatus/netstatus-enums.c \
	netstatus/netstatus-icon.c \
	netstatus/netstatus-iface.c \
	netstatus/netstatus-sysdeps.c \
	netstatus/netstatus-util.c
netstatus_la_CFLAGS = \
	-I$(srcdir)/netstatus \
	-DPACKAGE_UI_DIR=\""$(datadir)/lxpanel/ui"\"

# thermal
thermal_la_SOURCES = thermal/thermal.c

# volume
volume_la_SOURCES = volumealsa/volumealsa.c
if BUILD_ALSA_PLUGINS
volume_la_LIBADD = -lasound
endif

# weather
weather_la_SOURCES = \
	weather/logutil.c          \
	weather/httputil.c         \
	weather/openweathermap.c   \
	weather/location.c         \
	weather/forecast.c         \
	weather/weatherwidget.c    \
	weather/weather.c
weather_la_CFLAGS = \
	-I$(srcdir)/weather \
	$(LIBXML2_CFLAGS) \
	$(CURL_CFLAGS)
weather_la_LIBADD = \
	$(LIBXML2_LIBS) \
	$(CURL_LIBS)

# xkb
xkb_la_CFLAGS = \
	-I$(srcdir)/xkb \
	-DFLAGSDIR=\"$(datadir)/lxpanel/images/xkb-flags\" \
	-DFLAGSCUSTDIR=\"$(datadir)/lxpanel/images/xkb-flags-cust\" \
	-DXKBCONFDIR=\"$(datadir)/lxpanel/xkeyboardconfig\"
xkb_la_SOURCES = \
	xkb/xkb-plugin.c \
	xkb/xkb.c
xkb_la_LIBADD = $(X11_LIBS)

xkeyboardconfigdir=$(datadir)/lxpanel/xkeyboardconfig
xkeyboardconfig_DATA = \
	xkb/xkeyboardconfig/models.cfg \
	xkb/xkeyboardconfig/layouts.cfg \
	xkb/xkeyboardconfig/toggle.cfg

flagsdir=$(datadir)/lxpanel/images/xkb-flags
flags_DATA= \
	xkb/flags/ad.png \
	xkb/flags/ae.png \
	xkb/flags/af.png \
	xkb/flags/al.png \
	xkb/flags/am.png \
	xkb/flags/ar.png \
	xkb/flags/ara.png \
	xkb/flags/at.png \
	xkb/flags/az.png \
	xkb/flags/ba.png \
	xkb/flags/bd.png \
	xkb/flags/be.png \
	xkb/flags/ben.png \
	xkb/flags/bg.png \
	xkb/flags/bh.png \
	xkb/flags/br.png \
	xkb/flags/brai.png \
	xkb/flags/bt.png \
	xkb/flags/bw.png \
	xkb/flags/by.png \
	xkb/flags/ca.png \
	xkb/flags/cd.png \
	xkb/flags/ch.png \
	xkb/flags/cm.png \
	xkb/flags/cn.png \
	xkb/flags/cu.png \
	xkb/flags/cz.png \
	xkb/flags/de.png \
	xkb/flags/dev.png \
	xkb/flags/dj.png \
	xkb/flags/dk.png \
	xkb/flags/dvorak.png \
	xkb/flags/dz.png \
	xkb/flags/ee.png \
	xkb/flags/eg.png \
	xkb/flags/epo.png \
	xkb/flags/es.png \
	xkb/flags/et.png \
	xkb/flags/eu.png \
	xkb/flags/fi.png \
	xkb/flags/fo.png \
	xkb/flags/fr.png \
	xkb/flags/gb.png \
	xkb/flags/ge.png \
	xkb/flags/gh.png \
	xkb/flags/gn.png \
	xkb/flags/gr.png \
	xkb/flags/hr.png \
	xkb/flags/hu.png \
	xkb/flags/ie.png \
	xkb/flags/il.png \
	xkb/flags/in.png \
	xkb/flags/iq.png \
	xkb/flags/ir.png \
	xkb/flags/is.png \
	xkb/flags/it.png \
	xkb/flags/jo.png \
	xkb/flags/jp.png \
	xkb/flags/ke.png \
	xkb/flags/kg.png \
	xkb/flags/kh.png \
	xkb/flags/km.png \
	xkb/flags/kp.png \
	xkb/flags/kr.png \
	xkb/flags/kw.png \
	xkb/flags/kz.png \
	xkb/flags/la.png \
	xkb/flags/latam.png \
	xkb/flags/lb.png \
	xkb/flags/lk.png \
	xkb/flags/lt.png \
	xkb/flags/lv.png \
	xkb/flags/ly.png \
	xkb/flags/ma.png \
	xkb/flags/mao.png \
	xkb/flags/me.png \
	xkb/flags/mk.png \
	xkb/flags/ml.png \
	xkb/flags/mm.png \
	xkb/flags/mn.png \
	xkb/flags/mt.png \
	xkb/flags/mv.png \
	xkb/flags/mx.png \
	xkb/flags/nec_vndr-jp.png \
	xkb/flags/ng.png \
	xkb/flags/nl.png \
	xkb/flags/no.png \
	xkb/flags/np.png \
	xkb/flags/om.png \
	xkb/flags/ph.png \
	xkb/flags/pk.png \
	xkb/flags/pl.png \
	xkb/flags/ps.png \
	xkb/flags/pt.png \
	xkb/flags/qa.png \
	xkb/flags/qc.png \
	xkb/flags/ro.png \
	xkb/flags/rs.png \
	xkb/flags/ru.png \
	xkb/flags/sa.png \
	xkb/flags/sd.png \
	xkb/flags/se.png \
	xkb/flags/si.png \
	xkb/flags/sk.png \
	xkb/flags/sn.png \
	xkb/flags/so.png \
	xkb/flags/sr.png \
	xkb/flags/sy.png \
	xkb/flags/th.png \
	xkb/flags/tj.png \
	xkb/flags/tm.png \
	xkb/flags/tn.png \
	xkb/flags/tr.png \
	xkb/flags/tw.png \
	xkb/flags/tz.png \
	xkb/flags/ua.png \
	xkb/flags/uk.png \
	xkb/flags/us.png \
	xkb/flags/un.png \
	xkb/flags/uy.png \
	xkb/flags/uz.png \
	xkb/flags/vn.png \
	xkb/flags/ye.png \
	xkb/flags/yu.png \
	xkb/flags/za.png

EXTRA_DIST = \
	batt/batt_sys.h \
	netstat/netstat.h \
	netstat/nsconfig.h \
	netstat/devproc.h \
	netstat/statusicon.h \
	netstat/wireless.h \
	netstat/lxnm_client.h \
	netstat/passwd_gui.h \
	netstatus/COPYING \
	netstatus/netstatus-dialog.h \
	netstatus/netstatus-enums.h \
	netstatus/netstatus-fallback-pixbuf.h \
	netstatus/netstatus-icon.h \
	netstatus/netstatus-iface.h \
	netstatus/netstatus-sysdeps.h \
	netstatus/netstatus-util.h \
	weather/logutil.h \
	weather/httputil.h \
	weather/yahooutil.c \
	weather/yahooutil.h \
	weather/location.h \
	weather/forecast.h \
	weather/weatherwidget.h \
	weather/providers.h \
	weather/openweathermap.h \
	xkb/xkb.h \
	$(flags_DATA) \
	$(xkeyboardconfig_DATA) \
	task-button.h \
	launch-button.h \
	icon.xpm

install-exec-hook:
	rm -f $(DESTDIR)$(libdir)/lxpanel/plugins/*.la

if BUILD_PLUGIN_LOADER
DYNAMIC_PLUGINS_INSTALLED = $(DYNAMIC_PLUGINS:.la=.so)
uninstall-hook:
	cd $(DESTDIR)$(libdir)/lxpanel/plugins && rm -f $(DYNAMIC_PLUGINS_INSTALLED) || true
endif
