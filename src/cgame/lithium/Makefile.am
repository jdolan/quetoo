CGAME_COMMON_DIR = $(top_srcdir)/src/cgame/common

GAME_DIR = $(top_srcdir)/src/game/lithium
GAME_COMMON_DIR = $(top_srcdir)/src/game/common

vpath %.c $(srcdir):$(CGAME_COMMON_DIR):$(GAME_DIR):$(GAME_COMMON_DIR)

AM_CPPFLAGS = \
	-DG_HOOK \
	-DG_TECH \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/cgame \
	-I$(CGAME_COMMON_DIR) \
	-I$(CGAME_COMMON_DIR)/ui \
	-I$(CGAME_COMMON_DIR)/ui/controls \
	-I$(CGAME_COMMON_DIR)/ui/credits \
	-I$(CGAME_COMMON_DIR)/ui/editor \
	-I$(CGAME_COMMON_DIR)/ui/home \
	-I$(CGAME_COMMON_DIR)/ui/main \
	-I$(CGAME_COMMON_DIR)/ui/play \
	-I$(CGAME_COMMON_DIR)/ui/settings \
	-I$(CGAME_COMMON_DIR)/ui/teams \
	-I$(GAME_DIR) \
	-I$(GAME_COMMON_DIR)

AM_CFLAGS = \
	@SO_CFLAGS@ \
	@BASE_CFLAGS@ \
	@OBJECTIVELY_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@ \
	@OPENAL_CFLAGS@ \
	@SDL3_CFLAGS@ \
	@SNDFILE_CFLAGS@

cgamelibdir = \
	@PKGLIBDIR@/lithium

cgamelib_LTLIBRARIES = \
	cgame.la

cgame_la_SOURCES = \
	bg_item.c \
	bg_pmove.c \
	cg_client.c \
	cg_discord.c \
	cg_editor.c \
	cg_effect.c \
	cg_entity.c \
	cg_entity_effect.c \
	cg_entity_event.c \
	cg_entity_misc.c \
	cg_entity_trail.c \
	cg_flare.c \
	cg_hud.c \
	cg_hud_draw.c \
	cg_input.c \
	cg_inventory.c \
	cg_light.c \
	cg_main.c \
	cg_media.c \
	cg_muzzle_flash.c \
	cg_predict.c \
	cg_score.c \
	cg_sound.c \
	cg_sprite.c \
	cg_team_mode.c \
	cg_temp_entity.c \
	cg_ui.c \
	cg_view.c \
	cg_weapon.c \
	ui/BindTextView.c \
	ui/CvarCheckbox.c \
	ui/CvarSelect.c \
	ui/CvarSlider.c \
	ui/CvarTextView.c \
	ui/DialogViewController.c \
	ui/controls/ControlsViewController.c \
	ui/controls/CrosshairView.c \
	ui/controls/MovementCombatViewController.c \
	ui/controls/ResponseServiceViewController.c \
	ui/credits/CreditsViewController.c \
	ui/editor/EditorViewController.c \
	ui/editor/EntityView.c \
	ui/editor/EntityViewController.c \
	ui/editor/MaterialViewController.c \
	ui/editor/MeshViewController.c \
	ui/home/HomeViewController.c \
	ui/home/LeaderboardViewController.c \
	ui/home/StatsViewController.c \
	ui/main/LoadingViewController.c \
	ui/main/MainView.c \
	ui/main/MainViewController.c \
	ui/main/UpdateViewController.c \
	ui/play/CreateServerViewController.c \
	ui/play/JoinServerViewController.c \
	ui/play/MapListCollectionItemView.c \
	ui/play/MapListCollectionView.c \
	ui/play/PlayViewController.c \
	ui/play/PlayerModelView.c \
	ui/play/PlayerSetupViewController.c \
	ui/settings/SettingsViewController.c \
	ui/teams/TeamPlayerView.c \
	ui/teams/TeamView.c \
	ui/teams/TeamsViewController.c

cgame_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-shared

cgame_la_LIBADD = \
	$(top_builddir)/src/shared/libshared.la \
	$(top_builddir)/deps/discord-rpc/src/libdiscord-rpc.la \
	@OBJECTIVELY_LIBS@ \
	@OBJECTIVELYMVC_LIBS@

if APPLE
cgame_la_LIBADD += -lc++
cgame_la_LDFLAGS += -framework AppKit
else
cgame_la_LIBADD += -lstdc++
endif
