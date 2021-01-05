cguiplaydir = @PKGLIBDIR@/default/ui/play
cguiplay_DATA = \
	CreateServerViewController.css \
	CreateServerViewController.json \
	JoinServerViewController.css \
	JoinServerViewController.json \
	PlayerSetupViewController.css \
	PlayerSetupViewController.json \
	PlayViewController.json

noinst_HEADERS = \
	CreateServerViewController.h \
	JoinServerViewController.h \
	MapListCollectionItemView.h \
	MapListCollectionView.h \
	PlayerModelView.h \
	PlayerSetupViewController.h \
	PlayViewController.h

noinst_LTLIBRARIES = \
	libcguiplay.la

libcguiplay_la_SOURCES = \
	CreateServerViewController.c \
	JoinServerViewController.c \
	MapListCollectionItemView.c \
	MapListCollectionView.c \
	PlayerModelView.c \
	PlayerSetupViewController.c \
	PlayViewController.c

libcguiplay_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/cgame \
	-I$(top_srcdir)/src/cgame/default \
	-I$(top_srcdir)/src/cgame/default/ui \
	-I$(top_srcdir)/src/cgame/default/ui/common \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@ \
	@SDL2_CFLAGS@

libcguiplay_la_LDFLAGS = \
	-shared

libcguiplay_la_LIBADD = \
	@OBJECTIVELYMVC_LIBS@
