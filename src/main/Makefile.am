SUFFIXES = .rc
.rc.o:
	@WINDRES@ $< -o $@a

bin_PROGRAMS = \
	quake2world

quake2world_SOURCES = \
	main.c

EXTRA_quake2world_SOURCES = \
	quake2world-icon.rc

quake2world_DEPENDENCIES = \
	../client/libclient.la \
	../client/ui/libui.la \
	../client/ui/node/libuinode.la \
	../client/sound/libsound.la \
	../client/renderer/librenderer.la \
	../libcmd.la \
	../libcmodel.la \
	../libcommon.la \
	../libconsole.la \
	../libcvar.la \
	../libfilesystem.la \
	../libhash.la \
	../libimages.la \
	../libmem.la \
	../libnet.la \
	../libpak.la \
	../libpmove.la \
	../libshared.la \
	../libsys.la \
	../libwin32.la \
	../server/libserver.la \
	${ICON}

quake2world_CFLAGS = \
	-I.. \
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
