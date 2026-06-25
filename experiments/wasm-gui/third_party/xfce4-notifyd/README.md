# Xfce Notify Daemon

The Xfce Notify Daemon (or `xfce4-notifyd`) is a small program that implements
the "server-side" portion of the Freedesktop desktop notifications
specification. Applications that wish to pop up a notification bubble in a
standard way can implicitly make use of xfce4-notifyd to do so by sending
standard messages over D-Bus using the `org.freedesktop.Notifications`
interface.

Apart from the notification server, a panel plugin is included which shows recent
notifications in a dropdown menu in the Xfce Panel.


## Dependencies

* gtk+ 3.22
* glib 2.68
* libxfce4util 4.12.0
* libxfce4ui 4.12.0
* xfconf 4.10
* libnotify 0.7
* libxfce4panel 4.12.0 (for the panel plugin)
* sqlite 3.34
* libcanberra 0.30 (optional; required for sound support)

Additionally, on X11, having a compositing manager running is
recommended. This is necessary for features like transparency and
animations.

Wayland support seems to work, but should be considered experimental,
and may not work with all Wayland compositors. Wayland support
additionally requires:

* gtk-layer-shell 0.7

Your Wayland compositor must support the layer-shell protocol.


## Installation

The usual:

```
# If from a git checkout:
./autogen.sh
# Or from a release tarball:
./configure

make
make install
```

should work just fine.  Pass `--prefix=/path/to/wherever` to install in a
location other than the default `/usr/local`.

X11 and Wayland support will be autodetected, but you can pass
`--enable-x11`, `--enable-wayland`, `--disable-x11`, and/or
`--disable-wayland` to require and/or explicitly disable support for
either.

In order for xfce4-notifyd to be started automatically, you must have a
`<servicedir>` directive in your D-Bus session configuration file.  If
you install xfce4-notifyd to a standard prefix (like `/usr`), you
shouldn't have to worry about this.

If you install xfce4-notifyd to a non-standard prefix, the D-Bus and
systemd service and unit files will be installed to the non-standard
prefix as well, in places where the respective daemons may not be able
to find them.  You can pass `--with-dbus-service-dir=` and
`--with-systemd-user-service-dir=` to `configure` in order to set the
appropriate directories.  If you want `configure` to automatically
figure out the correct places to put those files (which may be outside
your installation prefix), you can pass `auto` as the value to those two
command-line options.


## Configuration

Run `xfce4-notifyd-config` to display the settings dialog.

The panel plugin has a separate properties dialog, which shows all configuration
options for it.

### Hidden Settings

There is currently only one hidden setting (all others are configurable
via the settings dialog), which can be set using `xfconf-query` (on
channel `xfce4-notifyd`):

* `/compat/use-override-redirect-windows` (boolean): this defaults to
  `false`.  If your window manager displays notification windows in a
  strange way (gives it borders or a titlebar, doesn't allow it above
  fullscreen windows, etc.), you can try setting this to `true`.  Be
  aware, though, that notifications may end up being displayed above
  your screen saver / screen locker, which you might consider an
  unacceptable security risk.


## Theming

Xfce4-notifyd uses Gtk+'s standard theming system.  For examples, check
out the themes included with xfce4-notifyd.  Custom themes can be placed
in `$HOME/.themes/$THEME_NAME/xfce4-notify-4.0`, using a file called
`gtk.css`.  You can also override notification styles directly in a GTK3
theme using the `#XfceNotifyWindow` widget name.

If you have created a cool theme you can submit it by opening an issue
on [Xfce GitLab](https://gitlab.xfce.org/apps/xfce4-notifyd/-/issues).
For themes shipped with xfce4-notifyd, all parts are required to be
redistributable under the terms of a license compatible with GPLv2.
