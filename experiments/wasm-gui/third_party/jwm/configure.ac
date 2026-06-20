############################################################################
# JWM autoconf.
############################################################################

AC_INIT([jwm],[2.4.6],[joewing@joewing.net])
AC_PREREQ([2.69])
AC_CONFIG_SRCDIR([src])
AC_CONFIG_SRCDIR([po])
AC_CONFIG_HEADERS([config.h])
AC_LANG(C)
AM_INIT_AUTOMAKE

AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_MKDIR_P

PACKAGE=jwm

############################################################################
# Check if we need _XOPEN_SOURCE
############################################################################
AC_MSG_CHECKING([if _XOPEN_SOURCE should be defined])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#define _XOPEN_SOURCE 600L
#include <unistd.h>
]])], [use_xopen_source="yes"], [use_xopen_source="no"])
AC_MSG_RESULT([$use_xopen_source])

if test $use_xopen_source = "yes"; then

  AC_DEFINE(_XOPEN_SOURCE, 600L, [Define for single UNIX conformance])

  # Needed for IRIX 6.2 so that struct timeval is declared.
  AC_DEFINE(_XOPEN_SOURCE_EXTENDED, 1, [Define for timeval on IRIX 6.2])

  # Needed for Solaris 2.5.1 so that struct timeval is declared.
  AC_DEFINE(__EXTENSIONS__, 1, [Define for timeval on Solaris 2.5.1])

fi

############################################################################
# Check for X11
############################################################################
AC_PATH_X

if test ! "$no_x" = "yes" ; then
   if test ! x"$x_libraries" = x ; then
      LDFLAGS="$LDFLAGS -L$x_libraries"
   fi
   if test ! x"$x_includes" = x ; then
      CFLAGS="$CFLAGS -I$x_includes"
   fi
else
   AC_MSG_ERROR([Could not find X11])
fi

AC_CHECK_LIB([X11], XOpenDisplay,
   [ LDFLAGS="$LDFLAGS -lX11" ],
   [ AC_MSG_ERROR([libX11 not found]) ])

AC_CHECK_LIB([X11], Xutf8TextPropertyToTextList,
   [ AC_DEFINE(USE_XUTF8, 1, [Define to use Xutf8TextPropertyToTextList]) ],
   [ AC_MSG_WARN([Xutf8TextPropertyToTextList not found in libX11]) ])

############################################################################
# Check for necessary include files.
############################################################################
AC_CHECK_HEADERS([stdarg.h stdio.h stdlib.h ctype.h], [],
   [ AC_MSG_ERROR([one or more necessary header files not found]) ])

AC_CHECK_HEADERS([sys/select.h signal.h unistd.h time.h sys/wait.h sys/time.h])

AC_CHECK_HEADERS([langinfo.h iconv.h])

AC_CHECK_HEADERS([locale.h libintl.h])

AC_CHECK_HEADERS([X11/Xlib.h], [],
   [ AC_MSG_ERROR([Xlib.h could not be found]) ],
   [
#include <X11/X.h>
   ])

AC_CHECK_HEADERS([X11/Xutil.h X11/cursorfont.h X11/Xproto.h \
   X11/Xatom.h X11/keysym.h X11/Xresource.h], [], [],
   [
#include <X11/Xlib.h>
   ])

AC_CHECK_FUNCS([unsetenv putenv setlocale])
AC_FUNC_ALLOCA()

############################################################################
# Check for pkg-config.
############################################################################

AC_DEFUN([JWM_PKGCONFIG_EXISTS],[AC_PATH_TOOL(PKGCONFIG,pkg-config)])

AC_DEFUN([JWM_PKGCONFIG],
[
   AC_REQUIRE([JWM_PKGCONFIG_EXISTS])
   if test "x$PKGCONFIG" != "x" ; then
      AC_MSG_CHECKING([if pkg-config knows about $2])
      if `$PKGCONFIG $2` ; then
         $1="yes"
      else
         $1="no"
      fi
      AC_MSG_RESULT($$1)
   else
      $1="no"
   fi
])

