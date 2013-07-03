SUBDIRS = \
	. \
	ai \
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
	image.h \
	mem.h \
	net.h \
	pmove.h \
	quake2world.h \
	shared.h \
	sys.h \
	thread.h \
	win32.h

noinst_LTLIBRARIES = \
	libcmodel.la \
	libcommon.la \
	libconsole.la \
	libfilesystem.la \
	libimage.la \
	libmem.la \
	libnet.la \
	libpmove.la \
	libshared.la \
	libswap.la \
	libsys.la \
	libthread.la \
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
	cmd.c \
	console.c \
	console-curses.c \
	cvar.c
libconsole_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@CURSES_CFLAGS@ \
	@GLIB_CFLAGS@
libconsole_la_LIBADD = \
	libfilesystem.la \
	@CURSES_LIBS@
	
libfilesystem_la_SOURCES = \
	filesystem.c
libfilesystem_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@PHYSFS_CFLAGS@
libfilesystem_la_LIBADD = \
	libmem.la \
	libswap.la \
	libsys.la \
	@PHYSFS_LIBS@

if BUILD_CLIENT

libimage_la_SOURCES = \
	image.c
libimage_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@SDL_CFLAGS@
libimage_la_LIBADD = \
	libfilesystem.la

else

libimage_la_SOURCES = \
	null.c
libimage_la_CFLAGS = \
	-D__LIBTOOL_LIBRARY__=libimage_la

endif

libmem_la_SOURCES = \
	mem.c
libmem_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@
libmem_la_LIBADD = \
	libcommon.la \
	@SDL_LIBS@

libnet_la_SOURCES = \
	net_chan.c \
	net_udp.c
libnet_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@
libnet_la_LIBADD = \
	libconsole.la

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
libshared_la_LIBADD = \
	@BASE_LIBS@ \
	@GLIB_LIBS@
	
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
	libcommon.la \
	libwin32.la

libthread_la_SOURCES = \
	thread.c
libthread_la_CFLAGS = \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@SDL_CFLAGS@
libthread_la_LIBADD = \
	libconsole.la

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
