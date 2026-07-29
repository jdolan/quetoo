cguiteamsdir = @PKGLIBDIR@/ctf/ui/teams
cguiteams_DATA = \
	TeamPlayerView.json \
	TeamsViewController.css \
	TeamsViewController.json \
	TeamView.json

noinst_HEADERS = \
	TeamPlayerView.h \
	TeamsViewController.h \
	TeamView.h

noinst_LTLIBRARIES = \
	libcguiteams.la

libcguiteams_la_SOURCES = \
	TeamPlayerView.c \
	TeamsViewController.c \
	TeamView.c

libcguiteams_la_CFLAGS = \
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

libcguiteams_la_LDFLAGS = \
	-shared

libcguiteams_la_LIBADD = \
	@OBJECTIVELYMVC_LIBS@
