# Shared client game UI sources, included by each cgame module's Makefile.am.
#
# These MUST be compiled per module rather than linked from a prebuilt library.
# cg_state_t embeds cg_team_info_t teams[MAX_TEAMS] along with g_gameplay_t and
# g_items_t, all of which come from the module's own game/<module>/g_types.h. A
# library built against one module's definitions would read cg_state at the
# wrong offsets in any module that changes the team roster or item set.
#
# Set CGUI_DIR to this directory, then add $(CGUI_SRC) to the module's
# cgame_la_SOURCES, $(CGUI_HDR) to its noinst_HEADERS, and $(CGUI_CFLAGS) to
# its cgame_la_CFLAGS. The _SRC/_HDR names avoid automake's _SOURCES/_HEADERS
# primary suffixes, which would otherwise demand a matching install dir.

CGUI_SRC = \
	$(CGUI_DIR)/common/BindTextView.c \
	$(CGUI_DIR)/common/CvarCheckbox.c \
	$(CGUI_DIR)/common/CvarSelect.c \
	$(CGUI_DIR)/common/CvarSlider.c \
	$(CGUI_DIR)/common/CvarTextView.c \
	$(CGUI_DIR)/common/DialogViewController.c \
	$(CGUI_DIR)/controls/ControlsViewController.c \
	$(CGUI_DIR)/controls/CrosshairView.c \
	$(CGUI_DIR)/controls/MovementCombatViewController.c \
	$(CGUI_DIR)/controls/ResponseServiceViewController.c \
	$(CGUI_DIR)/credits/CreditsViewController.c \
	$(CGUI_DIR)/editor/EditorViewController.c \
	$(CGUI_DIR)/editor/EntityView.c \
	$(CGUI_DIR)/editor/EntityViewController.c \
	$(CGUI_DIR)/editor/MaterialViewController.c \
	$(CGUI_DIR)/editor/MeshViewController.c \
	$(CGUI_DIR)/home/HomeViewController.c \
	$(CGUI_DIR)/home/LeaderboardViewController.c \
	$(CGUI_DIR)/home/StatsViewController.c \
	$(CGUI_DIR)/main/LoadingViewController.c \
	$(CGUI_DIR)/main/MainView.c \
	$(CGUI_DIR)/main/MainViewController.c \
	$(CGUI_DIR)/main/UpdateViewController.c \
	$(CGUI_DIR)/play/CreateServerViewController.c \
	$(CGUI_DIR)/play/JoinServerViewController.c \
	$(CGUI_DIR)/play/MapListCollectionItemView.c \
	$(CGUI_DIR)/play/MapListCollectionView.c \
	$(CGUI_DIR)/play/PlayerModelView.c \
	$(CGUI_DIR)/play/PlayerSetupViewController.c \
	$(CGUI_DIR)/play/PlayViewController.c \
	$(CGUI_DIR)/settings/SettingsViewController.c \
	$(CGUI_DIR)/teams/TeamPlayerView.c \
	$(CGUI_DIR)/teams/TeamsViewController.c \
	$(CGUI_DIR)/teams/TeamView.c

CGUI_HDR = \
	$(CGUI_DIR)/common/BindTextView.h \
	$(CGUI_DIR)/common/CvarCheckbox.h \
	$(CGUI_DIR)/common/CvarSelect.h \
	$(CGUI_DIR)/common/CvarSlider.h \
	$(CGUI_DIR)/common/CvarTextView.h \
	$(CGUI_DIR)/common/DialogViewController.h \
	$(CGUI_DIR)/controls/ControlsViewController.h \
	$(CGUI_DIR)/controls/CrosshairView.h \
	$(CGUI_DIR)/controls/MovementCombatViewController.h \
	$(CGUI_DIR)/controls/ResponseServiceViewController.h \
	$(CGUI_DIR)/credits/CreditsViewController.h \
	$(CGUI_DIR)/editor/EditorViewController.h \
	$(CGUI_DIR)/editor/EntityView.h \
	$(CGUI_DIR)/editor/EntityViewController.h \
	$(CGUI_DIR)/editor/MaterialViewController.h \
	$(CGUI_DIR)/editor/MeshViewController.h \
	$(CGUI_DIR)/home/HomeViewController.h \
	$(CGUI_DIR)/home/LeaderboardViewController.h \
	$(CGUI_DIR)/home/StatsViewController.h \
	$(CGUI_DIR)/main/LoadingViewController.h \
	$(CGUI_DIR)/main/MainView.h \
	$(CGUI_DIR)/main/MainViewController.h \
	$(CGUI_DIR)/main/UpdateViewController.h \
	$(CGUI_DIR)/play/CreateServerViewController.h \
	$(CGUI_DIR)/play/JoinServerViewController.h \
	$(CGUI_DIR)/play/MapListCollectionItemView.h \
	$(CGUI_DIR)/play/MapListCollectionView.h \
	$(CGUI_DIR)/play/PlayerModelView.h \
	$(CGUI_DIR)/play/PlayerSetupViewController.h \
	$(CGUI_DIR)/play/PlayViewController.h \
	$(CGUI_DIR)/settings/SettingsViewController.h \
	$(CGUI_DIR)/teams/TeamPlayerView.h \
	$(CGUI_DIR)/teams/TeamsViewController.h \
	$(CGUI_DIR)/teams/TeamView.h

CGUI_CFLAGS = \
	-I$(CGUI_DIR) \
	-I$(CGUI_DIR)/common \
	-I$(CGUI_DIR)/controls \
	-I$(CGUI_DIR)/credits \
	-I$(CGUI_DIR)/editor \
	-I$(CGUI_DIR)/home \
	-I$(CGUI_DIR)/main \
	-I$(CGUI_DIR)/play \
	-I$(CGUI_DIR)/settings \
	-I$(CGUI_DIR)/teams
