AM_CPPFLAGS = -I$(top_srcdir)/src
AM_LDFLAGS = -avoid-version -module -shared

pkglib_LTLIBRARIES = 

if BUILD_ARTISTIC
pkglib_LTLIBRARIES += artistic.la
artistic_la_SOURCES = artistic.c
artistic_la_LIBADD = $(ARTISTIC_LIBADD)
artistic_la_CFLAGS = $(ARTISTIC_CFLAGS)
endif

if BUILD_EDGES
pkglib_LTLIBRARIES += edges.la
edges_la_SOURCES = edges.c
edges_la_LIBADD = $(EDGES_LIBADD)
endif

if BUILD_FREEIMAGE
pkglib_LTLIBRARIES += freeimage.la
freeimage_la_SOURCES = freeimage.c
freeimage_la_LIBADD = $(FREEIMAGE_LIBADD)
endif

if BUILD_SIMPLEIO
pkglib_LTLIBRARIES += simpleio.la
simpleio_la_SOURCES= simpleio.c
endif

if BUILD_SWSCALE
pkglib_LTLIBRARIES += swscale.la
swscale_la_SOURCES = swscale.c
swscale_la_LIBADD = $(SWSCALE_LIBADD)
swscale_la_CFLAGS = $(SWSCALE_CFLAGS)
endif

if BUILD_V4L2
pkglib_LTLIBRARIES += v4l2.la
v4l2_la_SOURCES = v4l2.c
endif
