/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Define for debugging support */
/* #undef DEBUG */

/* Define for tracing support */
/* #undef DEBUG_TRACE */

/* Define to 1 if translation of program messages to the user's native
   language is requested. */
/* #undef ENABLE_NLS */

/* Define if m4_default(the Wayland windowing system, wayland) is enabled */
/* #undef ENABLE_WAYLAND */

/* Define if m4_default(the X11 windowing system, x11) is enabled */
#define ENABLE_X11 1

/* Name of default gettext domain */
#define GETTEXT_PACKAGE "xfce4-screenshooter"

/* Prevent post 2_66 APIs */
#define GLIB_VERSION_MAX_ALLOWED GLIB_VERSION_2_66

/* Ignore post 2_66 APIs */
#define GLIB_VERSION_MIN_REQUIRED GLIB_VERSION_2_66

/* Use GLib structured logging */
#define G_LOG_USE_STRUCTURED 1

/* Define to 1 if you have the Mac OS X function CFLocaleCopyCurrent in the
   CoreFoundation framework. */
/* #undef HAVE_CFLOCALECOPYCURRENT */

/* Define to 1 if you have the Mac OS X function CFPreferencesCopyAppValue in
   the CoreFoundation framework. */
/* #undef HAVE_CFPREFERENCESCOPYAPPVALUE */

/* Define if the GNU dcgettext() function is already present or preinstalled.
   */
/* #undef HAVE_DCGETTEXT */

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define if gdk-x11-3.0 >= 3.24.0 present */
#define HAVE_GDKX11 1

/* Define if gdk-wayland-3.0 >= 3.24.0 present */
/* #undef HAVE_GDK_WAYLAND */

/* Define if the GNU gettext() function is already present or preinstalled. */
/* #undef HAVE_GETTEXT */

/* Define if you have the iconv() function and it works. */
/* #undef HAVE_ICONV */

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define if x11 >= 1.6.7 present */
#define HAVE_LIBX11 1

/* Define if xext >= 1.0.0 present */
#define HAVE_LIBXEXT 1

/* Define if xtst >= libxtst_min_version present */
#define HAVE_LIBXTST 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define if wayland-client >= 1.15.0 present */
/* #undef HAVE_WAYLAND_CLIENT */

/* Define if wayland-scanner >= 1.15.0 present */
/* #undef HAVE_WAYLAND_SCANNER */

/* Define if xfixes >= 4.0.0 present */
#define HAVE_XFIXES 1

/* Define if xi >= 1.7.8 present */
#define HAVE_XINPUT2 1

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* Define to 1 if your C compiler doesn't accept -c and -o together. */
/* #undef NO_MINUS_C_MINUS_O */

/* Name of package */
#define PACKAGE "xfce4-screenshooter"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "https://gitlab.xfce.org/apps/xfce4-screenshooter"

/* Define to the full name of this package. */
#define PACKAGE_NAME "xfce4-screenshooter"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "xfce4-screenshooter 1.11.1"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "xfce4-screenshooter"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.11.1"

/* Define to 1 if all of the C90 standard headers exist (not just the ones
   required in a freestanding environment). This macro is provided for
   backward compatibility; new code need not use it. */
#define STDC_HEADERS 1

/* Version number of package */
#define VERSION "1.11.1"
