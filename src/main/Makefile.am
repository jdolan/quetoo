SUFFIXES = .rc
.rc.o:
	@WINDRES@ $< -o $@

bin_PROGRAMS = \
	quake2world

quake2world_SOURCES = \
	main.c

EXTRA_quake2world_SOURCES = \
	quake2world-icon.rc

quake2world_DEPENDENCIES = \
	${ICON}

quake2world_CFLAGS = \
	-I.. \
	-pipe \
	@BASE_CFLAGS@ \
	@CURSES_CFLAGS@ \
	@OPENGL_CFLAGS@ \
	@SDL_CFLAGS@

quake2world_LDADD = \
	${quake2world_DEPENDENCIES} \
	../client/libclient.la \
	../server/libserver.la \
	@BASE_LIBS@ \
	@CURL_LIBS@ \
	@CURSES_LIBS@ \
	@JPEG_LIBS@ \
	@OPENGL_LIBS@ \
	@SDL_LIBS@ \
	-lm -lz
