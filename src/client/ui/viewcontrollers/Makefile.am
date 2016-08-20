
noinst_HEADERS = \
	ControlsViewController.h \
	CreateServerViewController.h \
	MainViewController.h \
	MenuViewController.h \
	MultiplayerViewController.h \
	PlayerSetupViewController.h \
	SettingsViewController.h

noinst_LTLIBRARIES = \
	libviewcontrollers.la

libviewcontrollers_la_SOURCES = \
	ControlsViewController.c \
	CreateServerViewController.c \
	MainViewController.c \
	MenuViewController.c \
	MultiplayerViewController.c \
	PlayerSetupViewController.c \
	SettingsViewController.c

libviewcontrollers_la_CFLAGS = \
	-I.. -I../.. -I../../.. -I../views \
	@BASE_CFLAGS@ \
	-Wno-unused-parameter \
	@GLIB_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@

libviewcontrollers_la_LDFLAGS = \
	-shared

libviewcontrollers_la_LIBADD = \
	../views/libviews.la \
	../../../libcommon.la
