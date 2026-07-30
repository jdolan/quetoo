GCOMMON_DIR = $(top_srcdir)/src/game/common
include $(top_srcdir)/src/game/common/sources.mk

noinst_HEADERS = \
	bg_item.h \
	g_ai_goal.h \
    g_ai_grid.h \
	g_ai_info.h \
	g_ai_item.h \
	g_ai_main.h \
	g_ai_node.h \
	g_ai_types.h \
	g_ballistics.h \
	g_client_chase.h \
	g_client_stats.h \
	g_client_view.h \
	g_client.h \
	g_combat.h \
	g_cmd.h \
	g_entity_info.h \
	g_entity_trigger.h \
	g_entity.h \
	g_item.h \
	g_local.h \
	g_main.h \
	g_types.h \
	g_util.h \
	g_weapon.h \
	$(GCOMMON_HDR)

noinst_LTLIBRARIES = \
	libitem.la \
	libpmovectf.la \
	libgcommonctf.la

libgcommonctf_la_SOURCES = \
	$(GCOMMON_SRC)

libgcommonctf_la_CFLAGS = \
	-I$(top_srcdir)/src \
	$(GCOMMON_HOOK_CFLAGS) \
	-I$(srcdir) \
	$(GCOMMON_CFLAGS) \
	@SO_CFLAGS@ \
	@BASE_CFLAGS@ \
	@OBJECTIVELY_CFLAGS@

libitem_la_SOURCES = \
	bg_item.c
libitem_la_CFLAGS = \
	-I$(top_srcdir)/src \
	@BASE_CFLAGS@ \
	@OBJECTIVELY_CFLAGS@
libitem_la_LDFLAGS = \
	-shared
libitem_la_LIBADD =

libpmovectf_la_SOURCES = \
	$(GCOMMON_DIR)/bg_pmove.c
libpmovectf_la_CFLAGS = \
	-I$(top_srcdir)/src \
	$(GCOMMON_CFLAGS) \
	@BASE_CFLAGS@ \
	@OBJECTIVELY_CFLAGS@
libpmovectf_la_LDFLAGS = \
	-shared
libpmovectf_la_LIBADD = \
	$(top_builddir)/src/shared/libshared.la

gamelibdir = \
	@PKGLIBDIR@/ctf

gamelib_LTLIBRARIES = \
	game.la

game_la_SOURCES = \
	g_ai_goal.c \
    g_ai_grid.c \
	g_ai_info.c \
	g_ai_item.c \
	g_ai_main.c \
	g_ai_node.c \
	g_ballistics.c \
	g_client_chase.c \
	g_client_stats.c \
	g_client_view.c \
	g_client.c \
	g_cmd.c \
	g_combat.c \
	g_entity_info.c \
	g_entity_trigger.c \
	g_entity.c \
	g_item.c \
	g_main.c \
	g_util.c \
	g_weapon.c

game_la_CFLAGS = \
	-I$(top_srcdir)/src \
	$(GCOMMON_HOOK_CFLAGS) \
	-I$(srcdir) \
	$(GCOMMON_CFLAGS) \
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
	libpmovectf.la \
	libgcommonctf.la \
	@OBJECTIVELY_LIBS@
