cguicontrolsdir = @PKGLIBDIR@/default/ui/controls
cguicontrols_DATA = \
	ControlsViewController.json \
	MovementCombatViewController.css \
	MovementCombatViewController.json \
	ResponseServiceViewController.css \
	ResponseServiceViewController.json

noinst_HEADERS = \
	ControlsViewController.h \
	CrosshairView.h \
	MovementCombatViewController.h \
	ResponseServiceViewController.h

noinst_LTLIBRARIES = \
	libcguicontrols.la

libcguicontrols_la_SOURCES = \
	ControlsViewController.c \
	CrosshairView.c \
	MovementCombatViewController.c \
	ResponseServiceViewController.c

libcguicontrols_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/cgame \
	-I$(top_srcdir)/src/cgame/default \
	-I$(top_srcdir)/src/cgame/default/ui \
	-I$(top_srcdir)/src/cgame/default/ui/common \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@ \
	@OPENAL_CFLAGS@ \
	@SDL2_CFLAGS@

libcguicontrols_la_LDFLAGS = \
	-shared

libcguicontrols_la_LIBADD = \
	@OBJECTIVELYMVC_LIBS@
