GAME_COMMON_DIR = $(top_srcdir)/src/game/common

vpath %.c $(srcdir):$(GAME_COMMON_DIR)

AM_CPPFLAGS = \
	-DG_HOOK \
	-I$(top_srcdir)/src \
	-I$(GAME_COMMON_DIR)

AM_CFLAGS = \
	-ffp-contract=off \
	-fms-extensions \
	-Wno-microsoft-anon-tag \
	@SO_CFLAGS@ \
	@BASE_CFLAGS@ \
	@OBJECTIVELY_CFLAGS@

gamelibdir = \
	@PKGLIBDIR@/race

gamelib_LTLIBRARIES = \
	game.la

game_la_SOURCES = \
	bg_item.c \
	bg_pmove.c \
	bg_pmove_quake.c \
	bg_pmove_quake2.c \
	bg_pmove_quake3.c \
	bg_pmove_quetoo.c \
	bg_pmove_race.c \
	g_ai_goal.c \
	g_ai_grid.c \
	g_ai_info.c \
	g_ai_item.c \
	g_ai_main.c \
	g_ai_node.c \
	g_ballistics.c \
	g_client.c \
	g_client_chase.c \
	g_client_stats.c \
	g_client_view.c \
	g_cmd.c \
	g_combat.c \
	g_effect.c \
	g_entity.c \
	g_entity_func.c \
	g_entity_info.c \
	g_entity_misc.c \
	g_entity_target.c \
	g_entity_trigger.c \
	g_hook.c \
	g_item.c \
	g_main.c \
	g_module.c \
	g_physics.c \
	g_race.c \
	g_race_entity.c \
	g_rules.c \
	g_sound.c \
	g_util.c \
	g_vote.c \
	g_weapon.c

game_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-shared

game_la_LIBADD = \
	$(top_builddir)/src/shared/libshared.la \
	@OBJECTIVELY_LIBS@
