cguimaindir = @PKGLIBDIR@/default/ui/main
cguimain_DATA = \
	LoadingViewController.css \
	LoadingViewController.json \
	MainView.css \
	MainView.json

noinst_HEADERS = \
	LoadingViewController.h \
	MainView.h \
	MainViewController.h

noinst_LTLIBRARIES = \
	libcguimain.la

libcguimain_la_SOURCES = \
	LoadingViewController.c \
	MainView.c \
	MainViewController.c

libcguimain_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/cgame \
	-I$(top_srcdir)/src/cgame/default \
	-I$(top_srcdir)/src/cgame/default/ui \
	-I$(top_srcdir)/src/cgame/default/ui/common \
	-I$(top_srcdir)/src/cgame/default/ui/controls \
	-I$(top_srcdir)/src/cgame/default/ui/home \
	-I$(top_srcdir)/src/cgame/default/ui/play \
	-I$(top_srcdir)/src/cgame/default/ui/settings \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@ \
	@SDL2_CFLAGS@

libcguimain_la_LDFLAGS = \
	-shared

libcguimain_la_LIBADD = \
	@OBJECTIVELYMVC_LIBS@