JWM_PKGCONFIG([use_pkgconfig_png], [libpng])
JWM_PKGCONFIG([use_pkgconfig_jpeg], [libjpeg])
JWM_PKGCONFIG([use_pkgconfig_cairo], [cairo])
JWM_PKGCONFIG([use_pkgconfig_rsvg], [librsvg-2.0])
JWM_PKGCONFIG([use_pkgconfig_xft], [xft])
JWM_PKGCONFIG([use_pkgconfig_xrender], [xrender])
JWM_PKGCONFIG([use_pkgconfig_pango], [pangoxft])

############################################################################
# Check if confirm dialogs should be used.
############################################################################
AC_ARG_ENABLE(confirm,
   AS_HELP_STRING([--disable-confirm],[disable confirm dialogs]) )
if test "$enable_confirm" = "no" ; then
   AC_DEFINE(DISABLE_CONFIRM, 1, [Define to disable confirm dialogs])
else
   enable_confirm="yes"
fi

############################################################################
# Check if icon support was requested.
############################################################################
AC_ARG_ENABLE(icons,
   AS_HELP_STRING([--disable-icons],[disable icon support]) )
if test "$enable_icons" != "no" ; then
   enable_icons="yes"
   AC_DEFINE(USE_ICONS, 1, [Define to enable icon support] )
else
   enable_png="no"
   enable_xbm="no"
   enable_xpm="no"
   enable_rsvg="no"
   enable_cairo="no"
   enable_jpeg="no"
fi

############################################################################
# Check if PNG support was requested and available.
############################################################################
AC_ARG_ENABLE(png,
   AS_HELP_STRING([--disable-png],[disable PNG images]) )
if test "$enable_png" != "no" ; then
   if test "$use_pkgconfig_png" = "yes" ; then
      PNG_CFLAGS=`$PKGCONFIG --cflags libpng`
      PNG_LDFLAGS=`$PKGCONFIG --libs libpng`
      enable_png="yes"
   elif which libpng-config >/dev/null ; then
      PNG_CFLAGS=`libpng-config --cflags`
      PNG_LDFLAGS=`libpng-config --ldflags`
      enable_png="yes"
   else
      PNG_CFLAGS=""
      PNG_LDFLAGS="-lpng -lz -lm"
      AC_CHECK_LIB(png, png_read_image,
         [ enable_png="yes" ],
         [ enable_png="no"
           AC_MSG_WARN([unable to use libpng, PNG support disabled]) ],
         [ $PNG_LDFLAGS ])
   fi
   if test "$enable_png" = "yes" ; then
      CFLAGS="$CFLAGS $PNG_CFLAGS"
      LDFLAGS="$LDFLAGS $PNG_LDFLAGS"
      AC_DEFINE(USE_PNG, 1, [Define to use libpng])
   fi
fi

############################################################################
# Check if Cairo support was requested and available.
############################################################################
AC_ARG_ENABLE(cairo,
   AS_HELP_STRING([--disable-cairo],[disable Cairo support]) )
if test "$enable_cairo" != "no" ; then
   if test "$use_pkgconfig_cairo" = "yes" ; then
      CAIRO_CFLAGS=`$PKGCONFIG --cflags cairo`
      CAIRO_LDFLAGS=`$PKGCONFIG --libs cairo`
      enable_cairo="yes"
   elif which cairo-config >/dev/null ; then
      CAIRO_CFLAGS=`cairo-config --cflags`
      CAIRO_LDFLAGS=`cairo-config --libs`
      enable_cairo="yes"
   else
      CAIRO_LDFLAGS="-lcairo"
      AC_CHECK_LIB(cairo, cairo_create,
         [ enable_cairo="yes" ],
         [ enable_cairo="no"
           AC_MSG_WARN([unable to use Cairo, Cairo support disabled]) ],
         [ $CAIRO_LDFLAGS ])
   fi
