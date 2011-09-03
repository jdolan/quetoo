gamelibdir=$(libdir)/@PACKAGE@/default

noinst_HEADERS = \
	g_local.h

gamelib_LTLIBRARIES = \
	game.la

game_la_SOURCES = \
	g_ballistics.c \
	g_chase.c \
	g_client.c \
	g_commands.c \
	g_combat.c \
	g_entity_func.c \
	g_entity_info.c \
	g_entity_misc.c \
	g_entity_target.c \
	g_entity_trigger.c \
	g_items.c \
	g_main.c \
	g_physics.c \
	g_spawn.c \
	g_stats.c \
	g_utils.c \
	g_view.c \
	g_weapon.c

game_la_CFLAGS = \
	@SO_CFLAGS@ \
	-I../.. \
	-pipe \
	@BASE_CFLAGS@ \
	@MYSQL_CFLAGS@

game_la_LIBADD = \
	../../libshared.la \
	@MYSQL_LIBS@

game_la_LDFLAGS = \
	-module \
	-avoid-version \
	-rpath $(gamelibdir)

