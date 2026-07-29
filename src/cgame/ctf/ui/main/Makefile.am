cguimaindir = @PKGLIBDIR@/ctf/ui/main
cguimain_DATA = \
	LoadingViewController.css \
	LoadingViewController.json \
	MainView.css \
	MainView.json \
	UpdateViewController.css \
	UpdateViewController.json

noinst_HEADERS = \
	LoadingViewController.h \
	MainView.h \
	MainViewController.h \
	UpdateViewController.h

noinst_LTLIBRARIES = \
	libcguimain.la

libcguimain_la_SOURCES = \
	LoadingViewController.c \
	MainView.c \
	MainViewController.c \
	UpdateViewController.c

libcguimain_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/cgame \
	-I$(top_srcdir)/src/cgame/ctf \
	-I$(top_srcdir)/src/cgame/ctf/ui \
	-I$(top_srcdir)/src/cgame/ctf/ui/common \
	-I$(top_srcdir)/src/cgame/ctf/ui/controls \
	-I$(top_srcdir)/src/cgame/ctf/ui/credits \
	-I$(top_srcdir)/src/cgame/ctf/ui/home \
	-I$(top_srcdir)/src/cgame/ctf/ui/play \
	-I$(top_srcdir)/src/cgame/ctf/ui/settings \
	-I$(top_srcdir)/src/cgame/ctf/ui/teams \
	@BASE_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@ \
	@OPENAL_CFLAGS@ \
	@SDL3_CFLAGS@ \
	@SNDFILE_CFLAGS@

libcguimain_la_LDFLAGS = \
	-shared

libcguimain_la_LIBADD = \
	@OBJECTIVELYMVC_LIBS@
