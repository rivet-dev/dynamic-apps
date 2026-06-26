# generated automatically by aclocal 1.16.5 -*- Autoconf -*-

# Copyright (C) 1996-2021 Free Software Foundation, Inc.

# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.

m4_ifndef([AC_CONFIG_MACRO_DIRS], [m4_defun([_AM_CONFIG_MACRO_DIRS], [])m4_defun([AC_CONFIG_MACRO_DIRS], [_AM_CONFIG_MACRO_DIRS($@)])])
m4_ifndef([AC_AUTOCONF_VERSION],
  [m4_copy([m4_PACKAGE_VERSION], [AC_AUTOCONF_VERSION])])dnl
m4_if(m4_defn([AC_AUTOCONF_VERSION]), [2.71],,
[m4_warning([this file was generated for autoconf 2.71.
You have another version of autoconf.  It may work, but is not guaranteed to.
If you have problems, you may need to regenerate the build system entirely.
To do so, use the procedure documented by the package, typically 'autoreconf'.])])

# Copyright (C) 2002-2021 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# AM_AUTOMAKE_VERSION(VERSION)
# ----------------------------
# Automake X.Y traces this macro to ensure aclocal.m4 has been
# generated from the m4 files accompanying Automake X.Y.
# (This private macro should not be called outside this file.)
AC_DEFUN([AM_AUTOMAKE_VERSION],
[am__api_version='1.16'
dnl Some users find AM_AUTOMAKE_VERSION and mistake it for a way to
dnl require some minimum version.  Point them to the right macro.
m4_if([$1], [1.16.5], [],
      [AC_FATAL([Do not call $0, use AM_INIT_AUTOMAKE([$1]).])])dnl
])

# _AM_AUTOCONF_VERSION(VERSION)
# -----------------------------
# aclocal traces this macro to find the Autoconf version.
# This is a private macro too.  Using m4_define simplifies
# the logic in aclocal, which can simply ignore this definition.
m4_define([_AM_AUTOCONF_VERSION], [])

# AM_SET_CURRENT_AUTOMAKE_VERSION
# -------------------------------
# Call AM_AUTOMAKE_VERSION and AM_AUTOMAKE_VERSION so they can be traced.
# This function is AC_REQUIREd by AM_INIT_AUTOMAKE.
AC_DEFUN([AM_SET_CURRENT_AUTOMAKE_VERSION],
[AM_AUTOMAKE_VERSION([1.16.5])dnl
m4_ifndef([AC_AUTOCONF_VERSION],
  [m4_copy([m4_PACKAGE_VERSION], [AC_AUTOCONF_VERSION])])dnl
_AM_AUTOCONF_VERSION(m4_defn([AC_AUTOCONF_VERSION]))])

# AM_AUX_DIR_EXPAND                                         -*- Autoconf -*-

# Copyright (C) 2001-2021 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# For projects using AC_CONFIG_AUX_DIR([foo]), Autoconf sets
# $ac_aux_dir to '$srcdir/foo'.  In other projects, it is set to
# '$srcdir', '$srcdir/..', or '$srcdir/../..'.
#
# Of course, Automake must honor this variable whenever it calls a
# tool from the auxiliary directory.  The problem is that $srcdir (and
# therefore $ac_aux_dir as well) can be either absolute or relative,
# depending on how configure is run.  This is pretty annoying, since
# it makes $ac_aux_dir quite unusable in subdirectories: in the top
# source directory, any form will work fine, but in subdirectories a
# relative path needs to be adjusted first.
#
# $ac_aux_dir/missing
#    fails when called from a subdirectory if $ac_aux_dir is relative
# $top_srcdir/$ac_aux_dir/missing
#    fails if $ac_aux_dir is absolute,
#    fails when called from a subdirectory in a VPATH build with
#          a relative $ac_aux_dir
#
# The reason of the latter failure is that $top_srcdir and $ac_aux_dir
# are both prefixed by $srcdir.  In an in-source build this is usually
# harmless because $srcdir is '.', but things will broke when you
# start a VPATH build or use an absolute $srcdir.
#
# So we could use something similar to $top_srcdir/$ac_aux_dir/missing,
# iff we strip the leading $srcdir from $ac_aux_dir.  That would be:
#   am_aux_dir='\$(top_srcdir)/'`expr "$ac_aux_dir" : "$srcdir//*\(.*\)"`
# and then we would define $MISSING as
#   MISSING="\${SHELL} $am_aux_dir/missing"
# This will work as long as MISSING is not called from configure, because
# unfortunately $(top_srcdir) has no meaning in configure.
# However there are other variables, like CC, which are often used in
# configure, and could therefore not use this "fixed" $ac_aux_dir.
#
# Another solution, used here, is to always expand $ac_aux_dir to an
# absolute PATH.  The drawback is that using absolute paths prevent a
# configured tree to be moved without reconfiguration.

AC_DEFUN([AM_AUX_DIR_EXPAND],
[AC_REQUIRE([AC_CONFIG_AUX_DIR_DEFAULT])dnl
# Expand $ac_aux_dir to an absolute path.
am_aux_dir=`cd "$ac_aux_dir" && pwd`
])

# AM_CONDITIONAL                                            -*- Autoconf -*-

# Copyright (C) 1997-2021 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# AM_CONDITIONAL(NAME, SHELL-CONDITION)
# -------------------------------------
# Define a conditional.
AC_DEFUN([AM_CONDITIONAL],
[AC_PREREQ([2.52])dnl
 m4_if([$1], [TRUE],  [AC_FATAL([$0: invalid condition: $1])],
       [$1], [FALSE], [AC_FATAL([$0: invalid condition: $1])])dnl
AC_SUBST([$1_TRUE])dnl
AC_SUBST([$1_FALSE])dnl
_AM_SUBST_NOTMAKE([$1_TRUE])dnl
_AM_SUBST_NOTMAKE([$1_FALSE])dnl
m4_define([_AM_COND_VALUE_$1], [$2])dnl
if $2; then
  $1_TRUE=
  $1_FALSE='#'
else
  $1_TRUE='#'
  $1_FALSE=
fi
AC_CONFIG_COMMANDS_PRE(
[if test -z "${$1_TRUE}" && test -z "${$1_FALSE}"; then
  AC_MSG_ERROR([[conditional "$1" was never defined.
Usually this means the macro was only invoked conditionally.]])
fi])])

# Copyright (C) 1999-2021 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.


# There are a few dirty hacks below to avoid letting 'AC_PROG_CC' be
# written in clear, in which case automake, when reading aclocal.m4,
# will think it sees a *use*, and therefore will trigger all it's
# C support machinery.  Also note that it means that autoscan, seeing
# CC etc. in the Makefile, will ask for an AC_PROG_CC use...


# _AM_DEPENDENCIES(NAME)
# ----------------------
# See how the compiler implements dependency checking.
# NAME is "CC", "CXX", "OBJC", "OBJCXX", "UPC", or "GJC".
# We try a few techniques and use that to set a single cache variable.
#
# We don't AC_REQUIRE the corresponding AC_PROG_CC since the latter was
# modified to invoke _AM_DEPENDENCIES(CC); we would have a circular
# dependency, and given that the user is not expected to run this macro,
# just rely on AC_PROG_CC.
AC_DEFUN([_AM_DEPENDENCIES],
[AC_REQUIRE([AM_SET_DEPDIR])dnl
AC_REQUIRE([AM_OUTPUT_DEPENDENCY_COMMANDS])dnl
AC_REQUIRE([AM_MAKE_INCLUDE])dnl
AC_REQUIRE([AM_DEP_TRACK])dnl

m4_if([$1], [CC],   [depcc="$CC"   am_compiler_list=],
      [$1], [CXX],  [depcc="$CXX"  am_compiler_list=],
      [$1], [OBJC], [depcc="$OBJC" am_compiler_list='gcc3 gcc'],
      [$1], [OBJCXX], [depcc="$OBJCXX" am_compiler_list='gcc3 gcc'],
      [$1], [UPC],  [depcc="$UPC"  am_compiler_list=],
      [$1], [GCJ],  [depcc="$GCJ"  am_compiler_list='gcc3 gcc'],
                    [depcc="$$1"   am_compiler_list=])

AC_CACHE_CHECK([dependency style of $depcc],
               [am_cv_$1_dependencies_compiler_type],
[if test -z "$AMDEP_TRUE" && test -f "$am_depcomp"; then
  # We make a subdir and do the tests there.  Otherwise we can end up
  # making bogus files that we don't know about and never remove.  For
  # instance it was reported that on HP-UX the gcc test will end up
  # making a dummy file named 'D' -- because '-MD' means "put the output
  # in D".
  rm -rf conftest.dir
  mkdir conftest.dir
  # Copy depcomp to subdir because otherwise we won't find it if we're
  # using a relative directory.
  cp "$am_depcomp" conftest.dir
  cd conftest.dir
  # We will build objects and dependencies in a subdirectory because
  # it helps to detect inapplicable dependency modes.  For instance
  # both Tru64's cc and ICC support -MD to output dependencies as a
  # side effect of compilation, but ICC will put the dependencies in
  # the current directory while Tru64 will put them in the object
  # directory.
  mkdir sub

  am_cv_$1_dependencies_compiler_type=none
  if test "$am_compiler_list" = ""; then
     am_compiler_list=`sed -n ['s/^#*\([a-zA-Z0-9]*\))$/\1/p'] < ./depcomp`
  fi
  am__universal=false
  m4_case([$1], [CC],
    [case " $depcc " in #(
     *\ -arch\ *\ -arch\ *) am__universal=true ;;
     esac],
    [CXX],
    [case " $depcc " in #(
     *\ -arch\ *\ -arch\ *) am__universal=true ;;
     esac])

  for depmode in $am_compiler_list; do
    # Setup a source with many dependencies, because some compilers
    # like to wrap large dependency lists on column 80 (with \), and
    # we should not choose a depcomp mode which is confused by this.
    #
    # We need to recreate these files for each test, as the compiler may
    # overwrite some of them when testing with obscure command lines.
    # This happens at least with the AIX C compiler.
    : > sub/conftest.c
    for i in 1 2 3 4 5 6; do
      echo '#include "conftst'$i'.h"' >> sub/conftest.c
      # Using ": > sub/conftst$i.h" creates only sub/conftst1.h with
      # Solaris 10 /bin/sh.
      echo '/* dummy */' > sub/conftst$i.h
    done
    echo "${am__include} ${am__quote}sub/conftest.Po${am__quote}" > confmf

    # We check with '-c' and '-o' for the sake of the "dashmstdout"
    # mode.  It turns out that the SunPro C++ compiler does not properly
    # handle '-M -o', and we need to detect this.  Also, some Intel
    # versions had trouble with output in subdirs.
    am__obj=sub/conftest.${OBJEXT-o}
    am__minus_obj="-o $am__obj"
    case $depmode in
    gcc)
      # This depmode causes a compiler race in universal mode.
      test "$am__universal" = false || continue
      ;;
    nosideeffect)
      # After this tag, mechanisms are not by side-effect, so they'll
      # only be used when explicitly requested.
      if test "x$enable_dependency_tracking" = xyes; then
	continue
      else
	break
      fi
      ;;
    msvc7 | msvc7msys | msvisualcpp | msvcmsys)
      # This compiler won't grok '-c -o', but also, the minuso test has
      # not run yet.  These depmodes are late enough in the game, and
      # so weak that their functioning should not be impacted.
      am__obj=conftest.${OBJEXT-o}
      am__minus_obj=
      ;;
    none) break ;;
    esac
    if depmode=$depmode \
       source=sub/conftest.c object=$am__obj \
       depfile=sub/conftest.Po tmpdepfile=sub/conftest.TPo \
       $SHELL ./depcomp $depcc -c $am__minus_obj sub/conftest.c \
         >/dev/null 2>conftest.err &&
       grep sub/conftst1.h sub/conftest.Po > /dev/null 2>&1 &&
       grep sub/conftst6.h sub/conftest.Po > /dev/null 2>&1 &&
       grep $am__obj sub/conftest.Po > /dev/null 2>&1 &&
       ${MAKE-make} -s -f confmf > /dev/null 2>&1; then
      # icc doesn't choke on unknown options, it will just issue warnings
      # or remarks (even with -Werror).  So we grep stderr for any message
      # that says an option was ignored or not supported.
      # When given -MP, icc 7.0 and 7.1 complain thusly:
      #   icc: Command line warning: ignoring option '-M'; no argument required
      # The diagnosis changed in icc 8.0:
      #   icc: Command line remark: option '-MP' not supported
      if (grep 'ignoring option' conftest.err ||
          grep 'not supported' conftest.err) >/dev/null 2>&1; then :; else
        am_cv_$1_dependencies_compiler_type=$depmode
        break
      fi
    fi
  done

  cd ..
  rm -rf conftest.dir
