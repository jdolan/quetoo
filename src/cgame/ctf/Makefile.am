CGAME_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/cgame \
	-I$(top_srcdir)/src/game/ctf \
	-I$(top_srcdir)/src/game/common \
	-I$(srcdir) \
	-I$(top_srcdir)/src/cgame/common \
	-I$(top_srcdir)/src/cgame/common/ui \
	-I$(top_srcdir)/src/cgame/common/ui/controls \
	-I$(top_srcdir)/src/cgame/common/ui/credits \
	-I$(top_srcdir)/src/cgame/common/ui/editor \
	-I$(top_srcdir)/src/cgame/common/ui/home \
	-I$(top_srcdir)/src/cgame/common/ui/main \
	-I$(top_srcdir)/src/cgame/common/ui/play \
	-I$(top_srcdir)/src/cgame/common/ui/settings \
	-I$(top_srcdir)/src/cgame/common/ui/teams \
	-DG_HOOK \
	-DG_CTF \
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
	$(top_srcdir)/src/cgame/common/cg_client.h \
	$(top_srcdir)/src/cgame/common/cg_discord.h \
	$(top_srcdir)/src/cgame/common/cg_editor.h \
	$(top_srcdir)/src/cgame/common/cg_effect.h \
	$(top_srcdir)/src/cgame/common/cg_entity.h \
	$(top_srcdir)/src/cgame/common/cg_entity_effect.h \
	$(top_srcdir)/src/cgame/common/cg_entity_event.h \
	$(top_srcdir)/src/cgame/common/cg_entity_misc.h \
	$(top_srcdir)/src/cgame/common/cg_entity_trail.h \
	$(top_srcdir)/src/cgame/common/cg_flare.h \
	$(top_srcdir)/src/cgame/common/cg_hud_draw.h \
	$(top_srcdir)/src/cgame/common/cg_input.h \
	$(top_srcdir)/src/cgame/common/cg_inventory.h \
	$(top_srcdir)/src/cgame/common/cg_light.h \
	$(top_srcdir)/src/cgame/common/cg_local.h \
	$(top_srcdir)/src/cgame/common/cg_main.h \
	$(top_srcdir)/src/cgame/common/cg_media.h \
	$(top_srcdir)/src/cgame/common/cg_muzzle_flash.h \
	$(top_srcdir)/src/cgame/common/cg_predict.h \
	$(top_srcdir)/src/cgame/common/cg_sound.h \
	$(top_srcdir)/src/cgame/common/cg_sprite.h \
	$(top_srcdir)/src/cgame/common/cg_team_mode.h \
	$(top_srcdir)/src/cgame/common/cg_temp_entity.h \
	$(top_srcdir)/src/cgame/common/cg_types.h \
	$(top_srcdir)/src/cgame/common/cg_ui.h \
	$(top_srcdir)/src/cgame/common/cg_view.h \
	$(top_srcdir)/src/cgame/common/cg_weapon.h \
	$(top_srcdir)/src/cgame/common/ui/BindTextView.h \
	$(top_srcdir)/src/cgame/common/ui/CvarCheckbox.h \
	$(top_srcdir)/src/cgame/common/ui/CvarSelect.h \
	$(top_srcdir)/src/cgame/common/ui/CvarSlider.h \
	$(top_srcdir)/src/cgame/common/ui/CvarTextView.h \
	$(top_srcdir)/src/cgame/common/ui/DialogViewController.h \
	$(top_srcdir)/src/cgame/common/ui/controls/ControlsViewController.h \
	$(top_srcdir)/src/cgame/common/ui/controls/CrosshairView.h \
	$(top_srcdir)/src/cgame/common/ui/controls/MovementCombatViewController.h \
	$(top_srcdir)/src/cgame/common/ui/controls/ResponseServiceViewController.h \
	$(top_srcdir)/src/cgame/common/ui/credits/CreditsViewController.h \
	$(top_srcdir)/src/cgame/common/ui/editor/EditorViewController.h \
	$(top_srcdir)/src/cgame/common/ui/editor/EntityView.h \
	$(top_srcdir)/src/cgame/common/ui/editor/EntityViewController.h \
	$(top_srcdir)/src/cgame/common/ui/editor/MaterialViewController.h \
	$(top_srcdir)/src/cgame/common/ui/editor/MeshViewController.h \
	$(top_srcdir)/src/cgame/common/ui/home/HomeViewController.h \
	$(top_srcdir)/src/cgame/common/ui/home/LeaderboardViewController.h \
	$(top_srcdir)/src/cgame/common/ui/home/StatsViewController.h \
	$(top_srcdir)/src/cgame/common/ui/main/LoadingViewController.h \
	$(top_srcdir)/src/cgame/common/ui/main/MainView.h \
	$(top_srcdir)/src/cgame/common/ui/main/MainViewController.h \
	$(top_srcdir)/src/cgame/common/ui/main/UpdateViewController.h \
	$(top_srcdir)/src/cgame/common/ui/play/CreateServerViewController.h \
	$(top_srcdir)/src/cgame/common/ui/play/JoinServerViewController.h \
	$(top_srcdir)/src/cgame/common/ui/play/MapListCollectionItemView.h \
	$(top_srcdir)/src/cgame/common/ui/play/MapListCollectionView.h \
	$(top_srcdir)/src/cgame/common/ui/play/PlayerModelView.h \
	$(top_srcdir)/src/cgame/common/ui/play/PlayerSetupViewController.h \
	$(top_srcdir)/src/cgame/common/ui/play/PlayViewController.h \
	$(top_srcdir)/src/cgame/common/ui/settings/SettingsViewController.h \
	$(top_srcdir)/src/cgame/common/ui/teams/TeamPlayerView.h \
	$(top_srcdir)/src/cgame/common/ui/teams/TeamsViewController.h \
	$(top_srcdir)/src/cgame/common/ui/teams/TeamView.h

