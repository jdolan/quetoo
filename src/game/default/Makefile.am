noinst_HEADERS = \
	bg_item.h \
	g_client_stats.h \
	g_client.h \
	g_combat.h \
	g_cmd.h \
	g_entity.h \
	g_item.h \
	g_local.h \
	g_main.h \
	g_types.h \
	g_weapon.h \
	$(top_srcdir)/src/game/common/g_util.h \
	$(top_srcdir)/src/game/common/g_ballistics.h \
	$(top_srcdir)/src/game/common/g_client_chase.h \
	$(top_srcdir)/src/game/common/g_client_view.h \
	$(top_srcdir)/src/game/common/g_entity_info.h \
	$(top_srcdir)/src/game/common/g_entity_trigger.h \
	$(top_srcdir)/src/game/common/g_module.h \
	$(top_srcdir)/src/game/common/g_ai_goal.h \
	$(top_srcdir)/src/game/common/g_ai_grid.h \
	$(top_srcdir)/src/game/common/g_ai_info.h \
	$(top_srcdir)/src/game/common/g_ai_item.h \
	$(top_srcdir)/src/game/common/g_ai_main.h \
	$(top_srcdir)/src/game/common/g_ai_node.h \
	$(top_srcdir)/src/game/common/g_ai_types.h \
	$(top_srcdir)/src/game/common/bg_pmove.h \
	$(top_srcdir)/src/game/common/g_effect.h \
	$(top_srcdir)/src/game/common/g_entity_func.h \
	$(top_srcdir)/src/game/common/g_entity_misc.h \
	$(top_srcdir)/src/game/common/g_entity_target.h \
	$(top_srcdir)/src/game/common/g_physics.h \
	$(top_srcdir)/src/game/common/g_sound.h

noinst_LTLIBRARIES = \
	libitem.la \
	libpmovedefault.la \
	libgcommondefault.la

libgcommondefault_la_SOURCES = \
	$(top_srcdir)/src/game/common/g_util.c \
	$(top_srcdir)/src/game/common/g_ballistics.c \
	$(top_srcdir)/src/game/common/g_client_chase.c \
	$(top_srcdir)/src/game/common/g_client_view.c \
	$(top_srcdir)/src/game/common/g_entity_info.c \
	$(top_srcdir)/src/game/common/g_entity_trigger.c \
	$(top_srcdir)/src/game/common/g_ai_goal.c \
	$(top_srcdir)/src/game/common/g_ai_grid.c \
	$(top_srcdir)/src/game/common/g_ai_info.c \
	$(top_srcdir)/src/game/common/g_ai_item.c \
	$(top_srcdir)/src/game/common/g_ai_main.c \
	$(top_srcdir)/src/game/common/g_ai_node.c \
	$(top_srcdir)/src/game/common/g_effect.c \
	$(top_srcdir)/src/game/common/g_entity_func.c \
	$(top_srcdir)/src/game/common/g_entity_misc.c \
	$(top_srcdir)/src/game/common/g_entity_target.c \
	$(top_srcdir)/src/game/common/g_physics.c \
	$(top_srcdir)/src/game/common/g_sound.c

libgcommondefault_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(srcdir) \
	-I$(top_srcdir)/src/game/common \
	@SO_CFLAGS@ \
	@BASE_CFLAGS@ \
	@OBJECTIVELY_CFLAGS@

libitem_la_SOURCES = \
	bg_item.c
libitem_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/game/common \
	@BASE_CFLAGS@ \
	@OBJECTIVELY_CFLAGS@
libitem_la_LDFLAGS = \
	-shared
libitem_la_LIBADD =

libpmovedefault_la_SOURCES = \
	$(top_srcdir)/src/game/common/bg_pmove.c
libpmovedefault_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/game/common \
	@BASE_CFLAGS@ \
	@OBJECTIVELY_CFLAGS@
libpmovedefault_la_LDFLAGS = \
	-shared
libpmovedefault_la_LIBADD = \
	$(top_builddir)/src/shared/libshared.la

gamelibdir = \
	@PKGLIBDIR@/default

gamelib_LTLIBRARIES = \
	game.la

game_la_SOURCES = \
	g_client_stats.c \
	g_client.c \
	g_cmd.c \
	g_combat.c \
	g_entity.c \
	g_item.c \
	g_main.c \
	g_weapon.c

game_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(srcdir) \
	-I$(top_srcdir)/src/game/common \
	@SO_CFLAGS@ \
	@BASE_CFLAGS@ \
	@OBJECTIVELY_CFLAGS@

game_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-shared

game_la_LIBADD = \
	libitem.la \
	libpmovedefault.la \
	libgcommondefault.la \
	@OBJECTIVELY_LIBS@
