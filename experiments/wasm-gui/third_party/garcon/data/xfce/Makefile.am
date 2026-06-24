# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
#
# Copyright (c) 2010 Xfce Development Team
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Library General Public License for more details.
#
# You should have received a copy of the GNU Library General
# Public License along with this library; if not, write to the
# Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301, USA.

menudir = $(sysconfdir)/xdg/menus
menu_DATA =								\
	xfce-applications.menu

directorydir = $(datadir)/desktop-directories
directory_in_files =							\
	xfce-accessories.directory.in					\
	xfce-development.directory.in					\
	xfce-education.directory.in					\
	xfce-games.directory.in						\
	xfce-graphics.directory.in					\
	xfce-hardware.directory.in					\
	xfce-multimedia.directory.in					\
	xfce-network.directory.in					\
	xfce-office.directory.in					\
	xfce-other.directory.in						\
	xfce-personal.directory.in					\
	xfce-science.directory.in					\
	xfce-screensavers.directory.in					\
	xfce-settings.directory.in					\
	xfce-system.directory.in

directory_DATA = $(directory_in_files:.directory.in=.directory)
@INTLTOOL_DIRECTORY_RULE@

EXTRA_DIST =								\
	$(directory_in_files)						\
	$(menu_DATA)

DISTCLEANFILES =							\
	$(directory_DATA)
