bin_PROGRAMS = \
	pak

pak_SOURCES = \
	main.c

pak_CFLAGS = \
	-I../.. \
	-pipe \
	@BASE_CFLAGS@ \
	@SDL_CFLAGS@

pak_LDADD = \
	../../libfilesystem.la \
	@BASE_LIBS@ \
	@SDL_LIBS@ \
	-lm -lz