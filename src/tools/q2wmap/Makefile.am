bin_PROGRAMS = \
	q2wmap

noinst_HEADERS = \
	bspfile.h \
	monitor.h \
	polylib.h \
	q2wmap.h \
	qbsp.h \
	qlight.h \
	qvis.h \
	scriplib.h

q2wmap_SOURCES = \
	brush.c \
	bspfile.c \
	csg.c \
	faces.c \
	flow.c \
	leakfile.c \
	lightmap.c \
	main.c \
	map.c \
	monitor.c \
	patches.c \
	polylib.c \
	portals.c \
	prtfile.c \
	qaas.c \
	qbsp.c \
	qmat.c \
	qlight.c \
	qvis.c \
	qzip.c \
	scriplib.c \
	textures.c \
	threads.c \
	tree.c \
	writebsp.c

q2wmap_CFLAGS = \
	-I../.. \
	@BASE_CFLAGS@ \
	@CURSES_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@SDL_CFLAGS@ \
	@XML_CFLAGS@

q2wmap_LDADD = \
	../../../deps/minizip/libminizip.la \
	../../collision/libcmodel.la \
	../../net/libnet.la \
	../../libimage.la \
	../../libthread.la \
	@CURSES_LIBS@ \
	@JPEG_LIBS@ \
	@SDL_LIBS@ \
	@XML_LIBS@
