noinst_HEADERS = \
	archive.h \
	asset.h \
	atlas.h \
	cmd.h \
	common.h \
	console.h \
	cvar.h \
	file_formats.h \
	filesystem.h \
	image.h \
	installer.h \
	manifest.h \
	material.h \
	mem.h \
	mem_buf.h \
	rgb9e5.h \
	sys.h \
	thread.h \
	winding.h

noinst_LTLIBRARIES = \
	libcommon.la

libcommon_la_SOURCES = \
	archive.c \
	asset.c \
	atlas.c \
	cmd.c \
	common.c \
	console.c \
	cvar.c \
	filesystem.c \
	image.c \
	installer.c \
	manifest.c \
	material.c \
	mem.c \
	mem_buf.c \
	rgb9e5.c \
	sys.c \
	thread.c \
	winding.c

libcommon_la_CFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	@BASE_CFLAGS@ \
	@OBJECTIVELY_CFLAGS@ \
	@PHYSFS_CFLAGS@ \
	@SDL3_CFLAGS@
libcommon_la_LDFLAGS = \
	-shared
libcommon_la_LIBADD = \
	$(top_builddir)/deps/minizip/libminizip.la \
	$(top_builddir)/src/shared/libshared.la \
	@BASE_LIBS@ \
	@OBJECTIVELY_LIBS@ \
	@PHYSFS_LIBS@ \
	@SDL3_LIBS@
