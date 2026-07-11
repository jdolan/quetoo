cguieditordir = @PKGLIBDIR@/default/ui/editor
cguieditor_DATA = \
	EditorViewController.json \
	EntityView.json \
	EntityViewController.css \
	EntityViewController.json \
	MaterialViewController.css \
	MaterialViewController.json \
	MeshViewController.css \
	MeshViewController.json \
	editor.css

noinst_HEADERS = \
	EditorViewController.h \
	EntityView.h \
	EntityViewController.h \
	MaterialViewController.h \
	MeshViewController.h

noinst_LTLIBRARIES = \
	libcguieditor.la

libcguieditor_la_SOURCES = \
	EditorViewController.c \
	EntityView.c \
	EntityViewController.c \
	MaterialViewController.c \
	MeshViewController.c

libcguieditor_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/cgame \
	-I$(top_srcdir)/src/cgame/default \
	-I$(top_srcdir)/src/cgame/default/ui \
	@BASE_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@ \
	@OPENAL_CFLAGS@ \
	@SDL3_CFLAGS@ \
	@SNDFILE_CFLAGS@

libcguieditor_la_LDFLAGS = \
	-shared

libcguieditor_la_LIBADD = \
	@OBJECTIVELYMVC_LIBS@
