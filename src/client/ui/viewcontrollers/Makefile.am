
noinst_HEADERS = \
	CreateServerViewController.h \
	KeysViewController.h \
	MainViewController.h \
	MenuViewController.h \
	MouseViewController.h \
	MultiplayerViewController.h \
	PlayerViewController.h \
	SystemViewController.h

noinst_LTLIBRARIES = \
	libviewcontrollers.la

libviewcontrollers_la_SOURCES = \
	CreateServerViewController.c \
	KeysViewController.c \
	MainViewController.c \
	MenuViewController.c \
	MouseViewController.c \
	MultiplayerViewController.c \
	PlayerViewController.c \
	SystemViewController.c

libviewcontrollers_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/client \
	-I$(top_srcdir)/src/client/ui \
	-I$(top_srcdir)/src/client/ui/views \
	@BASE_CFLAGS@ \
	-Wno-unknown-pragmas \
	-Wno-unused-parameter \
	@GLIB_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@

libviewcontrollers_la_LDFLAGS = \
	-shared

libviewcontrollers_la_LIBADD = \
	../views/libviews.la \
	../../../libcommon.la
