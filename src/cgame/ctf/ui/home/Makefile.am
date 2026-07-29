cguihomedir = @PKGLIBDIR@/ctf/ui/home
cguihome_DATA = \
	HomeViewController.css \
	HomeViewController.json \
	LeaderboardViewController.css \
	LeaderboardViewController.json \
	StatsViewController.css \
	StatsViewController.json

noinst_HEADERS = \
	HomeViewController.h \
	LeaderboardViewController.h \
	StatsViewController.h

noinst_LTLIBRARIES = \
	libcguihome.la

libcguihome_la_SOURCES = \
	HomeViewController.c \
	LeaderboardViewController.c \
	StatsViewController.c

libcguihome_la_CFLAGS = \
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

libcguihome_la_LDFLAGS = \
	-shared

libcguihome_la_LIBADD = \
	@OBJECTIVELYMVC_LIBS@
