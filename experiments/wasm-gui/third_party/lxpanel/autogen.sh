#! /bin/sh
set -ex

test -d m4 || mkdir m4
autoreconf -i -f
intltoolize -c --automake --force
#xsltproc -o man/lxpanel.1 -nonet \
#    http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl man/lxpanel.xml
#xsltproc -o man/lxpanelctl.1 -nonet \
#    http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl man/lxpanelctl.xml

rm -rf autom4te.cache
