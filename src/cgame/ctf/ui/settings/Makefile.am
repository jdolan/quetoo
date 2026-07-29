cguisettingsdir = @PKGLIBDIR@/ctf/ui/settings
cguisettings_DATA = \
	SettingsViewController.json

noinst_HEADERS = \
	SettingsViewController.h

noinst_LTLIBRARIES = \
	libcguisettings.la

libcguisettings_la_SOURCES = \
	SettingsViewController.c

libcguisettings_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/cgame \
	-I$(top_srcdir)/src/cgame/ctf \
	-I$(top_srcdir)/src/cgame/ctf/ui \
	-I$(top_srcdir)/src/cgame/ctf/ui/common \
	@BASE_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@ \
	@OPENAL_CFLAGS@ \
	@SDL3_CFLAGS@ \
	@SNDFILE_CFLAGS@

libcguisettings_la_LDFLAGS = \
	-shared

libcguisettings_la_LIBADD = \
	@OBJECTIVELYMVC_LIBS@
