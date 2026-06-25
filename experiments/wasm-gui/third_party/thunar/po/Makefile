# Makefile for program source directory in GNU NLS utilities package.
# Copyright (C) 1995, 1996, 1997 by Ulrich Drepper <drepper@gnu.ai.mit.edu>
# Copyright (C) 2004-2008 Rodney Dawes <dobey.pwns@gmail.com>
#
# This file may be copied and used freely without restrictions.  It may
# be used in projects which are not available under a GNU Public License,
# but which still want to provide support for the GNU gettext functionality.
#
# - Modified by Owen Taylor <otaylor@redhat.com> to use GETTEXT_PACKAGE
#   instead of PACKAGE and to look for po2tbl in ./ not in intl/
#
# - Modified by jacob berkman <jacob@ximian.com> to install
#   Makefile.in.in and po2tbl.sed.in for use with glib-gettextize
#
# - Modified by Rodney Dawes <dobey.pwns@gmail.com> for use with intltool
#
# We have the following line for use by intltoolize:
# INTLTOOL_MAKEFILE

GETTEXT_PACKAGE = thunar
PACKAGE = thunar
VERSION = 4.18.10

SHELL = /bin/bash

srcdir = .
top_srcdir = ..
top_builddir = ..


prefix = /home/nathan/secure-exec-wasmgui/experiments/wasm-gui/third_party/wasm-prefix-threads
exec_prefix = ${prefix}
datadir = /usr/share
datarootdir = ${prefix}/share
libdir = ${exec_prefix}/lib
localedir = ${prefix}/share/locale
subdir = po
install_sh = ${SHELL} /home/nathan/secure-exec-wasmgui/experiments/wasm-gui/third_party/thunar/install-sh
# Automake >= 1.8 provides $(MKDIR_P).
# Until it can be supposed, use the safe fallback:
mkdir_p = $(install_sh) -d

INSTALL = /usr/bin/install -c
INSTALL_DATA = ${INSTALL} -m 644

GMSGFMT = /home/nathan/secure-exec-wasmgui/experiments/wasm-gui/toolchain/host-bin/msgfmt
MSGFMT = /home/nathan/secure-exec-wasmgui/experiments/wasm-gui/toolchain/host-bin/msgfmt
XGETTEXT = /home/nathan/secure-exec-wasmgui/experiments/wasm-gui/toolchain/host-bin/xgettext
INTLTOOL_UPDATE = /home/nathan/secure-exec-wasmgui/experiments/wasm-gui/toolchain/host-bin/intltool-update
INTLTOOL_EXTRACT = /home/nathan/secure-exec-wasmgui/experiments/wasm-gui/toolchain/host-bin/intltool-extract
MSGMERGE = INTLTOOL_EXTRACT="$(INTLTOOL_EXTRACT)" XGETTEXT="$(XGETTEXT)" srcdir=$(srcdir) $(INTLTOOL_UPDATE) --gettext-package $(GETTEXT_PACKAGE) --dist
GENPOT   = INTLTOOL_EXTRACT="$(INTLTOOL_EXTRACT)" XGETTEXT="$(XGETTEXT)" srcdir=$(srcdir) $(INTLTOOL_UPDATE) --gettext-package $(GETTEXT_PACKAGE) --pot

ALL_LINGUAS = am ar ast be bg bn ca cs da de el en_AU en_GB eo es et eu fa_IR fi fr gl he hr hu hy hy_AM hye id ie is it ja kk ko lt lv ms nb nl nn oc pa pl pt pt_BR ro ru si sk sl sq sr sv te th tr ug uk ur ur_PK vi zh_CN zh_HK zh_TW

PO_LINGUAS=$(shell if test -r $(srcdir)/LINGUAS; then grep -v "^\#" $(srcdir)/LINGUAS; else echo "$(ALL_LINGUAS)"; fi)

USER_LINGUAS=$(shell if test -n "$(LINGUAS)"; then LLINGUAS="$(LINGUAS)"; ALINGUAS="$(ALL_LINGUAS)"; for lang in $$LLINGUAS; do if test -n "`grep \^$$lang$$ $(srcdir)/LINGUAS 2>/dev/null`" -o -n "`echo $$ALINGUAS|tr ' ' '\n'|grep \^$$lang$$`"; then printf "$$lang "; fi; done; fi)

