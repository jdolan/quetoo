
noinst_HEADERS =

noinst_LTLIBRARIES = \
	libviews.la

libviews_la_SOURCES = \
	EditorView.c

libviews_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/client \
	-I$(top_srcdir)/src/client/ui \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@ \
	@OPENGL_CFLAGS@

libviews_la_LDFLAGS = \
	-shared

libviews_la_LIBADD = \
	@OBJECTIVELYMVC_LIBS@ \
	@OPENGL_LIBS@
