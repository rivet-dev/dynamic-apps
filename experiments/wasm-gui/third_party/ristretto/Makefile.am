SUBDIRS = \
	src \
	po \
	icons

desktopdir = $(datadir)/applications
desktop_in_files = org.xfce.ristretto.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

appdatadir = $(datadir)/metainfo
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
appdata_in_files = org.xfce.ristretto.appdata.xml.in

%.xml: %.xml.in
	$(AM_V_GEN) $(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST = \
	$(desktop_in_files) \
	$(appdata_in_files)

DISTCLEANFILES = \
	$(desktop_DATA) \
	$(appdata_DATA)

DISTCHECK_CONFIGURE_FLAGS = --enable-gen-doc

.PHONY: ChangeLog

ChangeLog: Makefile
	(GIT_DIR=$(top_srcdir)/.git git log > .changelog.tmp \
	&& mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) \
	|| (touch ChangeLog; echo 'Git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog
