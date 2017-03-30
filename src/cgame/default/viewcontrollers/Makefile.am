
noinst_HEADERS = \
	AudioViewController.h \
	BindingViewController.h \
	CreateServerViewController.h \
	HomeViewController.h \
	InputViewController.h \
	MainViewController.h \
	MenuViewController.h \
	MiscViewController.h \
	MultiplayerViewController.h \
	PlayViewController.h \
	PlayerViewController.h \
	QuickJoinViewController.h \
	SettingsViewController.h \
	TabViewController.h \
	VideoViewController.h

noinst_LTLIBRARIES = \
	libviewcontrollers.la

libviewcontrollers_la_SOURCES = \
	AudioViewController.c \
	BindingViewController.c \
	CreateServerViewController.c \
	HomeViewController.c \
	InputViewController.c \
	MainViewController.c \
	MenuViewController.c \
	MiscViewController.c \
	MultiplayerViewController.c \
	PlayViewController.c \
	PlayerViewController.c \
	QuickJoinViewController.c \
	SettingsViewController.c \
	TabViewController.c \
	VideoViewController.c

libviewcontrollers_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/cgame \
	-I$(top_srcdir)/src/cgame/default \
	-I$(top_srcdir)/src/cgame/default/views \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@

libviewcontrollers_la_LDFLAGS = \
	-shared

libviewcontrollers_la_LIBADD = \
	$(top_builddir)/src/cgame/default/views/libviews.la
