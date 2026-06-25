# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
# -
# Copyright (c) 2010 Jannis Pohlmann <jannis@xfce.org>
# 
# This program is free software; you can redistribute it and/or 
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of 
# the License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public 
# License along with this program; if not, write to the Free 
# Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301, USA.

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS =								\
	icons								\
	po								\
	thunarx								\
	thunar								\
	docs								\
	examples							\
	plugins

distclean-local:
	rm -rf *.cache *~
	find . -type f -name Makefile -delete
	find . -type f -name Makefile.in -delete

distuninstallcheck_listfiles = 						\
	find . -type f -print | grep -v ./share/icons/hicolor/icon-theme.cache

.PHONY: ChangeLog

ChangeLog: Makefile
	(GIT_DIR=$(top_srcdir)/.git git log > .changelog.tmp \
	&& mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) \
	|| (touch ChangeLog; echo 'Git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog

desktopdir = $(datadir)/applications
desktop_in_in_files = 							\
	thunar.desktop.in.in						\
	thunar-bulk-rename.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)
%.desktop.in: %.desktop.in.in Makefile
	$(AM_V_GEN) $(SED) -e "s,\@HELPERDIR\@,$(HELPER_PATH_PREFIX),g" < $< > $@
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

#
# .appdata
#
@INTLTOOL_XML_RULE@
appdatadir = $(datadir)/metainfo
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
appdata_in_files = org.xfce.thunar.appdata.xml.in

servicedir = $(datadir)/dbus-1/services
service_in_files = org.xfce.FileManager.service.in	\
	org.xfce.Thunar.service.in						\
	org.xfce.Thunar.FileManager1.service.in
service_DATA = $(service_in_files:.service.in=.service)

systemd_userdir = $(prefix)/lib/systemd/user
systemd_user_in_files = thunar.service.in
systemd_user_DATA = $(systemd_user_in_files:.service.in=.service)

%.service: %.service.in Makefile
	$(AM_V_GEN) $(SED) -e "s,\@bindir\@,$(bindir),g" < $< > $@

polkit_policydir = $(datadir)/polkit-1/actions
polkit_in_in_files =			\
	org.xfce.thunar.policy.in.in
polkit_in_files = $(polkit_in_in_files:.policy.in.in=.policy.in)
%.policy.in: %.policy.in.in Makefile
	sed -e "s,\@bindir\@,$(bindir),g" < $< > $@
polkit_policy_DATA = $(polkit_in_files:.policy.in=.policy)
@INTLTOOL_POLICY_RULE@

EXTRA_DIST =								\
	FAQ								\
	HACKING								\
	README.md								\
	intltool-extract.in						\
	intltool-merge.in						\
	intltool-update.in						\
	$(desktop_in_in_files)						\
	$(service_in_files)						\
	$(systemd_user_in_files)					\
	$(appdata_in_files)						\
	$(polkit_in_in_files)					\
	m4/introspection.m4

CLEANFILES		= $(appdata_DATA)

DISTCLEANFILES =							\
	intltool-extract						\
	intltool-merge							\
	intltool-update							\
	$(desktop_in_files)						\
	$(desktop_DATA)							\
	$(service_DATA)							\
	$(systemd_user_DATA)					\
	$(polkit_in_files)						\
	$(polkit_policy_DATA)					\
	po/.intltool-merge-cache.lock

DISTCHECK_CONFIGURE_FLAGS =						\
	--enable-gtk-doc							\
	--enable-gen-doc							\
	--enable-introspection
