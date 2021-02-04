noinst_HEADERS = \
	atlas.h \
	cmd.h \
	common.h \
	console.h \
	cvar.h \
	filesystem.h \
	image.h \
	mem.h \
	mem_buf.h \
	sys.h \
	thread.h

noinst_LTLIBRARIES = \
	libcommon.la

libcommon_la_SOURCES = \
	atlas.c \
	cmd.c \
	common.c \
	console.c \
	cvar.c \
	filesystem.c \
	image.c \
	installer.c \
	mem.c \
	mem_buf.c \
	sys.c \
	thread.c
libcommon_la_CFLAGS = \
	-I$(top_srcdir)/src \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OBJECTIVELY_CFLAGS@ \
	@SDL2_CFLAGS@
libcommon_la_LDFLAGS = \
	-shared
libcommon_la_LIBADD = \
	$(top_builddir)/src/shared/libshared.la \
	@BASE_LIBS@ \
	@GLIB_LIBS@ \
	@OBJECTIVELY_LIBS@ \
	@PHYSFS_LIBS@ \
	@SDL2_LIBS@
