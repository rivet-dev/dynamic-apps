SUBDIRS = doc specs src

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ice.pc

MAINTAINERCLEANFILES = ChangeLog INSTALL

.PHONY: ChangeLog INSTALL

INSTALL:
	$(INSTALL_CMD)

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog INSTALL

if LINT
lint:
	(cd src && $(MAKE) $(MFLAGS) lint)
endif LINT

EXTRA_DIST = README.md
