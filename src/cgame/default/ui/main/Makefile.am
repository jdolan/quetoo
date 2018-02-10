cguimaindir = @PKGLIBDIR@/default/ui/main
cguimain_DATA = \
	DialogViewController.css \
	HomeViewController.css \
	LoadingViewController.css \
	MainView.css

noinst_HEADERS = \
	DialogView.h \
	DialogViewController.h \
	HomeViewController.h \
	LoadingViewController.h \
	MainView.h \
	MainViewController.h

noinst_LTLIBRARIES = \
	libcguimain.la

libcguimain_la_SOURCES = \
	DialogView.c \
	DialogViewController.c \
	HomeViewController.c \
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
	-I$(top_srcdir)/src/cgame/default/ui/info \
	-I$(top_srcdir)/src/cgame/default/ui/play \
	-I$(top_srcdir)/src/cgame/default/ui/settings \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@ \
	@OPENGL_CFLAGS@

libcguimain_la_LDFLAGS = \
	-shared

libcguimain_la_LIBADD = \
	@OBJECTIVELYMVC_LIBS@ \
	@OPENGL_LIBS@