else
  am_cv_$1_dependencies_compiler_type=none
fi
])
AC_SUBST([$1DEPMODE], [depmode=$am_cv_$1_dependencies_compiler_type])
AM_CONDITIONAL([am__fastdep$1], [
  test "x$enable_dependency_tracking" != xno \
  && test "$am_cv_$1_dependencies_compiler_type" = gcc3])
])


# AM_SET_DEPDIR
# -------------
# Choose a directory name for dependency files.
# This macro is AC_REQUIREd in _AM_DEPENDENCIES.
AC_DEFUN([AM_SET_DEPDIR],
[AC_REQUIRE([AM_SET_LEADING_DOT])dnl
AC_SUBST([DEPDIR], ["${am__leading_dot}deps"])dnl
])


# AM_DEP_TRACK
# ------------
AC_DEFUN([AM_DEP_TRACK],
[AC_ARG_ENABLE([dependency-tracking], [dnl
AS_HELP_STRING(
  [--enable-dependency-tracking],
  [do not reject slow dependency extractors])
AS_HELP_STRING(
  [--disable-dependency-tracking],
  [speeds up one-time build])])
if test "x$enable_dependency_tracking" != xno; then
  am_depcomp="$ac_aux_dir/depcomp"
  AMDEPBACKSLASH='\'
  am__nodep='_no'
fi
AM_CONDITIONAL([AMDEP], [test "x$enable_dependency_tracking" != xno])
AC_SUBST([AMDEPBACKSLASH])dnl
_AM_SUBST_NOTMAKE([AMDEPBACKSLASH])dnl
AC_SUBST([am__nodep])dnl
_AM_SUBST_NOTMAKE([am__nodep])dnl
])

# Generate code to set up dependency tracking.              -*- Autoconf -*-

# Copyright (C) 1999-2021 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# _AM_OUTPUT_DEPENDENCY_COMMANDS
# ------------------------------
AC_DEFUN([_AM_OUTPUT_DEPENDENCY_COMMANDS],
[{
  # Older Autoconf quotes --file arguments for eval, but not when files
  # are listed without --file.  Let's play safe and only enable the eval
  # if we detect the quoting.
  # TODO: see whether this extra hack can be removed once we start
  # requiring Autoconf 2.70 or later.
  AS_CASE([$CONFIG_FILES],
          [*\'*], [eval set x "$CONFIG_FILES"],
          [*], [set x $CONFIG_FILES])
  shift
  # Used to flag and report bootstrapping failures.
  am_rc=0
  for am_mf
  do
    # Strip MF so we end up with the name of the file.
    am_mf=`AS_ECHO(["$am_mf"]) | sed -e 's/:.*$//'`
    # Check whether this is an Automake generated Makefile which includes
    # dependency-tracking related rules and includes.
    # Grep'ing the whole file directly is not great: AIX grep has a line
    # limit of 2048, but all sed's we know have understand at least 4000.
    sed -n 's,^am--depfiles:.*,X,p' "$am_mf" | grep X >/dev/null 2>&1 \
      || continue
    am_dirpart=`AS_DIRNAME(["$am_mf"])`
    am_filepart=`AS_BASENAME(["$am_mf"])`
    AM_RUN_LOG([cd "$am_dirpart" \
      && sed -e '/# am--include-marker/d' "$am_filepart" \
        | $MAKE -f - am--depfiles]) || am_rc=$?
  done
  if test $am_rc -ne 0; then
    AC_MSG_FAILURE([Something went wrong bootstrapping makefile fragments
    for automatic dependency tracking.  If GNU make was not used, consider
    re-running the configure script with MAKE="gmake" (or whatever is
    necessary).  You can also try re-running configure with the
    '--disable-dependency-tracking' option to at least be able to build
    the package (albeit without support for automatic dependency tracking).])
  fi
  AS_UNSET([am_dirpart])
  AS_UNSET([am_filepart])
  AS_UNSET([am_mf])
  AS_UNSET([am_rc])
  rm -f conftest-deps.mk
}
])# _AM_OUTPUT_DEPENDENCY_COMMANDS


# AM_OUTPUT_DEPENDENCY_COMMANDS
# -----------------------------
# This macro should only be invoked once -- use via AC_REQUIRE.
#
# This code is only required when automatic dependency tracking is enabled.
# This creates each '.Po' and '.Plo' makefile fragment that we'll need in
# order to bootstrap the dependency handling code.
AC_DEFUN([AM_OUTPUT_DEPENDENCY_COMMANDS],
[AC_CONFIG_COMMANDS([depfiles],
     [test x"$AMDEP_TRUE" != x"" || _AM_OUTPUT_DEPENDENCY_COMMANDS],
     [AMDEP_TRUE="$AMDEP_TRUE" MAKE="${MAKE-make}"])])

# Do all the work for Automake.                             -*- Autoconf -*-

# Copyright (C) 1996-2021 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This macro actually does too much.  Some checks are only needed if
# your package does certain things.  But this isn't really a big deal.

dnl Redefine AC_PROG_CC to automatically invoke _AM_PROG_CC_C_O.
m4_define([AC_PROG_CC],
m4_defn([AC_PROG_CC])
[_AM_PROG_CC_C_O
])

# AM_INIT_AUTOMAKE(PACKAGE, VERSION, [NO-DEFINE])
# AM_INIT_AUTOMAKE([OPTIONS])
# -----------------------------------------------
# The call with PACKAGE and VERSION arguments is the old style
# call (pre autoconf-2.50), which is being phased out.  PACKAGE
# and VERSION should now be passed to AC_INIT and removed from
# the call to AM_INIT_AUTOMAKE.
# We support both call styles for the transition.  After
# the next Automake release, Autoconf can make the AC_INIT
# arguments mandatory, and then we can depend on a new Autoconf
# release and drop the old call support.
AC_DEFUN([AM_INIT_AUTOMAKE],
[AC_PREREQ([2.65])dnl
m4_ifdef([_$0_ALREADY_INIT],
  [m4_fatal([$0 expanded multiple times
]m4_defn([_$0_ALREADY_INIT]))],
  [m4_define([_$0_ALREADY_INIT], m4_expansion_stack)])dnl
dnl Autoconf wants to disallow AM_ names.  We explicitly allow
dnl the ones we care about.
m4_pattern_allow([^AM_[A-Z]+FLAGS$])dnl
AC_REQUIRE([AM_SET_CURRENT_AUTOMAKE_VERSION])dnl
AC_REQUIRE([AC_PROG_INSTALL])dnl
if test "`cd $srcdir && pwd`" != "`pwd`"; then
  # Use -I$(srcdir) only when $(srcdir) != ., so that make's output
  # is not polluted with repeated "-I."
  AC_SUBST([am__isrc], [' -I$(srcdir)'])_AM_SUBST_NOTMAKE([am__isrc])dnl
  # test to see if srcdir already configured
  if test -f $srcdir/config.status; then
    AC_MSG_ERROR([source directory already configured; run "make distclean" there first])
  fi
fi

# test whether we have cygpath
if test -z "$CYGPATH_W"; then
  if (cygpath --version) >/dev/null 2>/dev/null; then
    CYGPATH_W='cygpath -w'
  else
    CYGPATH_W=echo
  fi
fi
AC_SUBST([CYGPATH_W])

# Define the identity of the package.
dnl Distinguish between old-style and new-style calls.
m4_ifval([$2],
[AC_DIAGNOSE([obsolete],
             [$0: two- and three-arguments forms are deprecated.])
m4_ifval([$3], [_AM_SET_OPTION([no-define])])dnl
 AC_SUBST([PACKAGE], [$1])dnl
 AC_SUBST([VERSION], [$2])],
[_AM_SET_OPTIONS([$1])dnl
dnl Diagnose old-style AC_INIT with new-style AM_AUTOMAKE_INIT.
m4_if(
  m4_ifset([AC_PACKAGE_NAME], [ok]):m4_ifset([AC_PACKAGE_VERSION], [ok]),
  [ok:ok],,
  [m4_fatal([AC_INIT should be called with package and version arguments])])dnl
 AC_SUBST([PACKAGE], ['AC_PACKAGE_TARNAME'])dnl
 AC_SUBST([VERSION], ['AC_PACKAGE_VERSION'])])dnl

_AM_IF_OPTION([no-define],,
[AC_DEFINE_UNQUOTED([PACKAGE], ["$PACKAGE"], [Name of package])
 AC_DEFINE_UNQUOTED([VERSION], ["$VERSION"], [Version number of package])])dnl

# Some tools Automake needs.
AC_REQUIRE([AM_SANITY_CHECK])dnl
AC_REQUIRE([AC_ARG_PROGRAM])dnl
AM_MISSING_PROG([ACLOCAL], [aclocal-${am__api_version}])
AM_MISSING_PROG([AUTOCONF], [autoconf])
AM_MISSING_PROG([AUTOMAKE], [automake-${am__api_version}])
AM_MISSING_PROG([AUTOHEADER], [autoheader])
AM_MISSING_PROG([MAKEINFO], [makeinfo])
AC_REQUIRE([AM_PROG_INSTALL_SH])dnl
AC_REQUIRE([AM_PROG_INSTALL_STRIP])dnl
AC_REQUIRE([AC_PROG_MKDIR_P])dnl
# For better backward compatibility.  To be removed once Automake 1.9.x
# dies out for good.  For more background, see:
# <https://lists.gnu.org/archive/html/automake/2012-07/msg00001.html>
# <https://lists.gnu.org/archive/html/automake/2012-07/msg00014.html>
AC_SUBST([mkdir_p], ['$(MKDIR_P)'])
# We need awk for the "check" target (and possibly the TAP driver).  The
# system "awk" is bad on some platforms.
AC_REQUIRE([AC_PROG_AWK])dnl
AC_REQUIRE([AC_PROG_MAKE_SET])dnl
AC_REQUIRE([AM_SET_LEADING_DOT])dnl
_AM_IF_OPTION([tar-ustar], [_AM_PROG_TAR([ustar])],
	      [_AM_IF_OPTION([tar-pax], [_AM_PROG_TAR([pax])],
			     [_AM_PROG_TAR([v7])])])
_AM_IF_OPTION([no-dependencies],,
[AC_PROVIDE_IFELSE([AC_PROG_CC],
		  [_AM_DEPENDENCIES([CC])],
		  [m4_define([AC_PROG_CC],
			     m4_defn([AC_PROG_CC])[_AM_DEPENDENCIES([CC])])])dnl
AC_PROVIDE_IFELSE([AC_PROG_CXX],
		  [_AM_DEPENDENCIES([CXX])],
		  [m4_define([AC_PROG_CXX],
			     m4_defn([AC_PROG_CXX])[_AM_DEPENDENCIES([CXX])])])dnl
AC_PROVIDE_IFELSE([AC_PROG_OBJC],
		  [_AM_DEPENDENCIES([OBJC])],
		  [m4_define([AC_PROG_OBJC],
			     m4_defn([AC_PROG_OBJC])[_AM_DEPENDENCIES([OBJC])])])dnl
AC_PROVIDE_IFELSE([AC_PROG_OBJCXX],
		  [_AM_DEPENDENCIES([OBJCXX])],
		  [m4_define([AC_PROG_OBJCXX],
			     m4_defn([AC_PROG_OBJCXX])[_AM_DEPENDENCIES([OBJCXX])])])dnl
])
# Variables for tags utilities; see am/tags.am
if test -z "$CTAGS"; then
  CTAGS=ctags
fi
AC_SUBST([CTAGS])
if test -z "$ETAGS"; then
  ETAGS=etags
fi
AC_SUBST([ETAGS])
if test -z "$CSCOPE"; then
  CSCOPE=cscope
fi
AC_SUBST([CSCOPE])

AC_REQUIRE([AM_SILENT_RULES])dnl
dnl The testsuite driver may need to know about EXEEXT, so add the
dnl 'am__EXEEXT' conditional if _AM_COMPILER_EXEEXT was seen.  This
dnl macro is hooked onto _AC_COMPILER_EXEEXT early, see below.
AC_CONFIG_COMMANDS_PRE(dnl
[m4_provide_if([_AM_COMPILER_EXEEXT],
  [AM_CONDITIONAL([am__EXEEXT], [test -n "$EXEEXT"])])])dnl

# POSIX will say in a future version that running "rm -f" with no argument
# is OK; and we want to be able to make that assumption in our Makefile
# recipes.  So use an aggressive probe to check that the usage we want is
# actually supported "in the wild" to an acceptable degree.
# See automake bug#10828.
# To make any issue more visible, cause the running configure to be aborted
# by default if the 'rm' program in use doesn't match our expectations; the
# user can still override this though.
if rm -f && rm -fr && rm -rf; then : OK; else
  cat >&2 <<'END'
Oops!

Your 'rm' program seems unable to run without file operands specified
on the command line, even when the '-f' option is present.  This is contrary
to the behaviour of most rm programs out there, and not conforming with
the upcoming POSIX standard: <http://austingroupbugs.net/view.php?id=542>

Please tell bug-automake@gnu.org about your system, including the value
of your $PATH and any error possibly output before this message.  This
can help us improve future automake versions.

END
  if test x"$ACCEPT_INFERIOR_RM_PROGRAM" = x"yes"; then
    echo 'Configuration will proceed anyway, since you have set the' >&2
    echo 'ACCEPT_INFERIOR_RM_PROGRAM variable to "yes"' >&2
    echo >&2
  else
    cat >&2 <<'END'
Aborting the configuration process, to ensure you take notice of the issue.

You can download and install GNU coreutils to get an 'rm' implementation
that behaves properly: <https://www.gnu.org/software/coreutils/>.

If you want to complete the configuration process using your problematic
'rm' anyway, export the environment variable ACCEPT_INFERIOR_RM_PROGRAM
to "yes", and re-run configure.

END
    AC_MSG_ERROR([Your 'rm' program is bad, sorry.])
  fi
fi
dnl The trailing newline in this macro's definition is deliberate, for
dnl backward compatibility and to allow trailing 'dnl'-style comments
dnl after the AM_INIT_AUTOMAKE invocation. See automake bug#16841.
])

dnl Hook into '_AC_COMPILER_EXEEXT' early to learn its expansion.  Do not
dnl add the conditional right here, as _AC_COMPILER_EXEEXT may be further
dnl mangled by Autoconf and run in a shell conditional statement.
m4_define([_AC_COMPILER_EXEEXT],
m4_defn([_AC_COMPILER_EXEEXT])[m4_provide([_AM_COMPILER_EXEEXT])])

# When config.status generates a header, we must update the stamp-h file.
# This file resides in the same directory as the config header
# that is generated.  The stamp files are numbered to have different names.

# Autoconf calls _AC_AM_CONFIG_HEADER_HOOK (when defined) in the
# loop where config.status creates the headers, so we can generate
# our stamp files there.
AC_DEFUN([_AC_AM_CONFIG_HEADER_HOOK],
[# Compute $1's index in $config_headers.
_am_arg=$1
_am_stamp_count=1
for _am_header in $config_headers :; do
  case $_am_header in
    $_am_arg | $_am_arg:* )
      break ;;
    * )
      _am_stamp_count=`expr $_am_stamp_count + 1` ;;
  esac
done
echo "timestamp for $_am_arg" >`AS_DIRNAME(["$_am_arg"])`/stamp-h[]$_am_stamp_count])

