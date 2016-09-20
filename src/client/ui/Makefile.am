SUBDIRS = \
	views \
	viewcontrollers

noinst_HEADERS = \
	ui.h \
	ui_data.h \
	ui_local.h \
	ui_main.h

noinst_LTLIBRARIES = \
	libui.la

libui_la_SOURCES = \
	ui_data.c \
	ui_main.c

libui_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/client \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@

libui_la_LDFLAGS = \
	-shared

libui_la_LIBADD = \
	viewcontrollers/libviewcontrollers.la \
	../../libcommon.la \
	@OBJECTIVELYMVC_LIBS@

