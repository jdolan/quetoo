SUBDIRS = \
	renderer \
	sound

noinst_HEADERS = \
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
	@WARN_CFLAGS@ \
	@DEBUG_CFLAGS@ \
	@SDL_CFLAGS@ \
	@CURL_CFLAGS@
endif
