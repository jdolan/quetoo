SUBDIRS = \
	. \
	cgame \
	client \
	server \
	main \
	game \
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
	hash.h \
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
	libhash.la \
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
	-pipe \
	@BASE_CFLAGS@
libcmodel_la_LIBADD = \
	libfilesystem.la

libcommon_la_SOURCES = \
	common.c
libcommon_la_CFLAGS = \
	-pipe \
	@BASE_CFLAGS@ \
	@SDL_CFLAGS@
	
libconsole_la_SOURCES = \
	console.c \
	console-curses.c
libconsole_la_CFLAGS = \
	-pipe \
	@BASE_CFLAGS@ \
	@CURSES_CFLAGS@
libconsole_la_LIBADD = \
	libfilesystem.la
	
libfilesystem_la_SOURCES = \
	cmd.c \
	cvar.c \
	filesystem.c \
	pak.c
libfilesystem_la_CFLAGS = \
	-pipe \
	@BASE_CFLAGS@
libfilesystem_la_LIBADD = \
	libcommon.la \
	libhash.la \
	libmem.la \
	libshared.la \
	libswap.la \
	libsys.la \
	libwin32.la

if BUILD_CLIENT

libimages_la_SOURCES = \
	images.c
libimages_la_CFLAGS = \
	-pipe \
	@BASE_CFLAGS@ \
	@SDL_CFLAGS@
libimages_la_LIBADD = \
	libfilesystem.la

else

libimages_la_SOURCES = \
	null.c
libimages_la_CFLAGS = \
	-D__LIBTOOL_LIBRARY__=libimages_la

endif

libhash_la_SOURCES = \
	hash.c
libhash_la_CFLAGS = \
	-pipe \
	@BASE_CFLAGS@

libmem_la_SOURCES = \
	mem.c
libmem_la_CFLAGS = \
	-pipe \
	@BASE_CFLAGS@

libnet_la_SOURCES = \
	net_chan.c \
	net_udp.c
libnet_la_CFLAGS = \
	-pipe \
	@BASE_CFLAGS@
libnet_la_LIBADD = \
	libfilesystem.la

libpmove_la_SOURCES = \
	pmove.c
libpmove_la_CFLAGS = \
	-pipe \
	@BASE_CFLAGS@

libshared_la_SOURCES = \
	shared.c
libshared_la_CFLAGS = \
	-pipe \
	@BASE_CFLAGS@
	
libswap_la_SOURCES = \
	swap.c
libswap_la_CFLAGS = \
	-pipe \
	@BASE_CFLAGS@

libsys_la_SOURCES = \
	sys.c
libsys_la_CFLAGS = \
	-pipe \
	@BASE_CFLAGS@

libthreads_la_SOURCES = \
	threads.c
libthreads_la_CFLAGS = \
	-pipe \
	@BASE_CFLAGS@ \
	@SDL_CFLAGS@

if WINDOWS

libwin32_la_SOURCES = \
	win32.c
libwin32_la_CFLAGS = \
	-pipe \
	@BASE_CFLAGS@

else

libwin32_la_SOURCES = \
	null.c
libwin32_la_CFLAGS = \
	-D__LIBTOOL_LIBRARY__=libwin32_la

endif