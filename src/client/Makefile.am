SUBDIRS = \
	renderer \
	sound \
	ui

noinst_HEADERS = \
	cl_binds.h \
	cl_keys.h \
	client.h

noinst_LTLIBRARIES = \
	libclient.la

if BUILD_CLIENT

libclient_la_SOURCES = \
	cl_client.c \
	cl_cmd.c \
	cl_console.c \
	cl_cgame.c \
	cl_demo.c \
	cl_emit.c \
	cl_entity.c \
	cl_effect.c \
	cl_forward.c \
	cl_http.c \
	cl_input.c \
	cl_keys.c \
	cl_loc.c \
	cl_main.c \
	cl_parse.c \
	cl_pred.c \
	cl_screen.c \
	cl_server.c \
	cl_tentity.c \
	cl_view.c

libclient_la_CFLAGS = \
	-I.. \
	-pipe \
	@BASE_CFLAGS@ \
	@CURL_CFLAGS@ \
	@OPENGL_CFLAGS@ \
	@SDL_CFLAGS@
	
libclient_la_LIBADD = \
	renderer/librenderer.la \
	sound/libsound.la \
	ui/libui.la \
	../libconsole.la \
	../libcmodel.la \
	../libnet.la \
	../libpmove.la \
	../libthreads.la
	
else

libclient_la_SOURCES = \
	../null.c
	
libclient_la_CFLAGS = \
	-D__LIBTOOL_LIBRARY__=libclient_la

endif
