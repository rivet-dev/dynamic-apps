[![License](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://gitlab.xfce.org/xfce/garcon/-/blob/master/COPYING)

# garcon


Garcon is an implementation of the [[https://specifications.freedesktop.org/menu-spec/latest/|freedesktop.org compliant menu]] specification
replacing the former Xfce menu library libxfce4menu. It is based on
GLib/GIO and aims at covering the entire specification except for
legacy menus. It was started as a complete rewrite of the former
Xfce menu library called libxfce4menu, which, in contrast to garcon,
was lacking menu merging features essential for loading menus modified
with menu editors.

----

### Homepage

[Garcon documentation](https://docs.xfce.org/xfce/garcon/start)

### Changelog

See [NEWS](https://gitlab.xfce.org/xfce/garcon/-/blob/master/NEWS) for details on changes and fixes made in the current release.

### Source Code Repository

[Garcon source code](https://gitlab.xfce.org/xfce/garcon)

### Download a Release Tarball

[Garcon archive](https://archive.xfce.org/src/xfce/garcon)
    or
[Garcon tags](https://gitlab.xfce.org/xfce/garcon/-/tags)

### Installation

From source: 

    % cd garcon
    % ./autogen.sh
    % make
    % make install

From release tarball:

    % tar xf garcon-<version>.tar.bz2
    % cd garcon-<version>
    % ./configure
    % make
    % make install

### Reporting Bugs

Visit the [reporting bugs](https://docs.xfce.org/xfce/garcon/bugs) page to view currently open bug reports and instructions on reporting new bugs or submitting bugfixes.