# Copyright (C) 2001-2021 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# AM_PROG_INSTALL_SH
# ------------------
# Define $install_sh.
AC_DEFUN([AM_PROG_INSTALL_SH],
[AC_REQUIRE([AM_AUX_DIR_EXPAND])dnl
if test x"${install_sh+set}" != xset; then
  case $am_aux_dir in
  *\ * | *\	*)
    install_sh="\${SHELL} '$am_aux_dir/install-sh'" ;;
  *)
    install_sh="\${SHELL} $am_aux_dir/install-sh"
  esac
fi
AC_SUBST([install_sh])])

# Copyright (C) 2003-2021 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# Check whether the underlying file-system supports filenames
# with a leading dot.  For instance MS-DOS doesn't.
AC_DEFUN([AM_SET_LEADING_DOT],
[rm -rf .tst 2>/dev/null
mkdir .tst 2>/dev/null
if test -d .tst; then
  am__leading_dot=.
else
  am__leading_dot=_
fi
rmdir .tst 2>/dev/null
AC_SUBST([am__leading_dot])])

# Add --enable-maintainer-mode option to configure.         -*- Autoconf -*-
# From Jim Meyering

# Copyright (C) 1996-2021 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# AM_MAINTAINER_MODE([DEFAULT-MODE])
# ----------------------------------
# Control maintainer-specific portions of Makefiles.
# Default is to disable them, unless 'enable' is passed literally.
# For symmetry, 'disable' may be passed as well.  Anyway, the user
# can override the default with the --enable/--disable switch.
AC_DEFUN([AM_MAINTAINER_MODE],
[m4_case(m4_default([$1], [disable]),
       [enable], [m4_define([am_maintainer_other], [disable])],
       [disable], [m4_define([am_maintainer_other], [enable])],
       [m4_define([am_maintainer_other], [enable])
        m4_warn([syntax], [unexpected argument to AM@&t@_MAINTAINER_MODE: $1])])
AC_MSG_CHECKING([whether to enable maintainer-specific portions of Makefiles])
  dnl maintainer-mode's default is 'disable' unless 'enable' is passed
  AC_ARG_ENABLE([maintainer-mode],
    [AS_HELP_STRING([--]am_maintainer_other[-maintainer-mode],
      am_maintainer_other[ make rules and dependencies not useful
      (and sometimes confusing) to the casual installer])],
    [USE_MAINTAINER_MODE=$enableval],
    [USE_MAINTAINER_MODE=]m4_if(am_maintainer_other, [enable], [no], [yes]))
  AC_MSG_RESULT([$USE_MAINTAINER_MODE])
  AM_CONDITIONAL([MAINTAINER_MODE], [test $USE_MAINTAINER_MODE = yes])
  MAINT=$MAINTAINER_MODE_TRUE
  AC_SUBST([MAINT])dnl
]
)

# Check to see how 'make' treats includes.	            -*- Autoconf -*-

# Copyright (C) 2001-2021 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# AM_MAKE_INCLUDE()
# -----------------
# Check whether make has an 'include' directive that can support all
# the idioms we need for our automatic dependency tracking code.
AC_DEFUN([AM_MAKE_INCLUDE],
[AC_MSG_CHECKING([whether ${MAKE-make} supports the include directive])
cat > confinc.mk << 'END'
am__doit:
	@echo this is the am__doit target >confinc.out
.PHONY: am__doit
END
am__include="#"
am__quote=
# BSD make does it like this.
echo '.include "confinc.mk" # ignored' > confmf.BSD
# Other make implementations (GNU, Solaris 10, AIX) do it like this.
echo 'include confinc.mk # ignored' > confmf.GNU
_am_result=no
for s in GNU BSD; do
  AM_RUN_LOG([${MAKE-make} -f confmf.$s && cat confinc.out])
  AS_CASE([$?:`cat confinc.out 2>/dev/null`],
      ['0:this is the am__doit target'],
      [AS_CASE([$s],
          [BSD], [am__include='.include' am__quote='"'],
          [am__include='include' am__quote=''])])
  if test "$am__include" != "#"; then
    _am_result="yes ($s style)"
    break
  fi
done
rm -f confinc.* confmf.*
AC_MSG_RESULT([${_am_result}])
AC_SUBST([am__include])])
AC_SUBST([am__quote])])

# Fake the existence of programs that GNU maintainers use.  -*- Autoconf -*-

# Copyright (C) 1997-2021 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# AM_MISSING_PROG(NAME, PROGRAM)
# ------------------------------
AC_DEFUN([AM_MISSING_PROG],
[AC_REQUIRE([AM_MISSING_HAS_RUN])
$1=${$1-"${am_missing_run}$2"}
AC_SUBST($1)])

# AM_MISSING_HAS_RUN
# ------------------
# Define MISSING if not defined so far and test if it is modern enough.
# If it is, set am_missing_run to use it, otherwise, to nothing.
AC_DEFUN([AM_MISSING_HAS_RUN],
[AC_REQUIRE([AM_AUX_DIR_EXPAND])dnl
AC_REQUIRE_AUX_FILE([missing])dnl
if test x"${MISSING+set}" != xset; then
  MISSING="\${SHELL} '$am_aux_dir/missing'"
fi
# Use eval to expand $SHELL
if eval "$MISSING --is-lightweight"; then
  am_missing_run="$MISSING "
else
  am_missing_run=
  AC_MSG_WARN(['missing' script is too old or missing])
fi
])

# Helper functions for option handling.                     -*- Autoconf -*-

# Copyright (C) 2001-2021 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# _AM_MANGLE_OPTION(NAME)
# -----------------------
AC_DEFUN([_AM_MANGLE_OPTION],
[[_AM_OPTION_]m4_bpatsubst($1, [[^a-zA-Z0-9_]], [_])])

# _AM_SET_OPTION(NAME)
# --------------------
# Set option NAME.  Presently that only means defining a flag for this option.
AC_DEFUN([_AM_SET_OPTION],
[m4_define(_AM_MANGLE_OPTION([$1]), [1])])

# _AM_SET_OPTIONS(OPTIONS)
# ------------------------
# OPTIONS is a space-separated list of Automake options.
AC_DEFUN([_AM_SET_OPTIONS],
[m4_foreach_w([_AM_Option], [$1], [_AM_SET_OPTION(_AM_Option)])])

# _AM_IF_OPTION(OPTION, IF-SET, [IF-NOT-SET])
# -------------------------------------------
# Execute IF-SET if OPTION is set, IF-NOT-SET otherwise.
AC_DEFUN([_AM_IF_OPTION],
[m4_ifset(_AM_MANGLE_OPTION([$1]), [$2], [$3])])

# Copyright (C) 1999-2021 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# _AM_PROG_CC_C_O
# ---------------
# Like AC_PROG_CC_C_O, but changed for automake.  We rewrite AC_PROG_CC
# to automatically call this.
AC_DEFUN([_AM_PROG_CC_C_O],
[AC_REQUIRE([AM_AUX_DIR_EXPAND])dnl
AC_REQUIRE_AUX_FILE([compile])dnl
AC_LANG_PUSH([C])dnl
AC_CACHE_CHECK(
  [whether $CC understands -c and -o together],
  [am_cv_prog_cc_c_o],
  [AC_LANG_CONFTEST([AC_LANG_PROGRAM([])])
  # Make sure it works both with $CC and with simple cc.
  # Following AC_PROG_CC_C_O, we do the test twice because some
  # compilers refuse to overwrite an existing .o file with -o,
  # though they will create one.
  am_cv_prog_cc_c_o=yes
  for am_i in 1 2; do
    if AM_RUN_LOG([$CC -c conftest.$ac_ext -o conftest2.$ac_objext]) \
         && test -f conftest2.$ac_objext; then
      : OK
    else
      am_cv_prog_cc_c_o=no
      break
    fi
  done
  rm -f core conftest*
  unset am_i])
if test "$am_cv_prog_cc_c_o" != yes; then
   # Losing compiler, so override with the script.
   # FIXME: It is wrong to rewrite CC.
   # But if we don't then we get into trouble of one sort or another.
   # A longer-term fix would be to have automake use am__CC in this case,
   # and then we could set am__CC="\$(top_srcdir)/compile \$(CC)"
   CC="$am_aux_dir/compile $CC"
fi
AC_LANG_POP([C])])

# For backward compatibility.
AC_DEFUN_ONCE([AM_PROG_CC_C_O], [AC_REQUIRE([AC_PROG_CC])])

# Copyright (C) 2001-2021 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# AM_RUN_LOG(COMMAND)
# -------------------
# Run COMMAND, save the exit status in ac_status, and log it.
# (This has been adapted from Autoconf's _AC_RUN_LOG macro.)
AC_DEFUN([AM_RUN_LOG],
[{ echo "$as_me:$LINENO: $1" >&AS_MESSAGE_LOG_FD
   ($1) >&AS_MESSAGE_LOG_FD 2>&AS_MESSAGE_LOG_FD
   ac_status=$?
   echo "$as_me:$LINENO: \$? = $ac_status" >&AS_MESSAGE_LOG_FD
   (exit $ac_status); }])

