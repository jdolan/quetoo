CGAME_COMMON_DIR = $(top_srcdir)/src/cgame/common
GAME_COMMON_DIR = $(top_srcdir)/src/game/common

VPATH = $(srcdir):$(CGAME_COMMON_DIR):$(CGAME_COMMON_DIR)/ui:$(CGAME_COMMON_DIR)/ui/controls:$(CGAME_COMMON_DIR)/ui/credits:$(CGAME_COMMON_DIR)/ui/editor:$(CGAME_COMMON_DIR)/ui/home:$(CGAME_COMMON_DIR)/ui/main:$(CGAME_COMMON_DIR)/ui/play:$(CGAME_COMMON_DIR)/ui/settings:$(CGAME_COMMON_DIR)/ui/teams

CGAME_COMMON_OBJECTS = \
	BindTextView.lo \
	ControlsViewController.lo \
	CreateServerViewController.lo \
	CreditsViewController.lo \
	CrosshairView.lo \
	CvarCheckbox.lo \
	CvarSelect.lo \
	CvarSlider.lo \
	CvarTextView.lo \
	DialogViewController.lo \
	EditorViewController.lo \
	EntityView.lo \
	EntityViewController.lo \
	HomeViewController.lo \
	JoinServerViewController.lo \
	LeaderboardViewController.lo \
	LoadingViewController.lo \
	MainView.lo \
	MainViewController.lo \
	MapListCollectionItemView.lo \
	MapListCollectionView.lo \
	MaterialViewController.lo \
	MeshViewController.lo \
	MovementCombatViewController.lo \
	PlayViewController.lo \
	PlayerModelView.lo \
	PlayerSetupViewController.lo \
	ResponseServiceViewController.lo \
	SettingsViewController.lo \
	StatsViewController.lo \
	TeamPlayerView.lo \
	TeamView.lo \
	TeamsViewController.lo \
	UpdateViewController.lo \
	cg_client.lo \
	cg_discord.lo \
	cg_editor.lo \
	cg_effect.lo \
	cg_entity.lo \
	cg_entity_effect.lo \
	cg_entity_event.lo \
	cg_entity_misc.lo \
	cg_entity_trail.lo \
	cg_flare.lo \
	cg_hud_draw.lo \
	cg_input.lo \
	cg_inventory.lo \
	cg_light.lo \
	cg_main.lo \
	cg_media.lo \
	cg_muzzle_flash.lo \
	cg_predict.lo \
	cg_sound.lo \
	cg_sprite.lo \
	cg_temp_entity.lo \
	cg_ui.lo \
	cg_view.lo \
	cg_weapon.lo

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
	$(CGAME_COMMON_OBJECTS) \
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

%.lo: %.c
	$(AM_V_CC)$(LIBTOOL) $(AM_V_lt) --tag=CC $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) \
		--mode=compile $(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
		$(AM_CPPFLAGS) $(CPPFLAGS) $(CGAME_CFLAGS) $(CFLAGS) \
		-MT $@ -MD -MP -MF $(DEPDIR)/$*.Tpo -c -o $@ $<
	$(AM_V_at)$(am__mv) $(DEPDIR)/$*.Tpo $(DEPDIR)/$*.Plo

-include $(CGAME_COMMON_OBJECTS:%.lo=$(DEPDIR)/%.Plo)