USE_LINGUAS=$(shell if test -n "$(USER_LINGUAS)" -o -n "$(LINGUAS)"; then LLINGUAS="$(USER_LINGUAS)"; else if test -n "$(PO_LINGUAS)"; then LLINGUAS="$(PO_LINGUAS)"; else LLINGUAS="$(ALL_LINGUAS)"; fi; fi; for lang in $$LLINGUAS; do printf "$$lang "; done)

POFILES=$(shell LINGUAS="$(PO_LINGUAS)"; for lang in $$LINGUAS; do printf "$$lang.po "; done)

DISTFILES = Makefile.in.in POTFILES.in $(POFILES)
EXTRA_DISTFILES = ChangeLog POTFILES.skip Makevars LINGUAS

POTFILES = \
	../thunar/main.c \
	../thunar/thunar-abstract-dialog.c \
	../thunar/thunar-abstract-icon-view.c \
	../thunar/thunar-action-manager.c \
	../thunar/thunar-application.c \
	../thunar/thunar-browser.c \
	../thunar/thunar-chooser-button.c \
	../thunar/thunar-chooser-dialog.c \
	../thunar/thunar-chooser-model.c \
	../thunar/thunar-clipboard-manager.c \
	../thunar/thunar-column-editor.c \
	../thunar/thunar-column-model.c \
	../thunar/thunar-compact-view.c \
	../thunar/thunar-component.c \
	../thunar/thunar-dbus-service.c \
	../thunar/thunar-deep-count-job.c \
	../thunar/thunar-details-view.c \
	../thunar/thunar-device-monitor.c \
	../thunar/thunar-device.c \
	../thunar/thunar-dialogs.c \
	../thunar/thunar-dnd.c \
	../thunar/thunar-emblem-chooser.c \
	../thunar/thunar-enum-types.c \
	../thunar/thunar-file.c \
	../thunar/thunar-file-monitor.c \
	../thunar/thunar-folder.c \
	../thunar/thunar-gdk-extensions.c \
	../thunar/thunar-gio-extensions.c \
	../thunar/thunar-gobject-extensions.c \
	../thunar/thunar-gtk-extensions.c \
	../thunar/thunar-history.c \
	../thunar/thunar-ice.c \
	../thunar/thunar-icon-factory.c \
	../thunar/thunar-icon-renderer.c \
	../thunar/thunar-icon-view.c \
	../thunar/thunar-image.c \
	../thunar/thunar-io-jobs.c \
	../thunar/thunar-io-jobs-util.c \
	../thunar/thunar-io-scan-directory.c \
	../thunar/thunar-job.c \
	../thunar/thunar-job-operation.c \
	../thunar/thunar-job-operation-history.c \
	../thunar/thunar-list-model.c \
	../thunar/thunar-location-bar.c \
	../thunar/thunar-location-button.c \
	../thunar/thunar-location-buttons.c \
	../thunar/thunar-location-entry.c \
	../thunar/thunar-menu.c \
	../thunar/thunar-notify.c \
	../thunar/thunar-navigator.c \
	../thunar/thunar-pango-extensions.c \
	../thunar/thunar-path-entry.c \
	../thunar/thunar-permissions-chooser.c \
	../thunar/thunar-preferences.c \
	../thunar/thunar-preferences-dialog.c \
	../thunar/thunar-progress-dialog.c \
	../thunar/thunar-progress-view.c \
	../thunar/thunar-properties-dialog.c \
	../thunar/thunar-renamer-dialog.c \
	../thunar/thunar-renamer-model.c \
	../thunar/thunar-renamer-pair.c \
	../thunar/thunar-renamer-progress.c \
	../thunar/thunar-sendto-model.c \
	../thunar/thunar-session-client.c \
	../thunar/thunar-shortcuts-icon-renderer.c \
	../thunar/thunar-shortcuts-model.c \
	../thunar/thunar-shortcuts-pane.c \
	../thunar/thunar-shortcuts-view.c \
	../thunar/thunar-side-pane.c \
	../thunar/thunar-simple-job.c \
	../thunar/thunar-size-label.c \
	../thunar/thunar-standard-view.c \
	../thunar/thunar-statusbar.c \
	../thunar/thunar-toolbar-editor.c \
	../thunar/thunar-thumbnail-cache.c \
	../thunar/thunar-thumbnailer.c \
	../thunar/thunar-transfer-job.c \
	../thunar/thunar-tree-model.c \
	../thunar/thunar-tree-pane.c \
	../thunar/thunar-tree-view.c \
	../thunar/thunar-user.c \
	../thunar/thunar-util.c \
	../thunar/thunar-view.c \
	../thunar/thunar-window.c \
	../thunarx/thunarx-property-page.c \
	../thunarx/thunarx-provider-plugin.c \
	../thunarx/thunarx-renamer.c \
	../plugins/thunar-apr/thunar-apr-abstract-page.c \
	../plugins/thunar-apr/thunar-apr-desktop-page.c \
	../plugins/thunar-apr/thunar-apr-image-page.c \
	../plugins/thunar-apr/thunar-apr-plugin.c \
	../plugins/thunar-apr/thunar-apr-provider.c \
	../plugins/thunar-sbr/thunar-sbr-case-renamer.c \
	../plugins/thunar-sbr/thunar-sbr-date-renamer.c \
	../plugins/thunar-sbr/thunar-sbr-enum-types.c \
	../plugins/thunar-sbr/thunar-sbr-insert-renamer.c \
	../plugins/thunar-sbr/thunar-sbr-number-renamer.c \
	../plugins/thunar-sbr/thunar-sbr-plugin.c \
	../plugins/thunar-sbr/thunar-sbr-provider.c \
	../plugins/thunar-sbr/thunar-sbr-remove-renamer.c \
	../plugins/thunar-sbr/thunar-sbr-replace-renamer.c \
	../plugins/thunar-sendto-email/main.c \
	../plugins/thunar-sendto-email/thunar-sendto-email.desktop.in.in \
	../plugins/thunar-tpa/thunar-tpa.c \
	../plugins/thunar-tpa/thunar-tpa.desktop.in \
	../plugins/thunar-uca/thunar-uca-chooser.c \
	../plugins/thunar-uca/thunar-uca-context.c \
	../plugins/thunar-uca/thunar-uca-editor.c \
	../plugins/thunar-uca/thunar-uca-model.c \
	../plugins/thunar-uca/thunar-uca-plugin.c \
	../plugins/thunar-uca/thunar-uca-provider.c \
	../plugins/thunar-uca/uca.xml.in \
	../plugins/thunar-uca/thunar-uca-editor.ui \
	../plugins/thunar-uca/thunar-uca-chooser.ui \
	../plugins/thunar-wallpaper/twp-provider.c \
	../thunar.desktop.in.in \
	../thunar-bulk-rename.desktop.in.in \
	../thunar/thunar-settings.desktop.in \
	../org.xfce.thunar.appdata.xml.in \
	../org.xfce.thunar.policy.in.in

