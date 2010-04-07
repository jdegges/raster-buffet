#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([raster-buffet], [dev], [jdegges@gmail.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])
PKG_PROG_PKG_CONFIG([0.22])

# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_CANONICAL_HOST
AC_CANONICAL_BUILD
AC_PROG_LIBTOOL

# Let user decide whether or not to build plugins
AC_ARG_ENABLE([plugins],
    [AS_HELP_STRING([--enable-plugins],
        [build all of the available plugins @<:@default=yes@:>@])],
    [],
    [enable_plugins=yes])

# Checks for headers/libraries used in the core.
ACX_PTHREAD([AC_SUBST([CC], ["${PTHREAD_CC}"])],
    [AC_MSG_ERROR(['pthreads' not found])])

PKG_CHECK_MODULES([LOOMLIB], [loomlib])

AC_CHECK_HEADER([ltdl.h],
    [AC_CHECK_LIB([ltdl], [lt_dladvise_init],
        [LTDL_LIBS=-lltdl], [LTDL_LIBS=])],
    [LTDL_LIBS=])
AS_IF([test -z "$LTDL_LIBS"],
    [AC_MSG_ERROR(['libltdl' not found])])
AC_SUBST([LTDL_LIBS], [${LTDL_LIBS}])

# header/library checks for plugin dependencies
#
# check for libfreeimage
#
AC_CHECK_HEADER([FreeImage.h],
    [AC_CHECK_LIB([freeimage], [FreeImage_OpenMemory],
        [FREEIMAGE_LIBS="-lfreeimage -lstdc++"], [FREEIMAGE_LIBS=], [-lstdc++])],
    [FREEIMAGE_LIBS=])
AS_IF([test -z "$FREEIMAGE_LIBS"],
    [AC_MSG_ERROR(['libfreeimage' not found])])
AC_SUBST([FREEIMAGE_LIBS], [${FREEIMAGE_LIBS}])

# check for libm
#
AC_CHECK_HEADER([math.h],
    [AC_CHECK_LIB([m], [sqrt],
        [M_LIBS=-lm], [M_LIBS=])],
    [M_LIBS=])
AS_IF([test -z "$M_LIBS"],
    [AC_MSG_ERROR(['libm' not found])])
AC_SUBST([M_LIBS], [${M_LIBS}])

# check for libfftw3 (and complex.h)
#
AC_CHECK_HEADERS([complex.h],
    [],
    [AC_MSG_ERROR(['complex.h' was not found])])
PKG_CHECK_MODULES([FFTW3], [fftw3 >= 3.0.0])

# check for libOpenCL
#
#AC_CHECK_HEADER([CL/cl.h],
#    [AC_CHECK_LIB([OpenCL], [clGetDeviceIDs],
#        [OPENCL_LIBS=-lOpenCL], [OPENCL_LIBS=])],
#    [OPENCL_LIBS=])
#AS_IF([test -z "$OPENCL_LIBS"],
#    [AC_MSG_ERROR(['libOpenCL' not found])])
#AC_SUBST([OPENCL_LIBS], [${OPENCL_LIBS}])

# add additional header/library checks here
#   - consult examples for how to structure new checks
#   - try to use PKG_CONFIG macros wherever possible
#
# example check the PKG_CONFIG way
#PKG_CHECK_MODULES([libname], [libname >= min_version])
#
# example check the autoconf way
#AC_CHECK_HEADER([include.h],
#    [AC_CHECK_LIB([libname], [libfunction],
#        [LIBNAME_LIBS=-llibname], [LIBNAME_LIBS=])],
#    [LIBNAME_LIBS=])
#AS_IF([test -z "$LIBNAME_LIBS"],
#    [AC_MSG_ERROR(['libname' not found])])
#AC_SUBST([LIBNAME_LIBS], [${LIBNAME_LIBS}])
#

# Checks for header files.
AC_CHECK_HEADERS([inttypes.h stdlib.h string.h unistd.h])

# Checks for types, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_INT64_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset pow sqrt strcasecmp strerror strstr])

AC_DEFINE_DIR([PKGLIBDIR], [libdir/${PACKAGE_NAME}], [The library directory])
AM_CONDITIONAL([COND_PLUGINS], [test "$xenable_plugins" != xno])
AC_CONFIG_FILES([Makefile
                 src/Makefile
                 src/plugins/Makefile])
AC_OUTPUT
