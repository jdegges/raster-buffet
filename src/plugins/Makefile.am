AM_CPPFLAGS = -I$(top_srcdir)/src
AM_LDFLAGS = -avoid-version -module -shared

pkglib_LTLIBRARIES =    \
    freeimage.la        \
    edges.la            \
    artistic.la         \
    v4l2.la

freeimage_la_SOURCES = freeimage.c
freeimage_la_LIBADD = $(FREEIMAGE_LIBS)

edges_la_SOURCES = edges.c
edges_la_LIBADD = $(M_LIBS)

artistic_la_SOURCES = artistic.c
artistic_la_LIBADD = $(FFTW3_LIBS)
artistic_la_CFLAGS = $(FFTW3_CFLAGS)

v4l2_la_SOURCES = v4l2.c
