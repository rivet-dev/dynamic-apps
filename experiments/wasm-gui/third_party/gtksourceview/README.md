GtkSourceView
=============

GtkSourceView is a GNOME library that extends GtkTextView, the standard GTK+
widget for multiline text editing. GtkSourceView adds support for syntax
highlighting, undo/redo, file loading and saving, search and replace, a
completion system, printing, displaying line numbers, and other features
typical of a source code editor.

The GtkSourceView library is free software and is released under the terms of
the GNU Lesser General Public License, see the 'COPYING' file for more details.
The official web site is https://wiki.gnome.org/Projects/GtkSourceView.

Dependencies
------------

* GLib >= 2.48
* GTK+ >= 3.24
* libxml2 >= 2.6
* freebidi >= 0.19.7

Installation
------------

Simple install procedure from a tarball:
```
  $ mkdir build
  $ meson build
  $ cd build
  [ Become root if necessary ]
  $ ninja install
```

See the file 'INSTALL' for more detailed information.

To build the latest version of GtkSourceView plus its dependencies from Git,
[Jhbuild](https://wiki.gnome.org/Projects/Jhbuild) is recommended.


How to contribute
-----------------

See the 'HACKING' file.
