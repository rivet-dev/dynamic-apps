# Copyright (c) 2007 Brian Tarricone <bjt23@cornell.edu>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License ONLY.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Library General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

.NOTPARALLEL:

LOG_COMPILER = $(top_srcdir)/tests/tests-driver.sh $(top_builddir)/xfconfd

TESTS = $(check_PROGRAMS)

AM_CFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/tests \
	$(GLIB_CFLAGS) \
	$(GIO_UNIX_CFLAGS)

LIBS = $(GIO_UNIX_LIBS) \
	$(top_builddir)/xfconf/libxfconf-$(LIBXFCONF_VERSION_API).la

