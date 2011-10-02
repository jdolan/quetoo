cgamelibdir=$(libdir)/@PACKAGE@/default

noinst_HEADERS = \
	cg_local.h

cgamelib_LTLIBRARIES = \
	cgame.la

cgame_la_SOURCES = \
	cg_hud.c \
	cg_main.c \
	cg_view.c

cgame_la_CFLAGS = \
	@SO_CFLAGS@ \
	-I../.. \
	-pipe \
	@BASE_CFLAGS@

cgame_la_LIBADD = \
	../../libshared.la

cgame_la_LDFLAGS = \
	-module \
	-avoid-version \
	-rpath $(cgamelibdir)

