Thunar Simple Builtin Renamers (thunar-sbr)
===========================================

Thunar-sbr is an extension to Thunar, which includes a bunch of simple renamers for the bulk rename dialog:

 * Insert / Overwrite
 * Insert Date / Time
 * Numbering
 * Remove Characters
 * Search & Replace
 * Uppercase / Lowercase

By default the thunar-sbr extension will be installed, but as with every extension, it will slightly increase the resource usage of Thunar (this shouldn't be a real problem unless you're targeting an embedded system), and so you can pass `--disable-sbr-plugin` to configure and the plugin won't be built and installed. Since it is an extension, you can also easily uninstall it afterwards by removing the thunar-sbr.so file from the lib/thunarx-2/ directory of your installation (be sure to quit Thunar before removing files though).


Regular Expressions
-------------------

The "Search & Replace" renamer can be built with support for regular expressions using the Perl-compatible regular expression (PCRE) library version 6.0 or above. The reasons to use PCRE instead of the POSIX regular expressions, that are part of the system C library, are:

* PCRE supports UTF-8, which Thunar uses internally for all user visible texts (be sure to enable this when installing PCRE using the `--enable-utf8` configure switch).
* PCRE uses a more consistent syntax, which is easier to understand for most people.


