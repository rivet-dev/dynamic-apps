ACLOCAL_AMFLAGS = -I m4

SUBDIRS =									\
	icons									\
	mousepad								\
	plugins								\
	po

distclean-local:
	rm -rf *.cache *~

distuninstallcheck_listfiles = 						\
	find . -type f \! -path ./share/icons/hicolor/icon-theme.cache

desktopdir = $(datadir)/applications
desktop_in_in_files = 								\
	org.xfce.mousepad.desktop.in.in \
	org.xfce.mousepad-settings.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

%.desktop.in: %.desktop.in.in
	$(SED) -e "s,\@libexecdir\@,$(libexecdir),g" < $< > $@

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

appdatadir = $(datadir)/metainfo
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
appdata_in_files = org.xfce.mousepad.appdata.xml.in

%.xml: %.xml.in
	$(AM_V_GEN) $(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

if HAVE_POLKIT

polkit_policydir = $(datadir)/polkit-1/actions
polkit_in_in_files =			\
	org.xfce.mousepad.policy.in.in
polkit_in_files = $(polkit_in_in_files:.policy.in.in=.policy.in)
polkit_policy_DATA = $(polkit_in_files:.policy.in=.policy)

%.policy.in: %.policy.in.in
	$(SED) -e "s,\@bindir\@,$(bindir),g" < $< > $@

%.policy: %.policy.in
	$(AM_V_GEN) $(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

endif

.PHONY: ChangeLog

ChangeLog: Makefile
	(GIT_DIR=$(top_srcdir)/.git git log > .changelog.tmp			\
	&& mv .changelog.tmp ChangeLog; rm -f .changelog.tmp)			\
	|| (touch ChangeLog; echo 'Git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog

EXTRA_DIST =									\
	$(desktop_in_in_files)						\
	$(appdata_in_files)							\
	$(polkit_in_in_files)

DISTCLEANFILES =								\
	$(desktop_in_files)							\
	$(desktop_DATA)								\
	$(appdata_DATA)								\
	$(polkit_in_files)							\
	$(polkit_policy_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
