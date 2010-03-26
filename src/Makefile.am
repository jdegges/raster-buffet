if COND_PLUGINS
    MAYBE_PLUGINS = plugins
endif
SUBDIRS = . $(MAYBE_PLUGINS)

bin_PROGRAMS = rb
rb_SOURCES = main.c plugin.c plugin.h image.h
rb_LDFLAGS = -rdynamic -rpath $(pkglibdir)
rb_LDADD = $(PTHREAD_LIBS) $(LTDL_LIBS) $(LOOMLIB_LIBS)
rb_CFLAGS = $(PTHREAD_CFLAGS) $(AM_CFLAGS) $(LOOMLIB_CFLAGS)