fi
if test "$enable_cairo" = "no" ; then
   if test "$enable_rsvg" != "no" ; then
      AC_MSG_WARN([disabling rsvg because Cairo is disabled])
      enable_rsvg="no"
   fi
fi

############################################################################
# Check if librsvg support was requested and available.
############################################################################
AC_ARG_ENABLE(rsvg,
   AS_HELP_STRING([--disable-rsvg],[disable rsvg support]) )
if test "$enable_rsvg" != "no" ; then
   if test "$use_pkgconfig_rsvg" = "yes" ; then
      RSVG_CFLAGS=`$PKGCONFIG --cflags librsvg-2.0`
      RSVG_LDFLAGS=`$PKGCONFIG --libs librsvg-2.0`
      enable_rsvg="yes"
   else
      RSVG_LDFLAGS="-lrsvg-2"
      AC_CHECK_LIB(rsvg-2, rsvg_handle_render_cairo,
         [ enable_rsvg="yes" ],
         [ enable_rsvg="no"
           AC_MSG_WARN([unable to use rsvg, rsvg support disabled]) ],
         [ $RSVG_LDFLAGS ])
   fi
fi
if test "$enable_rsvg" = "no" ; then
   if test "$enable_cairo" != "no" ; then
      AC_MSG_WARN([disabling Cairo because rsvg is disabled])
      enable_cairo="no"
   fi
fi
if test "$enable_rsvg" = "yes" ; then
   LDFLAGS="$LDFLAGS $RSVG_LDFLAGS $CAIRO_LDFLAGS"
   CFLAGS="$CFLAGS $RSVG_CFLAGS $CAIRO_CFLAGS"
   AC_DEFINE(USE_CAIRO, 1, [Define to use Cairo])
   AC_DEFINE(USE_RSVG, 1, [Define to use rsvg])
fi

############################################################################
# Check if JPEG support was requested and available.
############################################################################
AC_ARG_ENABLE(jpeg,
   AS_HELP_STRING([--disable-jpeg],[disable JPEG images]) )
if test "$enable_jpeg" != "no" ; then
   if test "$use_pkgconfig_jpeg" = "yes" ; then
      JPEG_CFLAGS=`$PKGCONFIG --cflags libjpeg`
      JPEG_LDFLAGS=`$PKGCONFIG --libs libjpeg`
      enable_jpeg="yes"
   else
      JPEG_LDFLAGS="-ljpeg"
      AC_CHECK_HEADERS([jpeglib.h], [],
         [ enable_jpeg="no"
           AC_MSG_WARN([unable to include jpeglib.h, JPEG support disabled])
         ])
      if test "$enable_jpeg" != "no" ; then
         AC_CHECK_LIB(jpeg, jpeg_start_decompress,
            [ enable_jpeg="yes" ],
            [ enable_jpeg="no"
              AC_MSG_WARN([unable to use libjpeg, JPEG support disabled]) ],
            [ $JPEG_LDFLAGS ])
      fi
   fi
fi
if test "$enable_jpeg" = "yes" ; then
   LDFLAGS="$LDFLAGS $JPEG_LDFLAGS"
   CFLAGS="$CFLAGS $JPEG_CFLAGS"
   AC_DEFINE(USE_JPEG, 1, [Define to use libjpeg])
fi

############################################################################
# Check if XFT support was requested and available.
############################################################################
AC_ARG_ENABLE(xft, AS_HELP_STRING([--disable-xft],[disable Xft]) )
if test "$enable_xft" != "no"; then

   if test "$use_pkgconfig_xft" = "yes" ; then
      XFT_CFLAGS=`$PKGCONFIG --cflags xft`
      XFT_LDFLAGS=`$PKGCONFIG --libs xft`
   elif which xft-config >/dev/null ; then
      XFT_CFLAGS=`xft-config --cflags`
      XFT_LDFLAGS=`xft-config --libs`
   else
      XFT_LDFLAGS="-lXft"
   fi