# Check to make sure that the build environment is sane.    -*- Autoconf -*-

# Copyright (C) 1996-2021 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# AM_SANITY_CHECK
# ---------------
AC_DEFUN([AM_SANITY_CHECK],
[AC_MSG_CHECKING([whether build environment is sane])
# Reject unsafe characters in $srcdir or the absolute working directory
# name.  Accept space and tab only in the latter.
am_lf='
'
case `pwd` in
  *[[\\\"\#\$\&\'\`$am_lf]]*)
    AC_MSG_ERROR([unsafe absolute working directory name]);;
esac
case $srcdir in
  *[[\\\"\#\$\&\'\`$am_lf\ \	]]*)
    AC_MSG_ERROR([unsafe srcdir value: '$srcdir']);;
esac

# Do 'set' in a subshell so we don't clobber the current shell's
# arguments.  Must try -L first in case configure is actually a
# symlink; some systems play weird games with the mod time of symlinks
# (eg FreeBSD returns the mod time of the symlink's containing
# directory).
if (
   am_has_slept=no
   for am_try in 1 2; do
     echo "timestamp, slept: $am_has_slept" > conftest.file
     set X `ls -Lt "$srcdir/configure" conftest.file 2> /dev/null`
     if test "$[*]" = "X"; then
	# -L didn't work.
	set X `ls -t "$srcdir/configure" conftest.file`
     fi
     if test "$[*]" != "X $srcdir/configure conftest.file" \
	&& test "$[*]" != "X conftest.file $srcdir/configure"; then

	# If neither matched, then we have a broken ls.  This can happen
	# if, for instance, CONFIG_SHELL is bash and it inherits a
	# broken ls alias from the environment.  This has actually
	# happened.  Such a system could not be considered "sane".
	AC_MSG_ERROR([ls -t appears to fail.  Make sure there is not a broken
  alias in your environment])
     fi
     if test "$[2]" = conftest.file || test $am_try -eq 2; then
       break
     fi
     # Just in case.
     sleep 1
     am_has_slept=yes
   done
   test "$[2]" = conftest.file
   )
then
   # Ok.
   :
else
   AC_MSG_ERROR([newly created file is older than distributed files!
Check your system clock])
fi
AC_MSG_RESULT([yes])
# If we didn't sleep, we still need to ensure time stamps of config.status and
# generated files are strictly newer.
am_sleep_pid=
if grep 'slept: no' conftest.file >/dev/null 2>&1; then
  ( sleep 1 ) &
  am_sleep_pid=$!
fi
AC_CONFIG_COMMANDS_PRE(
  [AC_MSG_CHECKING([that generated files are newer than configure])
   if test -n "$am_sleep_pid"; then
     # Hide warnings about reused PIDs.
     wait $am_sleep_pid 2>/dev/null
   fi
   AC_MSG_RESULT([done])])
rm -f conftest.file
])

# Copyright (C) 2009-2021 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# AM_SILENT_RULES([DEFAULT])
# --------------------------
# Enable less verbose build rules; with the default set to DEFAULT
# ("yes" being less verbose, "no" or empty being verbose).
AC_DEFUN([AM_SILENT_RULES],
[AC_ARG_ENABLE([silent-rules], [dnl
AS_HELP_STRING(
  [--enable-silent-rules],
  [less verbose build output (undo: "make V=1")])
AS_HELP_STRING(
  [--disable-silent-rules],
  [verbose build output (undo: "make V=0")])dnl
])
case $enable_silent_rules in @%:@ (((
  yes) AM_DEFAULT_VERBOSITY=0;;
   no) AM_DEFAULT_VERBOSITY=1;;
    *) AM_DEFAULT_VERBOSITY=m4_if([$1], [yes], [0], [1]);;
esac
dnl
dnl A few 'make' implementations (e.g., NonStop OS and NextStep)
dnl do not support nested variable expansions.
dnl See automake bug#9928 and bug#10237.
am_make=${MAKE-make}
AC_CACHE_CHECK([whether $am_make supports nested variables],
   [am_cv_make_support_nested_variables],
   [if AS_ECHO([['TRUE=$(BAR$(V))
BAR0=false
BAR1=true
V=1
am__doit:
	@$(TRUE)
.PHONY: am__doit']]) | $am_make -f - >/dev/null 2>&1; then
  am_cv_make_support_nested_variables=yes
else
  am_cv_make_support_nested_variables=no
fi])
if test $am_cv_make_support_nested_variables = yes; then
  dnl Using '$V' instead of '$(V)' breaks IRIX make.
  AM_V='$(V)'
  AM_DEFAULT_V='$(AM_DEFAULT_VERBOSITY)'
else
  AM_V=$AM_DEFAULT_VERBOSITY
  AM_DEFAULT_V=$AM_DEFAULT_VERBOSITY
fi
AC_SUBST([AM_V])dnl
AM_SUBST_NOTMAKE([AM_V])dnl
AC_SUBST([AM_DEFAULT_V])dnl
AM_SUBST_NOTMAKE([AM_DEFAULT_V])dnl
AC_SUBST([AM_DEFAULT_VERBOSITY])dnl
AM_BACKSLASH='\'
AC_SUBST([AM_BACKSLASH])dnl
_AM_SUBST_NOTMAKE([AM_BACKSLASH])dnl
])

# Copyright (C) 2001-2021 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# AM_PROG_INSTALL_STRIP
# ---------------------
# One issue with vendor 'install' (even GNU) is that you can't
# specify the program used to strip binaries.  This is especially
# annoying in cross-compiling environments, where the build's strip
# is unlikely to handle the host's binaries.
# Fortunately install-sh will honor a STRIPPROG variable, so we
# always use install-sh in "make install-strip", and initialize
# STRIPPROG with the value of the STRIP variable (set by the user).
AC_DEFUN([AM_PROG_INSTALL_STRIP],
[AC_REQUIRE([AM_PROG_INSTALL_SH])dnl
# Installed binaries are usually stripped using 'strip' when the user
# run "make install-strip".  However 'strip' might not be the right
# tool to use in cross-compilation environments, therefore Automake
# will honor the 'STRIP' environment variable to overrule this program.
dnl Don't test for $cross_compiling = yes, because it might be 'maybe'.
if test "$cross_compiling" != no; then
  AC_CHECK_TOOL([STRIP], [strip], :)
fi
INSTALL_STRIP_PROGRAM="\$(install_sh) -c -s"
AC_SUBST([INSTALL_STRIP_PROGRAM])])

# Copyright (C) 2006-2021 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# _AM_SUBST_NOTMAKE(VARIABLE)
# ---------------------------
# Prevent Automake from outputting VARIABLE = @VARIABLE@ in Makefile.in.
# This macro is traced by Automake.
AC_DEFUN([_AM_SUBST_NOTMAKE])

# AM_SUBST_NOTMAKE(VARIABLE)
# --------------------------
# Public sister of _AM_SUBST_NOTMAKE.
AC_DEFUN([AM_SUBST_NOTMAKE], [_AM_SUBST_NOTMAKE($@)])

# Check how to create a tarball.                            -*- Autoconf -*-

# Copyright (C) 2004-2021 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# _AM_PROG_TAR(FORMAT)
# --------------------
# Check how to create a tarball in format FORMAT.
# FORMAT should be one of 'v7', 'ustar', or 'pax'.
#
# Substitute a variable $(am__tar) that is a command
# writing to stdout a FORMAT-tarball containing the directory
# $tardir.
#     tardir=directory && $(am__tar) > result.tar
#
# Substitute a variable $(am__untar) that extract such
# a tarball read from stdin.
#     $(am__untar) < result.tar
#
AC_DEFUN([_AM_PROG_TAR],
[# Always define AMTAR for backward compatibility.  Yes, it's still used
# in the wild :-(  We should find a proper way to deprecate it ...
AC_SUBST([AMTAR], ['$${TAR-tar}'])

# We'll loop over all known methods to create a tar archive until one works.
_am_tools='gnutar m4_if([$1], [ustar], [plaintar]) pax cpio none'

m4_if([$1], [v7],
  [am__tar='$${TAR-tar} chof - "$$tardir"' am__untar='$${TAR-tar} xf -'],

  [m4_case([$1],
    [ustar],
     [# The POSIX 1988 'ustar' format is defined with fixed-size fields.
      # There is notably a 21 bits limit for the UID and the GID.  In fact,
      # the 'pax' utility can hang on bigger UID/GID (see automake bug#8343
      # and bug#13588).
      am_max_uid=2097151 # 2^21 - 1
      am_max_gid=$am_max_uid
      # The $UID and $GID variables are not portable, so we need to resort
      # to the POSIX-mandated id(1) utility.  Errors in the 'id' calls
      # below are definitely unexpected, so allow the users to see them
      # (that is, avoid stderr redirection).
      am_uid=`id -u || echo unknown`
      am_gid=`id -g || echo unknown`
      AC_MSG_CHECKING([whether UID '$am_uid' is supported by ustar format])
      if test $am_uid -le $am_max_uid; then
         AC_MSG_RESULT([yes])
      else
         AC_MSG_RESULT([no])
         _am_tools=none
      fi
      AC_MSG_CHECKING([whether GID '$am_gid' is supported by ustar format])
      if test $am_gid -le $am_max_gid; then
         AC_MSG_RESULT([yes])
      else
        AC_MSG_RESULT([no])
        _am_tools=none
      fi],

  [pax],
    [],

  [m4_fatal([Unknown tar format])])

  AC_MSG_CHECKING([how to create a $1 tar archive])

  # Go ahead even if we have the value already cached.  We do so because we
  # need to set the values for the 'am__tar' and 'am__untar' variables.
  _am_tools=${am_cv_prog_tar_$1-$_am_tools}

  for _am_tool in $_am_tools; do
    case $_am_tool in
    gnutar)
      for _am_tar in tar gnutar gtar; do
        AM_RUN_LOG([$_am_tar --version]) && break
      done
      am__tar="$_am_tar --format=m4_if([$1], [pax], [posix], [$1]) -chf - "'"$$tardir"'
      am__tar_="$_am_tar --format=m4_if([$1], [pax], [posix], [$1]) -chf - "'"$tardir"'
      am__untar="$_am_tar -xf -"
      ;;
    plaintar)
      # Must skip GNU tar: if it does not support --format= it doesn't create
      # ustar tarball either.
      (tar --version) >/dev/null 2>&1 && continue
      am__tar='tar chf - "$$tardir"'
      am__tar_='tar chf - "$tardir"'
      am__untar='tar xf -'
      ;;
    pax)
      am__tar='pax -L -x $1 -w "$$tardir"'
      am__tar_='pax -L -x $1 -w "$tardir"'
      am__untar='pax -r'
      ;;
    cpio)
      am__tar='find "$$tardir" -print | cpio -o -H $1 -L'
      am__tar_='find "$tardir" -print | cpio -o -H $1 -L'
      am__untar='cpio -i -H $1 -d'
      ;;
    none)
      am__tar=false
      am__tar_=false
      am__untar=false
      ;;
    esac

    # If the value was cached, stop now.  We just wanted to have am__tar
    # and am__untar set.
    test -n "${am_cv_prog_tar_$1}" && break

    # tar/untar a dummy directory, and stop if the command works.
    rm -rf conftest.dir
    mkdir conftest.dir
    echo GrepMe > conftest.dir/file
    AM_RUN_LOG([tardir=conftest.dir && eval $am__tar_ >conftest.tar])
    rm -rf conftest.dir
    if test -s conftest.tar; then
      AM_RUN_LOG([$am__untar <conftest.tar])
      AM_RUN_LOG([cat conftest.dir/file])
      grep GrepMe conftest.dir/file >/dev/null 2>&1 && break
    fi
  done
  rm -rf conftest.dir

  AC_CACHE_VAL([am_cv_prog_tar_$1], [am_cv_prog_tar_$1=$_am_tool])
  AC_MSG_RESULT([$am_cv_prog_tar_$1])])

AC_SUBST([am__tar])
AC_SUBST([am__untar])
]) # _AM_PROG_TAR

# pkg.m4 - Macros to locate and use pkg-config.   -*- Autoconf -*-
# serial 12 (pkg-config-0.29.2)

dnl Copyright © 2004 Scott James Remnant <scott@netsplit.com>.
dnl Copyright © 2012-2015 Dan Nicholson <dbn.lists@gmail.com>
dnl
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 2 of the License, or
dnl (at your option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful, but
dnl WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
dnl General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software
dnl Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
dnl 02111-1307, USA.
dnl
dnl As a special exception to the GNU General Public License, if you
dnl distribute this file as part of a program that contains a
dnl configuration script generated by Autoconf, you may include it under
dnl the same distribution terms that you use for the rest of that
dnl program.

dnl PKG_PREREQ(MIN-VERSION)
dnl -----------------------
dnl Since: 0.29
dnl
dnl Verify that the version of the pkg-config macros are at least
dnl MIN-VERSION. Unlike PKG_PROG_PKG_CONFIG, which checks the user's
dnl installed version of pkg-config, this checks the developer's version
dnl of pkg.m4 when generating configure.
dnl
dnl To ensure that this macro is defined, also add:
dnl m4_ifndef([PKG_PREREQ],
dnl     [m4_fatal([must install pkg-config 0.29 or later before running autoconf/autogen])])
dnl
dnl See the "Since" comment for each macro you use to see what version
dnl of the macros you require.
m4_defun([PKG_PREREQ],
[m4_define([PKG_MACROS_VERSION], [0.29.2])
m4_if(m4_version_compare(PKG_MACROS_VERSION, [$1]), -1,
    [m4_fatal([pkg.m4 version $1 or higher is required but ]PKG_MACROS_VERSION[ found])])
])dnl PKG_PREREQ

dnl PKG_PROG_PKG_CONFIG([MIN-VERSION])
dnl ----------------------------------
dnl Since: 0.16
dnl
dnl Search for the pkg-config tool and set the PKG_CONFIG variable to
dnl first found in the path. Checks that the version of pkg-config found
dnl is at least MIN-VERSION. If MIN-VERSION is not specified, 0.9.0 is
dnl used since that's the first version where most current features of
dnl pkg-config existed.
AC_DEFUN([PKG_PROG_PKG_CONFIG],
[m4_pattern_forbid([^_?PKG_[A-Z_]+$])
m4_pattern_allow([^PKG_CONFIG(_(PATH|LIBDIR|SYSROOT_DIR|ALLOW_SYSTEM_(CFLAGS|LIBS)))?$])
m4_pattern_allow([^PKG_CONFIG_(DISABLE_UNINSTALLED|TOP_BUILD_DIR|DEBUG_SPEW)$])
AC_ARG_VAR([PKG_CONFIG], [path to pkg-config utility])
AC_ARG_VAR([PKG_CONFIG_PATH], [directories to add to pkg-config's search path])
AC_ARG_VAR([PKG_CONFIG_LIBDIR], [path overriding pkg-config's built-in search path])

if test "x$ac_cv_env_PKG_CONFIG_set" != "xset"; then
	AC_PATH_TOOL([PKG_CONFIG], [pkg-config])
fi
if test -n "$PKG_CONFIG"; then
	_pkg_min_version=m4_default([$1], [0.9.0])
	AC_MSG_CHECKING([pkg-config is at least version $_pkg_min_version])
	if $PKG_CONFIG --atleast-pkgconfig-version $_pkg_min_version; then
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
		PKG_CONFIG=""
	fi
fi[]dnl
])dnl PKG_PROG_PKG_CONFIG

dnl PKG_CHECK_EXISTS(MODULES, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
dnl -------------------------------------------------------------------
dnl Since: 0.18
dnl
dnl Check to see whether a particular set of modules exists. Similar to
dnl PKG_CHECK_MODULES(), but does not set variables or print errors.
dnl
dnl Please remember that m4 expands AC_REQUIRE([PKG_PROG_PKG_CONFIG])
dnl only at the first occurrence in configure.ac, so if the first place
dnl it's called might be skipped (such as if it is within an "if", you
dnl have to call PKG_CHECK_EXISTS manually
AC_DEFUN([PKG_CHECK_EXISTS],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])dnl
if test -n "$PKG_CONFIG" && \
    AC_RUN_LOG([$PKG_CONFIG --exists --print-errors "$1"]); then
  m4_default([$2], [:])
m4_ifvaln([$3], [else
  $3])dnl
fi])

dnl _PKG_CONFIG([VARIABLE], [COMMAND], [MODULES])
dnl ---------------------------------------------
dnl Internal wrapper calling pkg-config via PKG_CONFIG and setting
dnl pkg_failed based on the result.
m4_define([_PKG_CONFIG],
[if test -n "$$1"; then
    pkg_cv_[]$1="$$1"
 elif test -n "$PKG_CONFIG"; then
    PKG_CHECK_EXISTS([$3],
                     [pkg_cv_[]$1=`$PKG_CONFIG --[]$2 "$3" 2>/dev/null`
		      test "x$?" != "x0" && pkg_failed=yes ],
		     [pkg_failed=yes])
 else
    pkg_failed=untried
fi[]dnl
])dnl _PKG_CONFIG

dnl _PKG_SHORT_ERRORS_SUPPORTED
dnl ---------------------------
dnl Internal check to see if pkg-config supports short errors.
AC_DEFUN([_PKG_SHORT_ERRORS_SUPPORTED],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])
if $PKG_CONFIG --atleast-pkgconfig-version 0.20; then
        _pkg_short_errors_supported=yes
else
        _pkg_short_errors_supported=no
fi[]dnl
])dnl _PKG_SHORT_ERRORS_SUPPORTED


dnl PKG_CHECK_MODULES(VARIABLE-PREFIX, MODULES, [ACTION-IF-FOUND],
dnl   [ACTION-IF-NOT-FOUND])
dnl --------------------------------------------------------------
dnl Since: 0.4.0
dnl
dnl Note that if there is a possibility the first call to
dnl PKG_CHECK_MODULES might not happen, you should be sure to include an
dnl explicit call to PKG_PROG_PKG_CONFIG in your configure.ac
AC_DEFUN([PKG_CHECK_MODULES],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])dnl
AC_ARG_VAR([$1][_CFLAGS], [C compiler flags for $1, overriding pkg-config])dnl
AC_ARG_VAR([$1][_LIBS], [linker flags for $1, overriding pkg-config])dnl

pkg_failed=no
AC_MSG_CHECKING([for $2])

_PKG_CONFIG([$1][_CFLAGS], [cflags], [$2])
_PKG_CONFIG([$1][_LIBS], [libs], [$2])

m4_define([_PKG_TEXT], [Alternatively, you may set the environment variables $1[]_CFLAGS
and $1[]_LIBS to avoid the need to call pkg-config.
See the pkg-config man page for more details.])

if test $pkg_failed = yes; then
        AC_MSG_RESULT([no])
        _PKG_SHORT_ERRORS_SUPPORTED
        if test $_pkg_short_errors_supported = yes; then
                $1[]_PKG_ERRORS=`$PKG_CONFIG --short-errors --print-errors --cflags --libs "$2" 2>&1`
        else
                $1[]_PKG_ERRORS=`$PKG_CONFIG --print-errors --cflags --libs "$2" 2>&1`
        fi
        # Put the nasty error message in config.log where it belongs
        echo "$$1[]_PKG_ERRORS" >&AS_MESSAGE_LOG_FD

        m4_default([$4], [AC_MSG_ERROR(
[Package requirements ($2) were not met:

$$1_PKG_ERRORS

Consider adjusting the PKG_CONFIG_PATH environment variable if you
installed software in a non-standard prefix.

_PKG_TEXT])[]dnl
        ])
elif test $pkg_failed = untried; then
        AC_MSG_RESULT([no])
        m4_default([$4], [AC_MSG_FAILURE(
[The pkg-config script could not be found or is too old.  Make sure it
is in your PATH or set the PKG_CONFIG environment variable to the full
path to pkg-config.

_PKG_TEXT

To get pkg-config, see <http://pkg-config.freedesktop.org/>.])[]dnl
        ])
else
        $1[]_CFLAGS=$pkg_cv_[]$1[]_CFLAGS
        $1[]_LIBS=$pkg_cv_[]$1[]_LIBS
        AC_MSG_RESULT([yes])
        $3
fi[]dnl
])dnl PKG_CHECK_MODULES


dnl PKG_CHECK_MODULES_STATIC(VARIABLE-PREFIX, MODULES, [ACTION-IF-FOUND],
dnl   [ACTION-IF-NOT-FOUND])
dnl ---------------------------------------------------------------------
dnl Since: 0.29
dnl
dnl Checks for existence of MODULES and gathers its build flags with
dnl static libraries enabled. Sets VARIABLE-PREFIX_CFLAGS from --cflags
dnl and VARIABLE-PREFIX_LIBS from --libs.
dnl
dnl Note that if there is a possibility the first call to
dnl PKG_CHECK_MODULES_STATIC might not happen, you should be sure to
dnl include an explicit call to PKG_PROG_PKG_CONFIG in your
dnl configure.ac.
AC_DEFUN([PKG_CHECK_MODULES_STATIC],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])dnl
_save_PKG_CONFIG=$PKG_CONFIG
PKG_CONFIG="$PKG_CONFIG --static"
PKG_CHECK_MODULES($@)
PKG_CONFIG=$_save_PKG_CONFIG[]dnl
])dnl PKG_CHECK_MODULES_STATIC


