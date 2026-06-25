@SET_MAKE@

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = \
	backgrounds \
	common \
	doc \
	po \
	settings \
	src \
	pixmaps

manpagedir = $(mandir)/man1
manpage_DATA = xfdesktop.1

EXTRA_DIST = \
	README.md \
	TODO \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	$(conf_DATA) \
	$(manpage_DATA)

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update \
	po/.intltool-merge-cache.lock

distuninstallcheck_listfiles = \
	find . -type f -print | grep -v ./share/icons/hicolor/icon-theme.cache

distclean-local:
	-rm -rf *.cache *~

.PHONY: ChangeLog

ChangeLog: Makefile
	(GIT_DIR=$(top_srcdir)/.git git log > .changelog.tmp \
	&& mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) \
	|| (touch ChangeLog; echo 'Git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog

if MAINTAINER_MODE

DISTCLEANFILES += \
	compile_flags.txt

all-local: compile_flags.txt

compile_flags.txt: $(shell find . -name Makefile)
	rm -f $@
	for flag in \
		-DPACKAGE=\"xfdesktop\" \
		-DGETTEXT_PACKAGE=\"xfdesktop\" \
		-DVERSION=\"4.0.0\" \
		-DENABLE_DESKTOP_ICONS=1 \
		-DENABLE_FILE_ICONS=1 \
		-DLOCALEDIR=\"$(localedir)\" \
		-DEXO_API_SUBJECT_TO_CHANGE \
		-DWNCK_I_KNOW_THIS_IS_UNSTABLE \
		-DDEFAULT_BACKDROP=\"\" \
		-I$(top_srcdir) \
		-I$(top_srcdir)/common	\
		-I$(top_builddir)/common \
		$(GIO_CFLAGS) \
		$(GIO_UNIX_CFLAGS) \
		$(GLIB_CFLAGS) \
		$(GTHREAD_CFLAGS) \
		$(GTK_CFLAGS) \
		$(LIBNOTIFY_CFLAGS) \
		$(LIBX11_CFLAGS) \
		$(LIBXFCE4UTIL_CFLAGS) \
		$(LIBXFCE4UI_CFLAGS) \
		$(LIBXFCE4SMCLIENT_PRIVATE_CFLAGS) \
		$(LIBWNCK_CFLAGS) \
		$(XFCONF_CFLAGS) \
		$(LIBEXO_CFLAGS) \
		$(CAIRO_CFLAGS) \
		$(THUNARX_CFLAGS) \
		$(GARCON_CFLAGS) \
		$(GARCON_GTK3_CFLAGS) \
		$(LIBNOTIFY_CFLAGS) \
	; do \
		echo "$$flag" >> $@ || exit 1; \
	done

endif
