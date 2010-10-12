SUBDIRS = \
	client \
	server \
	. \
	game \
	tools \
	master

SUFFIXES = .rc
.rc.o:
	@WINDRES@ $< -o $@

noinst_HEADERS = \
	anorms.h \
	binds.h \
	cmd.h \
	common.h \
	console.h \
	cvar.h \
	files.h \
	filesystem.h \
	hash.h \
	images.h \
	keys.h \
	mem.h \
	net.h \
	pak.h \
	shared.h \
	sys.h \
	win32.h

noinst_LTLIBRARIES = \
	libcmd.la \
	libcmodel.la \
	libcommon.la \
	libcvar.la \
	libfilesystem.la \
	libhash.la \
	libimages.la \
	libmem.la \
	libnet.la \
	libpak.la \
	libshared.la \
	libsys.la

libcmd_la_SOURCES = \
	cmd.c
libcmd_la_CFLAGS = \
	-pipe \
	@DEBUG_CFLAGS@ \
	@WARN_CFLAGS@

libcmodel_la_SOURCES = \
	cmodel.c
libcmodel_la_CFLAGS = \
	-pipe \
	@DEBUG_CFLAGS@ \
	@WARN_CFLAGS@ 

libcommon_la_SOURCES = \
	common.c
libcommon_la_CFLAGS = \
	-pipe \
	@DEBUG_CFLAGS@ \
	@SDL_CFLAGS@ \
	@WARN_CFLAGS@
libcommon_la_LIBADD = \
	@SDL_LIBS@
	
libcvar_la_SOURCES = \
	cvar.c
libcvar_la_CFLAGS = \
	-pipe \
	@DEBUG_CFLAGS@ \
	@WARN_CFLAGS@

libfilesystem_la_SOURCES = \
	filesystem.c
libfilesystem_la_CFLAGS = \
	-pipe \
	@DEBUG_CFLAGS@ \
	@WARN_CFLAGS@

libimages_la_SOURCES = \
	images.c
libimages_la_CFLAGS = \
	-pipe \
	@DEBUG_CFLAGS@ \
	@SDL_CFLAGS@ \
	@WARN_CFLAGS@

libhash_la_SOURCES = \
	hash.c
libhash_la_CFLAGS = \
	-pipe \
	@DEBUG_CFLAGS@ \
	@WARN_CFLAGS@

libmem_la_SOURCES = \
	mem.c
libmem_la_CFLAGS = \
	-pipe \
	@DEBUG_CFLAGS@ \
	@WARN_CFLAGS@

libnet_la_SOURCES = \
	net_chan.c \
	net_udp.c
libnet_la_CFLAGS = \
	-pipe \
	@DEBUG_CFLAGS@ \
	@WARN_CFLAGS@
	
libpak_la_SOURCES = \
	pak.c \
	filesystem.c
libpak_la_CFLAGS = \
	-pipe \
	-D__LIBPAK_LA__ \
	@DEBUG_CFLAGS@ \
	@WARN_CFLAGS@
libpak_la_LIBADD = \
	libhash.la

libshared_la_SOURCES = \
	shared.c
libshared_la_CFLAGS = \
	-pipe \
	@DEBUG_CFLAGS@ \
	@WARN_CFLAGS@

libsys_la_SOURCES = \
	sys.c
libsys_la_CFLAGS = \
	-pipe \
	@DEBUG_CFLAGS@ \
	@WARN_CFLAGS@

bin_PROGRAMS = \
	quake2world

quake2world_SOURCES = \
	console.c \
	curses.c \
	main.c \
	pmove.c \
	win32.c

EXTRA_quake2world_SOURCES = \
	quake2world-icon.rc

quake2world_DEPENDENCIES = \
	${noinst_LTLIBRARIES} \
	client/libclient.la \
	client/ui/libui.la \
	client/ui/node/libuinode.la \
	client/sound/libsound.la \
	client/renderer/librenderer.la \
	server/libserver.la \
	${ICON}

quake2world_CFLAGS = \
	-pipe \
	@CURSES_CFLAGS@ \
	@DEBUG_CFLAGS@ \
	@OPENGL_CFLAGS@ \
	@SDL_CFLAGS@ \
	@WARN_CFLAGS@

quake2world_LDADD = \
	${quake2world_DEPENDENCIES} \
	@CURL_LIBS@ \
	@CURSES_LIBS@ \
	@DEBUG_LIBS@ \
	@HOST_LIBS@ \
	@ICON@ \
	@JPEG_LIBS@ \
	@OPENGL_LIBS@ \
	@SDL_LIBS@ \
	-lm -lz
