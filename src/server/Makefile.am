noinst_HEADERS = \
	server.h \
	sv_admin.h \
	sv_client.h \
	sv_entity.h \
	sv_game.h \
	sv_init.h \
	sv_local.h \
	sv_main.h \
	sv_send.h \
	sv_types.h \
	sv_world.h

noinst_LTLIBRARIES = \
	libserver.la

libserver_la_SOURCES = \
	sv_admin.c \
	sv_client.c \
	sv_entity.c \
	sv_game.c \
	sv_init.c \
	sv_main.c \
	sv_send.c \
	sv_world.c

libserver_la_CFLAGS = \
	-I.. \
	-pipe \
	@BASE_CFLAGS@ \
	@CURSES_CFLAGS@

libserver_la_LIBADD = \
	../libcmodel.la \
	../libconsole.la \
	../libnet.la \
	../libpmove.la \
	../libthreads.la