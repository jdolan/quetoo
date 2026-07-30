# Common game module sources, included by each game module's Makefile.am.
#
# These hold map and BSP infrastructure that mods are not expected to reshape:
# the func_* brush entities, target_* entities, entity physics, sound and effect
# emission. Gameplay rules - match flow, scoring, damage, items, weapons, client
# handling - stay in each module so a mod owns them outright.
#
# As with the common cgame UI, these MUST be compiled per module rather than
# linked from a prebuilt library. Every file here includes the module's own
# g_local.h, so g_entity_t, g_client_t and g_level all take that module's
# definitions. A library built against one module's layout would corrupt memory
# in any module that added a field.
#
# Set GCOMMON_DIR to this directory, then add $(GCOMMON_SRC) to the module's
# convenience library and $(GCOMMON_CFLAGS) to its compile flags. The _SRC/_HDR
# names avoid automake's _SOURCES/_HEADERS primary suffixes, which would
# otherwise demand a matching install directory.
#
# Common code calls back into per-module code in a few places. Those names are
# contract, not implementation detail, and a module MUST keep them resolvable:
#
#   G_Damage, G_RadiusDamage  (g_combat.c)
#   G_Explode, G_UseTargets   (g_util.c)
#   MOD_CRUSH, MOD_TELEFRAG, MOD_ACT_OF_GOD  (g_means_of_death, g_types.h)

GCOMMON_SRC = \
	$(GCOMMON_DIR)/g_effect.c \
	$(GCOMMON_DIR)/g_entity_func.c \
	$(GCOMMON_DIR)/g_entity_misc.c \
	$(GCOMMON_DIR)/g_entity_target.c \
	$(GCOMMON_DIR)/g_physics.c \
	$(GCOMMON_DIR)/g_sound.c

GCOMMON_HDR = \
	$(GCOMMON_DIR)/bg_pmove.h \
	$(GCOMMON_DIR)/g_effect.h \
	$(GCOMMON_DIR)/g_entity_func.h \
	$(GCOMMON_DIR)/g_entity_misc.h \
	$(GCOMMON_DIR)/g_entity_target.h \
	$(GCOMMON_DIR)/g_physics.h \
	$(GCOMMON_DIR)/g_sound.h

GCOMMON_CFLAGS = \
	-I$(GCOMMON_DIR)

# Optional features. A module opts in by adding the define to its compile
# flags, which switches on the matching code in the common sources - the
# teleporter's hook handling in g_entity_misc.c, for instance. A module that
# does not opt in never sees those references, so it needs none of the hook's
# fields or symbols.
GCOMMON_HOOK_SRC = \
	$(GCOMMON_DIR)/g_hook.c

GCOMMON_HOOK_HDR = \
	$(GCOMMON_DIR)/g_hook.h \
	$(GCOMMON_DIR)/g_hook_types.h

GCOMMON_HOOK_CFLAGS = \
	-DG_HOOK
