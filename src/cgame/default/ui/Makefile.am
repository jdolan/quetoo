noinst_HEADERS = \
	common/CvarCheckbox.h \
	common/CvarSelect.h \
	common/CvarSlider.h \
	common/CvarTextView.h \
	common/Theme.h \
	\
	info/CreditsViewController.h \
	info/InfoViewController.h \
	\
	main/DialogView.h \
	main/HomeViewController.h \
	main/LoadingViewController.h \
	main/MainView.h \
	main/MainViewController.h \
	main/PrimaryButton.h \
	main/PrimaryIcon.h \
	\
	play/CreateServerViewController.h \
	play/JoinServerViewController.h \
	play/MapListCollectionItemView.h \
	play/MapListCollectionView.h \
	play/PlayViewController.h \
	\
	settings/BindTextView.h \
	settings/CrosshairView.h \
	settings/KeysViewController.h \
	settings/MouseViewController.h \
	settings/OptionsViewController.h \
	settings/PlayerModelView.h \
	settings/PlayerViewController.h \
	settings/SettingsViewController.h \
	settings/SystemViewController.h \
	settings/VideoModeSelect.h

noinst_LTLIBRARIES = \
	libui.la

libui_la_SOURCES = \
	common/CvarCheckbox.c \
	common/CvarSelect.c \
	common/CvarSlider.c \
	common/CvarTextView.c \
	common/Theme.c \
	\
	info/CreditsViewController.c \
	info/InfoViewController.c \
	\
	main/DialogView.c \
	main/HomeViewController.c \
	main/LoadingViewController.c \
	main/MainView.c \
	main/MainViewController.c \
	main/PrimaryButton.c \
	main/PrimaryIcon.c \
	\
	play/CreateServerViewController.c \
	play/JoinServerViewController.c \
	play/MapListCollectionItemView.c \
	play/MapListCollectionView.c \
	play/PlayViewController.c \
	\
	settings/BindTextView.c \
	settings/CrosshairView.c \
	settings/KeysViewController.c \
	settings/MouseViewController.c \
	settings/OptionsViewController.c \
	settings/PlayerModelView.c \
	settings/PlayerViewController.c \
	settings/SettingsViewController.c \
	settings/SystemViewController.c \
	settings/VideoModeSelect.c

libui_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/cgame \
	-I$(top_srcdir)/src/cgame/default \
	-I$(top_srcdir)/src/cgame/default/ui \
	-I$(top_srcdir)/src/cgame/default/ui/common \
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

