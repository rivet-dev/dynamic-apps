[![License](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://gitlab.xfce.org/apps/mousepad/-/blob/master/COPYING)

# mousepad

Mousepad is a simple text editor for the [Xfce](https://www.xfce.org) desktop environment.

Mousepad aims to be an easy-to-use and fast editor. Our target is an
editor for quickly editing text files, not a development environment or an
editor with a huge bunch of plugins. On the other hand we try to use the latest
GTK features available, which means that if GTK adds something new in a major
release that is useful for the editor, we will likely bump the GTK dependency
and integrate this new feature in Mousepad.

----

### Homepage

[Mousepad documentation](https://docs.xfce.org/apps/mousepad/start)

### Changelog

See [NEWS](https://gitlab.xfce.org/apps/mousepad/-/blob/master/NEWS) for details on changes and fixes made in the current release.


### Required Packages 

Mousepad depends on the following packages:

* [GLib](https://wiki.gnome.org/Projects/GLib) >= 2.52.0
* [GTK](https://www.gtk.org) >= 3.22.0
* [GtkSourceView](https://wiki.gnome.org/Projects/GtkSourceView) >= 3.24.0 or >= 4.0.0

### Plugins

Mousepad optionally depends on the following packages:

* [gspell](https://wiki.gnome.org/Projects/gspell) >= 1.6.0
* [libxfce4ui](https://gitlab.xfce.org/xfce/libxfce4ui) >= 4.17.5

### Source Code Repository

[Mousepad source code](https://gitlab.xfce.org/apps/mousepad)

### Download a Release Tarball

[Mousepad archive](https://archive.xfce.org/src/apps/mousepad)
    or
[Mousepad tags](https://gitlab.xfce.org/apps/mousepad/-/tags)

### Installation

From source code repository: 

    % cd mousepad
    % ./autogen.sh
    % make
    % make install

From release tarball:

    % tar xf mousepad-<version>.tar.bz2
    % cd mousepad-<version>
    % ./configure
    % make
    % make install

From [Flathub](https://flathub.org/apps/details/org.xfce.mousepad):

    % flatpak install flathub org.xfce.mousepad

### Reporting Bugs

Visit the [reporting bugs](https://docs.xfce.org/apps/mousepad/bugs) page to view currently open bug reports and instructions on reporting new bugs or submitting bugfixes.

