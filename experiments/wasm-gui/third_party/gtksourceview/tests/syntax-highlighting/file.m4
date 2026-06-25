dnl an m4 file
AC_DEFINE([foo],[echo "Hi there!"])
AC_CHECK_FUNC([foo],[yes=yes],[yes=no])
foo()
