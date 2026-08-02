CGAME_COMMON_DIR = $(top_srcdir)/src/cgame/common
GAME_COMMON_DIR = $(top_srcdir)/src/game/common

CGAME_CFLAGS = \
	-I$(srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/cgame \
	-I$(CGAME_COMMON_DIR) \
	-I$(GAME_COMMON_DIR) \
	-I$(CGAME_COMMON_DIR)/ui \
	-I$(CGAME_COMMON_DIR)/ui/controls \
	-I$(CGAME_COMMON_DIR)/ui/credits \
	-I$(CGAME_COMMON_DIR)/ui/editor \
	-I$(CGAME_COMMON_DIR)/ui/home \
	-I$(CGAME_COMMON_DIR)/ui/main \
	-I$(CGAME_COMMON_DIR)/ui/play \
	-I$(CGAME_COMMON_DIR)/ui/settings \
	-I$(CGAME_COMMON_DIR)/ui/teams \
	-I$(top_srcdir)/src/game/default \
	@SO_CFLAGS@ \
	@BASE_CFLAGS@ \
	@OBJECTIVELY_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@ \
	@OPENAL_CFLAGS@ \
	@SDL3_CFLAGS@ \
	@SNDFILE_CFLAGS@

noinst_HEADERS = \
	cg_hud.h \
	cg_score.h \
	$(CGAME_COMMON_DIR)/cg_client.h \
	$(CGAME_COMMON_DIR)/cg_discord.h \
	$(CGAME_COMMON_DIR)/cg_editor.h \
	$(CGAME_COMMON_DIR)/cg_effect.h \
	$(CGAME_COMMON_DIR)/cg_entity.h \
	$(CGAME_COMMON_DIR)/cg_entity_effect.h \
	$(CGAME_COMMON_DIR)/cg_entity_event.h \
	$(CGAME_COMMON_DIR)/cg_entity_misc.h \
	$(CGAME_COMMON_DIR)/cg_entity_trail.h \
	$(CGAME_COMMON_DIR)/cg_flare.h \
	$(CGAME_COMMON_DIR)/cg_hud_draw.h \
	$(CGAME_COMMON_DIR)/cg_input.h \
	$(CGAME_COMMON_DIR)/cg_inventory.h \
	$(CGAME_COMMON_DIR)/cg_light.h \
	$(CGAME_COMMON_DIR)/cg_local.h \
	$(CGAME_COMMON_DIR)/cg_main.h \
	$(CGAME_COMMON_DIR)/cg_media.h \
	$(CGAME_COMMON_DIR)/cg_muzzle_flash.h \
	$(CGAME_COMMON_DIR)/cg_predict.h \
	$(CGAME_COMMON_DIR)/cg_sound.h \
	$(CGAME_COMMON_DIR)/cg_sprite.h \
	$(CGAME_COMMON_DIR)/cg_team_mode.h \
	$(CGAME_COMMON_DIR)/cg_temp_entity.h \
	$(CGAME_COMMON_DIR)/cg_types.h \
	$(CGAME_COMMON_DIR)/cg_ui.h \
	$(CGAME_COMMON_DIR)/cg_view.h \
	$(CGAME_COMMON_DIR)/cg_weapon.h \
	$(CGAME_COMMON_DIR)/ui/BindTextView.h \
	$(CGAME_COMMON_DIR)/ui/CvarCheckbox.h \
	$(CGAME_COMMON_DIR)/ui/CvarSelect.h \
	$(CGAME_COMMON_DIR)/ui/CvarSlider.h \
	$(CGAME_COMMON_DIR)/ui/CvarTextView.h \
	$(CGAME_COMMON_DIR)/ui/DialogViewController.h \
	$(CGAME_COMMON_DIR)/ui/controls/ControlsViewController.h \
	$(CGAME_COMMON_DIR)/ui/controls/CrosshairView.h \
	$(CGAME_COMMON_DIR)/ui/controls/MovementCombatViewController.h \
	$(CGAME_COMMON_DIR)/ui/controls/ResponseServiceViewController.h \
	$(CGAME_COMMON_DIR)/ui/credits/CreditsViewController.h \
	$(CGAME_COMMON_DIR)/ui/editor/EditorViewController.h \
	$(CGAME_COMMON_DIR)/ui/editor/EntityView.h \
	$(CGAME_COMMON_DIR)/ui/editor/EntityViewController.h \
	$(CGAME_COMMON_DIR)/ui/editor/MaterialViewController.h \
	$(CGAME_COMMON_DIR)/ui/editor/MeshViewController.h \
	$(CGAME_COMMON_DIR)/ui/home/HomeViewController.h \
	$(CGAME_COMMON_DIR)/ui/home/LeaderboardViewController.h \
	$(CGAME_COMMON_DIR)/ui/home/StatsViewController.h \
	$(CGAME_COMMON_DIR)/ui/main/LoadingViewController.h \
	$(CGAME_COMMON_DIR)/ui/main/MainView.h \
	$(CGAME_COMMON_DIR)/ui/main/MainViewController.h \
	$(CGAME_COMMON_DIR)/ui/main/UpdateViewController.h \
	$(CGAME_COMMON_DIR)/ui/play/CreateServerViewController.h \
	$(CGAME_COMMON_DIR)/ui/play/JoinServerViewController.h \
	$(CGAME_COMMON_DIR)/ui/play/MapListCollectionItemView.h \
	$(CGAME_COMMON_DIR)/ui/play/MapListCollectionView.h \
	$(CGAME_COMMON_DIR)/ui/play/PlayViewController.h \
	$(CGAME_COMMON_DIR)/ui/play/PlayerModelView.h \
	$(CGAME_COMMON_DIR)/ui/play/PlayerSetupViewController.h \
	$(CGAME_COMMON_DIR)/ui/settings/SettingsViewController.h \
	$(CGAME_COMMON_DIR)/ui/teams/TeamPlayerView.h \
	$(CGAME_COMMON_DIR)/ui/teams/TeamView.h \
	$(CGAME_COMMON_DIR)/ui/teams/TeamsViewController.h

# The common UI and client game sources are compiled here rather than linked
# from a prebuilt library, because cg_state_t and the item roster resolve from
# this module's game directory. This library's name MUST stay unique across
# modules: with subdir-objects automake derives object file names from it, so
# two modules sharing a prefix would collide in the common directories.
noinst_LTLIBRARIES = \
	libcgcommondefault.la

libcgcommondefault_la_SOURCES = \
	$(CGAME_COMMON_DIR)/cg_client.c \
	$(CGAME_COMMON_DIR)/cg_discord.c \
	$(CGAME_COMMON_DIR)/cg_editor.c \
	$(CGAME_COMMON_DIR)/cg_effect.c \
	$(CGAME_COMMON_DIR)/cg_entity.c \
	$(CGAME_COMMON_DIR)/cg_entity_effect.c \
	$(CGAME_COMMON_DIR)/cg_entity_event.c \
	$(CGAME_COMMON_DIR)/cg_entity_misc.c \
	$(CGAME_COMMON_DIR)/cg_entity_trail.c \
	$(CGAME_COMMON_DIR)/cg_flare.c \
	$(CGAME_COMMON_DIR)/cg_hud_draw.c \
	$(CGAME_COMMON_DIR)/cg_input.c \
	$(CGAME_COMMON_DIR)/cg_inventory.c \
	$(CGAME_COMMON_DIR)/cg_light.c \
	$(CGAME_COMMON_DIR)/cg_main.c \
	$(CGAME_COMMON_DIR)/cg_media.c \
	$(CGAME_COMMON_DIR)/cg_muzzle_flash.c \
	$(CGAME_COMMON_DIR)/cg_predict.c \
	$(CGAME_COMMON_DIR)/cg_sound.c \
	$(CGAME_COMMON_DIR)/cg_sprite.c \
	$(CGAME_COMMON_DIR)/cg_temp_entity.c \
	$(CGAME_COMMON_DIR)/cg_ui.c \
	$(CGAME_COMMON_DIR)/cg_view.c \
	$(CGAME_COMMON_DIR)/cg_weapon.c \
	$(CGAME_COMMON_DIR)/ui/BindTextView.c \
	$(CGAME_COMMON_DIR)/ui/CvarCheckbox.c \
	$(CGAME_COMMON_DIR)/ui/CvarSelect.c \
	$(CGAME_COMMON_DIR)/ui/CvarSlider.c \
	$(CGAME_COMMON_DIR)/ui/CvarTextView.c \
	$(CGAME_COMMON_DIR)/ui/DialogViewController.c \
	$(CGAME_COMMON_DIR)/ui/controls/ControlsViewController.c \
	$(CGAME_COMMON_DIR)/ui/controls/CrosshairView.c \
	$(CGAME_COMMON_DIR)/ui/controls/MovementCombatViewController.c \
	$(CGAME_COMMON_DIR)/ui/controls/ResponseServiceViewController.c \
	$(CGAME_COMMON_DIR)/ui/credits/CreditsViewController.c \
	$(CGAME_COMMON_DIR)/ui/editor/EditorViewController.c \
	$(CGAME_COMMON_DIR)/ui/editor/EntityView.c \
	$(CGAME_COMMON_DIR)/ui/editor/EntityViewController.c \
	$(CGAME_COMMON_DIR)/ui/editor/MaterialViewController.c \
	$(CGAME_COMMON_DIR)/ui/editor/MeshViewController.c \
	$(CGAME_COMMON_DIR)/ui/home/HomeViewController.c \
	$(CGAME_COMMON_DIR)/ui/home/LeaderboardViewController.c \
	$(CGAME_COMMON_DIR)/ui/home/StatsViewController.c \
	$(CGAME_COMMON_DIR)/ui/main/LoadingViewController.c \
	$(CGAME_COMMON_DIR)/ui/main/MainView.c \
	$(CGAME_COMMON_DIR)/ui/main/MainViewController.c \
	$(CGAME_COMMON_DIR)/ui/main/UpdateViewController.c \
	$(CGAME_COMMON_DIR)/ui/play/CreateServerViewController.c \
	$(CGAME_COMMON_DIR)/ui/play/JoinServerViewController.c \
	$(CGAME_COMMON_DIR)/ui/play/MapListCollectionItemView.c \
	$(CGAME_COMMON_DIR)/ui/play/MapListCollectionView.c \
	$(CGAME_COMMON_DIR)/ui/play/PlayViewController.c \
	$(CGAME_COMMON_DIR)/ui/play/PlayerModelView.c \
	$(CGAME_COMMON_DIR)/ui/play/PlayerSetupViewController.c \
	$(CGAME_COMMON_DIR)/ui/settings/SettingsViewController.c \
	$(CGAME_COMMON_DIR)/ui/teams/TeamPlayerView.c \
	$(CGAME_COMMON_DIR)/ui/teams/TeamView.c \
	$(CGAME_COMMON_DIR)/ui/teams/TeamsViewController.c

libcgcommondefault_la_CFLAGS = \
	$(CGAME_CFLAGS)

cgamelibdir = \
	@PKGLIBDIR@/default

cgamelib_LTLIBRARIES = \
	cgame.la

cgame_la_SOURCES = \
	cg_hud.c \
	cg_score.c \
	cg_team_mode.c

cgame_la_CFLAGS = \
	$(CGAME_CFLAGS)

cgame_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-shared

cgame_la_LIBADD = \
	libcgcommondefault.la \
	$(top_builddir)/src/game/default/libitem.la \
	$(top_builddir)/src/game/default/libpmovedefault.la \
	$(top_builddir)/deps/discord-rpc/src/libdiscord-rpc.la \
	@OBJECTIVELYMVC_LIBS@

if APPLE
cgame_la_LIBADD += -lc++
cgame_la_LDFLAGS += -framework AppKit
else
cgame_la_LIBADD += -lstdc++
endif
