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
	lightmap.h \
	material.h \
	monitor.h \
	patch.h \
	polylib.h \
	portal.h \
	prtfile.h \
	qbsp.h \
	qlight.h \
	qmat.h \
	quemap.h \
	qvis.h \
	qzip.h \
	texinfo.h \
	tjunction.h \
	tree.h \
	vis.h \
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
	qvis.c \
	qzip.c \
	texinfo.c \
	tjunction.c \
	tree.c \
	vis.c \
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
	$(top_builddir)/src/libatlas.la \
	$(top_builddir)/src/collision/libcollision.la \
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