fi
if test "$enable_xft" != "no" ; then
   AC_CHECK_LIB(Xft, XftFontOpenName,
      [ LDFLAGS="$LDFLAGS $XFT_LDFLAGS $FT2_LDFLAGS"
        CFLAGS="$CFLAGS $XFT_CFLAGS $FT2_CFLAGS"
        enable_xft="yes"
        AC_DEFINE(USE_XFT, 1, [Define to enable Xft]) ],
      [ enable_xft="no"
        AC_MSG_WARN([unable to use Xft]) ],
      [ $XFT_LDFLAGS $FT2_LDFLAGS ])
fi
if test "$enable_xft" != "no" ; then
   AC_CHECK_HEADER([ft2build.h], [], [], [
#include <X11/Xlib.h>
      ])
fi

############################################################################
# Check if support for the XRENDER extension was requested and available.
############################################################################
AC_ARG_ENABLE(xrender,
   AS_HELP_STRING([--disable-xrender],[disable XRender]) )
if test "$enable_xrender" != "no"; then

   if test "$use_pkgconfig_xrender" = "yes" ; then
      XRENDER_CFLAGS=`$PKGCONFIG --cflags xrender`
      XRENDER_LDFLAGS=`$PKGCONFIG --libs xrender`
   else
      XRENDER_LDFLAGS="-lXrender"
   fi

   AC_CHECK_HEADERS([X11/extensions/Xrender.h], [],
      [
         enable_xrender="no";
         AC_MSG_WARN([unable to use X11/extensions/Xrender.h])
      ], [
#include <X11/Xlib.h>
      ])

fi
if test "$enable_xrender" != "no" ; then
   AC_CHECK_LIB(Xrender, XRenderComposite,
      [ LDFLAGS="$LDFLAGS $XRENDER_LDFLAGS"
        CFLAGS="$CFLAGS $XRENDER_CFLAGS"
        enable_xrender="yes"
        AC_DEFINE(USE_XRENDER, 1, [Define to enable the XRender extension]) ],
      [ enable_xrender="no"
        AC_MSG_WARN([unable to use the XRender extension]) ],
      [ $XRENDER_LDFLAGS ])
fi

############################################################################
# Check if Pango support was requested and available.
############################################################################
AC_ARG_ENABLE(pango,
   AS_HELP_STRING([--disable-pango],[disable pango font support]) )
if test "$enable_xft" != "yes" ; then
   if test "$enable_pango" != "no" ; then
      AC_MSG_WARN(["xft required for pango support; disabling pango"])
      enable_pango="no"
   fi
fi
if test "$enable_pango" != "no" ; then
   if test "$use_pkgconfig_pango" = "yes" ; then
      PANGO_CFLAGS=`$PKGCONFIG --cflags pangoxft`
      PANGO_LDFLAGS=`$PKGCONFIG --libs pangoxft`
      enable_pango="yes"
   else
      PANGO_LDFLAGS="-lpango -lpango-xft"
      AC_CHECK_LIB(pango-xft, pango_xft_get_context,
         [ enable_pango="yes" ],
         [ enable_pango="no"
           AC_MSG_WARN([unable to use Pango]) ],
         [ $PANGO_LDFLAGS ])
   fi
fi
if test "$enable_pango" = "yes" ; then
   LDFLAGS="$LDFLAGS $PANGO_LDFLAGS"
   CFLAGS="$CFLAGS $PANGO_CFLAGS"
   AC_DEFINE(USE_PANGO, 1, [Define to use Pango])
fi

############################################################################
# Check if XPM support was requested and available.
############################################################################
AC_ARG_ENABLE(xpm,
   AS_HELP_STRING([--disable-xpm],[disable XPM images]) )
