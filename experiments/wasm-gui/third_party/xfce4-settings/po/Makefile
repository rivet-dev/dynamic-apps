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

GETTEXT_PACKAGE = xfce4-settings
PACKAGE = xfce4-settings
VERSION = 4.18.4

SHELL = /bin/bash

srcdir = .
top_srcdir = ..
top_builddir = ..


prefix = /home/nathan/secure-exec-wasmgui/experiments/wasm-gui/third_party/wasm-prefix-threads
exec_prefix = ${prefix}
datadir = ${datarootdir}
datarootdir = ${prefix}/share
libdir = ${exec_prefix}/lib
localedir = ${prefix}/share/locale
subdir = po
install_sh = ${SHELL} /home/nathan/secure-exec-wasmgui/experiments/wasm-gui/third_party/xfce4-settings/install-sh
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

ALL_LINGUAS = am ar ast be be@tarask bg bn ca cs da de el en_AU en_CA en_GB es et eu fi fr gl he hr hu hy_AM hye id ie is it ja ka kab kk ko lt lv ms nb nl nn oc pa pl pt pt_BR ro ru si sk sl sq sr sv te th tr ug uk ur ur_PK zh_CN zh_HK zh_TW

PO_LINGUAS=$(shell if test -r $(srcdir)/LINGUAS; then grep -v "^\#" $(srcdir)/LINGUAS; else echo "$(ALL_LINGUAS)"; fi)

USER_LINGUAS=$(shell if test -n "$(LINGUAS)"; then LLINGUAS="$(LINGUAS)"; ALINGUAS="$(ALL_LINGUAS)"; for lang in $$LLINGUAS; do if test -n "`grep \^$$lang$$ $(srcdir)/LINGUAS 2>/dev/null`" -o -n "`echo $$ALINGUAS|tr ' ' '\n'|grep \^$$lang$$`"; then printf "$$lang "; fi; done; fi)

USE_LINGUAS=$(shell if test -n "$(USER_LINGUAS)" -o -n "$(LINGUAS)"; then LLINGUAS="$(USER_LINGUAS)"; else if test -n "$(PO_LINGUAS)"; then LLINGUAS="$(PO_LINGUAS)"; else LLINGUAS="$(ALL_LINGUAS)"; fi; fi; for lang in $$LLINGUAS; do printf "$$lang "; done)

POFILES=$(shell LINGUAS="$(PO_LINGUAS)"; for lang in $$LINGUAS; do printf "$$lang.po "; done)

DISTFILES = Makefile.in.in POTFILES.in $(POFILES)
EXTRA_DISTFILES = ChangeLog POTFILES.skip Makevars LINGUAS

