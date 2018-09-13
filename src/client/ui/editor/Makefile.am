editordir = @PKGLIBDIR@/default/ui/editor
editor_DATA = \
	EditorView.css \
	EditorView.json

noinst_HEADERS = \
	EditorView.h \
	EditorViewController.h

noinst_LTLIBRARIES = \
	libuieditor.la

libuieditor_la_SOURCES = \
	EditorView.c \
	EditorViewController.c

libuieditor_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/client \
	-I$(top_srcdir)/src/client/renderer \
	-I$(top_srcdir)/src/client/ui \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@ \
	@SDL2_CFLAGS@

libuieditor_la_LDFLAGS = \
	-shared

libuieditor_la_LIBADD = \
	$(top_builddir)/src/client/renderer/librenderer.la \
	@OBJECTIVELYMVC_LIBS@