if test "$enable_xpm" != "no"; then

   # We need to use the XPM libraries in Motif-2.1 on IRIX.
   if test `uname` = "IRIX" ; then
      CFLAGS="$CFLAGS -I/usr/Motif-2.1/include"
      LDFLAGS="$LDFLAGS -L/usr/Motif-2.1/lib32"
   fi
   if test `uname` = "IRIX64" ; then
      CFLAGS="$CFLAGS -I/usr/Motif-2.1/include"
      LDFLAGS="$LDFLAGS -L/usr/Motif-2.1/lib64"
   fi

   AC_CHECK_HEADERS([X11/xpm.h], [],
      [ enable_xpm="no";
        AC_MSG_WARN([unable to use X11/xpm.h]) ],
      [
#include <X11/X.h>
      ])

fi
if test "$enable_xpm" != "no"; then
   AC_CHECK_DECL(XpmAllocColor, [],
      [ enable_xpm="no"
        AC_MSG_WARN([XPM library too old]) ],
      [
#include <X11/xpm.h>
])
fi
if test "$enable_xpm" != "no"; then
   AC_CHECK_LIB(Xpm, XpmReadFileToImage,
      [ LDFLAGS="$LDFLAGS -lXpm";
        enable_xpm="yes"
        AC_DEFINE(USE_XPM, 1, [Define to enable XPM support]) ],
      [ enable_xpm="no"
        AC_MSG_WARN([unable to use libXpm]) ])
fi

############################################################################
# Check if XBM support was requested and available.
############################################################################
AC_ARG_ENABLE(xbm,
    AS_HELP_STRING([--disable-xbm],[disable XBM images]) )
if test "$enable_xbm" != "no"; then
    AC_CHECK_LIB(X11,  XReadBitmapFileData,
        [ enable_xbm="yes"
          AC_DEFINE(USE_XBM, 1, [Define to enable XBM images]) ],
        [ enable_xbm="no"
          AC_MSG_WARN([unable to use XBM images]) ])
fi

############################################################################
# Check if support for the shape extension was requested and available.
############################################################################
AC_ARG_ENABLE(shape,
   AS_HELP_STRING([--disable-shape],[disable use of the X shape extension]) )
if test "$enable_shape" != "no"; then
   AC_CHECK_LIB(Xext, XShapeCombineRectangles,
      [ LDFLAGS="$LDFLAGS -lXext"
        enable_shape="yes"
        AC_DEFINE(USE_SHAPE, 1, [Define to enable the X shape extension]) ],
      [ enable_shape="no"
        AC_MSG_WARN([unable to use the X shape extension]) ])
fi

############################################################################
# Check if support for Xmu was requested and available.
# Note that Xmu appears to be broken on IRIX (drawing rounded rectangles
# larger than 800 pixels in either direction causes problems).
############################################################################
AC_ARG_ENABLE(xmu,
   AS_HELP_STRING([--disable-xmu],[disable Xmu support]),
   [],
   [  if test `uname` = "IRIX" -o `uname` = "IRIX64" ; then
         AC_MSG_WARN([disabling Xmu (it is broken on IRIX)])
         enable_xmu="no"
      fi
   ])
if test "$enable_xmu" != "no"; then
   AC_CHECK_LIB(Xmu, XmuDrawRoundedRectangle,
      [ LDFLAGS="$LDFLAGS -lXmu"
        enable_xmu="yes"
        AC_DEFINE(USE_XMU, 1, [Define to use Xmu]) ],
      [ enable_xmu="no"
        AC_MSG_WARN([unable to use Xmu]) ])
fi

############################################################################
# Check if support for Xinerama was requested and available.
############################################################################
AC_ARG_ENABLE(xinerama,
   AS_HELP_STRING([--disable-xinerama],[disable Xinerama support]) )
if test "$enable_xinerama" != "no"; then
   AC_CHECK_LIB(Xinerama, XineramaQueryExtension,
      [ LDFLAGS="$LDFLAGS -lXinerama"
        enable_xinerama="yes"
        AC_DEFINE(USE_XINERAMA, 1, [Define to enable Xinerama]) ],
      [ enable_xinerama="no"
        AC_MSG_WARN([unable to use Xinerama]) ])