CATALOGS=$(shell LINGUAS="$(USE_LINGUAS)"; for lang in $$LINGUAS; do printf "$$lang.gmo "; done)

.SUFFIXES:
.SUFFIXES: .po .pox .gmo .mo .msg .cat

AM_DEFAULT_VERBOSITY = 0
INTLTOOL_V_MSGFMT = $(INTLTOOL__v_MSGFMT_$(V))
INTLTOOL__v_MSGFMT_= $(INTLTOOL__v_MSGFMT_$(AM_DEFAULT_VERBOSITY))
INTLTOOL__v_MSGFMT_0 = @echo "  MSGFMT" $@;

.po.pox:
	$(MAKE) $(GETTEXT_PACKAGE).pot
	$(MSGMERGE) $* $(GETTEXT_PACKAGE).pot -o $*.pox

.po.mo:
	$(INTLTOOL_V_MSGFMT)$(MSGFMT) -o $@ $<

.po.gmo:
	$(INTLTOOL_V_MSGFMT)file=`echo $* | sed 's,.*/,,'`.gmo \
	  && rm -f $$file && $(GMSGFMT) -o $$file $<

.po.cat:
	sed -f ../intl/po2msg.sed < $< > $*.msg \
	  && rm -f $@ && gencat $@ $*.msg


all: all-no

all-yes: $(CATALOGS)
all-no:

$(GETTEXT_PACKAGE).pot: $(POTFILES)
	$(GENPOT)

