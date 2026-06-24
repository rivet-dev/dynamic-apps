# OB_DEBUG()
#
# Check if the user has requested a debug build.
# Sets the DEBUG or NDEBUG variables as appropriate
# Sets the CVS environment variable when building CVS sources.
AC_DEFUN([OB_DEBUG],
[
    AC_MSG_CHECKING([build type])

    AC_ARG_ENABLE([strict-ansi],
    AC_HELP_STRING([--enable-strict-ansi],[Enable strict ANSI compliance build [[default=no]]]),
    [STRICT=$enableval], [STRICT="no"])

    AC_ARG_ENABLE([super-warnings],
    AC_HELP_STRING([--enable-super-warnings],[Enable extra compiler warnings [[default=no]]]),
    [SUPERWARN=$enableval], [SUPERWARN="no"])

    AC_ARG_ENABLE([debug],
    AC_HELP_STRING([--enable-debug],[build a debug version [[default=no]]]),
    [DEBUG=$enableval], [DEBUG="no"])

    AC_ARG_ENABLE([gprof],
    AC_HELP_STRING([--enable-gprof],[Enable gprof profiling output [[default=no]]]),
    [PROF=$enableval], [PROF="no"])

    AC_ARG_ENABLE([gprof-libc],
    AC_HELP_STRING([--enable-gprof-libc],[Link against libc with profiling support [[default=no]]]),
    [PROFLC=$enableval], [PROFLC="no"])

    if test "$PROFLC" = "yes"; then
        PROF="yes" # always enable profiling then
    fi

    TEST=""
    test "${PACKAGE_VERSION%*alpha*}" != "$PACKAGE_VERSION" && TEST="yes"
    test "${PACKAGE_VERSION%*beta*}" != "$PACKAGE_VERSION" && TEST="yes"
    test "$TEST" = "yes" && DEBUG="yes"

    if test "$DEBUG" = "yes"; then
	MSG="DEBUG"
    else
	MSG="RELEASE"
    fi
    if test "$TEST" = "yes"; then
	MSG="$MSG (test release)"
    fi
    if test "$STRICT" = "yes"; then
	MSG="$MSG with strict ANSI compliance"
    fi
    if test "$SUPERWARN" = "yes"; then
	MSG="$MSG with super warnings"
    fi
    AC_MSG_RESULT([$MSG])
    
    test "$DEBUG" = "yes" && \
	AC_DEFINE([DEBUG], [1], [Creating a debug build])
])


# OB_COMPILER_FLAGS()
#
# Check what compiler is being used for compilation.
# It sets the CFLAGS variable appropriately for the compiler, including flags
# for debug builds.
AC_DEFUN([OB_COMPILER_FLAGS],
[
    AC_REQUIRE([AC_PROG_CPP])
    AC_REQUIRE([AC_PROG_CC])

    FLAGS=""
    L=""

    if test "$DEBUG" = "yes"; then
        FLAGS="-DDEBUG"
    else
        FLAGS="-DNDEBUG -DG_DISABLE_ASSERT"
    fi

    # Check what compiler we are using
    AC_MSG_CHECKING([for GNU CC])
    if test "$GCC" = "yes"; then
	AC_MSG_RESULT([yes])
	if test "$DEBUG" = "yes"; then
	    FLAGS="$FLAGS -O0 -ggdb -fno-inline -Wwrite-strings"
	    FLAGS="$FLAGS -Wall -Wsign-compare -Waggregate-return"
	    FLAGS="$FLAGS -Wbad-function-cast -Wpointer-arith"
	    FLAGS="$FLAGS -Wno-write-strings"
            # for Python.h
	    #FLAGS="$FLAGS -Wno-long-long"
	fi
	if test "$SUPERWARN" = "yes"; then
	    # glib can't handle -Wcast-qual
	    FLAGS="$FLAGS -Wcast-qual -Wextra"
	fi
	if test "$STRICT" = "yes"; then
	    FLAGS="$FLAGS -ansi -pedantic -D_XOPEN_SOURCE"
	fi
	if test "$PROF" = "yes"; then
	    FLAGS="$FLAGS -pg -fno-inline"
	fi
	if test "$PROFLC" = "yes"; then
	    L="$L -lc_p -lm_p"
	fi
	FLAGS="$FLAGS -fno-strict-aliasing"
    fi
    AC_MSG_CHECKING([for compiler specific flags])
    AC_MSG_RESULT([$FLAGS])
    CFLAGS="$CFLAGS $FLAGS"
    LIBS="$LIBS $L"
])

AC_DEFUN([OB_NLS],
[
    AC_ARG_ENABLE([nls],
    AC_HELP_STRING([--enable-nls],[Enable NLS translations [[default=yes]]]),
    [NLS=$enableval], [NLS="yes"])

    if test "$NLS" = yes; then
	DEFS="$DEFS -DENABLE_NLS"
    fi
])

