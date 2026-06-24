#!/bin/sh
#
# Copyright (c) 2004      The GLib Development Team.
# Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free
# Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301 USA
#

cpp -P -DINCLUDE_INTERNAL_SYMBOLS -DINCLUDE_VARIABLES -DALL_FILES ${srcdir:-.}/libxfce4util.symbols | sed -e '/^$/d' -e 's/ G_GNUC.*$//' -e 's/ PRIVATE//' | sort > expected-abi
nm -D .libs/libxfce4util.so | grep " [TRGD] " | cut -d ' ' -f 3 | grep -v '^_.*' | grep -v '^ *$' | sort > actual-abi
diff -u expected-abi actual-abi && rm expected-abi actual-abi
