# Common client game sources, included by each cgame module's Makefile.am.
#
# These MUST be compiled per module rather than linked from a prebuilt library:
# they resolve g_types.h and bg_item.h from the module's own game directory, so
# cg_state_t and the item roster take that module's definitions.
#
# Set CGCOMMON_DIR to this directory, then add $(CGCOMMON_SRC) to the module's
# cgame_la_SOURCES and $(CGCOMMON_HDR) to its noinst_HEADERS. Optional features
# are switched on with the G_HOOK and G_CTF defines; without them the grapple
# and capture-play code here is never compiled.

CGCOMMON_SRC = \
	$(CGCOMMON_DIR)/cg_client.c \
	$(CGCOMMON_DIR)/cg_discord.c \
	$(CGCOMMON_DIR)/cg_editor.c \
	$(CGCOMMON_DIR)/cg_effect.c \
	$(CGCOMMON_DIR)/cg_entity.c \
	$(CGCOMMON_DIR)/cg_entity_effect.c \
	$(CGCOMMON_DIR)/cg_entity_event.c \
	$(CGCOMMON_DIR)/cg_entity_misc.c \
	$(CGCOMMON_DIR)/cg_entity_trail.c \
	$(CGCOMMON_DIR)/cg_flare.c \
	$(CGCOMMON_DIR)/cg_hud.c \
	$(CGCOMMON_DIR)/cg_input.c \
	$(CGCOMMON_DIR)/cg_inventory.c \
	$(CGCOMMON_DIR)/cg_light.c \
	$(CGCOMMON_DIR)/cg_main.c \
	$(CGCOMMON_DIR)/cg_media.c \
	$(CGCOMMON_DIR)/cg_muzzle_flash.c \
	$(CGCOMMON_DIR)/cg_predict.c \
	$(CGCOMMON_DIR)/cg_score.c \
	$(CGCOMMON_DIR)/cg_sound.c \
	$(CGCOMMON_DIR)/cg_sprite.c \
	$(CGCOMMON_DIR)/cg_temp_entity.c \
	$(CGCOMMON_DIR)/cg_ui.c \
	$(CGCOMMON_DIR)/cg_view.c \
	$(CGCOMMON_DIR)/cg_weapon.c

CGCOMMON_HDR = \
	$(CGCOMMON_DIR)/cg_client.h \
	$(CGCOMMON_DIR)/cg_discord.h \
	$(CGCOMMON_DIR)/cg_editor.h \
	$(CGCOMMON_DIR)/cg_effect.h \
	$(CGCOMMON_DIR)/cg_entity.h \
	$(CGCOMMON_DIR)/cg_entity_effect.h \
	$(CGCOMMON_DIR)/cg_entity_event.h \
	$(CGCOMMON_DIR)/cg_entity_misc.h \
	$(CGCOMMON_DIR)/cg_entity_trail.h \
	$(CGCOMMON_DIR)/cg_flare.h \
	$(CGCOMMON_DIR)/cg_hud.h \
	$(CGCOMMON_DIR)/cg_input.h \
	$(CGCOMMON_DIR)/cg_inventory.h \
	$(CGCOMMON_DIR)/cg_light.h \
	$(CGCOMMON_DIR)/cg_local.h \
	$(CGCOMMON_DIR)/cg_main.h \
	$(CGCOMMON_DIR)/cg_media.h \
	$(CGCOMMON_DIR)/cg_muzzle_flash.h \
	$(CGCOMMON_DIR)/cg_predict.h \
	$(CGCOMMON_DIR)/cg_score.h \
	$(CGCOMMON_DIR)/cg_sound.h \
	$(CGCOMMON_DIR)/cg_sprite.h \
	$(CGCOMMON_DIR)/cg_team_mode.h \
	$(CGCOMMON_DIR)/cg_temp_entity.h \
	$(CGCOMMON_DIR)/cg_types.h \
	$(CGCOMMON_DIR)/cg_ui.h \
	$(CGCOMMON_DIR)/cg_view.h \
	$(CGCOMMON_DIR)/cg_weapon.h

CGCOMMON_CFLAGS = \
	-I$(CGCOMMON_DIR)
