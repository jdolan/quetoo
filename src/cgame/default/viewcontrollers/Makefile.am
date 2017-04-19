
noinst_HEADERS = \
	AudioViewController.h \
	CreateServerViewController.h \
	CreditsViewController.h \
	HomeViewController.h \
	InfoViewController.h \
	InputViewController.h \
	InterfaceViewController.h \
	LoadingViewController.h \
	MainViewController.h \
	MenuViewController.h \
	PlayViewController.h \
	PlayerViewController.h \
	QuickJoinViewController.h \
	ServerBrowserViewController.h \
	SettingsViewController.h \
	TabViewController.h \
	VideoViewController.h

noinst_LTLIBRARIES = \
	libviewcontrollers.la

libviewcontrollers_la_SOURCES = \
	AudioViewController.c \
	CreateServerViewController.c \
	CreditsViewController.c \
	HomeViewController.c \
	InfoViewController.c \
	InputViewController.c \
	InterfaceViewController.c \
	LoadingViewController.c \
	MainViewController.c \
	MenuViewController.c \
	PlayViewController.c \
	PlayerViewController.c \
	QuickJoinViewController.c \
	ServerBrowserViewController.c \
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
