cguieditordir = @PKGLIBDIR@/default/ui/editor
cguieditor_DATA = \
	EditorViewController.json \
	EntityViewController.css \
	EntityViewController.json \
	MaterialViewController.css \
	MaterialViewController.json \
	MeshViewController.css \
	MeshViewController.json \
	TestScrollViewController.css \
	TestScrollViewController.json \
	editor.css \
	icon_add.png \
	icon_delete.png

noinst_HEADERS = \
	EditorViewController.h \
	EntityView.h \
	EntityViewController.h \
	MaterialViewController.h \
	MeshViewController.h \
	StageView.h \
	TableViewUtil.h \
	TestScrollViewController.h

noinst_LTLIBRARIES = \
	libcguieditor.la

libcguieditor_la_SOURCES = \
	EditorViewController.c \
	EntityView.c \
	EntityViewController.c \
	MaterialViewController.c \
	MeshViewController.c \
	StageView.c \
	TableViewUtil.c \
	TestScrollViewController.c

libcguieditor_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/cgame \
	-I$(top_srcdir)/src/cgame/default \
	-I$(top_srcdir)/src/cgame/default/ui \
	-I$(top_srcdir)/src/cgame/default/ui/common \
	-I$(top_srcdir)/src/cgame/default/ui/controls \
	-I$(top_srcdir)/src/cgame/default/ui/settings \
	@BASE_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@ \
	@OPENAL_CFLAGS@ \
	@SDL3_CFLAGS@ \
	@SNDFILE_CFLAGS@

libcguieditor_la_LDFLAGS = \
	-shared

libcguieditor_la_LIBADD = \
	@OBJECTIVELYMVC_LIBS@
