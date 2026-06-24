dnl
dnl Copyright (c) 2002 Dean Povey <povey@wedgetail.com>
dnl Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>.
dnl



dnl # AC_PROG_PERL_MODULES([MODULES], [ACTION-IF-TRUE], [ACTION-IF-FALSE])
dnl #
dnl #  Checks to see if the the given perl modules are available. If true the
dnl # shell commands in ACTION-IF-TRUE are executed. If not the shell commands
dnl # in ACTION-IF-FALSE are run. Note if $PERL is not set (for example by
dnl # calling AC_CHECK_PROG(), or AC_PATH_PROG), AC_CHECK_PROG(PERL, perl, perl)
dnl # will be run.
dnl #
AC_DEFUN([AC_PROG_PERL_MODULES],[dnl
ac_perl_modules="$1"
# Make sure we have perl
if test -z "$PERL"; then
AC_CHECK_PROG(PERL,perl,perl)
fi

if test "x$PERL" != x; then
  ac_perl_modules_failed=0
  for ac_perl_module in $ac_perl_modules; do
    AC_MSG_CHECKING(for perl module $ac_perl_module)

    # Would be nice to log result here, but can't rely on autoconf internals
    $PERL "-M$ac_perl_module" -e exit > /dev/null 2>&1
    if test $? -ne 0; then
      AC_MSG_RESULT(no);
      ac_perl_modules_failed=1
   else
      AC_MSG_RESULT(ok);
    fi
  done

  # Run optional shell commands
  if test "$ac_perl_modules_failed" = 0; then
    :
    $2
  else
    :
    $3
  fi
else
  AC_MSG_WARN(could not find perl)
fi])dnl


