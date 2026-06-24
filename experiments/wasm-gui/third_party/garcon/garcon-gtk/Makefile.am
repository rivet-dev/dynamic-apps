# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
# 
# Copyright (c) 2013 Nick Schermer <nick@xfce.org>
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

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DGARCON_COMPILATION \
	-DGARCON_VERSION_API=\"$(GARCON_VERSION_API)\" \
	-DG_LOG_DOMAIN=\"garcon\" \
	-DSYSCONFIGDIR=\"$(sysconfdir)/xdg\" \
	$(PLATFORM_CFLAGS)

lib_LTLIBRARIES = \
	libgarcon-gtk3-1.la

libgarcon_gtk_headers = \
	garcon-gtk.h \
	garcon-gtk-menu.h

libgarcon_gtk_sources = \
	garcon-gtk-menu.c


libgarcon_gtk3includedir = \
	$(includedir)/garcon-gtk3-1/garcon-gtk

libgarcon_gtk3include_HEADERS = \
	$(libgarcon_gtk_headers)

libgarcon_gtk3_1_la_SOURCES = \
	$(libgarcon_gtk_sources) \
	$(libgarcon_gtk_headers)

libgarcon_gtk3_1_la_CFLAGS = \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK3_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GTHREAD_CFLAGS) \
	$(LIBXFCE4UI2_CFLAGS) \
	$(PLATFORM_CFLAGS)

libgarcon_gtk3_1_la_LDFLAGS = \
	-no-undefined \
	-export-dynamic \
	-version-info $(GARCON_VERINFO) \
	-export-symbols-regex "^[^_].*" \
	$(PLATFORM_LDFLAGS)

libgarcon_gtk3_1_la_LIBADD = \
	$(top_builddir)/garcon/libgarcon-$(GARCON_VERSION_API).la \
	$(GIO_LIBS) \
	$(GLIB_LIBS) \
	$(GTK3_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(GOBJECT_LIBS) \
	$(GTHREAD_LIBS) \
	$(LIBXFCE4UI2_LIBS)

libgarcon_gtk3_1_la_DEPENDENCIES = \
	$(top_builddir)/garcon/libgarcon-$(GARCON_VERSION_API).la


pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = garcon-gtk3-1.pc


# Required for gtk-doc and make distcheck
dist-hook: all

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ENV = CC="$(CC)"
INTROSPECTION_SCANNER_ARGS = \
	--add-include-path=$(srcdir) \
	--add-include-path=$(top_buildir)/garcon \
	--include-uninstalled=$(top_builddir)/garcon/Garcon-1.0.gir \
	--warn-all \
	-DGARCON_COMPILATION \
	--c-include=garcon-gtk/garcon-gtk.h
INTROSPECTION_COMPILER_ARGS = \
	--includedir=$(srcdir) \
	--includedir=. \
	--includedir=../garcon

if HAVE_INTROSPECTION
introspection_sources = $(libgarcon_gtk3_1_la_SOURCES)

GarconGtk-1.0.gir: libgarcon-gtk3-1.la \
	$(top_builddir)/garcon/Garcon-1.0.gir Makefile
GarconGtk_1_0_gir_INCLUDES = GObject-2.0 GLib-2.0 Gio-2.0 \
	Gtk-3.0 Libxfce4util-1.0 Libxfce4ui-2.0
GarconGtk_1_0_gir_CFLAGS = $(AM_CPPFLAGS)
GarconGtk_1_0_gir_LIBS = libgarcon-gtk3-1.la \
	$(top_builddir)/garcon/libgarcon-1.la
GarconGtk_1_0_gir_FILES = $(introspection_sources)
GarconGtk_1_0_gir_EXPORT_PACKAGES = garcon-gtk3-1
INTROSPECTION_GIRS += GarconGtk-1.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

DISTCLEANFILES = $(gir_DATA) $(typelib_DATA)
endif
