SUBDIRS =         \
	icons

appsdir = $(datadir)/applications
apps_in_files = \
	xfce4-appfinder.desktop.in \
	xfce4-run.desktop.in
apps_DATA = $(apps_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

#
# .appdata
#
@INTLTOOL_XML_RULE@
appdatadir = $(datadir)/metainfo
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
appdata_in_files = org.xfce.xfce4-appfinder.appdata.xml.in

EXTRA_DIST = \
	$(apps_in_files)	\
	$(appdata_in_files)

CLEANFILES = $(appdata_DATA)

DISTCLEANFILES = \
	$(apps_DATA)

# vi:set ts=8 sw=8 noet ai nocindent:
