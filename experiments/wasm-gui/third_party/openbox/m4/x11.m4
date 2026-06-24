# X11_DEVEL()
#
# Check for the presence of the X Window System headers and libraries.
# Sets the CPPFLAGS and LIBS variables as appropriate.
AC_DEFUN([X11_DEVEL],
[
  AC_PATH_XTRA
  test "$no_x" = "yes" && \
    AC_MSG_ERROR([The X Window System could not be found.])

  # Store these
  OLDLIBS=$LIBS
  OLDCPPFLAGS=$CPPFLAGS

  CPPFLAGS="$CPPFLAGS $X_CFLAGS"
  X_LIBS="$X_PRE_LIBS $X_LIBS -lX11"
  LIBS="$LIBS $X_LIBS"

  # Check for required functions in -lX11
  AC_CHECK_LIB(
    [X11], [XOpenDisplay],
    ,
    AC_MSG_ERROR([Could not find XOpenDisplay in -lX11.])
  )

  # Restore the old values. Use X_CFLAGS and X_LIBS in
  # the Makefiles
  LIBS=$OLDLIBS
  CPPFLAGS=$OLDCPPFLAGS
])

# X11_EXT_XKB()
#
# Check for the presence of the "Xkb" X Window System extension.
# Defines "XKB" and sets the $(XKB) variable to "yes" if the extension is
# present.
AC_DEFUN([X11_EXT_XKB],
[
  AC_REQUIRE([X11_DEVEL])

  AC_ARG_ENABLE([xkb],
  AC_HELP_STRING(
  [--disable-xkb],
  [build without support for xkb extension [default=enabled]]),
  [USE=$enableval], [USE="yes"])

  if test "$USE" = "yes"; then
    # Store these
    OLDLIBS=$LIBS
    OLDCPPFLAGS=$CPPFLAGS

    CPPFLAGS="$CPPFLAGS $X_CFLAGS"
    LIBS="$LIBS $X_LIBS"

    AC_CHECK_LIB([X11], [XkbBell],
      AC_MSG_CHECKING([for X11/XKBlib.h])
      AC_TRY_LINK(
      [
        #include <X11/Xlib.h>
        #include <X11/Xutil.h>
        #include <X11/XKBlib.h>
      ],
      [
        Display *d;
        Window w;
        XkbBell(d, w, 0, 0);
      ],
      [
        AC_MSG_RESULT([yes])
        XKB="yes"
        AC_DEFINE([XKB], [1], [Found the XKB extension])

        XKB_CFLAGS=""
        XKB_LIBS=""
        AC_SUBST(XKB_CFLAGS)
        AC_SUBST(XKB_LIBS)
      ],
      [
        AC_MSG_RESULT([no])
        XKB="no"
      ])
    )

    LIBS=$OLDLIBS
    CPPFLAGS=$OLDCPPFLAGS
  fi

  AC_MSG_CHECKING([for the Xkb extension])
  if test "$XKB" = "yes"; then
    AC_MSG_RESULT([yes])
  else
    AC_MSG_RESULT([no])
  fi
])

# X11_EXT_XRANDR()
#
# Check for the presence of the "XRandR" X Window System extension.
# Defines "XRANDR" and sets the $(XRANDR) variable to "yes" if the extension is
# present.
AC_DEFUN([X11_EXT_XRANDR],
[
  AC_REQUIRE([X11_DEVEL])

  AC_ARG_ENABLE([xrandr],
  AC_HELP_STRING(
  [--disable-xrandr],
  [build without support for xrandr extension [default=enabled]]),
  [USE=$enableval], [USE="yes"])

  if test "$USE" = "yes"; then
    # Store these
    OLDLIBS=$LIBS
    OLDCPPFLAGS=$CPPFLAGS

    CPPFLAGS="$CPPFLAGS $X_CFLAGS"
    LIBS="$LIBS $X_LIBS -lXext -lXrender -lXrandr"

    AC_CHECK_LIB([Xrandr], [XRRSelectInput],
      AC_MSG_CHECKING([for X11/extensions/Xrandr.h])
      AC_TRY_LINK(
      [
        #include <X11/Xlib.h>
        #include <X11/extensions/Xrandr.h>
      ],
      [
        Display *d;
        Drawable r;
        int i;
        XRRQueryExtension(d, &i, &i);
        XRRGetScreenInfo(d, r);
      ],
      [
        AC_MSG_RESULT([yes])
        XRANDR="yes"
        AC_DEFINE([XRANDR], [1], [Found the XRandR extension])

        XRANDR_CFLAGS=""
        XRANDR_LIBS="-lXext -lXrender -lXrandr"
        AC_SUBST(XRANDR_CFLAGS)
        AC_SUBST(XRANDR_LIBS)
      ],
      [
        AC_MSG_RESULT([no])
        XRANDR="no"
      ])
    )

    LIBS=$OLDLIBS
    CPPFLAGS=$OLDCPPFLAGS
  fi

  AC_MSG_CHECKING([for the XRandR extension])
  if test "$XRANDR" = "yes"; then
    AC_MSG_RESULT([yes])
  else
    AC_MSG_RESULT([no])
  fi
])

