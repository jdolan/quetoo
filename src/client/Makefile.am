SUBDIRS = \
	renderer \
	sound \
	ui

noinst_HEADERS = \
	cl_binds.h \
	cl_cgame.h \
	cl_cmd.h \
	cl_console.h \
	cl_demo.h \
	cl_entity.h \
	cl_input.h \
	cl_keys.h \
	cl_local.h \
	cl_main.h \
	cl_media.h \
	cl_mouse.h \
	cl_parse.h \
	cl_predict.h \
	cl_renderer.h \
	cl_screen.h \
	cl_server.h \
	cl_sound.h \
	cl_types.h \
	client.h

noinst_LTLIBRARIES = \
	libclient.la \
	libclient_null.la

libclient_la_SOURCES = \
	cl_cgame.c \
	cl_cmd.c \
	cl_console.c \
	cl_demo.c \
	cl_entity.c \
	cl_input.c \
	cl_keys.c \
	cl_main.c \
	cl_media.c \
	cl_mouse.c \
	cl_parse.c \
	cl_predict.c \
	cl_renderer.c \
	cl_screen.c \
	cl_server.c \
	cl_sound.c

libclient_la_CFLAGS = \
	-I$(top_srcdir)/src \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OPENAL_CFLAGS@ \
	@OPENGL_CFLAGS@ \
	@SDL2_CFLAGS@ \
	@SNDFILE_CFLAGS@

libclient_la_LDFLAGS = \
	-shared

libclient_la_LIBADD = \
	renderer/librenderer.la \
	sound/libsound.la \
	ui/libui.la

libclient_null_la_SOURCES = \
	cl_null.c

libclient_null_la_CFLAGS = \
	-I$(top_srcdir)/src \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OPENAL_CFLAGS@ \
	@OPENGL_CFLAGS@ \
	@SDL2_CFLAGS@ \
	@SNDFILE_CFLAGS@

libclient_null_la_LDFLAGS = \
	-shared
