GAME_COMMON_DIR = $(top_srcdir)/src/game/common

VPATH = $(srcdir):$(GAME_COMMON_DIR)

GAME_COMMON_OBJECTS = \
	g_ai_goal.lo \
	g_ai_grid.lo \
	g_ai_info.lo \
	g_ai_item.lo \
	g_ai_main.lo \
	g_ai_node.lo \
	g_ballistics.lo \
	g_client_chase.lo \
	g_client_view.lo \
	g_effect.lo \
	g_entity_func.lo \
	g_entity_info.lo \
	g_entity_misc.lo \
	g_entity_target.lo \
	g_entity_trigger.lo \
	g_physics.lo \
	g_sound.lo \
	g_util.lo

GAME_CFLAGS = \
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
	g_item.h \
	g_local.h \
	g_main.h \
	g_types.h \
	g_weapon.h \
	$(GAME_COMMON_DIR)/bg_pmove.h \
	$(GAME_COMMON_DIR)/g_ai_goal.h \
	$(GAME_COMMON_DIR)/g_ai_grid.h \
	$(GAME_COMMON_DIR)/g_ai_info.h \
	$(GAME_COMMON_DIR)/g_ai_item.h \
	$(GAME_COMMON_DIR)/g_ai_main.h \
	$(GAME_COMMON_DIR)/g_ai_node.h \
	$(GAME_COMMON_DIR)/g_ai_types.h \
	$(GAME_COMMON_DIR)/g_ballistics.h \
	$(GAME_COMMON_DIR)/g_client_chase.h \
	$(GAME_COMMON_DIR)/g_client_view.h \
	$(GAME_COMMON_DIR)/g_effect.h \
	$(GAME_COMMON_DIR)/g_entity_func.h \
	$(GAME_COMMON_DIR)/g_entity_info.h \
	$(GAME_COMMON_DIR)/g_entity_misc.h \
	$(GAME_COMMON_DIR)/g_entity_target.h \
	$(GAME_COMMON_DIR)/g_entity_trigger.h \
	$(GAME_COMMON_DIR)/g_module.h \
	$(GAME_COMMON_DIR)/g_physics.h \
	$(GAME_COMMON_DIR)/g_sound.h \
	$(GAME_COMMON_DIR)/g_util.h

noinst_LTLIBRARIES = \
	libitem.la \
	libpmovedefault.la

libitem_la_SOURCES = \
	bg_item.c
libitem_la_CFLAGS = \
	$(GAME_LIB_CFLAGS)
libitem_la_LDFLAGS = \
	-shared
libitem_la_LIBADD =

libpmovedefault_la_SOURCES = \
	$(GAME_COMMON_DIR)/bg_pmove.c
libpmovedefault_la_CFLAGS = \
	$(GAME_LIB_CFLAGS)
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
	$(GAME_CFLAGS)

game_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-shared

game_la_LIBADD = \
	$(GAME_COMMON_OBJECTS) \
	libitem.la \
	libpmovedefault.la \
	@OBJECTIVELY_LIBS@

%.lo: %.c
	$(AM_V_CC)$(LIBTOOL) $(AM_V_lt) --tag=CC $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) \
		--mode=compile $(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
		$(AM_CPPFLAGS) $(CPPFLAGS) $(GAME_CFLAGS) $(CFLAGS) \
		-MT $@ -MD -MP -MF $(DEPDIR)/$*.Tpo -c -o $@ $<
	$(AM_V_at)$(am__mv) $(DEPDIR)/$*.Tpo $(DEPDIR)/$*.Plo

-include $(GAME_COMMON_OBJECTS:%.lo=$(DEPDIR)/%.Plo)
