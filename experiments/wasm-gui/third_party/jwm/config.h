/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Define to one of `_getb67', `GETB67', `getb67' for Cray-2 and Cray-YMP
   systems. This function is required for `alloca.c' support on those systems.
   */
/* #undef CRAY_STACKSEG_END */

/* Define to 1 if using `alloca.c'. */
/* #undef C_ALLOCA */

/* Define to debug JWM */
/* #undef DEBUG */

/* Define to disable confirm dialogs */
#define DISABLE_CONFIRM 1

/* Define to 1 if translation of program messages to the user's native
   language is requested. */
/* #undef ENABLE_NLS */

/* Define to 1 if you have `alloca', as a function or macro. */
#define HAVE_ALLOCA 1

/* Define to 1 if you have <alloca.h> and it should be used (not on Ultrix).
   */
#define HAVE_ALLOCA_H 1

/* Define to 1 if you have the Mac OS X function
   CFLocaleCopyPreferredLanguages in the CoreFoundation framework. */
/* #undef HAVE_CFLOCALECOPYPREFERREDLANGUAGES */

/* Define to 1 if you have the Mac OS X function CFPreferencesCopyAppValue in
   the CoreFoundation framework. */
/* #undef HAVE_CFPREFERENCESCOPYAPPVALUE */

/* Define to 1 if you have the <ctype.h> header file. */
#define HAVE_CTYPE_H 1

/* Define if the GNU dcgettext() function is already present or preinstalled.
   */
/* #undef HAVE_DCGETTEXT */

/* Define if the GNU gettext() function is already present or preinstalled. */
/* #undef HAVE_GETTEXT */

/* Define if you have the iconv() function and it works. */
#define HAVE_ICONV 1

/* Define to 1 if you have the <iconv.h> header file. */
#define HAVE_ICONV_H 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the <jpeglib.h> header file. */
/* #undef HAVE_JPEGLIB_H */

/* Define to 1 if you have the <langinfo.h> header file. */
#define HAVE_LANGINFO_H 1

/* Define to 1 if you have the <libintl.h> header file. */
/* #undef HAVE_LIBINTL_H */

/* Define to 1 if you have the <locale.h> header file. */
#define HAVE_LOCALE_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `putenv' function. */
#define HAVE_PUTENV 1

/* Define to 1 if you have the `setlocale' function. */
#define HAVE_SETLOCALE 1

/* Define to 1 if you have the <signal.h> header file. */
#define HAVE_SIGNAL_H 1

/* Define to 1 if you have the <stdarg.h> header file. */
#define HAVE_STDARG_H 1

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

/* Define to 1 if you have the <sys/select.h> header file. */
#define HAVE_SYS_SELECT_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/wait.h> header file. */
#define HAVE_SYS_WAIT_H 1

/* Define to 1 if you have the <time.h> header file. */
#define HAVE_TIME_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the `unsetenv' function. */
#define HAVE_UNSETENV 1

/* Define to 1 if you have the <X11/cursorfont.h> header file. */
#define HAVE_X11_CURSORFONT_H 1

/* Define to 1 if you have the <X11/extensions/Xrender.h> header file. */
#define HAVE_X11_EXTENSIONS_XRENDER_H 1

/* Define to 1 if you have the <X11/keysym.h> header file. */
#define HAVE_X11_KEYSYM_H 1

/* Define to 1 if you have the <X11/Xatom.h> header file. */
#define HAVE_X11_XATOM_H 1

/* Define to 1 if you have the <X11/Xlib.h> header file. */
#define HAVE_X11_XLIB_H 1

/* Define to 1 if you have the <X11/xpm.h> header file. */
/* #undef HAVE_X11_XPM_H */

/* Define to 1 if you have the <X11/Xproto.h> header file. */
#define HAVE_X11_XPROTO_H 1

/* Define to 1 if you have the <X11/Xresource.h> header file. */
#define HAVE_X11_XRESOURCE_H 1

/* Define to 1 if you have the <X11/Xutil.h> header file. */
#define HAVE_X11_XUTIL_H 1

/* Define as const if the declaration of iconv() needs const. */
#define ICONV_CONST 

/* Name of package */
#define PACKAGE "jwm"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "joewing@joewing.net"

/* Define to the full name of this package. */
#define PACKAGE_NAME "jwm"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "jwm 2.4.6"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "jwm"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "2.4.6"

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at runtime.
	STACK_DIRECTION > 0 => grows toward higher addresses
	STACK_DIRECTION < 0 => grows toward lower addresses
	STACK_DIRECTION = 0 => direction of growth unknown */
/* #undef STACK_DIRECTION */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* default system configuration path */
#define SYSTEM_CONFIG "/home/nathan/secure-exec-wasmgui/experiments/wasm-gui/third_party/wasm-prefix/etc/system.jwmrc"

/* Define to use Cairo */
/* #undef USE_CAIRO */

/* Define to enable icon support */
#define USE_ICONS 1

/* Define to use libjpeg */
/* #undef USE_JPEG */

/* Define to use Pango */
/* #undef USE_PANGO */

/* Define to use libpng */
/* #undef USE_PNG */

/* Define to use rsvg */
/* #undef USE_RSVG */

/* Define to enable the X shape extension */
/* #undef USE_SHAPE */

/* Define to enable XBM images */
/* #undef USE_XBM */

/* Define to enable Xft */
#define USE_XFT 1

/* Define to enable Xinerama */
/* #undef USE_XINERAMA */

/* Define to use Xmu */
#define USE_XMU 1

/* Define to enable XPM support */
/* #undef USE_XPM */

/* Define to enable the XRender extension */
#define USE_XRENDER 1

/* Define to use Xutf8TextPropertyToTextList */
/* #undef USE_XUTF8 */

/* Version number of package */
#define VERSION "2.4.6"

/* Define for single UNIX conformance */
#define _XOPEN_SOURCE 600L

/* Define for timeval on IRIX 6.2 */
#define _XOPEN_SOURCE_EXTENDED 1

/* Define for timeval on Solaris 2.5.1 */
#define __EXTENSIONS__ 1

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */
