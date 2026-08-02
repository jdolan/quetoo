GAME_COMMON_DIR = $(top_srcdir)/src/game/common

GAME_CFLAGS = \
	-DG_HOOK \
	-DG_CTF \
	-I$(srcdir) \
	-I$(top_srcdir)/src \
	-I$(GAME_COMMON_DIR) \
	@SO_CFLAGS@ \
	@BASE_CFLAGS@ \
	@OBJECTIVELY_CFLAGS@

GAME_LIB_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(GAME_COMMON_DIR) \
	@BASE_CFLAGS@ \
	@OBJECTIVELY_CFLAGS@

noinst_HEADERS = \
	bg_item.h \
	g_client_stats.h \
	g_client.h \
	g_combat.h \
	g_cmd.h \
	g_entity.h \
	g_flag.h \
	g_item.h \
	g_local.h \
	g_main.h \
	g_types.h \
	g_weapon.h \
	$(GAME_COMMON_DIR)/g_util.h \
	$(GAME_COMMON_DIR)/g_ballistics.h \
	$(GAME_COMMON_DIR)/g_client_chase.h \
	$(GAME_COMMON_DIR)/g_client_view.h \
	$(GAME_COMMON_DIR)/g_entity_info.h \
	$(GAME_COMMON_DIR)/g_entity_trigger.h \
	$(GAME_COMMON_DIR)/g_module.h \
	$(GAME_COMMON_DIR)/g_ai_goal.h \
	$(GAME_COMMON_DIR)/g_ai_grid.h \
	$(GAME_COMMON_DIR)/g_ai_info.h \
	$(GAME_COMMON_DIR)/g_ai_item.h \
	$(GAME_COMMON_DIR)/g_ai_main.h \
	$(GAME_COMMON_DIR)/g_ai_node.h \
	$(GAME_COMMON_DIR)/g_ai_types.h \
	$(GAME_COMMON_DIR)/bg_pmove.h \
	$(GAME_COMMON_DIR)/g_effect.h \
	$(GAME_COMMON_DIR)/g_entity_func.h \
	$(GAME_COMMON_DIR)/g_entity_misc.h \
	$(GAME_COMMON_DIR)/g_entity_target.h \
	$(GAME_COMMON_DIR)/g_physics.h \
	$(GAME_COMMON_DIR)/g_sound.h \
	$(GAME_COMMON_DIR)/g_hook.h \
	$(GAME_COMMON_DIR)/g_hook_types.h

noinst_LTLIBRARIES = \
	libitem.la \
	libpmovectf.la \
	libgcommonctf.la

libgcommonctf_la_SOURCES = \
	$(GAME_COMMON_DIR)/g_util.c \
	$(GAME_COMMON_DIR)/g_ballistics.c \
	$(GAME_COMMON_DIR)/g_client_chase.c \
	$(GAME_COMMON_DIR)/g_client_view.c \
	$(GAME_COMMON_DIR)/g_entity_info.c \
	$(GAME_COMMON_DIR)/g_entity_trigger.c \
	$(GAME_COMMON_DIR)/g_ai_goal.c \
	$(GAME_COMMON_DIR)/g_ai_grid.c \
	$(GAME_COMMON_DIR)/g_ai_info.c \
	$(GAME_COMMON_DIR)/g_ai_item.c \
	$(GAME_COMMON_DIR)/g_ai_main.c \
	$(GAME_COMMON_DIR)/g_ai_node.c \
	$(GAME_COMMON_DIR)/g_effect.c \
	$(GAME_COMMON_DIR)/g_entity_func.c \
	$(GAME_COMMON_DIR)/g_entity_misc.c \
	$(GAME_COMMON_DIR)/g_entity_target.c \
	$(GAME_COMMON_DIR)/g_physics.c \
	$(GAME_COMMON_DIR)/g_sound.c \
	$(GAME_COMMON_DIR)/g_hook.c

libgcommonctf_la_CFLAGS = \
	$(GAME_CFLAGS)

libitem_la_SOURCES = \
	bg_item.c
libitem_la_CFLAGS = \
	$(GAME_LIB_CFLAGS)
libitem_la_LDFLAGS = \
	-shared
libitem_la_LIBADD =

libpmovectf_la_SOURCES = \
	$(GAME_COMMON_DIR)/bg_pmove.c
libpmovectf_la_CFLAGS = \
	$(GAME_LIB_CFLAGS)
libpmovectf_la_LDFLAGS = \
	-shared
libpmovectf_la_LIBADD = \
	$(top_builddir)/src/shared/libshared.la

gamelibdir = \
	@PKGLIBDIR@/ctf

gamelib_LTLIBRARIES = \
	game.la

game_la_SOURCES = \
	g_client_stats.c \
	g_client.c \
	g_cmd.c \
	g_combat.c \
	g_entity.c \
	g_flag.c \
	g_item.c \
	g_main.c \
	g_weapon.c

game_la_CFLAGS = \
	$(GAME_CFLAGS)

game_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-shared

game_la_LIBADD = \
	libitem.la \
	libpmovectf.la \
	libgcommonctf.la \
	@OBJECTIVELY_LIBS@
