bin_PROGRAMS = \
	pak

pak_SOURCES = \
	main.c

pak_CFLAGS = \
	-I../.. \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@SDL_CFLAGS@

pak_LDADD = \
	../../libfilesystem.la \
	@BASE_LIBS@ \
	@GLIB_LIBS@ \
	@SDL_LIBS@ \
	-lm -lz