install: install-data
install-data: install-data-no
install-data-no: all
install-data-yes: all
	linguas="$(USE_LINGUAS)"; \
	for lang in $$linguas; do \
	  dir=$(DESTDIR)$(localedir)/$$lang/LC_MESSAGES; \
	  $(mkdir_p) $$dir; \
	  if test -r $$lang.gmo; then \
	    $(INSTALL_DATA) $$lang.gmo $$dir/$(GETTEXT_PACKAGE).mo; \
	    echo "installing $$lang.gmo as $$dir/$(GETTEXT_PACKAGE).mo"; \
	  else \
	    $(INSTALL_DATA) $(srcdir)/$$lang.gmo $$dir/$(GETTEXT_PACKAGE).mo; \
	    echo "installing $(srcdir)/$$lang.gmo as" \
		 "$$dir/$(GETTEXT_PACKAGE).mo"; \
	  fi; \
	  if test -r $$lang.gmo.m; then \
	    $(INSTALL_DATA) $$lang.gmo.m $$dir/$(GETTEXT_PACKAGE).mo.m; \
	    echo "installing $$lang.gmo.m as $$dir/$(GETTEXT_PACKAGE).mo.m"; \
	  else \
	    if test -r $(srcdir)/$$lang.gmo.m ; then \
	      $(INSTALL_DATA) $(srcdir)/$$lang.gmo.m \
		$$dir/$(GETTEXT_PACKAGE).mo.m; \
	      echo "installing $(srcdir)/$$lang.gmo.m as" \
		   "$$dir/$(GETTEXT_PACKAGE).mo.m"; \
	    else \
	      true; \
	    fi; \
	  fi; \
	done

# Empty stubs to satisfy archaic automake needs
dvi info ctags tags CTAGS TAGS ID:

# Define this as empty until I found a useful application.
install-exec installcheck:

uninstall:
	linguas="$(USE_LINGUAS)"; \
	for lang in $$linguas; do \
	  rm -f $(DESTDIR)$(localedir)/$$lang/LC_MESSAGES/$(GETTEXT_PACKAGE).mo; \
	  rm -f $(DESTDIR)$(localedir)/$$lang/LC_MESSAGES/$(GETTEXT_PACKAGE).mo.m; \
	done

check: all $(GETTEXT_PACKAGE).pot
	rm -f missing notexist
	srcdir=$(srcdir) $(INTLTOOL_UPDATE) -m
	if [ -r missing -o -r notexist ]; then \
	  exit 1; \
	fi

mostlyclean:
	rm -f *.pox $(GETTEXT_PACKAGE).pot *.old.po cat-id-tbl.tmp
	rm -f .intltool-merge-cache

clean: mostlyclean

distclean: clean
	rm -f Makefile Makefile.in POTFILES stamp-it
	rm -f *.mo *.msg *.cat *.cat.m *.gmo

maintainer-clean: distclean
	@echo "This command is intended for maintainers to use;"
	@echo "it deletes files that may require special tools to rebuild."
	rm -f Makefile.in.in

distdir = ../$(PACKAGE)-$(VERSION)/$(subdir)
dist distdir: $(DISTFILES)
	dists="$(DISTFILES)"; \
	extra_dists="$(EXTRA_DISTFILES)"; \
	for file in $$extra_dists; do \
	  test -f $(srcdir)/$$file && dists="$$dists $(srcdir)/$$file"; \
	done; \
	for file in $$dists; do \
	  test -f $$file || file="$(srcdir)/$$file"; \
	  ln $$file $(distdir) 2> /dev/null \
	    || cp -p $$file $(distdir); \
	done

update-po: Makefile
	$(MAKE) $(GETTEXT_PACKAGE).pot
	tmpdir=`pwd`; \
	linguas="$(USE_LINGUAS)"; \
	for lang in $$linguas; do \
	  echo "$$lang:"; \
	  result="`$(MSGMERGE) -o $$tmpdir/$$lang.new.po $$lang`"; \
	  if $$result; then \
	    if cmp $(srcdir)/$$lang.po $$tmpdir/$$lang.new.po >/dev/null 2>&1; then \
	      rm -f $$tmpdir/$$lang.new.po; \
            else \
	      if mv -f $$tmpdir/$$lang.new.po $$lang.po; then \
	        :; \
	      else \
	        echo "msgmerge for $$lang.po failed: cannot move $$tmpdir/$$lang.new.po to $$lang.po" 1>&2; \
	        rm -f $$tmpdir/$$lang.new.po; \
	        exit 1; \
	      fi; \
	    fi; \
	  else \
	    echo "msgmerge for $$lang.gmo failed!"; \
	    rm -f $$tmpdir/$$lang.new.po; \
	  fi; \
	done

Makefile POTFILES: stamp-it
	@if test ! -f $@; then \
	  rm -f stamp-it; \
	  $(MAKE) stamp-it; \
	fi

stamp-it: Makefile.in.in $(top_builddir)/config.status POTFILES.in
	cd $(top_builddir) \
	  && CONFIG_FILES=$(subdir)/Makefile.in CONFIG_HEADERS= CONFIG_LINKS= \
	       $(SHELL) ./config.status

# Tell versions [3.59,3.63) of GNU make not to export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