dnl PKG_INSTALLDIR([DIRECTORY])
dnl -------------------------
dnl Since: 0.27
dnl
dnl Substitutes the variable pkgconfigdir as the location where a module
dnl should install pkg-config .pc files. By default the directory is
dnl $libdir/pkgconfig, but the default can be changed by passing
dnl DIRECTORY. The user can override through the --with-pkgconfigdir
dnl parameter.
AC_DEFUN([PKG_INSTALLDIR],
[m4_pushdef([pkg_default], [m4_default([$1], ['${libdir}/pkgconfig'])])
m4_pushdef([pkg_description],
    [pkg-config installation directory @<:@]pkg_default[@:>@])
AC_ARG_WITH([pkgconfigdir],
    [AS_HELP_STRING([--with-pkgconfigdir], pkg_description)],,
    [with_pkgconfigdir=]pkg_default)
AC_SUBST([pkgconfigdir], [$with_pkgconfigdir])
m4_popdef([pkg_default])
m4_popdef([pkg_description])
])dnl PKG_INSTALLDIR


dnl PKG_NOARCH_INSTALLDIR([DIRECTORY])
dnl --------------------------------
dnl Since: 0.27
dnl
dnl Substitutes the variable noarch_pkgconfigdir as the location where a
dnl module should install arch-independent pkg-config .pc files. By
dnl default the directory is $datadir/pkgconfig, but the default can be
dnl changed by passing DIRECTORY. The user can override through the
dnl --with-noarch-pkgconfigdir parameter.
AC_DEFUN([PKG_NOARCH_INSTALLDIR],
[m4_pushdef([pkg_default], [m4_default([$1], ['${datadir}/pkgconfig'])])
m4_pushdef([pkg_description],
    [pkg-config arch-independent installation directory @<:@]pkg_default[@:>@])
AC_ARG_WITH([noarch-pkgconfigdir],
    [AS_HELP_STRING([--with-noarch-pkgconfigdir], pkg_description)],,
    [with_noarch_pkgconfigdir=]pkg_default)
AC_SUBST([noarch_pkgconfigdir], [$with_noarch_pkgconfigdir])
m4_popdef([pkg_default])
m4_popdef([pkg_description])
])dnl PKG_NOARCH_INSTALLDIR


dnl PKG_CHECK_VAR(VARIABLE, MODULE, CONFIG-VARIABLE,
dnl [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
dnl -------------------------------------------
dnl Since: 0.28
dnl
dnl Retrieves the value of the pkg-config variable for the given module.
AC_DEFUN([PKG_CHECK_VAR],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])dnl
AC_ARG_VAR([$1], [value of $3 for $2, overriding pkg-config])dnl

_PKG_CONFIG([$1], [variable="][$3]["], [$2])
AS_VAR_COPY([$1], [pkg_cv_][$1])

AS_VAR_IF([$1], [""], [$5], [$4])dnl
])dnl PKG_CHECK_VAR

