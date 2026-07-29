cguicreditsdir = @PKGLIBDIR@/ctf/ui/credits
cguicredits_DATA = \
	CreditsViewController.json \
	CreditsViewController.css

noinst_HEADERS = \
	CreditsViewController.h

noinst_LTLIBRARIES = \
	libcguicredits.la

libcguicredits_la_SOURCES = \
	CreditsViewController.c

libcguicredits_la_CFLAGS = \
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

libcguicredits_la_LDFLAGS = \
	-shared

libcguicredits_la_LIBADD = \
	@OBJECTIVELYMVC_LIBS@
