#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([raster-buffet], [dev], [jdegges@gmail.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_CANONICAL_HOST
AC_CANONICAL_BUILD
AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL

# Let user decide whether or not to build plugins
AC_ARG_ENABLE([plugins],
    [AS_HELP_STRING([--enable-plugins],
        [build all of the available plugins @<:@default=yes@:>@])],
    [],
    [enable_plugins=yes])

# Checks for optional libraries/headers
AC_ARG_WITH([pthread],
    [AS_HELP_STRING([--with-pthread],
        [use pthread library for multithreading @<:@default=check@:>@])],
    [],
    [with_pthread=check])
AS_IF([test "x$with_pthread" != xno],
    [AC_HAVE_LIBRARY([pthread])
     AC_CHECK_HEADERS([pthread.h])])

# Checks for mandatory libraries.
AC_HAVE_LIBRARY([ltdl])

# Checks for types, structures, and compiler characteristics.
AC_TYPE_UINT8_T
AC_TYPE_INT64_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset strerror strstr])

AC_DEFUN([AC_DEFINE_DIR], [
    prefix_NONE=
    exec_prefix_NONE=
    test "x$prefix" = xNONE && prefix_NONE=yes && prefix=$ac_default_prefix
    test "x$exec_prefix" = xNONE && exec_prefix_NONE=yes && exec_prefix=$prefix
    # In Autoconf 2.60, ${datadir} refers to ${datarootdir}, which in turn
    # refers to ${prefix}.  Thus we have to use `eval' twice.
    eval ac_define_dir="\"[$]$2\""
    eval ac_define_dir="\"$ac_define_dir\""
    AC_SUBST($1, "$ac_define_dir")
    AC_DEFINE_UNQUOTED($1, "$ac_define_dir", [$3])
    test "$prefix_NONE" && prefix=NONE
    test "$exec_prefix_NONE" && exec_prefix=NONE
])

AC_DEFINE_DIR([PKGLIBDIR], [libdir/${PACKAGE_NAME}], [The library directory])
AM_CONDITIONAL([COND_PLUGINS], [test "$xenable_plugins" != xno])
AC_CONFIG_FILES([Makefile src/Makefile src/plugins/Makefile])
AC_OUTPUT
