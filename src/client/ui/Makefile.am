SUBDIRS = \
	renderers \
	views \
	viewcontrollers

noinst_HEADERS = \
	ui.h \
	ui_data.h \
	ui_editor.h \
	ui_local.h \
	ui_main.h

noinst_LTLIBRARIES = \
	libui.la

libui_la_SOURCES = \
	ui_data.c \
	ui_editor.c \
	ui_main.c

libui_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/client \
	-I$(top_srcdir)/src/client/renderer \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@

libui_la_LDFLAGS = \
	-shared

libui_la_LIBADD = \
	renderers/librenderers.la \
	viewcontrollers/libviewcontrollers.la \
	$(top_builddir)/src/libcommon.la \
	@OBJECTIVELYMVC_LIBS@
