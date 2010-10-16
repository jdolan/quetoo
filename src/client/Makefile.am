SUBDIRS = \
	ui \
	renderer \
	sound

noinst_HEADERS = \
	cl_binds.h \
	cl_keys.h \
	client.h

noinst_LTLIBRARIES = \
	libclient.la

if BUILD_CLIENT

libclient_la_SOURCES = \
	cl_cmd.c \
	cl_console.c \
	cl_emit.c \
	cl_entity.c \
	cl_effect.c \
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
	@CURL_CFLAGS@ \
	@DEBUG_CFLAGS@ \
	@OPENGL_CFLAGS@ \
	@SDL_CFLAGS@ \
	@WARN_CFLAGS@
	
libclient_la_LIBADD = \
	ui/node/libuinode.la \
	sound/libsound.la \
	renderer/librenderer.la \
	../libconsole.la \
	../libcmodel.la \
	../libnet.la \
	../libpmove.la
	
else

libclient_la_SOURCES = \
	../null.c
	
libclient_la_CFLAGS = \
	-D__LIBTOOL_LIBRARY__=libclient_la

endif