fi

############################################################################
# Check if support for gettext was requested and available.
############################################################################
AM_ICONV
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.20])
LDFLAGS="$LDFLAGS $LIBINTL $LIBICONV"

############################################################################
# Check if debug mode was requested.
############################################################################
AC_ARG_ENABLE(debug,
   AS_HELP_STRING([--enable-debug],[use this to debug JWM]) )
if test "$enable_debug" = "yes"; then
   AC_DEFINE(DEBUG, 1, [Define to debug JWM])
   CFLAGS="$CFLAGS -Wall -g -DDEBUG"
   LDFLAGS="$LDFLAGS -g"
else
   enable_debug="no"
fi

############################################################################
# Create the output files.
############################################################################
if test "$prefix" = "NONE" ; then
   PREFIX="$ac_default_prefix"
   prefix="$ac_default_prefix"
else
   PREFIX="$prefix"
fi

if test "$exec_prefix" = "NONE" ; then
   exec_prefix="$PREFIX"
fi

if test "$sysconfdir" = "" ; then
   sysconfdir="$ac_default_sysconfdir"
fi

if test "$mandir" = "" ; then
   mandir="$ac_default_mandir"
fi

if test "$datadir" = "" ; then
    datadir="$ac_default_datadir"
fi

if test "$LOCALEDIR" = "" ; then
   localedir=`eval echo \""$datadir"/locale\"`
   localedir=`eval echo \""$localedir"\"`
   CFLAGS="$CFLAGS -DLOCALEDIR=\\\"$localedir\\\""
fi

BINDIR=`eval echo \""$bindir"\"`
SYSCONF=`eval echo \""$sysconfdir"\"`
MANDIR=`eval echo \""$mandir"\"`
MANDIR=`eval echo \""$MANDIR"\"`
DATADIR=`eval echo \""$datadir"\"`
DATADIR=`eval echo \""$DATADIR"\"`

AC_DEFINE_UNQUOTED(SYSTEM_CONFIG, "$SYSCONF/system.jwmrc",
   [default system configuration path])

AC_SUBST(CFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(VERSION, "$PACKAGE_VERSION")
AC_SUBST(INSTVERSION, `echo $PACKAGE_VERSION | tr -d .`)
AC_SUBST(BINDIR, "$BINDIR")
AC_SUBST(MANDIR, "$MANDIR")
AC_SUBST(DATADIR, "$DATADIR")
AC_SUBST(DATE, `date -u -d "@${SOURCE_DATE_EPOCH:-$(date +%s)}" +%Y-%m-%d`)
AC_SUBST(SYSCONF, "$SYSCONF")
AC_SUBST(PACKAGE, "$PACKAGE")

AC_CONFIG_FILES([po/Makefile.in
   Makefile
   src/Makefile
   contrib/Makefile
   jwm.1
])
AC_OUTPUT

############################################################################
# Display the status.
############################################################################

echo "Compiler:      $CC"
echo "Compile flags: $CFLAGS"
echo "Link flags:    $LDFLAGS"
echo
echo "Options"
echo
echo "    Confirm:  $enable_confirm"
echo "    Icon:     $enable_icons"
echo "    Cairo:    $enable_cairo"
echo "    RSVG:     $enable_rsvg"
echo "    PNG:      $enable_png"
echo "    JPEG:     $enable_jpeg"
echo "    XBM:      $enable_xbm"
echo "    XPM:      $enable_xpm"
echo "    XFT:      $enable_xft"
echo "    XRender:  $enable_xrender"
echo "    Pango:    $enable_pango"
echo "    Shape:    $enable_shape"
echo "    Xmu:      $enable_xmu"
echo "    Xinerama: $enable_xinerama"
echo "    Debug:    $enable_debug"
echo

