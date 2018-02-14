cguisettingsdir = @PKGLIBDIR@/default/ui/settings
cguisettings_DATA = \
	OptionsViewController.css \
	OptionsViewController.json \
	SettingsViewController.json \
	SystemViewController.css \
	SystemViewController.json

noinst_HEADERS = \
	OptionsViewController.h \
	SettingsViewController.h \
	SystemViewController.h \
	VideoModeSelect.h

noinst_LTLIBRARIES = \
	libcguisettings.la

libcguisettings_la_SOURCES = \
	OptionsViewController.c \
	SettingsViewController.c \
	SystemViewController.c \
	VideoModeSelect.c

libcguisettings_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/cgame \
	-I$(top_srcdir)/src/cgame/default \
	-I$(top_srcdir)/src/cgame/default/ui \
	-I$(top_srcdir)/src/cgame/default/ui/common \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@ \
	@OPENGL_CFLAGS@

libcguisettings_la_LDFLAGS = \
	-shared

libcguisettings_la_LIBADD = \
	@OBJECTIVELYMVC_LIBS@ \
	@OPENGL_LIBS@
