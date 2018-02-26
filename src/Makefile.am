SUBDIRS = \
	. \
	collision \
	net \
	server \
	game \
	ai \
	client \
	cgame \
	main \
	tools \
	tests

noinst_HEADERS = \
	cmd.h \
	common.h \
	console.h \
	cvar.h \
	filesystem.h \
	image.h \
	matrix.h \
	mem.h \
	mem_buf.h \
	quetoo.h \
	shared.h \
	swap.h \
	sys.h \
	thread.h

noinst_LTLIBRARIES = \
	libcommon.la \
	libconsole.la \
	libfilesystem.la \
	libimage.la \
	libmatrix.la \
	libmem.la \
	libparse.la \
	libshared.la \
	libswap.la \
	libsys.la \
	libthread.la

libcommon_la_SOURCES = \
	common.c
libcommon_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@SDL2_CFLAGS@
libcommon_la_LDFLAGS = \
	-shared
libcommon_la_LIBADD = \
	libshared.la
	
libconsole_la_SOURCES = \
	cmd.c \
	console.c \
	cvar.c
libconsole_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@
libconsole_la_LDFLAGS = \
	-shared
libconsole_la_LIBADD = \
	libfilesystem.la

libfilesystem_la_SOURCES = \
	filesystem.c
libfilesystem_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@
libfilesystem_la_LDFLAGS = \
	-shared
libfilesystem_la_LIBADD = \
	libmem.la \
	libswap.la \
	libsys.la \
	@PHYSFS_LIBS@

libimage_la_SOURCES = \
	image.c
libimage_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@SDL2_CFLAGS@
libimage_la_LDFLAGS = \
	-shared
libimage_la_LIBADD = \
	libfilesystem.la

libmatrix_la_SOURCES = \
	matrix.c
libmatrix_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@
libmatrix_la_LDFLAGS = \
	-shared
libmatrix_la_LIBADD = \
	@BASE_LIBS@

libmem_la_SOURCES = \
	mem.c \
	mem_buf.c
libmem_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@SDL2_CFLAGS@
libmem_la_LDFLAGS = \
	-shared
libmem_la_LIBADD = \
	@BASE_LIBS@ \
	@GLIB_LIBS@ \
	@SDL2_LIBS@

libparse_la_SOURCES = \
	parse.c
libparse_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@
libparse_la_LDFLAGS = \
	-shared
libparse_la_LIBADD = \
	@BASE_LIBS@

libshared_la_SOURCES = \
	shared.c \
	shared-anorms.c
libshared_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@
libshared_la_LDFLAGS = \
	-shared
libshared_la_LIBADD = \
	@BASE_LIBS@ \
	@GLIB_LIBS@

libswap_la_SOURCES = \
	swap.c
libswap_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@
libswap_la_LDFLAGS = \
	-shared

libsys_la_SOURCES = \
	sys.c
libsys_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@SDL2_CFLAGS@
libsys_la_LDFLAGS = \
	-shared
libsys_la_LIBADD = \
	libcommon.la

libthread_la_SOURCES = \
	thread.c
libthread_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@SDL2_CFLAGS@
libthread_la_LDFLAGS = \
	-shared
libthread_la_LIBADD = \
	libmem.la
