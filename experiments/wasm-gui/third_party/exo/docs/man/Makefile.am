# Other files to distribute
EXTRA_DIST = exo-open.1

# manual pages
man_MANS = exo-open.1

if MAINTAINER_MODE
%.1: %.xml
	xsltproc -nonet http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<
endif
