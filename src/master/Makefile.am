if BUILD_MASTER

bin_PROGRAMS = \
	q2wmaster

q2wmaster_SOURCES = \
	main.c

q2wmaster_CFLAGS = \
	-I.. \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@SDL_CFLAGS@

q2wmaster_LDADD = \
	../libfs.la \
	@BASE_LIBS@ \
	@GLIB_LIBS@ \
	@SDL_LIBS@ \
	-lz

endif