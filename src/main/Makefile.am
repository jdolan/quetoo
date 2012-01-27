bin_PROGRAMS = \
	quake2world

quake2world_SOURCES = \
	main.c

if WINDOWS

SUFFIXES = .rc

.rc.o:
	@WINDRES@ $< -o $@

quake2world_SOURCES += \
	quake2world-icon.rc

endif
	
if BUILD_CLIENT

quake2world_CFLAGS = \
	-I.. \
	-pipe \
	@BASE_CFLAGS@ \
	@CURSES_CFLAGS@ \
	@OPENGL_CFLAGS@ \
	@SDL_CFLAGS@

quake2world_LDADD = \
	../client/libclient.la \
	../server/libserver.la \
	@BASE_LIBS@ \
	@ANTTWEAKBAR_LIBS@ \
	-lstdc++ \
	@CURL_LIBS@ \
	@CURSES_LIBS@ \
	@JPEG_LIBS@ \
	@OPENGL_LIBS@ \
	@SDL_LIBS@ \
	-lm -lz
	
else

quake2world_CFLAGS = \
	-I.. \
	-pipe \
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