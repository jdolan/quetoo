bin_PROGRAMS = \
	quemap

noinst_HEADERS = \
	brush.h \
	bsp.h \
	csg.h \
	entity.h \
	face.h \
	fog.h \
	leakfile.h \
	light.h \
	lightgrid.h \
	lightmap.h \
	material.h \
	monitor.h \
	patch.h \
	points.h \
	polylib.h \
	portal.h \
	prtfile.h \
	qbsp.h \
	qlight.h \
	qmat.h \
	quemap.h \
	qzip.h \
	simplex.h \
	texture.h \
	tjunction.h \
	tree.h \
	work.h \
	writebsp.h

quemap_SOURCES = \
	brush.c \
	bsp.c \
	csg.c \
	entity.c \
	face.c \
	fog.c \
	leakfile.c \
	light.c \
	lightgrid.c \
	lightmap.c \
	main.c \
	map.c \
	material.c \
	monitor.c \
	patch.c \
	polylib.c \
	portal.c \
	prtfile.c \
	qbsp.c \
	qmat.c \
	qlight.c \
	qzip.c \
	simplex.c \
	texture.c \
	tjunction.c \
	tree.c \
	work.c \
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
	$(top_builddir)/src/collision/libcollision.la \
	$(top_builddir)/src/net/libnet.la \
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
