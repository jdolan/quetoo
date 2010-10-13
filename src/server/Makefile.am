noinst_HEADERS = \
	server.h

noinst_LTLIBRARIES = \
	libserver.la

libserver_la_SOURCES = \
	sv_ccmds.c \
	sv_ents.c \
	sv_game.c \
	sv_init.c \
	sv_main.c \
	sv_send.c \
	sv_user.c \
	sv_world.c

libserver_la_CFLAGS = \
	-I.. -I../libs \
	-pipe \
	@WARN_CFLAGS@ \
	@DEBUG_CFLAGS@ \
	@CURSES_CFLAGS@

