cguicontrolsdir = @PKGLIBDIR@/default/ui/controls
cguicontrols_DATA = \
	ControlsViewController.json \
	LookViewController.css \
	LookViewController.json \
	ShootViewController.css \
	ShootViewController.json

noinst_HEADERS = \
	ControlsViewController.h \
	CrosshairView.h \
	LookViewController.h \
	ShootViewController.h

noinst_LTLIBRARIES = \
	libcguicontrols.la

libcguicontrols_la_SOURCES = \
	ControlsViewController.c \
	CrosshairView.c \
	LookViewController.c \
	ShootViewController.c

libcguicontrols_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/cgame \
	-I$(top_srcdir)/src/cgame/default \
	-I$(top_srcdir)/src/cgame/default/ui \
	-I$(top_srcdir)/src/cgame/default/ui/common \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@ \
	@OPENGL_CFLAGS@

libcguicontrols_la_LDFLAGS = \
	-shared

libcguicontrols_la_LIBADD = \
	@OBJECTIVELYMVC_LIBS@ \
	@OPENGL_LIBS@
