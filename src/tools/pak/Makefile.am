bin_PROGRAMS = \
	pak

pak_SOURCES = \
	main.c

pak_CFLAGS = \
	-I../.. \
	-pipe \
	@DEBUG_CFLAGS@ \
	@SDL_CFLAGS@ \
	@WARN_CFLAGS@

pak_LDADD = \
	../../libfilesystem.la \
	@DEBUG_LIBS@ \
	@HOST_LIBS@ \
	@SDL_LIBS@ \
	-lm -lz