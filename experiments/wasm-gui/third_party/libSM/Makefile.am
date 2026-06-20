SUBDIRS=src doc

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = sm.pc

MAINTAINERCLEANFILES = ChangeLog INSTALL

.PHONY: ChangeLog INSTALL

INSTALL:
	$(INSTALL_CMD)

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog INSTALL

EXTRA_DIST = README.md
