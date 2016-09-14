if BUILD_MASTER

bin_PROGRAMS = \
	quetoo-master

quetoo_master_SOURCES = \
	main.c

quetoo_master_CFLAGS = \
	-I.. \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@

quetoo_master_LDADD = \
	../libfilesystem.la

# Build a statically linked master server on Linux

quetoo-master-static: quetoo_master-main.o
	libtool link gcc -all-static -o quetoo-master \
		quetoo_master-main.o ../libfilesystem.la \
		@GLIB_LIBS@ -lrt @PHYSFS_LIBS@ @SDL2_LIBS@ -lpthread

endif
