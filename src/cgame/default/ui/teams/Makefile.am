cguiteamsdir = @PKGLIBDIR@/default/ui/teams
cguiteams_DATA = \
	TeamsViewController.css \
	TeamsViewController.json \
	TeamView.css \
	TeamView.json

noinst_HEADERS = \
	TeamsViewController.h \
	TeamView.h

noinst_LTLIBRARIES = \
	libcguiteams.la

libcguiteams_la_SOURCES = \
	TeamsViewController.c \
	TeamView.c

libcguiteams_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/cgame \
	-I$(top_srcdir)/src/cgame/default \
	-I$(top_srcdir)/src/cgame/default/ui \
	-I$(top_srcdir)/src/cgame/default/ui/common \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@ \
	@SDL2_CFLAGS@

libcguiteams_la_LDFLAGS = \
	-shared

libcguiteams_la_LIBADD = \
	@OBJECTIVELYMVC_LIBS@
