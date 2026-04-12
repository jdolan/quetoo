bin_PROGRAMS = \
	quemap

noinst_HEADERS = \
	brush.h \
	bsp.h \
	csg.h \
	entity.h \
	face.h \
	leakfile.h \
	light.h \
	material.h \
	patch.h \
	points.h \
	polylib.h \
	portal.h \
	qbsp.h \
	qlight.h \
	quemap.h \
	qzip.h \
	simplex.h \
	texture.h \
	tjunction.h \
	tree.h \
	voxel.h \
	work.h \
	writebsp.h

quemap_SOURCES = \
	brush.c \
	bsp.c \
	csg.c \
	entity.c \
	face.c \
	leakfile.c \
	light.c \
	main.c \
	map.c \
	material.c \
	patch.c \
	polylib.c \
	portal.c \
	qbsp.c \
	qlight.c \
	qzip.c \
	simplex.c \
	texture.c \
	tjunction.c \
	tree.c \
	voxel.c \
	work.c \
	writebsp.c

quemap_CFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	@BASE_CFLAGS@ \
	@CURSES_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@SDL3_CFLAGS@

quemap_LDADD = \
	$(top_builddir)/deps/minizip/libminizip.la \
	$(top_builddir)/src/collision/libcollision.la \
	$(top_builddir)/src/net/libnet.la \
	@CURSES_LIBS@ \
	@SDL3_LIBS@

if MINGW

.rc.o:
	@WINDRES@ $^ -o $@

quemap_SOURCES += \
	quemap-icon.rc

quemap_LDFLAGS = \
	-mconsole

endif
