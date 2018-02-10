cguicontrolsdir = @PKGLIBDIR@/default/ui/controls
cguicontrols_DATA = \
	LookViewController.css \
	MoveViewController.css \
	MiscViewController.css \
	ShootViewController.css

noinst_HEADERS = \
	ControlsViewController.h \
	CrosshairView.h \
	MiscViewController.h \
	MoveViewController.h \
	LookViewController.h \
	ShootViewController.h

noinst_LTLIBRARIES = \
	libcguicontrols.la

libcguicontrols_la_SOURCES = \
	ControlsViewController.c \
	CrosshairView.c \
	MiscViewController.c \
	MoveViewController.c \
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
