gamelibdir=$(libdir)/@PACKAGE@/default

gamelib_LTLIBRARIES = game.la

noinst_HEADERS = g_local.h m_player.h

game_la_SOURCES = g_cmds.c g_combat.c g_func.c g_items.c g_main.c \
	g_misc.c g_phys.c g_spawn.c g_target.c g_trigger.c g_utils.c g_weapon.c \
	p_chase.c p_client.c p_hud.c p_view.c p_weapon.c

game_la_CFLAGS = @SO_CFLAGS@ -pipe -I../.. @WARN_CFLAGS@ @MYSQL_CFLAGS@

game_la_LIBADD = ../../libshared.la @MYSQL_LIBS@
game_la_LDFLAGS = -module -avoid-version -rpath $(gamelibdir)
