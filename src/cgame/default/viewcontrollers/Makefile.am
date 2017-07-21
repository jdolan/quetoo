
noinst_HEADERS = \
	CreateServerViewController.h \
	CreditsViewController.h \
	HomeViewController.h \
	InfoViewController.h \
	JoinServerViewController.h \
	LoadingViewController.h \
	MainViewController.h \
	MenuViewController.h \
	PlayViewController.h \
	PlayerViewController.h \
	SettingsViewController.h

noinst_LTLIBRARIES = \
	libviewcontrollers.la

libviewcontrollers_la_SOURCES = \
	CreateServerViewController.c \
	CreditsViewController.c \
	HomeViewController.c \
	InfoViewController.c \
	JoinServerViewController.c \
	LoadingViewController.c \
	MainViewController.c \
	MenuViewController.c \
	PlayViewController.c \
	PlayerViewController.c \
	SettingsViewController.c

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
