noinst_HEADERS = \
	ui.h \
	ui_local.h \
	ui_main.h

noinst_LTLIBRARIES = \
	libui.la

libui_la_SOURCES = \
	ui_main.c

libui_la_CFLAGS = \
	-I.. -I../.. \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@

libui_la_LDFLAGS = \
	-shared

libui_la_LIBADD = \
	../../libcommon.la \
	@OBJECTIVELYMVC_LIBS@
