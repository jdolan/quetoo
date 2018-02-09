commondir = @PKGLIBDIR@/default/ui/common
common_DATA = \
	common/common.css

_infodir = @PKGLIBDIR@/default/ui/info
_info_DATA = \
	info/CreditsViewController.css

controlsdir = @PKGLIBDIR@/default/ui/controls
controls_DATA = \
	controls/LookViewController.css \
	controls/MiscViewController.css \
	controls/ShootViewController.css

maindir = @PKGLIBDIR@/default/ui/main
main_DATA = \
	main/DialogViewController.css \
	main/HomeViewController.css \
	main/LoadingViewController.css \
	main/MainView.css

playdir = @PKGLIBDIR@/default/ui/play
play_DATA = \
	play/CreateServerViewController.css \
	play/JoinServerViewController.css \
	play/PlayerSetupViewController.css

settingsdir = @PKGLIBDIR@/default/ui/settings
settings_DATA = \
	settings/OptionsViewController.css \
	settings/SystemViewController.css

noinst_HEADERS = \
	common/BindTextView.h \
	common/CvarCheckbox.h \
	common/CvarSelect.h \
	common/CvarSlider.h \
	common/CvarTextView.h \
	common/QuetooTheme.h \
	\
	controls/ControlsViewController.h \
	controls/CrosshairView.h \
	controls/MiscViewController.h \
	controls/MoveViewController.h \
	controls/LookViewController.h \
	controls/ShootViewController.h \
	\
	info/CreditsViewController.h \
	info/InfoViewController.h \
	\
	main/DialogView.h \
	main/DialogViewController.h \
	main/HomeViewController.h \
	main/LoadingViewController.h \
	main/MainView.h \
	main/MainViewController.h \
	\
	play/CreateServerViewController.h \
	play/JoinServerViewController.h \
	play/MapListCollectionItemView.h \
	play/MapListCollectionView.h \
	play/PlayerModelView.h \
	play/PlayerSetupViewController.h \
	play/PlayViewController.h \
	\
	settings/OptionsViewController.h \
	settings/SettingsViewController.h \
	settings/SystemViewController.h \
	settings/VideoModeSelect.h

noinst_LTLIBRARIES = \
	libui.la

libui_la_SOURCES = \
	common/BindTextView.c \
	common/CvarCheckbox.c \
	common/CvarSelect.c \
	common/CvarSlider.c \
	common/CvarTextView.c \
	common/QuetooTheme.c \
	\
	controls/ControlsViewController.c \
	controls/CrosshairView.c \
	controls/MiscViewController.c \
	controls/MoveViewController.c \
	controls/LookViewController.c \
	controls/ShootViewController.c \
	\
	info/CreditsViewController.c \
	info/InfoViewController.c \
	\
	main/DialogView.c \
	main/DialogViewController.c \
	main/HomeViewController.c \
	main/LoadingViewController.c \
	main/MainView.c \
	main/MainViewController.c \
	\
	play/CreateServerViewController.c \
	play/JoinServerViewController.c \
	play/MapListCollectionItemView.c \
	play/MapListCollectionView.c \
	play/PlayerModelView.c \
	play/PlayerSetupViewController.c \
	play/PlayViewController.c \
	\
	settings/OptionsViewController.c \
	settings/SettingsViewController.c \
	settings/SystemViewController.c \
	settings/VideoModeSelect.c

libui_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/cgame \
	-I$(top_srcdir)/src/cgame/default \
	-I$(top_srcdir)/src/cgame/default/ui \
	-I$(top_srcdir)/src/cgame/default/ui/common \
	-I$(top_srcdir)/src/cgame/default/ui/controls \
	-I$(top_srcdir)/src/cgame/default/ui/info \
	-I$(top_srcdir)/src/cgame/default/ui/main \
	-I$(top_srcdir)/src/cgame/default/ui/play \
	-I$(top_srcdir)/src/cgame/default/ui/settings \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@ \
	@OPENGL_CFLAGS@

libui_la_LDFLAGS = \
	-shared

libui_la_LIBADD = \
	@OBJECTIVELYMVC_LIBS@ \
	@OPENGL_LIBS@

