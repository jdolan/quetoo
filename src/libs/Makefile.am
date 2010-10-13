noinst_HEADERS = \
	cmd.h \
	cmodel.h \
	common.h \
	common-anorms.h \
	console.h \
	curses.h \
	cvar.h \
	filesystem.h \
	hash.h \
	images.h \
	mem.h \
	net.h \
	pak.h \
	pmove.h \
	shared.h \
	sys.h \
	win32.h

noinst_LTLIBRARIES = \
	libcmd.la \
	libcmodel.la \
	libcommon.la \
	libconsole.la \
	libcvar.la \
	libfilesystem.la \
	libhash.la \
	libimages.la \
	libmem.la \
	libnet.la \
	libpak.la \
	libpmove.la \
	libshared.la \
	libsys.la \
	libwin32.la

libcmd_la_SOURCES = \
	cmd.c
libcmd_la_CFLAGS = \
	-I.. \
	-pipe \
	@DEBUG_CFLAGS@ \
	@WARN_CFLAGS@

libcmodel_la_SOURCES = \
	cmodel.c
libcmodel_la_CFLAGS = \
	-I.. \
	-pipe \
	@DEBUG_CFLAGS@ \
	@WARN_CFLAGS@ 

libcommon_la_SOURCES = \
	common.c
libcommon_la_CFLAGS = \
	-I.. \
	-pipe \
	@DEBUG_CFLAGS@ \
	@SDL_CFLAGS@ \
	@WARN_CFLAGS@
	
libconsole_la_SOURCES = \
	console.c \
	curses.c
libconsole_la_CFLAGS = \
	-I.. \
	-pipe \
	@DEBUG_CFLAGS@ \
	@CURSES_CFLAGS@ \
	@WARN_CFLAGS@
	
libcvar_la_SOURCES = \
	cvar.c
libcvar_la_CFLAGS = \
	-I.. \
	-pipe \
	@DEBUG_CFLAGS@ \
	@WARN_CFLAGS@

libfilesystem_la_SOURCES = \
	filesystem.c
libfilesystem_la_CFLAGS = \
	-I.. \
	-pipe \
	@DEBUG_CFLAGS@ \
	@WARN_CFLAGS@

libimages_la_SOURCES = \
	images.c
libimages_la_CFLAGS = \
	-I.. \
	-pipe \
	@DEBUG_CFLAGS@ \
	@SDL_CFLAGS@ \
	@WARN_CFLAGS@

libhash_la_SOURCES = \
	hash.c
libhash_la_CFLAGS = \
	-I.. \
	-pipe \
	@DEBUG_CFLAGS@ \
	@WARN_CFLAGS@

libmem_la_SOURCES = \
	mem.c
libmem_la_CFLAGS = \
	-I.. \
	-pipe \
	@DEBUG_CFLAGS@ \
	@WARN_CFLAGS@

libnet_la_SOURCES = \
	net_chan.c \
	net_udp.c
libnet_la_CFLAGS = \
	-I.. \
	-pipe \
	@DEBUG_CFLAGS@ \
	@WARN_CFLAGS@
	
libpak_la_SOURCES = \
	pak.c
libpak_la_CFLAGS = \
	-I.. \
	-pipe \
	@DEBUG_CFLAGS@ \
	@WARN_CFLAGS@
	
libpmove_la_SOURCES = \
	pmove.c
libpmove_la_CFLAGS = \
	-I.. \
	-pipe \
	@DEBUG_CFLAGS@ \
	@WARN_CFLAGS@

libshared_la_SOURCES = \
	shared.c
libshared_la_CFLAGS = \
	-I.. \
	-pipe \
	@DEBUG_CFLAGS@ \
	@WARN_CFLAGS@

libsys_la_SOURCES = \
	sys.c
libsys_la_CFLAGS = \
	-I.. \
	-pipe \
	@DEBUG_CFLAGS@ \
	@WARN_CFLAGS@
	
libwin32_la_SOURCES = \
	win32.c
libwin32_la_CFLAGS = \
	-I.. \
	-pipe \
	@DEBUG_CFLAGS@ \
	@WARN_CFLAGS@