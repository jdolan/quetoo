if BUILD_MASTER

bin_PROGRAMS = \
	q2wmaster

q2wmaster_SOURCES = \
	main.c

q2wmaster_CFLAGS = \
	-I.. \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@

q2wmaster_LDADD = \
	../libfilesystem.la

# Build a statically linked master server on Linux

q2wmaster_static: q2wmaster-main.o
	libtool link gcc -all-static -o q2wmaster \
		q2wmaster-main.o ../libfilesystem.la \
		@GLIB_LIBS@ -lrt @PHYSFS_LIBS@ @SDL_LIBS@ -lpthread

endif
