noinst_HEADERS = \
	EditorViewController.h

noinst_LTLIBRARIES = \
	libviewcontrollers.la

libviewcontrollers_la_SOURCES = \
	EditorViewController.c

libviewcontrollers_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/client \
	-I$(top_srcdir)/src/client/ui \
	-I$(top_srcdir)/src/client/ui/views \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@

libviewcontrollers_la_LDFLAGS = \
	-shared

libviewcontrollers_la_LIBADD = \
	$(top_builddir)/src/client/ui/views/libviews.la
