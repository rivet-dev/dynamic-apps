/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* The normal alignment of `gboolean', in bytes. */
#define ALIGNOF_GBOOLEAN 4

/* The normal alignment of `gchar', in bytes. */
#define ALIGNOF_GCHAR 1

/* The normal alignment of `gdouble', in bytes. */
#define ALIGNOF_GDOUBLE 8

/* The normal alignment of `gfloat', in bytes. */
#define ALIGNOF_GFLOAT 4

/* The normal alignment of `gint16', in bytes. */
#define ALIGNOF_GINT16 2

/* The normal alignment of `gint32', in bytes. */
#define ALIGNOF_GINT32 4

/* The normal alignment of `gint64', in bytes. */
#define ALIGNOF_GINT64 8

/* The normal alignment of `gpointer', in bytes. */
#define ALIGNOF_GPOINTER 4

/* The normal alignment of `guchar', in bytes. */
#define ALIGNOF_GUCHAR 1

/* The normal alignment of `guint16', in bytes. */
#define ALIGNOF_GUINT16 2

/* The normal alignment of `guint32', in bytes. */
#define ALIGNOF_GUINT32 4

/* The normal alignment of `guint64', in bytes. */
#define ALIGNOF_GUINT64 8

/* Define if the perchannel-xml backend should be built */
#define BUILD_XFCONF_BACKEND_PERCHANNEL_XML 1

/* Define for debugging support */
/* #undef DEBUG */

/* Define for tracing support */
/* #undef DEBUG_TRACE */

/* Name of default gettext domain */
#define GETTEXT_PACKAGE "xfconf"

/* Prevent post 2_66 APIs */
#define GLIB_VERSION_MAX_ALLOWED GLIB_VERSION_2_66

/* Ignore post 2_66 APIs */
#define GLIB_VERSION_MIN_REQUIRED GLIB_VERSION_2_66

/* Use GLib structured logging */
#define G_LOG_USE_STRUCTURED 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the <errno.h> header file. */
#define HAVE_ERRNO_H 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the `fdatasync' function. */
#define HAVE_FDATASYNC 1

/* Define to 1 if you have the `fsync' function. */
#define HAVE_FSYNC 1

/* Define to 1 if you have the <grp.h> header file. */
#define HAVE_GRP_H 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the <locale.h> header file. */
#define HAVE_LOCALE_H 1

/* Define to 1 if you have the `setlocale' function. */
#define HAVE_SETLOCALE 1

/* Define to 1 if you have the <signal.h> header file. */
#define HAVE_SIGNAL_H 1

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

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/wait.h> header file. */
#define HAVE_SYS_WAIT_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* Name of package */
#define PACKAGE "xfconf"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "https://gitlab.xfce.org/xfce/xfconf"

/* Define to the full name of this package. */
#define PACKAGE_NAME "xfconf"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "xfconf 4.18.3"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "xfconf"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "4.18.3"

/* Define to 1 if all of the C90 standard headers exist (not just the ones
   required in a freestanding environment). This macro is provided for
   backward compatibility; new code need not use it. */
#define STDC_HEADERS 1

/* Version number of package */
#define VERSION "4.18.3"

/* Define if runtime checks should be performed */
/* #undef XFCONF_ENABLE_CHECKS */

/* Define if gprof profiling should be compiled in */
/* #undef XFCONF_ENABLE_PROFILING */

/* defines how to decorate public symbols while building */
#define XFCONF_EXPORT __attribute__((visibility("default")))

/* Name prefix for the Xfconf service */
#define XFCONF_SERVICE_NAME_PREFIX "org.xfce"

/* Path prefix for the Xfconf service */
#define XFCONF_SERVICE_PATH_PREFIX "/org/xfce"
