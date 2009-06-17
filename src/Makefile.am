SUBDIRS = client server . game tools master

SUFFIXES = .rc
.rc.o:
	@WINDRES@ $< -o $@

noinst_HEADERS = \
	anorms.h \
	binds.h \
	common.h \
	console.h \
	files.h \
	hash.h \
	images.h \
	keys.h \
	pak.h \
	shared.h \
	win32.h

noinst_LTLIBRARIES = \
	libcmodel.la \
	libcommon.la \
	libfiles.la \
	libhash.la \
	libimages.la \
	libpak.la \
	libshared.la \
	libsys.la

libcmodel_la_SOURCES = \
	cmodel.c
libcmodel_la_CFLAGS = \
	-pipe \
	@WARN_CFLAGS@ \
	@DEBUG_CFLAGS@

libcommon_la_SOURCES = \
	common.c
libcommon_la_CFLAGS = \
	-pipe \
	@WARN_CFLAGS@ \
	@DEBUG_CFLAGS@ \
	@SDL_CFLAGS@
libcommon_la_LIBADD = \
	@SDL_LIBS@

libfiles_la_SOURCES = \
	files.c
libfiles_la_CFLAGS = \
	-pipe \
	@WARN_CFLAGS@ \
	@DEBUG_CFLAGS@

libimages_la_SOURCES = \
	images.c
libimages_la_CFLAGS = \
	-pipe \
	@WARN_CFLAGS@ \
	@DEBUG_CFLAGS@ \
	@SDL_CFLAGS@

libhash_la_SOURCES = \
	hash.c
libhash_la_CFLAGS = \
	-pipe \
	@WARN_CFLAGS@ \
	@DEBUG_CFLAGS@

libpak_la_SOURCES = \
	pak.c \
	files.c
libpak_la_CFLAGS = \
	-pipe \
	-D__LIBPAK_LA__ \
	@WARN_CFLAGS@ \
	@DEBUG_CFLAGS@
libpak_la_LIBADD = \
	libhash.la

libshared_la_SOURCES = \
	shared.c
libshared_la_CFLAGS = \
	-pipe \
	@WARN_CFLAGS@ \
	@DEBUG_CFLAGS@

libsys_la_SOURCES = \
	main.c
libsys_la_CFLAGS = \
	-pipe \
	-D__LIBSYS_LA__ \
	@WARN_CFLAGS@ \
	@DEBUG_CFLAGS@

bin_PROGRAMS = \
	quake2world

quake2world_SOURCES = \
	cmd.c \
	cmodel.c \
	console.c \
	curses.c \
	cvar.c \
	images.c \
	main.c \
	net_chan.c \
	net_udp.c \
	pmove.c \
	win32.c

EXTRA_quake2world_SOURCES = \
	quake2world-icon.rc

quake2world_DEPENDENCIES = \
	${noinst_LTLIBRARIES} \
	client/libclient.la \
	client/sound/libsound.la \
	client/renderer/librenderer.la \
	server/libserver.la \
	${ICON}

quake2world_CFLAGS = \
	-pipe \
	@DEBUG_CFLAGS@ \
	@WARN_CFLAGS@ \
	@CURSES_CFLAGS@ \
	@SDL_CFLAGS@

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

