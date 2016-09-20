bin_PROGRAMS = \
	quemap

noinst_HEADERS = \
	bspfile.h \
	monitor.h \
	polylib.h \
	quemap.h \
	qbsp.h \
	qlight.h \
	qvis.h \
	scriplib.h

quemap_SOURCES = \
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

quemap_CFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	@BASE_CFLAGS@ \
	@CURSES_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@SDL2_CFLAGS@ \
	@XML_CFLAGS@

quemap_LDADD = \
	../../../deps/minizip/libminizip.la \
	../../collision/libcmodel.la \
	../../net/libnet.la \
	../../libimage.la \
	../../libthread.la \
	@CURSES_LIBS@ \
	@JPEG_LIBS@ \
	@SDL2_LIBS@ \
	@XML_LIBS@

if WINDOWS

.rc.o:
	@WINDRES@ $^ -o $@

quemap_SOURCES += \
	quemap-icon.rc

endif
