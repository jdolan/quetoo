SUBDIRS = \
	libs \
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
	binds.h \
	files.h

bin_PROGRAMS = \
	quake2world

quake2world_SOURCES = \
	main.c

EXTRA_quake2world_SOURCES = \
	quake2world-icon.rc

quake2world_DEPENDENCIES = \
	client/libclient.la \
	client/ui/libui.la \
	client/ui/node/libuinode.la \
	client/sound/libsound.la \
	client/renderer/librenderer.la \
	libs/libcmd.la \
	libs/libcmodel.la \
	libs/libcommon.la \
	libs/libconsole.la \
	libs/libcvar.la \
	libs/libfilesystem.la \
	libs/libhash.la \
	libs/libimages.la \
	libs/libmem.la \
	libs/libnet.la \
	libs/libpak.la \
	libs/libpmove.la \
	libs/libshared.la \
	libs/libsys.la \
	libs/libwin32.la \
	server/libserver.la \
	${ICON}

quake2world_CFLAGS = \
	-I./libs \
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
