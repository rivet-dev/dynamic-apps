AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = \
	icons	\
	common \
	panel-plugin \
	po \
	themes \
	xfce4-notifyd \
	xfce4-notifyd-config \
	tests

dist_autogen = autogen.sh
dist_misc = TODO

distuninstallcheck_listfiles = \
	find . -type f -print | grep -v ./share/icons/hicolor/icon-theme.cache

icon_dir = $(datadir)/icons/hicolor
gtk_update_icon_cache = gtk-update-icon-cache -f -t $(icon_dir)

dist_noinst_DATA = \
	$(dist_misc)

dist_noinst_SCRIPTS = \
	$(dist_autogen)

DISTCLEANFILES = \
	$(distclean_service) \
	$(distclean_autostart)

if MAINTAINER_MODE

all-local: compile_flags.txt

compile_flags.txt: $(shell find . -name Makefile)
	rm -f $@
	for flag in \
		-DHAVE_CONFIG_H=1 \
		-DPACKAGE=\"xfce4-notifyd\" \
		-DGETTEXT_PACKAGE=\"xfce4-notifyd\" \
		-DVERSION=\"1.0.0\" \
		-DLOCALEDIR=\"$(localedir)\" \
		-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
		-DG_LOG_DOMAIN=\"xfce4-notification-plugin\" \
		-I$(top_srcdir) \
		$(GTK_CFLAGS) \
		$(LIBXFCE4UTIL_CFLAGS) \
		$(LIBXFCE4UI_CFLAGS) \
		$(LIBXFCE4PANEL_CFLAGS) \
		$(XFCONF_CFLAGS) \
		$(GTK_LAYER_SHELL_CFLAGS) \
		$(SQLITE_CFLAGS) \
		$(PLATFORM_CFLAGS) \
		$(PLATFORM_CPPFLAGS) \
		$(LIBXFCE4UI_LIBS) \
		$(GTK_LIBS) \
		$(LIBXFCE4UTIL_LIBS) \
		$(LIBXFCE4UI_LIBS) \
		$(LIBXFCE4PANEL_LIBS) \
		$(XFCONF_LIBS) \
		$(GTK_LAYER_SHELL_LIBS) \
		$(SQLITE_LIBS) \
	; do \
		echo "$$flag" >> $@ || exit 1; \
	done

endif

install-data-hook:
	@-if test -z "$(DESTDIR)"; then \
	    echo "Updating Gtk icon cache."; \
	    $(gtk_update_icon_cache); \
	else \
	    echo "*** Icon cache not updated.  Remember to run:"; \
	    echo "***"; \
	    echo "***   $(gtk_update_icon_cache)"; \
	    echo "***"; \
	fi

distclean-local:
	-rm -rf *.cache *~

checksums: distcheck
	sha1sum $(PACKAGE)-$(VERSION).tar.bz2 > $(PACKAGE)-$(VERSION).tar.bz2.sha1.asc

gpg-sign: checksums
	gpg -b --armor $(PACKAGE)-$(VERSION).tar.bz2

release: gpg-sign
	-mkdir releases/
	mv $(PACKAGE)-$(VERSION).tar.bz2* releases/
	-rm $(PACKAGE)-$(VERSION).tar.gz