# X11_EXT_SHAPE()
#
# Check for the presence of the "Shape" X Window System extension.
# Defines "SHAPE", sets the $(SHAPE) variable to "yes", and sets the $(LIBS)
# appropriately if the extension is present.
AC_DEFUN([X11_EXT_SHAPE],
[
  AC_REQUIRE([X11_DEVEL])

  AC_ARG_ENABLE([xshape],
  AC_HELP_STRING(
  [--disable-xshape],
  [build without support for xshape extension [default=enabled]]),
  [USE=$enableval], [USE="yes"])

  if test "$USE" = "yes"; then
    # Store these
    OLDLIBS=$LIBS
    OLDCPPFLAGS=$CPPFLAGS

    CPPFLAGS="$CPPFLAGS $X_CFLAGS"
    LIBS="$LIBS $X_LIBS"

    AC_CHECK_LIB([Xext], [XShapeCombineShape],
      AC_MSG_CHECKING([for X11/extensions/shape.h])
      AC_TRY_LINK(
      [
        #include <X11/Xlib.h>
        #include <X11/Xutil.h>
        #include <X11/extensions/shape.h>
      ],
      [
        long foo = ShapeSet;
      ],
      [
        AC_MSG_RESULT([yes])
        SHAPE="yes"
        AC_DEFINE([SHAPE], [1], [Found the XShape extension])

        XSHAPE_CFLAGS=""
        XSHAPE_LIBS="-lXext"
        AC_SUBST(XSHAPE_CFLAGS)
        AC_SUBST(XSHAPE_LIBS)
      ],
      [
        AC_MSG_RESULT([no])
        SHAPE="no"
      ])
    )

    LIBS=$OLDLIBS
    CPPFLAGS=$OLDCPPFLAGS
  fi

  AC_MSG_CHECKING([for the Shape extension])
  if test "$SHAPE" = "yes"; then
    AC_MSG_RESULT([yes])
  else
    AC_MSG_RESULT([no])
  fi
])


# X11_EXT_XINERAMA()
#
# Check for the presence of the "Xinerama" X Window System extension.
# Defines "XINERAMA", sets the $(XINERAMA) variable to "yes", and sets the
# $(LIBS) appropriately if the extension is present.
AC_DEFUN([X11_EXT_XINERAMA],
[
  AC_REQUIRE([X11_DEVEL])

  AC_ARG_ENABLE([xinerama],
  AC_HELP_STRING(
  [--disable-xinerama],
  [build without support for xinerama [default=enabled]]),
  [USE=$enableval], [USE="yes"])

  if test "$USE" = "yes"; then
    # Store these
    OLDLIBS=$LIBS
    OLDCPPFLAGS=$CPPFLAGS

    CPPFLAGS="$CPPFLAGS $X_CFLAGS"
    LIBS="$LIBS $X_LIBS -lXext"

    AC_CHECK_LIB([Xinerama], [XineramaQueryExtension],
    [
      AC_MSG_CHECKING([for X11/extensions/Xinerama.h])
      AC_TRY_LINK(
      [
        #include <X11/Xlib.h>
        #include <X11/extensions/Xinerama.h>
      ],
      [
        XineramaScreenInfo foo;
      ],
      [
        AC_MSG_RESULT([yes])
        XINERAMA="yes"
        AC_DEFINE([XINERAMA], [1], [Enable support of the Xinerama extension])
        XINERAMA_LIBS="-lXext -lXinerama"
        AC_SUBST(XINERAMA_LIBS)
      ],
      [
        AC_MSG_RESULT([no])
        XINERAMA="no"
      ])
    ])

    LIBS=$OLDLIBS
    CPPFLAGS=$OLDCPPFLAGS
  fi

  AC_MSG_CHECKING([for the Xinerama extension])
  if test "$XINERAMA" = "yes"; then
    AC_MSG_RESULT([yes])
  else
    AC_MSG_RESULT([no])
  fi
])

