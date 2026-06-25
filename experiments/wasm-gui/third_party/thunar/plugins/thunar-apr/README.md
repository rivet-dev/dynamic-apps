Thunar Advanced Properties (thunar-apr)
=======================================

Thunar-apr is an extension to Thunar, which provides additional pages forthe file properties dialog, that are displayed for certain kinds of files:

 * Launcher/Link
 * Image

The Launcher/Link page is displayed for .desktop files and allows users to easily edit the most important values (i.e. the command for a launcher and the URL for a link).

The Image page is displayed for all kinds of image files supported by the GTK+ version used by Thunar. The page displays atleast the exact format of the image and the dimensions in pixels. If the plugin is build with support for libexif (you can explicitly disable this using the `--disable-exif` configure option) and the image file includes Exif tags, the most important Exif tags found in the image file will also be displayed.

By default the thunar-apr extension will be installed, but as with every extension, it will slightly increase the resource usage of Thunar (this shouldn't be a real problem unless you're targeting an embedded system), and so you can pass `--disable-apr-plugin` to configure and the plugin won't be built and installed. Since it is an extension, you can also easily uninstall it afterwards by removing the thunar-apr.so file from the lib/thunarx-2/ directory of your installation (be sure to quit Thunar before removing files though).