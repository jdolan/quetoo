CGAME_COMMON_DIR = $(top_srcdir)/src/cgame/common

GAME_DIR = $(top_srcdir)/src/game/default
GAME_COMMON_DIR = $(top_srcdir)/src/game/common

vpath %.c $(srcdir):$(CGAME_COMMON_DIR):$(GAME_DIR):$(GAME_COMMON_DIR)

AM_CPPFLAGS = \
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

CGUI_DIR = $(CGAME_COMMON_DIR)/ui

cguidir = \
	@PKGLIBDIR@/default/ui

cgui_DATA = \
	$(CGUI_DIR)/DialogViewController.css \
	$(CGUI_DIR)/DialogViewController.json \
	$(CGUI_DIR)/change.wav \
	$(CGUI_DIR)/clack.wav \
	$(CGUI_DIR)/click.wav \
	$(CGUI_DIR)/common.css \
	$(CGUI_DIR)/conback.png \
	$(CGUI_DIR)/loading.png \
	$(CGUI_DIR)/logo.png \
	$(CGUI_DIR)/progress_bar.png

cguibackgroundsdir = \
	@PKGLIBDIR@/default/ui/backgrounds

cguibackgrounds_DATA = \
	$(CGUI_DIR)/backgrounds/0.png \
	$(CGUI_DIR)/backgrounds/1.png \
	$(CGUI_DIR)/backgrounds/2.png \
	$(CGUI_DIR)/backgrounds/3.png \
	$(CGUI_DIR)/backgrounds/4.png \
	$(CGUI_DIR)/backgrounds/5.png

cguifontsdir = \
	@PKGLIBDIR@/default/ui/fonts

cguifonts_DATA = \
	$(CGUI_DIR)/fonts/large.png \
	$(CGUI_DIR)/fonts/medium.png \
	$(CGUI_DIR)/fonts/small.png

cguicontrolsdir = \
	@PKGLIBDIR@/default/ui/controls

cguicontrols_DATA = \
	$(CGUI_DIR)/controls/ControlsViewController.json \
	$(CGUI_DIR)/controls/MovementCombatViewController.css \
	$(CGUI_DIR)/controls/MovementCombatViewController.json \
	$(CGUI_DIR)/controls/ResponseServiceViewController.css \
	$(CGUI_DIR)/controls/ResponseServiceViewController.json

cguicreditsdir = \
	@PKGLIBDIR@/default/ui/credits

cguicredits_DATA = \
	$(CGUI_DIR)/credits/CreditsViewController.css \
	$(CGUI_DIR)/credits/CreditsViewController.json

cguieditordir = \
	@PKGLIBDIR@/default/ui/editor

cguieditor_DATA = \
	$(CGUI_DIR)/editor/EditorViewController.json \
	$(CGUI_DIR)/editor/EntityView.json \
	$(CGUI_DIR)/editor/EntityViewController.css \
	$(CGUI_DIR)/editor/EntityViewController.json \
	$(CGUI_DIR)/editor/MaterialViewController.css \
	$(CGUI_DIR)/editor/MaterialViewController.json \
	$(CGUI_DIR)/editor/MeshViewController.css \
	$(CGUI_DIR)/editor/MeshViewController.json \
	$(CGUI_DIR)/editor/editor.css

cguihomedir = \
	@PKGLIBDIR@/default/ui/home

cguihome_DATA = \
	$(CGUI_DIR)/home/HomeViewController.css \
	$(CGUI_DIR)/home/HomeViewController.json \
	$(CGUI_DIR)/home/LeaderboardViewController.css \
	$(CGUI_DIR)/home/LeaderboardViewController.json \
	$(CGUI_DIR)/home/StatsViewController.css \
	$(CGUI_DIR)/home/StatsViewController.json

cguimaindir = \
	@PKGLIBDIR@/default/ui/main

cguimain_DATA = \
	$(CGUI_DIR)/main/LoadingViewController.css \
	$(CGUI_DIR)/main/LoadingViewController.json \
	$(CGUI_DIR)/main/MainView.css \
	$(CGUI_DIR)/main/MainView.json \
	$(CGUI_DIR)/main/UpdateViewController.css \
	$(CGUI_DIR)/main/UpdateViewController.json

cguiplaydir = \
	@PKGLIBDIR@/default/ui/play

cguiplay_DATA = \
	$(CGUI_DIR)/play/CreateServerViewController.css \
	$(CGUI_DIR)/play/CreateServerViewController.json \
	$(CGUI_DIR)/play/JoinServerViewController.css \
	$(CGUI_DIR)/play/JoinServerViewController.json \
	$(CGUI_DIR)/play/PlayViewController.json \
	$(CGUI_DIR)/play/PlayerSetupViewController.css \
	$(CGUI_DIR)/play/PlayerSetupViewController.json

cguisettingsdir = \
	@PKGLIBDIR@/default/ui/settings

cguisettings_DATA = \
	$(CGUI_DIR)/settings/SettingsViewController.json

cguiteamsdir = \
	@PKGLIBDIR@/default/ui/teams

cguiteams_DATA = \
	$(CGUI_DIR)/teams/TeamPlayerView.json \
	$(CGUI_DIR)/teams/TeamView.json \
	$(CGUI_DIR)/teams/TeamsViewController.css \
	$(CGUI_DIR)/teams/TeamsViewController.json

cgamelibdir = \
	@PKGLIBDIR@/default

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
