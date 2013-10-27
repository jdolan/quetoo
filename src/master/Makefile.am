if BUILD_MASTER

bin_PROGRAMS = \
	q2wmaster

q2wmaster_SOURCES = \
	main.c

q2wmaster_CFLAGS = \
	-I.. \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@

q2wmaster_LDFLAGS = \
	-all-static

q2wmaster_LDADD = \
	../libfilesystem.la \
	@GLIB_LIBS@ -lrt \
	@PHYSFS_LIBS@ \
	@SDL_LIBS@ -lpthread

endif
