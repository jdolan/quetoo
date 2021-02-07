SUBDIRS = \
	editor

noinst_HEADERS = \
	QuetooRenderer.h \
	ui.h \
	ui_data.h \
	ui_editor.h \
	ui_local.h \
	ui_main.h

noinst_LTLIBRARIES = \
	libui.la

libui_la_SOURCES = \
	QuetooRenderer.c \
	ui_data.c \
	ui_editor.c \
	ui_main.c

libui_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/client \
	-I$(top_srcdir)/src/client/renderer \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@ \
	@SDL2_CFLAGS@

libui_la_LDFLAGS = \
	-shared

libui_la_LIBADD = \
	$(top_builddir)/src/common/libcommon.la \
	$(top_builddir)/src/client/ui/editor/libuieditor.la \
	@OBJECTIVELYMVC_LIBS@
