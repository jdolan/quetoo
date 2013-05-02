gamelibdir=$(libdir)/@PACKAGE@/default

noinst_HEADERS = \
	g_ballistics.h \
	g_client_chase.h \
	g_client_stats.h \
	g_client_view.h \
	g_client.h \
	g_combat.h \
	g_commands.h \
	g_entity_func.h \
	g_entity_info.h \
	g_entity_misc.h \
	g_entity_target.h \
	g_entity_trigger.h \
	g_entity.h \
	g_item.h \
	g_local.h \
	g_main.h \
	g_physics.h \
	g_types.h \
	g_utils.h \
	g_weapon.h

gamelib_LTLIBRARIES = \
	game.la

game_la_SOURCES = \
	g_ballistics.c \
	g_client_chase.c \
	g_client_stats.c \
	g_client_view.c \
	g_client.c \
	g_commands.c \
	g_combat.c \
	g_entity_func.c \
	g_entity_info.c \
	g_entity_misc.c \
	g_entity_target.c \
	g_entity_trigger.c \
	g_entity.c \
	g_item.c \
	g_main.c \
	g_physics.c \
	g_utils.c \
	g_weapon.c

game_la_CFLAGS = \
	-I../.. \
	@SO_CFLAGS@ \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@MYSQL_CFLAGS@

game_la_LIBADD = \
	../../libshared.la \
	@MYSQL_LIBS@

game_la_LDFLAGS = \
	-module \
	-avoid-version \
	-no-undefined \
	-rpath $(gamelibdir)

