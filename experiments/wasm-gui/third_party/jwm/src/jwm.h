/**
 * @file jwm.h
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief The main JWM header file.
 *
 */

#ifndef JWM_H
#define JWM_H

#include "../config.h"

#ifndef MAKE_DEPEND

#  include <stdio.h>
#  include <stdlib.h>
#  include <string.h>
#  include <ctype.h>
#  include <limits.h>

   /* Ideally png.h would be included in image.c, which is the only
    * file that references it. Unfortunately, if setjmp.h is included
    * before png.h, png.h will complain about only including setjmp.h
    * once. The X headers apparently include setjmp.h, so I don't have
    * any control over the situation. Fortunately png.h can't complain
    * if it was included first. */
#  ifdef USE_PNG
#     include <png.h>
#  else
#     include <setjmp.h>
#  endif

#  ifdef HAVE_LOCALE_H
#     include <locale.h>
#  endif
#  ifdef HAVE_LIBINTL_H
#     include <libintl.h>
#  endif
#  ifdef HAVE_STDARG_H
#     include <stdarg.h>
#  endif
#  ifdef HAVE_SIGNAL_H
#     include <signal.h>
#  endif
#  ifdef HAVE_UNISTD_H
#     include <unistd.h>
#  endif
#  ifdef HAVE_TIME_H
#     include <time.h>
#  endif
#  ifdef HAVE_SYS_WAIT_H
#     include <sys/wait.h>
#  endif
#  ifdef HAVE_SYS_TIME_H
#     include <sys/time.h>
#  endif
#  ifdef HAVE_SYS_SELECT_H
#     include <sys/select.h>
#  endif

#  include <X11/Xlib.h>
#  ifdef HAVE_X11_XUTIL_H
#     include <X11/Xutil.h>
#  endif
#  ifdef HAVE_X11_XRESOURCE_H
#     include <X11/Xresource.h>
#  endif
#  ifdef HAVE_X11_CURSORFONT_H
#     include <X11/cursorfont.h>
#  endif
#  ifdef HAVE_X11_XPROTO_H
#     include <X11/Xproto.h>
#  endif
#  ifdef HAVE_X11_XATOM_H
#     include <X11/Xatom.h>
#  endif
#  ifdef HAVE_X11_KEYSYM_H
#     include <X11/keysym.h>
#  endif

#  ifdef USE_SHAPE
#     include <X11/extensions/shape.h>
#  endif

#  ifdef USE_XMU
#     include <X11/Xmu/Xmu.h>
#  endif

#  ifdef USE_XINERAMA
#     include <X11/extensions/Xinerama.h>
#  endif
#  ifdef USE_XFT
#     ifdef HAVE_FT2BUILD_H
#        include <ft2build.h>
#     endif
#     include <X11/Xft/Xft.h>
#  endif
#  ifdef USE_XRENDER
#     include <X11/extensions/Xrender.h>
#  endif
#  ifdef USE_FRIBIDI
#     include <fribidi/fribidi.h>
#  endif

#endif /* MAKE_DEPEND */

#ifndef _
#  ifdef HAVE_GETTEXT
#     define _ gettext
#  else
#     define _
#  endif
#endif

/** Maximum window size.
 * Making this larger will require code changes in some places where
 * there are fixed point calculations. */
#define MAX_WINDOW_WIDTH   (1 << 15)
#define MAX_WINDOW_HEIGHT  (1 << 15)

#define MAX_INCLUDE_DEPTH  16    /**< Max includes. */
#define MOVE_DELTA         3     /**< Pixels before trigging a move. */
#define RESTART_DELAY      1000  /**< Max timeout in ms before restarting. */
#define URGENCY_DELAY      500   /**< Flash timeout in ms for urgency. */
#define MENU_TIMEOUT_MS    5000  /**< Default menu pipe timeout. */
#define INCLUDE_TIMEOUT_MS 60000 /**< Default include pipe timeout. */

#define SHELL_NAME "/bin/sh"

#ifdef __GNUC__
#  if __GNUC__ >= 3
#     define JLIKELY(x)   __builtin_expect(!!(x), 1)
#     define JUNLIKELY(x) __builtin_expect(!!(x), 0)
#  else
#     warning "JLIKELY/JUNLIKELY not available with this version of gcc"
#     define JLIKELY(x) (x)
#     define JUNLIKELY(x) (x)
#  endif
#else
#  warning "JLIKELY/JUNLIKELY not available with this compiler"
#  define JLIKELY(x) (x)
#  define JUNLIKELY(x) (x)
#endif

#include "debug.h"
#include "jxlib.h"

#endif /* JWM_H */

