AM_CFLAGS = -I..
AM_LDFLAGS = -avoid-version -module -shared

pkglib_LTLIBRARIES =    \
    freeimage.la        \
    edges.la            \
    artistic.la

freeimage_la_SOURCES = freeimage.c
freeimage_la_LIBADD = -lfreeimage -lstdc++

edges_la_SOURCES = edges.c
edges_la_LIBADD = -lm

artistic_la_SOURCES = artistic.c
artistic_la_LIBADD = -lfftw3 -lm

