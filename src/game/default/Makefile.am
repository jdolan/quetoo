GCOMMON_DIR = $(top_srcdir)/src/game/common
include $(top_srcdir)/src/game/common/sources.mk

noinst_HEADERS = \
	bg_item.h \
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
	libpmovedefault.la \
	libgcommondefault.la

libgcommondefault_la_SOURCES = \
	$(GCOMMON_SRC)

libgcommondefault_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(srcdir) \
	$(GCOMMON_CFLAGS) \
	@SO_CFLAGS@ \
	@BASE_CFLAGS@ \
	@OBJECTIVELY_CFLAGS@

libitem_la_SOURCES = \
	bg_item.c
libitem_la_CFLAGS = \
	-I$(top_srcdir)/src \
	$(GCOMMON_CFLAGS) \
	@BASE_CFLAGS@ \
	@OBJECTIVELY_CFLAGS@
libitem_la_LDFLAGS = \
	-shared
libitem_la_LIBADD =

libpmovedefault_la_SOURCES = \
	$(GCOMMON_DIR)/bg_pmove.c
libpmovedefault_la_CFLAGS = \
	-I$(top_srcdir)/src \
	$(GCOMMON_CFLAGS) \
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
	libpmovedefault.la \
	libgcommondefault.la \
	@OBJECTIVELY_LIBS@
