SUBDIRS = \
	views \
	viewcontrollers

noinst_HEADERS = \
	ui.h \
	ui_context.h \
	ui_local.h \
	ui_main.h

noinst_LTLIBRARIES = \
	libui.la

libui_la_SOURCES = \
	ui_context.c \
	ui_main.c

libui_la_CFLAGS = \
	-I.. -I../.. \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@

libui_la_LDFLAGS = \
	-shared

libui_la_LIBADD = \
	viewcontrollers/libviewcontrollers.la \
	../../libcommon.la

