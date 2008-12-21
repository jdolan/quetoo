bin_PROGRAMS = q2wmap

noinst_HEADERS = bspfile.h polylib.h q2wmap.h qbsp.h qlight.h qvis.h \
	scriplib.h

q2wmap_SOURCES = brushbsp.c bspfile.c csg.c dummy.c faces.c flow.c \
	leakfile.c lightmap.c main.c map.c patches.c polylib.c portals.c \
	prtfile.c qbsp.c qmat.c qpak.c qlight.c qvis.c scriplib.c textures.c \
	threads.c tree.c writebsp.c

q2wmap_CFLAGS = -pipe -I../../ @WARN_CFLAGS@ @SDL_CFLAGS@

q2wmap_LDADD = ../../libcmodel.la ../../libcommon.la ../../libfiles.la \
	../../libimages.la ../../libpak.la ../../libshared.la ../../libsys.la \
	@SDL_LIBS@ @CURSES_LIBS@ -lm -lz

