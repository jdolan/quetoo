noinst_HEADERS = \
	server.h \
	sv_admin.h \
	sv_client.h \
	sv_console.h \
	sv_entity.h \
	sv_game.h \
	sv_init.h \
	sv_local.h \
	sv_main.h \
	sv_master.h \
	sv_send.h \
	sv_types.h \
	sv_world.h

noinst_LTLIBRARIES = \
	libserver.la

libserver_la_SOURCES = \
	sv_admin.c \
	sv_client.c \
	sv_console.c \
	sv_entity.c \
	sv_game.c \
	sv_init.c \
	sv_main.c \
	sv_master.c \
	sv_send.c \
	sv_world.c

libserver_la_CFLAGS = \
	-I$(top_srcdir)/src \
	@BASE_CFLAGS@ \
	@CURSES_CFLAGS@ \
	@GLIB_CFLAGS@

libserver_la_LDFLAGS = \
	-shared

libserver_la_LIBADD = \
	$(top_builddir)/src/collision/libcmodel.la \
	$(top_builddir)/src/net/libnet.la \
	$(top_builddir)/src/libconsole.la \
	$(top_builddir)/src/libthread.la \
	@CURSES_LIBS@
