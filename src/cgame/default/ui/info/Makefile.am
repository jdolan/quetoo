cguiinfodir = @PKGLIBDIR@/default/ui/info
cguiinfo_DATA = \
	CreditsViewController.css

noinst_HEADERS = \
	CreditsViewController.h \
	InfoViewController.h

noinst_LTLIBRARIES = \
	libcguiinfo.la

libcguiinfo_la_SOURCES = \
	CreditsViewController.c \
	InfoViewController.c

libcguiinfo_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/cgame \
	-I$(top_srcdir)/src/cgame/default \
	-I$(top_srcdir)/src/cgame/default/ui \
	-I$(top_srcdir)/src/cgame/default/ui/common \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@ \
	@OPENGL_CFLAGS@

libcguiinfo_la_LDFLAGS = \
	-shared

libcguiinfo_la_LIBADD = \
	@OBJECTIVELYMVC_LIBS@ \
	@OPENGL_LIBS@

