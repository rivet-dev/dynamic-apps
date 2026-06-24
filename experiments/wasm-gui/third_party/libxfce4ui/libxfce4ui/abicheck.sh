#!/bin/sh
#
# Copyright (c) 2004 The GLib Development Team.
# Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>.
# Copyright (c) 2011 Guido Berhoerster <gber@opensuse.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License ONLY.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
#

trap 'rm expected-abi actual-abi' EXIT
${CPP:-cpp} -DINCLUDE_INTERNAL_SYMBOLS -DINCLUDE_VARIABLES -DALL_FILES ${srcdir:-.}/libxfce4ui.symbols | sed 's/ G_GNUC.*$//;s/ PRIVATE//;/^ *$/d;/^#/d' | sort >expected-abi
${NM:-nm} .libs/libxfce4ui-2.so* | awk '/ [DRTG] / {print $3}' | sort | uniq > actual-abi
diff -u expected-abi actual-abi
