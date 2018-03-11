bin_PROGRAMS = \
	quemap

noinst_HEADERS = \
	bspfile.h \
	materials.h \
	monitor.h \
	polylib.h \
	quemap.h \
	qbsp.h \
	qlight.h \
	qmat.h \
	qvis.h \
	qzip.h \
	scriptlib.h

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
	materials.c \
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
	scriptlib.c \
	textures.c \
	threads.c \
	tjunctions.c \
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
	$(top_builddir)/deps/minizip/libminizip.la \
	$(top_builddir)/src/collision/libcmodel.la \
	$(top_builddir)/src/net/libnet.la \
	$(top_builddir)/src/libimage.la \
	$(top_builddir)/src/libthread.la \
	@CURSES_LIBS@ \
	@SDL2_LIBS@ \
	@XML_LIBS@

if MINGW

.rc.o:
	@WINDRES@ $^ -o $@

quemap_SOURCES += \
	quemap-icon.rc

quemap_LDFLAGS = \
	-mconsole

endif