# X11_EXT_SYNC()
#
# Check for the presence of the "Sync" X Window System extension.
# Defines "SYNC", sets the $(SYNC) variable to "yes", and sets the $(LIBS)
# appropriately if the extension is present.
AC_DEFUN([X11_EXT_SYNC],
[
  AC_REQUIRE([X11_DEVEL])

  AC_ARG_ENABLE([xsync],
  AC_HELP_STRING(
  [--disable-xsync],
  [build without support for xsync extension [default=enabled]]),
  [USE=$enableval], [USE="yes"])

  if test "$USE" = "yes"; then
    # Store these
    OLDLIBS=$LIBS
    OLDCPPFLAGS=$CPPFLAGS

    CPPFLAGS="$CPPFLAGS $X_CFLAGS"
    LIBS="$LIBS $X_LIBS"

    AC_CHECK_LIB([Xext], [XSyncInitialize],
      AC_MSG_CHECKING([for X11/extensions/sync.h])
      AC_TRY_LINK(
      [
        #include <X11/Xlib.h>
        #include <X11/Xutil.h>
        #include <X11/extensions/sync.h>
      ],
      [
        XSyncValueType foo;
      ],
      [
        AC_MSG_RESULT([yes])
        SYNC="yes"
        AC_DEFINE([SYNC], [1], [Found the XSync extension])

        XSYNC_CFLAGS=""
        XSYNC_LIBS="-lXext"
        AC_SUBST(XSYNC_CFLAGS)
        AC_SUBST(XSYNC_LIBS)
      ],
      [
        AC_MSG_RESULT([no])
        SYNC="no"
      ])
    )

    LIBS=$OLDLIBS
    CPPFLAGS=$OLDCPPFLAGS
  fi

  AC_MSG_CHECKING([for the Sync extension])
  if test "$SYNC" = "yes"; then
    AC_MSG_RESULT([yes])
  else
    AC_MSG_RESULT([no])
  fi
])

# X11_EXT_AUTH()
#
# Check for the presence of the "Xau" X Window System extension.
# Defines "AUTH, sets the $(AUTH) variable to "yes", and sets the $(LIBS)
# appropriately if the extension is present.
AC_DEFUN([X11_EXT_AUTH],
[
  AC_REQUIRE([X11_DEVEL])

  # Store these
  OLDLIBS=$LIBS
  OLDCPPFLAGS=$CPPFLAGS

  CPPFLAGS="$CPPFLAGS $X_CFLAGS"
  LIBS="$LIBS $X_LIBS"

  AC_CHECK_LIB([Xau], [XauReadAuth],
    AC_MSG_CHECKING([for X11/Xauth.h])
    AC_TRY_LINK(
    [
      #include <X11/Xlib.h>
      #include <X11/Xutil.h>
      #include <X11/Xauth.h>
    ],
    [
    ],
    [
      AC_MSG_RESULT([yes])
      AUTH="yes"
      AC_DEFINE([AUTH], [1], [Found the Xauth extension])

      XAUTH_CFLAGS=""
      XAUTH_LIBS="-lXau"
      AC_SUBST(XAUTH_CFLAGS)
      AC_SUBST(XAUTH_LIBS)
    ],
    [
      AC_MSG_RESULT([no])
      AUTH="no"
    ])
  )

  LIBS=$OLDLIBS
  CPPFLAGS=$OLDCPPFLAGS

  AC_MSG_CHECKING([for the Xauth extension])
  if test "$AUTH" = "yes"; then
    AC_MSG_RESULT([yes])
  else
    AC_MSG_RESULT([no])
  fi
])

# X11_SM()
#
# Check for the presence of SMlib for session management.
# Defines "USE_SM" if SMlib is present.
AC_DEFUN([X11_SM],
[
  AC_REQUIRE([X11_DEVEL])

  AC_ARG_ENABLE([session-management],
  AC_HELP_STRING(
  [--disable-session-management],
  [build without support for session managers [default=enabled]]),
  [SM=$enableval], [SM="yes"])

  if test "$SM" = "yes"; then
    # Store these
    OLDLIBS=$LIBS
    OLDCPPFLAGS=$CPPFLAGS

    CPPFLAGS="$CPPFLAGS $X_CFLAGS"
    LIBS="$LIBS $X_LIBS"

    SM="no"

    AC_CHECK_LIB([SM], [SmcSaveYourselfDone], [
      AC_CHECK_HEADERS([X11/SM/SMlib.h], [
        SM_CFLAGS="$X_CFLAGS"
        SM_LIBS="-lSM -lICE"
        AC_DEFINE(USE_SM, 1, [Use session management])
        AC_SUBST(SM_CFLAGS)
        AC_SUBST(SM_LIBS)
        SM="yes"
      ])
    ])

    LIBS=$OLDLIBS
    CPPFLAGS=$OLDCPPFLAGS
  fi

  AC_MSG_CHECKING([for session management support])
  if test "$SM" = "yes"; then
    AC_MSG_RESULT([yes])
  else
    AC_MSG_RESULT([no])
  fi
])
