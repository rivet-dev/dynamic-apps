[![License](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://gitlab.xfce.org/xfce/thunar/COPYING)

# thunar


Thunar is a modern file manager for the Xfce Desktop Environment. Thunar has been designed from the ground up to be fast and easy to use. Its user interface is clean and intuitive and does not include any confusing or useless options by default. Thunar starts up quickly and navigating through files and folders is fast and responsive.

----

### Homepage

[Thunar documentation](https://docs.xfce.org/xfce/thunar/start)

### Changelog

See [NEWS](https://gitlab.xfce.org/xfce/thunar/-/blob/master/NEWS) for details on changes and fixes made in the current release.

### Source Code Repository

[Thunar source code](https://gitlab.xfce.org/xfce/thunar)

### Download a Release Tarball

[Thunar archive](https://archive.xfce.org/src/xfce/thunar)
    or
[Thunar tags](https://gitlab.xfce.org/xfce/thunar/-/tags)

### Installation

From source: 

    % git clone https://gitlab.xfce.org/xfce/thunar
    % git checkout <branch|tag>  #optional step. Per default master is checked out
    % cd thunar
    % ./autogen.sh
    % make
    # make install

From release tarball:

    % tar xf thunar-<version>.tar.bz2
    % cd thunar-<version>
    % ./configure
    % make
    # make install

 Both autogen.sh and configure will list missing dependencies. 
 If your distribution provides development versions of the related packages, 
 install them. Otherwise you will need to build and install the missing dependencies from source.

For additional build & debug hints, check the [Thunar wiki pages](https://wiki.xfce.org/thunar/dev) and the [detailed building wiki manual](https://docs.xfce.org/xfce/building).

### Reporting Bugs

Visit the [reporting bugs](https://docs.xfce.org/xfce/thunar/bugs) page to view currently open bug reports and instructions on reporting new bugs or submitting bugfixes.