# The common UI and client game sources are compiled here rather than linked
# from a prebuilt library, because cg_state_t and the item roster resolve from
# this module's game directory. This library's name MUST stay unique across
# modules: with subdir-objects automake derives object file names from it, so
# two modules sharing a prefix would collide in the common directories.
noinst_LTLIBRARIES = \
	libcgcommonctf.la

libcgcommonctf_la_SOURCES = \
	$(top_srcdir)/src/cgame/common/cg_client.c \
	$(top_srcdir)/src/cgame/common/cg_discord.c \
	$(top_srcdir)/src/cgame/common/cg_editor.c \
	$(top_srcdir)/src/cgame/common/cg_effect.c \
	$(top_srcdir)/src/cgame/common/cg_entity.c \
	$(top_srcdir)/src/cgame/common/cg_entity_effect.c \
	$(top_srcdir)/src/cgame/common/cg_entity_event.c \
	$(top_srcdir)/src/cgame/common/cg_entity_misc.c \
	$(top_srcdir)/src/cgame/common/cg_entity_trail.c \
	$(top_srcdir)/src/cgame/common/cg_flare.c \
	$(top_srcdir)/src/cgame/common/cg_hud_draw.c \
	$(top_srcdir)/src/cgame/common/cg_input.c \
	$(top_srcdir)/src/cgame/common/cg_inventory.c \
	$(top_srcdir)/src/cgame/common/cg_light.c \
	$(top_srcdir)/src/cgame/common/cg_main.c \
	$(top_srcdir)/src/cgame/common/cg_media.c \
	$(top_srcdir)/src/cgame/common/cg_muzzle_flash.c \
	$(top_srcdir)/src/cgame/common/cg_predict.c \
	$(top_srcdir)/src/cgame/common/cg_sound.c \
	$(top_srcdir)/src/cgame/common/cg_sprite.c \
	$(top_srcdir)/src/cgame/common/cg_temp_entity.c \
	$(top_srcdir)/src/cgame/common/cg_ui.c \
	$(top_srcdir)/src/cgame/common/cg_view.c \
	$(top_srcdir)/src/cgame/common/cg_weapon.c \
	$(top_srcdir)/src/cgame/common/ui/BindTextView.c \
	$(top_srcdir)/src/cgame/common/ui/CvarCheckbox.c \
	$(top_srcdir)/src/cgame/common/ui/CvarSelect.c \
	$(top_srcdir)/src/cgame/common/ui/CvarSlider.c \
	$(top_srcdir)/src/cgame/common/ui/CvarTextView.c \
	$(top_srcdir)/src/cgame/common/ui/DialogViewController.c \
	$(top_srcdir)/src/cgame/common/ui/controls/ControlsViewController.c \
	$(top_srcdir)/src/cgame/common/ui/controls/CrosshairView.c \
	$(top_srcdir)/src/cgame/common/ui/controls/MovementCombatViewController.c \
	$(top_srcdir)/src/cgame/common/ui/controls/ResponseServiceViewController.c \
	$(top_srcdir)/src/cgame/common/ui/credits/CreditsViewController.c \
	$(top_srcdir)/src/cgame/common/ui/editor/EditorViewController.c \
	$(top_srcdir)/src/cgame/common/ui/editor/EntityView.c \
	$(top_srcdir)/src/cgame/common/ui/editor/EntityViewController.c \
	$(top_srcdir)/src/cgame/common/ui/editor/MaterialViewController.c \
	$(top_srcdir)/src/cgame/common/ui/editor/MeshViewController.c \
	$(top_srcdir)/src/cgame/common/ui/home/HomeViewController.c \
	$(top_srcdir)/src/cgame/common/ui/home/LeaderboardViewController.c \
	$(top_srcdir)/src/cgame/common/ui/home/StatsViewController.c \
	$(top_srcdir)/src/cgame/common/ui/main/LoadingViewController.c \
	$(top_srcdir)/src/cgame/common/ui/main/MainView.c \
	$(top_srcdir)/src/cgame/common/ui/main/MainViewController.c \
	$(top_srcdir)/src/cgame/common/ui/main/UpdateViewController.c \
	$(top_srcdir)/src/cgame/common/ui/play/CreateServerViewController.c \
	$(top_srcdir)/src/cgame/common/ui/play/JoinServerViewController.c \
	$(top_srcdir)/src/cgame/common/ui/play/MapListCollectionItemView.c \
	$(top_srcdir)/src/cgame/common/ui/play/MapListCollectionView.c \
	$(top_srcdir)/src/cgame/common/ui/play/PlayerModelView.c \
	$(top_srcdir)/src/cgame/common/ui/play/PlayerSetupViewController.c \
	$(top_srcdir)/src/cgame/common/ui/play/PlayViewController.c \
	$(top_srcdir)/src/cgame/common/ui/settings/SettingsViewController.c \
	$(top_srcdir)/src/cgame/common/ui/teams/TeamPlayerView.c \
	$(top_srcdir)/src/cgame/common/ui/teams/TeamsViewController.c \
	$(top_srcdir)/src/cgame/common/ui/teams/TeamView.c

libcgcommonctf_la_CFLAGS = \
	$(CGAME_CFLAGS)

cgamelibdir = \
	@PKGLIBDIR@/ctf

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
	libcgcommonctf.la \
	$(top_builddir)/src/game/ctf/libitem.la \
	$(top_builddir)/src/game/ctf/libpmovectf.la \
	$(top_builddir)/deps/discord-rpc/src/libdiscord-rpc.la \
	@OBJECTIVELYMVC_LIBS@

if APPLE
cgame_la_LIBADD += -lc++
cgame_la_LDFLAGS += -framework AppKit
else
cgame_la_LIBADD += -lstdc++
endif