POTFILES = \
	../common/display-name.c \
	../common/xfce-randr.c \
	../dialogs/accessibility-settings/accessibility-dialog.glade \
	../dialogs/accessibility-settings/main.c \
	../dialogs/accessibility-settings/xfce4-accessibility-settings.desktop.in \
	../dialogs/appearance-settings/appearance-dialog.glade \
	../dialogs/appearance-settings/main.c \
	../dialogs/appearance-settings/xfce-ui-settings.desktop.in \
	../dialogs/color-settings/color-device.c \
	../dialogs/color-settings/color-dialog.glade \
	../dialogs/color-settings/color-profile.c \
	../dialogs/color-settings/main.c \
	../dialogs/color-settings/xfce4-color-settings.desktop.in \
	../dialogs/display-settings/confirmation-dialog.glade \
	../dialogs/display-settings/display-dialog.glade \
	../dialogs/display-settings/minimal-display-dialog.glade \
	../dialogs/display-settings/profile-changed-dialog.glade \
	../dialogs/display-settings/main.c \
	../dialogs/display-settings/xfce-display-settings.desktop.in \
	../dialogs/keyboard-settings/command-dialog.c \
	../dialogs/keyboard-settings/keyboard-dialog.glade \
	../dialogs/keyboard-settings/main.c \
	../dialogs/keyboard-settings/xfce-keyboard-settings.c \
	../dialogs/keyboard-settings/xfce-keyboard-settings.desktop.in \
	../dialogs/mime-settings/main.c \
	../dialogs/mime-settings/xfce-mime-chooser.c \
	../dialogs/mime-settings/xfce-mime-helper-chooser.c \
	../dialogs/mime-settings/xfce-mime-helper-launcher-dialog.c \
	../dialogs/mime-settings/xfce-mime-helper-main.c \
	../dialogs/mime-settings/xfce-mime-helper.c \
	../dialogs/mime-settings/xfce-mime-window.c \
	../dialogs/mime-settings/xfce4-mime-settings.desktop.in \
	../dialogs/mime-settings/xfce4-file-manager.desktop.in \
	../dialogs/mime-settings/xfce4-mail-reader.desktop.in \
	../dialogs/mime-settings/xfce4-terminal-emulator.desktop.in \
	../dialogs/mime-settings/xfce4-web-browser.desktop.in \
	../dialogs/mime-settings/helpers/alacritty.desktop.in.in \
	../dialogs/mime-settings/helpers/aterm.desktop.in.in \
	../dialogs/mime-settings/helpers/balsa.desktop.in.in \
	../dialogs/mime-settings/helpers/brave.desktop.in.in \
	../dialogs/mime-settings/helpers/caja.desktop.in.in \
	../dialogs/mime-settings/helpers/chromium.desktop.in.in \
	../dialogs/mime-settings/helpers/debian-sensible-browser.desktop.in.in \
	../dialogs/mime-settings/helpers/debian-x-terminal-emulator.desktop.in.in \
	../dialogs/mime-settings/helpers/dillo.desktop.in.in \
	../dialogs/mime-settings/helpers/encompass.desktop.in.in \
	../dialogs/mime-settings/helpers/epiphany.desktop.in.in \
	../dialogs/mime-settings/helpers/eterm.desktop.in.in \
	../dialogs/mime-settings/helpers/evolution.desktop.in.in \
	../dialogs/mime-settings/helpers/firefox.desktop.in.in \
	../dialogs/mime-settings/helpers/galeon.desktop.in.in \
	../dialogs/mime-settings/helpers/geary.desktop.in.in \
	../dialogs/mime-settings/helpers/google-chrome.desktop.in.in \
	../dialogs/mime-settings/helpers/gnome-terminal.desktop.in.in \
	../dialogs/mime-settings/helpers/icecat.desktop.in.in \
	../dialogs/mime-settings/helpers/icedove.desktop.in.in \
	../dialogs/mime-settings/helpers/iceweasel.desktop.in.in \
	../dialogs/mime-settings/helpers/jumanji.desktop.in.in \
	../dialogs/mime-settings/helpers/kitty.desktop.in.in \
	../dialogs/mime-settings/helpers/kmail.desktop.in.in \
	../dialogs/mime-settings/helpers/konqueror.desktop.in.in \
	../dialogs/mime-settings/helpers/links.desktop.in.in \
	../dialogs/mime-settings/helpers/lynx.desktop.in.in \
	../dialogs/mime-settings/helpers/midori.desktop.in.in \
	../dialogs/mime-settings/helpers/mozilla-browser.desktop.in.in \
	../dialogs/mime-settings/helpers/mozilla-mailer.desktop.in.in \
	../dialogs/mime-settings/helpers/mutt.desktop.in.in \
	../dialogs/mime-settings/helpers/nautilus.desktop.in.in \
	../dialogs/mime-settings/helpers/netscape-navigator.desktop.in.in \
	../dialogs/mime-settings/helpers/nxterm.desktop.in.in \
	../dialogs/mime-settings/helpers/opera-browser.desktop.in.in \
	../dialogs/mime-settings/helpers/pcmanfm.desktop.in.in \
	../dialogs/mime-settings/helpers/pcmanfm-qt.desktop.in.in \
	../dialogs/mime-settings/helpers/qterminal.desktop.in.in \
	../dialogs/mime-settings/helpers/qtfm.desktop.in.in \
	../dialogs/mime-settings/helpers/qupzilla.desktop.in.in \
	../dialogs/mime-settings/helpers/rodent.desktop.in.in \
	../dialogs/mime-settings/helpers/rox-filer.desktop.in.in \
	../dialogs/mime-settings/helpers/sakura.desktop.in.in \
	../dialogs/mime-settings/helpers/surf.desktop.in.in \
	../dialogs/mime-settings/helpers/sylpheed.desktop.in.in \
	../dialogs/mime-settings/helpers/sylpheed-claws.desktop.in.in \
	../dialogs/mime-settings/helpers/terminator.desktop.in.in \
	../dialogs/mime-settings/helpers/thunar.desktop.in.in \
	../dialogs/mime-settings/helpers/thunderbird.desktop.in.in \
	../dialogs/mime-settings/helpers/urxvt.desktop.in.in \
	../dialogs/mime-settings/helpers/vimprobable2.desktop.in.in \
	../dialogs/mime-settings/helpers/w3m.desktop.in.in \
	../dialogs/mime-settings/helpers/xfce4-terminal.desktop.in.in \
	../dialogs/mime-settings/helpers/xfe.desktop.in.in \
	../dialogs/mime-settings/helpers/xterm.desktop.in.in \
	../dialogs/mouse-settings/main.c \
	../dialogs/mouse-settings/mouse-dialog.glade \
	../dialogs/mouse-settings/xfce-mouse-settings.desktop.in \
	../xfce4-settings-editor/main.c \
	../xfce4-settings-editor/xfce-settings-cell-renderer.c \
	../xfce4-settings-editor/xfce-settings-editor-box.c \
	../xfce4-settings-editor/xfce-settings-prop-dialog.c \
	../xfce4-settings-editor/xfce4-settings-editor.desktop.in \
	../xfce4-settings-manager/main.c \
	../xfce4-settings-manager/xfce-settings-manager-dialog.c \
	../xfce4-settings-manager/xfce-settings-manager.desktop.in \
	../xfsettingsd/accessibility.c \
	../xfsettingsd/keyboard-layout.c \
	../xfsettingsd/keyboard-shortcuts.c \
	../xfsettingsd/main.c \
	../xfsettingsd/workspaces.c \
	../xfsettingsd/xsettings.c \
	../xfsettingsd/xfsettingsd.desktop.in

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