dnl PKG_WITH_MODULES(VARIABLE-PREFIX, MODULES,
dnl   [ACTION-IF-FOUND],[ACTION-IF-NOT-FOUND],
dnl   [DESCRIPTION], [DEFAULT])
dnl ------------------------------------------
dnl
dnl Prepare a "--with-" configure option using the lowercase
dnl [VARIABLE-PREFIX] name, merging the behaviour of AC_ARG_WITH and
dnl PKG_CHECK_MODULES in a single macro.
AC_DEFUN([PKG_WITH_MODULES],
[
m4_pushdef([with_arg], m4_tolower([$1]))

m4_pushdef([description],
           [m4_default([$5], [build with ]with_arg[ support])])

m4_pushdef([def_arg], [m4_default([$6], [auto])])
m4_pushdef([def_action_if_found], [AS_TR_SH([with_]with_arg)=yes])
m4_pushdef([def_action_if_not_found], [AS_TR_SH([with_]with_arg)=no])

m4_case(def_arg,
            [yes],[m4_pushdef([with_without], [--without-]with_arg)],
            [m4_pushdef([with_without],[--with-]with_arg)])

AC_ARG_WITH(with_arg,
     AS_HELP_STRING(with_without, description[ @<:@default=]def_arg[@:>@]),,
    [AS_TR_SH([with_]with_arg)=def_arg])

AS_CASE([$AS_TR_SH([with_]with_arg)],
            [yes],[PKG_CHECK_MODULES([$1],[$2],$3,$4)],
            [auto],[PKG_CHECK_MODULES([$1],[$2],
                                        [m4_n([def_action_if_found]) $3],
                                        [m4_n([def_action_if_not_found]) $4])])

m4_popdef([with_arg])
m4_popdef([description])
m4_popdef([def_arg])

])dnl PKG_WITH_MODULES

dnl PKG_HAVE_WITH_MODULES(VARIABLE-PREFIX, MODULES,
dnl   [DESCRIPTION], [DEFAULT])
dnl -----------------------------------------------
dnl
dnl Convenience macro to trigger AM_CONDITIONAL after PKG_WITH_MODULES
dnl check._[VARIABLE-PREFIX] is exported as make variable.
AC_DEFUN([PKG_HAVE_WITH_MODULES],
[
PKG_WITH_MODULES([$1],[$2],,,[$3],[$4])

AM_CONDITIONAL([HAVE_][$1],
               [test "$AS_TR_SH([with_]m4_tolower([$1]))" = "yes"])
])dnl PKG_HAVE_WITH_MODULES

dnl PKG_HAVE_DEFINE_WITH_MODULES(VARIABLE-PREFIX, MODULES,
dnl   [DESCRIPTION], [DEFAULT])
dnl ------------------------------------------------------
dnl
dnl Convenience macro to run AM_CONDITIONAL and AC_DEFINE after
dnl PKG_WITH_MODULES check. HAVE_[VARIABLE-PREFIX] is exported as make
dnl and preprocessor variable.
AC_DEFUN([PKG_HAVE_DEFINE_WITH_MODULES],
[
PKG_HAVE_WITH_MODULES([$1],[$2],[$3],[$4])

AS_IF([test "$AS_TR_SH([with_]m4_tolower([$1]))" = "yes"],
        [AC_DEFINE([HAVE_][$1], 1, [Enable ]m4_tolower([$1])[ support])])
])dnl PKG_HAVE_DEFINE_WITH_MODULES

