GCOMMON_DIR = $(top_srcdir)/src/game/common
include $(top_srcdir)/src/game/common/sources.mk

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
	g_util.h \
	g_weapon.h \
	$(GCOMMON_HDR) \
	$(GCOMMON_HOOK_HDR)

noinst_LTLIBRARIES = \
	libitem.la \
	libpmovectf.la \
	libgcommonctf.la

libgcommonctf_la_SOURCES = \
	$(GCOMMON_SRC) \
	$(GCOMMON_HOOK_SRC)

libgcommonctf_la_CFLAGS = \
	-I$(top_srcdir)/src \
	$(GCOMMON_HOOK_CFLAGS) \
	$(GCOMMON_CTF_CFLAGS) \
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
	g_client_stats.c \
	g_client.c \
	g_cmd.c \
	g_combat.c \
	g_entity.c \
	g_item.c \
	g_main.c \
	g_util.c \
	g_weapon.c

game_la_CFLAGS = \
	-I$(top_srcdir)/src \
	$(GCOMMON_HOOK_CFLAGS) \
	$(GCOMMON_CTF_CFLAGS) \
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
