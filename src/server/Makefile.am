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
	@BASE_CFLAGS@ \
	@CURSES_CFLAGS@ \
	@GLIB_CFLAGS@

libserver_la_LDFLAGS = \
	-shared

libserver_la_LIBADD = \
	../collision/libcmodel.la \
	../libconsole.la \
	../libnet.la \
	../libthread.la
