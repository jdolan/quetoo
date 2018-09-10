cguihomedir = @PKGLIBDIR@/default/ui/home
cguihome_DATA = \
	HomeViewController.json

noinst_HEADERS = \
	HomeViewController.h

noinst_LTLIBRARIES = \
	libcguihome.la

libcguihome_la_SOURCES = \
	HomeViewController.c

libcguihome_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/cgame \
	-I$(top_srcdir)/src/cgame/default \
	-I$(top_srcdir)/src/cgame/default/ui \
	-I$(top_srcdir)/src/cgame/default/ui/common \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@ \
	@SDL2_CFLAGS@

libcguihome_la_LDFLAGS = \
	-shared

libcguihome_la_LIBADD = \
	@OBJECTIVELYMVC_LIBS@
