ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS =								\
	data								\
	po								\
	src

desktopdir = $(datadir)/applications
desktop_in_files = xfce4-taskmanager.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

ChangeLog: Makefile
	ChangeLog-git.sh > $(srcdir)/ChangeLog || touch $(srcdir)/ChangeLog

EXTRA_DIST =								\
	README.md								\
	intltool-extract.in						\
	intltool-merge.in						\
	intltool-update.in						\
	$(desktop_in_files)

DISTCLEANFILES =							\
	intltool-extract						\
	intltool-merge							\
	intltool-update							\
	po/.intltool-merge-cache.lock					\
	$(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
