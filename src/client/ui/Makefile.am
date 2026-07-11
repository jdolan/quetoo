SUBDIRS =

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
	-I$(top_srcdir)/src/client/renderer \
	@BASE_CFLAGS@ \
	@OBJECTIVELY_CFLAGS@ \
	@OBJECTIVELYGPU_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@ \
	@OPENAL_CFLAGS@ \
	@SDL3_CFLAGS@ \
	@SNDFILE_CFLAGS@

libui_la_LDFLAGS = \
	-shared

libui_la_LIBADD = \
	$(top_builddir)/src/common/libcommon.la \
	@OBJECTIVELYGPU_LIBS@ \
	@OBJECTIVELYMVC_LIBS@