dnl Copyright (c) 2002-2015
dnl         The Xfce development team. All rights reserved.
dnl
dnl Written for Xfce by Benedikt Meurer <benny@xfce.org>.
dnl
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 2 of the License, or
dnl (at your option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License along
dnl with this program; if not, write to the Free Software Foundation, Inc.,
dnl 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
dnl
dnl xdt-depends
dnl -----------
dnl  Contains M4 macros to check for software dependencies.
dnl  Partly based on prior work of the XDG contributors.
dnl



dnl We need recent a autoconf version
AC_PREREQ([2.69])



dnl XDT_PROG_PKG_CONFIG()
dnl
dnl Checks for the freedesktop.org pkg-config
dnl utility and sets the PKG_CONFIG environment
dnl variable to the full path if found.
dnl
AC_DEFUN([XDT_PROG_PKG_CONFIG],
[
  # minimum supported version of pkg-config
  xdt_cv_PKG_CONFIG_MIN_VERSION=0.9.0

  m4_ifdef([PKG_PROG_PKG_CONFIG],
    [
      PKG_PROG_PKG_CONFIG([$xdt_cv_PKG_CONFIG_MIN_VERSION])

      if test x"$PKG_CONFIG" = x""; then
        echo
        echo "*** Your version of pkg-config is too old. You need atleast"
        echo "*** pkg-config $xdt_cv_PKG_CONFIG_MIN_VERSION or newer. You can download pkg-config"
        echo "*** from the freedesktop.org software repository at"
        echo "***"
        echo "***    https://www.freedesktop.org/wiki/Software/pkg-config/"
        echo "***"
        exit 1;
      fi
    ],
    [
      echo
      echo "*** The pkg-config utility could not be found on your system."
      echo "*** Make sure it is in your path, or set the PKG_CONFIG"
      echo "*** environment variable to the full path to pkg-config."
      echo "*** You can download pkg-config from the freedesktop.org"
      echo "*** software repository at"
      echo "***"
      echo "***    https://www.freedesktop.org/wiki/Software/pkg-config/"
      echo "***"
      exit 1
    ])
])



dnl XDT_CHECK_PACKAGE(varname, package, version, [action-if], [action-if-not])
dnl
dnl Checks if "package" >= "version" is installed on the
dnl target system, using the pkg-config utility. If the
dnl dependency is met, "varname"_CFLAGS, "varname"_LIBS,
dnl "varname"_VERSION and "varname"_REQUIRED_VERSION
dnl will be set and marked for substition.
dnl
dnl "varname"_REQUIRED_VERSION will be set to the value of
dnl "version". This is mostly useful to automatically
dnl place the correct version information into the RPM
dnl .spec file.
dnl
dnl In addition, if the dependency is met, "action-if" will
dnl be executed if given.
dnl
dnl If the package check fails, "action-if-not" will be
dnl executed. If this parameter isn't specified, a diagnostic
dnl message will be printed and the configure script will
dnl be terminated with exit code 1.
dnl
AC_DEFUN([XDT_CHECK_PACKAGE],
[
  XDT_PROG_PKG_CONFIG()

  AC_MSG_CHECKING([for $2 >= $3])
  if $PKG_CONFIG "--atleast-version=$3" "$2" >/dev/null 2>&1; then
    $1_VERSION=`$PKG_CONFIG --modversion "$2"`
    AC_MSG_RESULT([$$1_VERSION])

    AC_MSG_CHECKING([$1_CFLAGS])
    $1_CFLAGS=`$PKG_CONFIG --cflags "$2"`
    AC_MSG_RESULT([$$1_CFLAGS])

    AC_MSG_CHECKING([$1_LIBS])
    $1_LIBS=`$PKG_CONFIG --libs "$2"`
    AC_MSG_RESULT([$$1_LIBS])

    $1_REQUIRED_VERSION=$3

    AC_SUBST([$1_VERSION])
    AC_SUBST([$1_CFLAGS])
    AC_SUBST([$1_LIBS])
    AC_SUBST([$1_REQUIRED_VERSION])

    if test x"$1" = x"GLIB"; then
      dnl Use GLib structured logging, see https://docs.gtk.org/glib/logging.html
      dnl XFCE apps&libraries can override this setting after XDT_CHECK_PACKAGE(GLIB)
      dnl using AC_DEFINE.
      dnl Note that it requires GLIB_VERSION_MAX_ALLOWED >= GLIB_VERSION_2_56 to work
      dnl properly in GLib logging macros (not documented, see glib/gmessages.h).
      AC_DEFINE(G_LOG_USE_STRUCTURED, 1, [Use GLib structured logging])
    fi

    ifelse([$1], GLIB, [
      dnl Report uses of GLib functions newer than $3 as C compiler warnings.
      dnl XFCE apps&libraries can override this setting after XDT_CHECK_PACKAGE(GLIB)
      dnl using AC_DEFINE, in which case it is recommended to override both MAX and MIN.
      AC_MSG_NOTICE([setting GLIB_VERSION_MAX_ALLOWED and GLIB_VERSION_MIN_REQUIRED according to $3])
      m4_pushdef([SUFFIX], translit($3, `.', `_'))
      m4_define([SUFFIX], ifelse(regexp(SUFFIX, [[0-9]+_[0-9]+_[0-9]+]), -1, SUFFIX, patsubst(SUFFIX, [_[0-9]+$])))
      AC_DEFINE(GLIB_VERSION_MAX_ALLOWED, m4_format(GLIB_VERSION_%s, SUFFIX), m4_format(Prevent post %s APIs, SUFFIX))
      AC_DEFINE(GLIB_VERSION_MIN_REQUIRED, m4_format(GLIB_VERSION_%s, SUFFIX), m4_format(Ignore post %s APIs, SUFFIX))
      m4_popdef([SUFFIX])
    ])

    ifelse([$4], , , [$4])
  elif $PKG_CONFIG --exists "$2" >/dev/null 2>&1; then
    xdt_cv_version=`$PKG_CONFIG --modversion "$2"`
    AC_MSG_RESULT([found, but $xdt_cv_version])

    ifelse([$5], ,
    [
      echo "*** The required package $2 was found on your system,"
      echo "*** but the installed version ($xdt_cv_version) is too old."
      echo "*** Please upgrade $2 to atleast version $3, or adjust"
      echo "*** the PKG_CONFIG_PATH environment variable if you installed"
      echo "*** the new version of the package in a nonstandard prefix so"
      echo "*** pkg-config is able to find it."
      exit 1
    ], [$5])
  else
    AC_MSG_RESULT([not found])

    ifelse([$5], ,
    [
      echo "*** The required package $2 was not found on your system."
      echo "*** Please install $2 (atleast version $3) or adjust"
      echo "*** the PKG_CONFIG_PATH environment variable if you"
      echo "*** installed the package in a nonstandard prefix so that"
      echo "*** pkg-config is able to find it."
      exit 1
    ], [$5])
  fi
])



dnl XDT_CHECK_OPTIONAL_PACKAGE(varname, package, version, optionname, helpstring, [default])
dnl
dnl Checks for an optional dependency on "package" >= "version". "default"
dnl can be "yes" or "no" (defaults to "yes" if not specified) and controls
dnl whether configure should check this dependency by default, or only if
dnl the user explicitly enables it using a command line switch.
dnl
dnl This macro automatically adds a commandline switch based on the "optionname"
dnl parameter (--enable-optionname/--disable-optionname), which allows the
dnl user to explicitly control whether this optional dependency should be
dnl enabled or not. The "helpstring" parameter gives a brief(!) description
dnl about this dependency.
dnl
dnl If the user chose to enable this dependency and the required package
dnl was found, this macro defines the variable "varname"_FOUND and sets it
dnl to the string "yes", in addition to the 4 variables set by XDT_CHECK_PACKAGE.
dnl But "varname"_FOUND will not be marked for substition. Furthermore,
dnl a CPP define HAVE_"varname" will be placed in config.h (or added to
dnl the cc command line, depending on your configure.ac) and set to
dnl 1.
dnl
AC_DEFUN([XDT_CHECK_OPTIONAL_PACKAGE],
[
  AC_REQUIRE([XDT_PROG_PKG_CONFIG])

  AC_ARG_ENABLE([$4],
AS_HELP_STRING([--enable-$4],[Enable checking for $5 (default=m4_default([$6], [yes]))])
AS_HELP_STRING([--disable-$4],[Disable checking for $5]),
    [xdt_cv_$1_check=$enableval], [xdt_cv_$1_check=m4_default([$6], [yes])])

  if test x"$xdt_cv_$1_check" = x"yes"; then
    if $PKG_CONFIG --exists "$2 >= $3" >/dev/null 2>&1; then
      XDT_CHECK_PACKAGE([$1], [$2], [$3],
      [
        AC_DEFINE([HAVE_$1], [1], [Define if $2 >= $3 present])
        $1_FOUND="yes"
      ])
    else
      AC_MSG_CHECKING([for optional package $2 >= $3])
      AC_MSG_RESULT([not found])
    fi
  else
    AC_MSG_CHECKING([for optional package $2])
    AC_MSG_RESULT([disabled])
  fi

  AM_CONDITIONAL([HAVE_$1], [test x"$$1_FOUND" = x"yes"])
])



dnl XDT_FEATURE_DEPENDENCY(varname, package, version)
dnl
dnl Used only as an argument to XDT_CHECK_OPTIONAL_FEATURE(), this macro
dnl declares a dependency required for the feature to be enabled.
dnl
dnl If the dependency is checked and found, variables and substitutions will be
dnl created and set as in XDT_CHECK_PACKAGE(), plus varname_FOUND will be set
dnl to "yes", and HAVE_varname will be set in config.h.
dnl
dnl Note that, unlike with XDT_CHECK_OPTIONAL_PACKAGE(), no automake
dnl conditional is set for this dependency.
dnl
dnl See XDT_CHECK_OPTIONAL_FEATURE() for more information.
dnl
AC_DEFUN([XDT_FEATURE_DEPENDENCY],
[
  AC_REQUIRE([XDT_PROG_PKG_CONFIG])

  if test x"$xdt_feature_deps_check_only" = x"yes"; then
    if ! $PKG_CONFIG --exists "$2 >= $3" >/dev/null 2>&1; then
      if test x"$xdt_feature_deps_missing" = x""; then
        xdt_feature_deps_missing="$2 >= $3"
      else
        xdt_feature_deps_missing="$xdt_feature_deps_missing, $2 >= $3"
      fi
    fi
  else
    XDT_CHECK_PACKAGE([$1], [$2], [$3],
    [
      AC_DEFINE([HAVE_$1], [1], [Define if $2 >= $3 present])
      $1_FOUND="yes"
    ])
  fi
])



dnl XDT_CHECK_OPTIONAL_FEATURE(varname, optionname, dependency-checks, [helpstring], [default])
dnl
dnl Introduces an --enable-optionname/--disable-optionname flag pair for a
dnl named feature. If neither flag is provided, the feature will be enabled or
dnl disabled depending on whether or not dependency-checks succeed or fail.
dnl If --enable-optionname is provided, the configure script will error out if
dnl the dependencies are not met.  If --disable-optionname is provided, the
dnl feature will be disabled, and dependencies will not be checked.
dnl
dnl The dependency-checks argument should be a series of
dnl XDT_FEATURE_DEPENDENCY() macro calls, passed as a single quoted argument to
dnl XDT_CHECK_OPTIONAL_FEATURE().
dnl
dnl If helpstring is not provided, optionname is used instead.
dnl
dnl The default is "auto", and semantics are as described above. If default is
dnl set to "yes", then the feature will be required unless --disable-optionname
dnl is passed to configure.  If default is set to "no", the feature will not be
dnl built unless --enable-optionname is passed.
dnl
dnl If the feature is enabled and dependencies are met, ENABLE_varname will be
dnl defined in config.h. Additionally, an automake conditional called
dnl ENABLE_varname will be created, and a shell variable called ENABLE_varname
dnl will set to "yes" or "no".
dnl
dnl Example usage:
dnl
dnl XDT_CHECK_OPTIONAL_FEATURE([WAYLAND],
dnl                            [wayland],
dnl                            [
dnl                              XDT_FEATURE_DEPENDENCY([GDK_WAYLAND], [gdk-wayland-3.0], [3.24.0])
dnl                              XDT_FEATURE_DEPENDENCY([GTK_LAYER_SHELL], [gtk-layer-shell-0], [0.7.0])
dnl                            ],
dnl                            [the Wayland windowing system])
dnl
dnl Note that there are no commas between the XDT_FEATURE_DEPENDENCY()
dnl invocations; they should all form a single "argument" to
dnl XDT_CHECK_OPTIONAL_FEATURE().
dnl
dnl Also note that you must quote the dependency-checks argument with square
dnl brackets, or you will get syntax errors in the generated configure script.
dnl
AC_DEFUN([XDT_CHECK_OPTIONAL_FEATURE],
[
  AC_ARG_ENABLE([$2],
AS_HELP_STRING([--enable-$2], [Enable support for m4_default($4, $2) (default=m4_default([$5], [auto]))])
AS_HELP_STRING([--disable-$2], [Disable support for m4_default($4, $2)]),
    [xdt_cv_$1_enabled=$enableval], [xdt_cv_$1_enabled=m4_default([$5], [auto])])

  if test x"$xdt_cv_$1_enabled" != x"no"; then
    xdt_feature_deps_check_only=yes
    xdt_feature_deps_missing=
    $3
    if test x"$xdt_feature_deps_missing" = x""; then
      xdt_feature_deps_check_only=
      $3
      ENABLE_$1="yes"
      AC_DEFINE([ENABLE_$1], [1], [Define if m4_default($4, $2) is enabled])
      AC_MSG_CHECKING([if m4_default($4, $2) is enabled])
      AC_MSG_RESULT([yes])
    else
      AC_MSG_CHECKING([if m4_default($4, $2) is enabled])
      AC_MSG_RESULT([dependencies missing: $xdt_feature_deps_missing])
      if test x"$xdt_cv_$1_enabled" = x"yes"; then
        AC_MSG_ERROR([support for m4_default($4, $2) was required, but dependencies were not met])
      else
        ENABLE_$1="no"
      fi
    fi
    xdt_feature_deps_check_only=
    xdt_feature_deps_missing=
  else
    ENABLE_$1="no"
    AC_MSG_CHECKING([if m4_default($4, $2) is enabled])
    AC_MSG_RESULT([disabled])
  fi

  AM_CONDITIONAL([ENABLE_$1], [test x"$ENABLE_$1" = x"yes"])
])



dnl XDT_CHECK_PACKAGE_BINARY(varname, package, pkgvarname, [binname])
dnl
dnl Finds a program by looking for a variable named pkgvarname in the
dnl pkg-config file for package. If found, varname (both a shell variable and
dnl automake substitution) is set to the name of the binary (and possibly full
dnl path, if that is how it is specified in the pkg-config file). If not found,
dnl or not present and executable, configure will exit with an error.
dnl
dnl Users can also override this detection by specfying a varname on the
dnl configure command line. In that case, the provided value is still checked
dnl to ensure it exists and is executable.
dnl
dnl If binname is not provided (for documentation purposes), pkgvarname will be
dnl used instead.
dnl
dnl Example usage:
dnl
dnl XDT_CHECK_PACKAGE_BINARY([GLIB_GENMARSHAL], [glib-2.0], [glib_genmarshal], [glib-genmarshal])
dnl
AC_DEFUN([XDT_CHECK_PACKAGE_BINARY],
[
  AC_REQUIRE([XDT_PROG_PKG_CONFIG])

  AC_ARG_VAR([$1], [Location of program ]m4_default($4, $3))
  AC_MSG_CHECKING([for m4_default($4, $3)])

  if test x"$$1" = x""; then
    $1=`$PKG_CONFIG --variable=$3 $2`
  fi
  if test x"$$1" != x"" -a -x "$$1"; then
    AC_MSG_RESULT([$$1])
  else
    AC_MSG_ERROR([could not find m4_default($4, $3). You can run:
./configure $1=/path/to/m4_default($4, $3)
to provide a custom location for it.])
  fi
])



dnl XDT_CHECK_LIBX11()
dnl
dnl Executes various checks for X11. Sets LIBX11_CFLAGS, LIBX11_LDFLAGS
dnl and LIBX11_LIBS (and marks them for substitution). In addition
dnl HAVE_LIBX11 is set to 1 in config.h, if the X window system and
dnl the development files are detected on the target system.
dnl
AC_DEFUN([XDT_CHECK_LIBX11],
[
  AC_REQUIRE([AC_PATH_XTRA])

  LIBX11_CFLAGS= LIBX11_LDFLAGS= LIBX11_LIBS=
  if test x"$no_x" != x"yes"; then
    AC_CHECK_LIB([X11], [main],
    [
      AC_DEFINE([HAVE_LIBX11], [1], [Define if libX11 is available])
      LIBX11_CFLAGS="$X_CFLAGS"
      for option in $X_PRE_LIBS $X_EXTRA_LIBS $X_LIBS; do
      	case "$option" in
        -L*)
          path=`echo $option | sed 's/^-L//'`
          if test x"$path" != x""; then
            LIBX11_LDFLAGS="$LIBX11_LDFLAGS -L$path"
          fi
          ;;
        *)
          LIBX11_LIBS="$LIBX11_LIBS $option"
          ;;
        esac
      done
      if ! echo $LIBX11_LIBS | grep -- '-lX11' >/dev/null; then
        LIBX11_LIBS="$LIBX11_LIBS -lX11"
      fi
    ], [], [$X_CFLAGS $X_PRE_LIBS $X_EXTRA_LIBS $X_LIBS])
  fi
  AC_SUBST([LIBX11_CFLAGS])
  AC_SUBST([LIBX11_LDFLAGS])
  AC_SUBST([LIBX11_LIBS])
])



dnl XDT_CHECK_LIBX11_REQUIRE()
dnl
dnl Similar to XDT_CHECK_LIBX11(), but terminates with an error if
dnl the X window system and development files aren't detected on the
dnl target system.
dnl
AC_DEFUN([XDT_CHECK_LIBX11_REQUIRE],
[
  AC_REQUIRE([XDT_CHECK_LIBX11])
  
  if test x"$no_x" = x"yes"; then
    AC_MSG_ERROR([X Window system libraries and header files are required])
  fi
])



dnl XDT_CHECK_LIBSM()
dnl
dnl Checks whether the session management library is present on the
dnl target system, and sets LIBSM_CFLAGS, LIBSM_LDFLAGS and LIBSM_LIBS
dnl properly. In addition, HAVE_LIBSM will be set to 1 in config.h
dnl if libSM is detected.
dnl
AC_DEFUN([XDT_CHECK_LIBSM],
[
  AC_REQUIRE([XDT_CHECK_LIBX11])

  LIBSM_CFLAGS= LIBSM_LDFLAGS= LIBSM_LIBS=
  if test x"$no_x" != x"yes"; then
    AC_CHECK_LIB([SM], [SmcSaveYourselfDone],
    [
      AC_DEFINE([HAVE_LIBSM], [1], [Define if libSM is available])
      LIBSM_CFLAGS="$LIBX11_CFLAGS"
      LIBSM_LDFLAGS="$LIBX11_LDFLAGS"
      LIBSM_LIBS="$LIBX11_LIBS"
      if ! echo $LIBSM_LIBS | grep -- '-lSM' >/dev/null; then
        LIBSM_LIBS="$LIBSM_LIBS -lSM -lICE"
      fi
    ], [], [$LIBX11_CFLAGS $LIBX11_LDFLAGS $LIBX11_LIBS -lICE])
  fi
  AC_SUBST([LIBSM_CFLAGS])
  AC_SUBST([LIBSM_LDFLAGS])
  AC_SUBST([LIBSM_LIBS])
])



dnl XDT_CHECK_LIBXPM()
dnl
dnl Checks if the Xpm library is present on the target system, and
dnl sets LIBXPM_CFLAGS, LIBXPM_LDFLAGS and LIBXPM_LIBS. In addition,
dnl HAVE_LIBXPM will be set to 1 in config.h if libXpm is detected.
dnl
AC_DEFUN([XDT_CHECK_LIBXPM],
[
  AC_REQUIRE([XDT_CHECK_LIBX11])

  LIBXPM_CFLAGS= LIBXPM_LDFLAGS= LIBXPM_LIBS=
  if test "$no_x" != "yes"; then
    AC_CHECK_LIB([Xpm], [main],
    [
      AC_DEFINE([HAVE_LIBXPM], [1], [Define if libXpm is available])
      LIBXPM_CFLAGS="$LIBX11_CFLAGS"
      LIBXPM_LDFLAGS="$LIBX11_LDFLAGS"
      LIBXPM_LIBS="$LIBX11_LIBS"
      if ! echo $LIBXPM_LIBS | grep -- '-lXpm' >/dev/null; then
        LIBXPM_LIBS="$LIBXPM_LIBS -lXpm"
      fi
    ], [], [$LIBX11_CFLAGS $LIBX11_LDFLAGS $LIBX11_LIBS -lXpm])
  fi
  AC_SUBST([LIBXPM_CFLAGS])
  AC_SUBST([LIBXPM_LDFLAGS])
  AC_SUBST([LIBXPM_LIBS])
])



dnl XDT_CHECK_LIBXPM_REQUIRE()
dnl
dnl Similar to XDT_CHECK_LIBXPM(), but fails if the Xpm library isn't
dnl present on the target system.
dnl
AC_DEFUN([XDT_CHECK_LIBXPM_REQUIRE],
[
  AC_REQUIRE([XDT_CHECK_LIBX11_REQUIRE])
  AC_REQUIRE([XDT_CHECK_LIBXPM])

  if test x"$LIBXPM_LIBS" = x""; then
    AC_MSG_ERROR([The Xpm library was not found on your system])
  fi
])


dnl Copyright (c) 2002-2015
dnl         The Xfce development team. All rights reserved.
dnl
dnl Written for Xfce by Benedikt Meurer <benny@xfce.org>.
dnl
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 2 of the License, or
dnl (at your option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License along
dnl with this program; if not, write to the Free Software Foundation, Inc.,
dnl 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
dnl
dnl xdt-depends
dnl -----------
dnl  Contains M4 macros to check for software dependencies.
dnl  Partly based on prior work of the XDG contributors.
dnl



dnl We need recent a autoconf version
AC_PREREQ([2.69])


dnl XDT_SUPPORTED_FLAGS(VAR, FLAGS)
dnl
dnl For each token in FLAGS, checks to be sure the compiler supports
dnl the flag, and if so, adds each one to VAR.
dnl
AC_DEFUN([XDT_SUPPORTED_FLAGS],
[
  for flag in $2; do
    AC_MSG_CHECKING([if $CC supports $flag])
    saved_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS $flag"
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([ ])], [flag_supported=yes], [flag_supported=no])
    CFLAGS="$saved_CFLAGS"
    AC_MSG_RESULT([$flag_supported])

    if test "x$flag_supported" = "xyes"; then
      $1="$$1 $flag"
    fi
  done
])



