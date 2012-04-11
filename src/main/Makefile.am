bin_PROGRAMS = \
	quake2world

quake2world_SOURCES = \
	main.c

quake2world_CFLAGS = \
	-I.. \
	@BASE_CFLAGS@ \
	@CURSES_CFLAGS@ \
	@OPENGL_CFLAGS@ \
	@SDL_CFLAGS@

quake2world_LDADD = \
	../client/libclient.la \
	../server/libserver.la \
	@BASE_LIBS@ \
	@ANTTWEAKBAR_LIBS@ \
	@CURL_LIBS@ \
	@CURSES_LIBS@ \
	@JPEG_LIBS@ \
	@OPENGL_LIBS@ \
	@SDL_LIBS@ \
	-lm -lz -lstdc++
	
else

quake2world_CFLAGS = \
	-I.. \
	@BASE_CFLAGS@ \
	@CURSES_CFLAGS@ \
	@SDL_CFLAGS@

quake2world_LDADD = \
	../client/libclient.la \
	../server/libserver.la \
	@BASE_LIBS@ \
	@CURSES_LIBS@ \
	@SDL_LIBS@ \
	-lm -lz
	
endif