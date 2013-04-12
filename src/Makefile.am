SUBDIRS = \
	. \
	cgame \
	client \
	server \
	main \
	game \
	tests \
	tools \
	master

noinst_HEADERS = \
	cmd.h \
	cmodel.h \
	common.h \
	common-anorms.h \
	console.h \
	cvar.h \
	filesystem.h \
	images.h \
	mem.h \
	net.h \
	pak.h \
	pmove.h \
	quake2world.h \
	shared.h \
	sys.h \
	threads.h \
	win32.h

noinst_LTLIBRARIES = \
	libcmodel.la \
	libcommon.la \
	libconsole.la \
	libfilesystem.la \
	libimages.la \
	libmem.la \
	libnet.la \
	libpmove.la \
	libshared.la \
	libswap.la \
	libsys.la \
	libthreads.la \
	libwin32.la

libcmodel_la_SOURCES = \
	cmodel.c
libcmodel_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@
libcmodel_la_LIBADD = \
	libfilesystem.la

libcommon_la_SOURCES = \
	common.c
libcommon_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@SDL_CFLAGS@
libcommon_la_LIBADD = \
	libshared.la
	
libconsole_la_SOURCES = \
	console.c \
	console-curses.c
libconsole_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@CURSES_CFLAGS@ \
	@GLIB_CFLAGS@
libconsole_la_LIBADD = \
	libfilesystem.la
	
libfilesystem_la_SOURCES = \
	cmd.c \
	cvar.c \
	filesystem.c \
	pak.c
libfilesystem_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@
libfilesystem_la_LIBADD = \
	libmem.la \
	libswap.la \
	libsys.la \
	libwin32.la

if BUILD_CLIENT

libimages_la_SOURCES = \
	images.c
libimages_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@SDL_CFLAGS@
libimages_la_LIBADD = \
	libfilesystem.la

else

libimages_la_SOURCES = \
	null.c
libimages_la_CFLAGS = \
	-D__LIBTOOL_LIBRARY__=libimages_la

endif

libmem_la_SOURCES = \
	mem.c
libmem_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@
libmem_la_LIBADD = \
	libcommon.la

libnet_la_SOURCES = \
	net_chan.c \
	net_udp.c
libnet_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@
libnet_la_LIBADD = \
	libfilesystem.la

libpmove_la_SOURCES = \
	pmove.c
libpmove_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@
libpmove_la_LIBADD = \
	libshared.la

libshared_la_SOURCES = \
	shared.c
libshared_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@
	
libswap_la_SOURCES = \
	swap.c
libswap_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@

libsys_la_SOURCES = \
	sys.c
libsys_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@
libsys_la_LIBADD = \
	libcommon.la

libthreads_la_SOURCES = \
	threads.c
libthreads_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@SDL_CFLAGS@
libthreads_la_LIBADD = \
	libcommon.la

if WINDOWS

libwin32_la_SOURCES = \
	win32.c
libwin32_la_CFLAGS = \
	@BASE_CFLAGS@

else

libwin32_la_SOURCES = \
	null.c
libwin32_la_CFLAGS = \
	-D__LIBTOOL_LIBRARY__=libwin32_la

endif