dnl XDT_FEATURE_DEBUG(default_level=minimum)
dnl
AC_DEFUN([XDT_FEATURE_DEBUG],
[
  dnl weird indentation to keep output indentation correct
  AC_ARG_ENABLE([debug],
                AS_HELP_STRING([--enable-debug@<:@=no|minimum|yes|full|werror@:>@],[Build with debugging support @<:@default=m4_default([$1], [minimum])@:>@])
AS_HELP_STRING([--disable-debug],[Include no debugging support]),
                [enable_debug=$enableval], [enable_debug=m4_default([$1], [minimum])])

  dnl Enable most warnings regardless of debug level. Common flags for both C and C++.
  xdt_cv_additional_COMMON_FLAGS="-Wall -Wextra \
                                  -Wno-missing-field-initializers \
                                  -Wno-unused-parameter \
                                  -Wmissing-declarations \
                                  -Wmissing-noreturn -Wpointer-arith \
                                  -Wcast-align -Wformat -Wformat-security -Wformat-y2k \
                                  -Winit-self -Wmissing-include-dirs -Wundef \
                                  -Wredundant-decls -Wshadow"

  AC_MSG_CHECKING([whether to build with debugging support])
  if test x"$enable_debug" = x"werror" -o x"$enable_debug" = x"full" -o x"$enable_debug" = x"yes"; then
    AC_DEFINE([DEBUG], [1], [Define for debugging support])

    CPPFLAGS="$CPPFLAGS"

    if test x`uname` = x"Linux"; then
      xdt_cv_additional_COMMON_FLAGS="$xdt_cv_additional_COMMON_FLAGS -fstack-protector"
    fi

    if test x"$enable_debug" = x"werror" -o x"$enable_debug" = x"full"; then
      AC_DEFINE([DEBUG_TRACE], [1], [Define for tracing support])
      xdt_cv_additional_COMMON_FLAGS="$xdt_cv_additional_COMMON_FLAGS -O0 -g"
      CPPFLAGS="$CPPFLAGS -DG_ENABLE_DEBUG"
      if test x"$enable_debug" = x"werror"; then
        xdt_cv_additional_COMMON_FLAGS="$xdt_cv_additional_COMMON_FLAGS -Werror -Wno-error=deprecated-declarations"
        AC_MSG_RESULT([werror])
      else
        AC_MSG_RESULT([full])
      fi
    else
      xdt_cv_additional_COMMON_FLAGS="$xdt_cv_additional_COMMON_FLAGS -g"
      AC_MSG_RESULT([yes])
    fi
  else
    xdt_cv_additional_COMMON_FLAGS="$xdt_cv_additional_COMMON_FLAGS"
    CPPFLAGS="$CPPFLAGS -DNDEBUG"

    if test x"$enable_debug" = x"no"; then
      CPPFLAGS="$CPPFLAGS -DG_DISABLE_CAST_CHECKS -DG_DISABLE_ASSERT"
      AC_MSG_RESULT([no])
    else
      AC_MSG_RESULT([minimum])
    fi
  fi

  xdt_cv_additional_CFLAGS="$xdt_cv_additional_COMMON_FLAGS \
                            -Wnested-externs \
                            -Wold-style-definition"
  xdt_cv_additional_CXXFLAGS="$xdt_cv_additional_COMMON_FLAGS"

  XDT_SUPPORTED_FLAGS([supported_CFLAGS], [$xdt_cv_additional_CFLAGS])
  XDT_SUPPORTED_FLAGS([supported_CXXFLAGS], [$xdt_cv_additional_CXXFLAGS])

  CFLAGS="$CFLAGS $supported_CFLAGS"
  CXXFLAGS="$CXXFLAGS $supported_CXXFLAGS"
  if test x"$enable_debug" = x"werror"; then
    dnl Useless on C++, only generates warnings.
    CXXFLAGS="$CXXFLAGS -Wno-error=implicit-function-declaration"
  fi
])


dnl XDT_FEATURE_VISIBILITY()
dnl
dnl Checks to see if the compiler supports the 'visibility' attribute
dnl If so, adds -DHAVE_GNUC_VISIBILTY to CPPFLAGS.  Also sets the
dnl automake conditional HAVE_GNUC_VISIBILITY.
dnl
AC_DEFUN([XDT_FEATURE_VISIBILITY],
[
  AC_ARG_ENABLE([visibility],
                AS_HELP_STRING([--disable-visibility],[Don't use ELF visibility attributes]),
                [enable_visibility=$enableval], [enable_visibility=yes])
  have_gnuc_visibility=no
  if test "x$enable_visibility" != "xno"; then
    XDT_SUPPORTED_FLAGS([xdt_vis_test_cflags], [-Wall -Werror -Wno-unused-parameter])
    saved_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS $xdt_vis_test_cflags"
    AC_MSG_CHECKING([whether $CC supports the GNUC visibility attribute])
    AC_COMPILE_IFELSE([AC_LANG_SOURCE(
    [
      void test_default (void);
      void test_hidden (void);

      void __attribute__ ((visibility("default"))) test_default (void) {}
      void __attribute__ ((visibility("hidden"))) test_hidden (void) {}

      int main (int argc, char **argv) {
        test_default ();
        test_hidden ();
        return 0;
      }
    ])],
    [
      have_gnuc_visibility=yes
      AC_MSG_RESULT([yes])
    ],
    [
      AC_MSG_RESULT([no])
    ])
    CFLAGS="$saved_CFLAGS"
  fi

  if test "x$have_gnuc_visibility" = "xyes"; then
    CPPFLAGS="$CPPFLAGS -DHAVE_GNUC_VISIBILITY"
    xdt_vis_hidden_cflags=""
    XDT_SUPPORTED_FLAGS([xdt_vis_hidden_cflags], [-xldscope=hidden])
    if test "x$xdt_vis_hidden_cflags" = "x"; then
      XDT_SUPPORTED_FLAGS([xdt_vis_hidden_cflags], [-fvisibility=hidden])
    fi
    CFLAGS="$CFLAGS $xdt_vis_hidden_cflags"
  fi

  AM_CONDITIONAL([HAVE_GNUC_VISIBILITY], [test "x$have_gnuc_visibility" = "xyes"])
])

dnl XDT_FEATURE_LINKER_OPTS
dnl
dnl Checks for and enables any special linker optimizations.
dnl
AC_DEFUN([XDT_FEATURE_LINKER_OPTS],
[
  AC_ARG_ENABLE([linker-opts],
                AS_HELP_STRING([--disable-linker-opts],[Disable linker optimizations]),
                [enable_linker_opts=$enableval], [enable_linker_opts=yes])

  if test "x$enable_linker_opts" != "xno"; then
    if test x`uname` != x"OpenBSD"; then
      AC_MSG_CHECKING([whether $LD accepts --as-needed])
      case `$LD --as-needed -v 2>&1 </dev/null` in
      *GNU* | *'with BFD'*)
        LDFLAGS="$LDFLAGS -Wl,--as-needed"
        AC_MSG_RESULT([yes])
        ;;
      *)
        AC_MSG_RESULT([no])
        ;;
      esac
    fi
    AC_MSG_CHECKING([whether $LD accepts -O1])
    case `$LD -O1 -v 2>&1 </dev/null` in
    *GNU* | *'with BFD'*)
      LDFLAGS="$LDFLAGS -Wl,-O1"
      AC_MSG_RESULT([yes])
      ;;
    *)
      AC_MSG_RESULT([no])
      ;;
    esac
  fi
])

dnl Copyright (c) 2002-2020
dnl         The Xfce development team. All rights reserved.
dnl
dnl Written for Xfce by Natanael Copa <ncopa@alpinelinux.org>
dnl
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 2 of the License, or
dnl (at your option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License along
dnl with this program; if not, write to the Free Software Foundation, Inc.,
dnl 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
dnl
dnl xdt-version
dnl --------
dnl  Version initialization M4 macros.
dnl


dnl XDT_VERSION_INIT(SEMVER, [TAG])
dnl
dnl This macro takes care of setting up the version numbering.
dnl
dnl It will define the following macros based on SEMVER and TAG:
dnl
dnl  - xdt_version
dnl  - xdt_version_major
dnl  - xdt_version_minor
dnl  - xdt_version_micro
dnl  - xdt_version_tag
dnl  - xdt_version_build
dnl  - xdt_debug_default
dnl
dnl If TAG isn't specified, the xdt_version_tag and xdt_version_git
dnl will be empty and xdt_debug_default will be set to "minimum",
dnl otherwise the xdt_version_build will contain a git hash and
dnl xdt_debug_default will be set to "yes"
dnl
dnl Example usage:
dnl
dnl XDT_VERSION_INIT([4.15.3],[git])
dnl AC_INIT([xfce4-someproject],[xdt_version()])
dnl ...
dnl XDT_FEATURE_DEBUG([xdt_debug_default])
dnl

AC_DEFUN([XDT_VERSION_INIT],
[
  m4_define([xdt_version_tag], [$2])

  dnl set git revision in xdt_version_build if TAG is set
  m4_define([xdt_version_build], [ifelse(xdt_version_tag(), [git],
    [esyscmd([
      if test -d .git; then
        revision=$(git rev-parse --short HEAD 2>/dev/null)
      fi
      printf "%s" "${revision:-UNKNOWN}"
    ])])])

  dnl define xdt_debug_default to "yes" if TAG is set
  m4_define([xdt_debug_default], [ifelse(xdt_version_tag(), [git], [yes], [minimum])])

  dnl define xdt_version string
  m4_define([xdt_version], [$1][ifelse(xdt_version_tag(), [git], [xdt_version_tag()-xdt_version_build()], [xdt_version_tag()])])

  dnl define major, minor and micro
  m4_define([xdt_version_major], [esyscmd([
    version="$1"
    printf "%s" "${version%%.*}"
  ])])

  m4_define([xdt_version_minor], [esyscmd([
    version="$1"
    case "$version" in
      *.*)
        major="${version%%.*}"
        minor_micro="${version#${major}.}"
        printf "%s" "${minor_micro%%.*}"
        ;;
    esac
  ])])

  m4_define([xdt_version_micro], [esyscmd([
    version="$1"
    case "$version" in
      *.*.*)
        major=${version%%.*}
        minor_micro=${version#${major}.}
        minor="${minor_micro%%.*}"
        reminder=${version#${major}.${minor}.}
        printf "%s" "${reminder%%.*}"
        ;;
    esac
  ])])
])


m4_include([m4/gettext.m4])
m4_include([m4/iconv.m4])
m4_include([m4/intlmacosx.m4])
m4_include([m4/lib-ld.m4])
m4_include([m4/lib-link.m4])
m4_include([m4/lib-prefix.m4])
m4_include([m4/libtool.m4])
m4_include([m4/ltoptions.m4])
m4_include([m4/ltsugar.m4])
m4_include([m4/ltversion.m4])
m4_include([m4/lt~obsolete.m4])
m4_include([m4/nls.m4])
m4_include([m4/po.m4])
m4_include([m4/progtest.m4])
