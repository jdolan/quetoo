noinst_HEADERS = \
	bg_pmove.h \
	g_ai.h \
	g_ballistics.h \
	g_client_chase.h \
	g_client_stats.h \
	g_client_view.h \
	g_client.h \
	g_combat.h \
	g_cmd.h \
	g_entity_func.h \
	g_entity_info.h \
	g_entity_misc.h \
	g_entity_target.h \
	g_entity_trigger.h \
	g_entity.h \
	g_item.h \
	g_local.h \
	g_main.h \
	g_map_list.h \
	g_mysql.h \
	g_physics.h \
	g_types.h \
	g_util.h \
	g_weapon.h

noinst_LTLIBRARIES = \
	libpmove.la

libpmove_la_SOURCES = \
	bg_pmove.c
libpmove_la_CFLAGS = \
	-I$(top_srcdir)/src \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@
libpmove_la_LDFLAGS = \
	-shared
libpmove_la_LIBADD = \
	$(top_builddir)/src/libshared.la

gamelibdir = \
	@PKGLIBDIR@/default

gamelib_LTLIBRARIES = \
	game.la

game_la_SOURCES = \
	g_ai.c \
	g_ballistics.c \
	g_client_chase.c \
	g_client_stats.c \
	g_client_view.c \
	g_client.c \
	g_cmd.c \
	g_combat.c \
	g_entity_func.c \
	g_entity_info.c \
	g_entity_misc.c \
	g_entity_target.c \
	g_entity_trigger.c \
	g_entity.c \
	g_item.c \
	g_main.c \
	g_map_list.c \
	g_mysql.c \
	g_physics.c \
	g_util.c \
	g_weapon.c

game_la_CFLAGS = \
	-I$(top_srcdir)/src \
	@SO_CFLAGS@ \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@MYSQL_CFLAGS@

game_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-shared

game_la_LIBADD = \
	libpmove.la \
	$(top_builddir)/src/libparse.la \
	@GLIB_LIBS@ \
	@MYSQL_LIBS@

