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
AM_PROG_AR
AC_PROG_LIBTOOL
LT_CONFIG_LTDL_DIR([libltdl])
LTDL_INIT

# Enable debug build
AC_ARG_ENABLE([debug],
    AC_HELP_STRING([--enable-debug], [Build in debug mode.]),
    [AC_DEFINE([BUILD_DEBUG], [1], [Build in debug mode.])],
    [AC_DEFINE([BUILD_DEBUG], [0], [Do not build in debug mode.])])

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
        [AC_SUBST([FREEIMAGE_LIBS], ["-lfreeimage -lstdc++"])],
        [],
        [-lstdc++])],
    [])

# check for libm
#
AC_CHECK_HEADER([math.h],
    [AC_CHECK_LIB([m], [sqrt],
        [AC_SUBST([M_LIBS], ["-lm"])], [M_LIBS=])],
    [M_LIBS=])

# check for libfftw3 (and complex.h)
#
AC_CHECK_HEADERS([complex.h],
    [PKG_CHECK_MODULES([FFTW3], [fftw3 >= 3.0.0], [], [])])

# check for libOpenCL
#
AC_CHECK_HEADER([CL/cl.h],
    [AC_CHECK_LIB([OpenCL], [clGetDeviceIDs],
        [AC_SUBST([OPENCL_LIBS], ["-lOpenCL"])])])

PKG_CHECK_MODULES([libswscale], [libswscale >= 0.10.0], [], [])
PKG_CHECK_MODULES([libavutil], [libavutil >= 50.12.0], [], [])
PKG_CHECK_MODULES([libavcodec], [libavcodec >= 52.59.0], [], [])

# check for V4L2 headers
#
AC_CHECK_HEADER([linux/videodev2.h], [HAVE_VIDEODEV2_H=1])


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

# Conditionally mark plugins for building
#
BUILD_ARTISTIC=no
AS_IF([test "$FFTW3_LIBS"], [BUILD_ARTISTIC=yes])
AC_ARG_WITH([artistic],
    AC_HELP_STRING([--without-artistic], [Do not build the artistic plugin.]),
    [BUILD_ARTISTIC=no])
AC_SUBST([BUILD_ARTISTIC], [${BUILD_ARTISTIC}])
AC_SUBST([ARTISTIC_LIBADD], [${FFTW3_LIBS}])
AC_SUBST([ARTISTIC_CFLAGS], [${FFTW3_CFLAGS}])
AM_CONDITIONAL([BUILD_ARTISTIC], [test x$BUILD_ARTISTIC = xyes])

BUILD_EDGES=no
AS_IF([test "$M_LIBS"], [BUILD_EDGES=yes])
AC_ARG_WITH([edges],
    AC_HELP_STRING([--without-edges], [Do not build the edges plugin.]),
    [BUILD_EDGES=no])
AC_SUBST([BUILD_EDGES], [${BUILD_EDGES}])
AC_SUBST([EDGES_LIBADD], [${M_LIBS}])
AM_CONDITIONAL([BUILD_EDGES], [test x$BUILD_EDGES = xyes])

BUILD_FREEIMAGE=no
AS_IF([test "$FREEIMAGE_LIBS"], [BUILD_FREEIMAGE=yes])
AC_ARG_WITH([freeimage],
    AC_HELP_STRING([--without-freeimage], [Do not build the freeimage plugin.]),
    [BUILD_FREEIMAGE=no])
AC_SUBST([BUILD_FREEIMAGE], [${BUILD_FREEIMAGE}])
AC_SUBST([FREEIMAGE_LIBADD], [${FREEIMAGE_LIBS}])
AM_CONDITIONAL([BUILD_FREEIMAGE], [test x$BUILD_FREEIMAGE = xyes])

BUILD_SIMPLEIO=yes
AC_ARG_WITH([simpleio],
    AC_HELP_STRING([--without-simpleio], [Do not build the simpleio plugin.]),
    [BUILD_SIMPLEIO=no])
AC_SUBST([BUILD_SIMPLEIO], [${BUILD_SIMPLEIO}])
AM_CONDITIONAL([BUILD_SIMPLEIO], [test x$BUILD_SIMPLEIO = xyes])

BUILD_SWSCALE=no
AS_IF([test "$libswscale_LIBS" -a "$libavutil_LIBS" -a "$libavcodec_LIBS"],
    [BUILD_SWSCALE=yes])
AC_ARG_WITH([swscale],
    AC_HELP_STRING([--without-swscale], [Do not build the swscale plugin.]),
    [BUILD_SWSCALE=no])
AC_SUBST([BUILD_SWSCALE], [${BUILD_SWSCALE}])
AC_SUBST([SWSCALE_LIBADD],
    ["${libswscale_LIBS} ${libavutil_LIBS} ${libavcodec_LIBS} ${LOOMLIB_LIBS}"])
AC_SUBST([SWSCALE_CFLAGS],
    ["${libswscale_CFLAGS} ${libavutil_CFLAGS} ${libavcodec_CFLAGS} ${LOOMLIB_CFLAGS}"])
AM_CONDITIONAL([BUILD_SWSCALE], [test x$BUILD_SWSCALE = xyes])

BUILD_V4L2=no
AS_IF([test "$HAVE_VIDEODEV2_H"], [BUILD_V4L2=yes])
AC_ARG_WITH([v4l2],
    AC_HELP_STRING([--without-v4l2], [Do not build the V4L2 plugin.]),
    [BUILD_V4L2=no])
AC_SUBST([BUILD_V4L2], [${BUILD_V4L2}])
AM_CONDITIONAL([BUILD_V4L2], [test x$BUILD_V4L2 = xyes])


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
                 src/plugins/Makefile
                 scripts/Makefile
                 scripts/aeps_easy])
AC_OUTPUT

echo
echo "raster-buffet configure summary"
echo "==============================="
echo "Artistic plugin  : $BUILD_ARTISTIC"
echo "Edges plugin     : $BUILD_EDGES"
echo "FreeImage plugin : $BUILD_FREEIMAGE"
echo "SimpleIO plugin  : $BUILD_SIMPLEIO"
echo "SWScale plugin   : $BUILD_SWSCALE"
echo "V4L2 plugin      : $BUILD_V4L2"
echo
