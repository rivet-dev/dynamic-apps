# $Id$

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS =								\
	libxfce4util							\
	docs								\
	po								\
	xfce4-kiosk-query

distclean-local:
	rm -rf *.cache *~

.PHONY: ChangeLog

ChangeLog: Makefile
	(GIT_DIR=$(top_srcdir)/.git git log > .changelog.tmp \
	&& mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) \
	|| (touch ChangeLog; echo 'Git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog

EXTRA_DIST =								\
	README.Kiosk							\
	README.md							\
	THANKS								\
	TODO								\
	gtk-doc.make							\
	intltool-extract.in						\
	intltool-merge.in						\
	intltool-update.in						\
	m4/introspection.m4						\
	m4/vapigen.m4

DISTCLEANFILES =							\
	intltool-extract						\
	intltool-merge							\
	intltool-update	

DISTCHECK_CONFIGURE_FLAGS =						\
	--enable-gtk-doc                                                \
	--enable-introspection						\
	--enable-vala=no

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
