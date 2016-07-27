noinst_HEADERS = \
	ui.h \
	ui_context.h \
	ui_local.h \
	ui_main.h \
	\
	viewcontrollers/ControlsViewController.h \
	viewcontrollers/MainViewController.h \
	viewcontrollers/MenuViewController.h \
	viewcontrollers/MultiplayerViewController.h \
	viewcontrollers/PlayerSetupViewController.h \
	viewcontrollers/SettingsViewController.h \
	\
	views/BindTextView.h \
	views/CvarCheckbox.h \
	views/CvarSlider.h \
	views/MenuInput.h \
	views/PrimaryButton.h

noinst_LTLIBRARIES = \
	libui.la

libui_la_SOURCES = \
	ui_context.c \
	ui_main.c \
	\
	viewcontrollers/ControlsViewController.c \
	viewcontrollers/MainViewController.c \
	viewcontrollers/MenuViewController.c \
	viewcontrollers/MultiplayerViewController.c \
	viewcontrollers/PlayerSetupViewController.c \
	viewcontrollers/SettingsViewController.c \
	\
	views/BindTextView.c \
	views/CvarCheckbox.c \
	views/CvarSlider.c \
	views/MenuInput.c \
	views/PrimaryButton.c

libui_la_CFLAGS = \
	-I.. -I../.. \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@

libui_la_LDFLAGS = \
	-shared

libui_la_LIBADD = \
	../../libcommon.la \
	@OBJECTIVELYMVC_LIBS@
