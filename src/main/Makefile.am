bin_PROGRAMS = \
	quake2world

quake2world_SOURCES = \
	main.c

quake2world_CFLAGS = \
	-I.. \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@SDL_CFLAGS@

quake2world_LDADD = \
	../client/libclient.la \
	../server/libserver.la \
	@SDL_LIBS@