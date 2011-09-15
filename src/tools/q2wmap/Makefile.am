bin_PROGRAMS = \
	q2wmap

noinst_HEADERS = \
	bspfile.h \
	polylib.h \
	q2wmap.h \
	qbsp.h \
	qlight.h \
	qvis.h \
	scriplib.h

q2wmap_SOURCES = \
	brushbsp.c \
	bspfile.c \
	csg.c \
	faces.c \
	flow.c \
	leakfile.c \
	lightmap.c \
	main.c \
	map.c \
	patches.c \
	polylib.c \
	portals.c \
	prtfile.c \
	qbsp.c \
	qmat.c \
	qpak.c \
	qlight.c \
	qvis.c \
	scriplib.c \
	textures.c \
	threads.c \
	tree.c \
	writebsp.c

q2wmap_CFLAGS = \
	-I../.. \
	-pipe \
	@BASE_CFLAGS@ \
	@SDL_CFLAGS@

q2wmap_LDADD = \
	../../libcmodel.la \
	../../libimages.la \
	../../libthreads.la \
	@BASE_LIBS@ \
	@CURSES_LIBS@ \
	@JPEG_LIBS@ \
	@SDL_LIBS@ \
	-lm -lz

