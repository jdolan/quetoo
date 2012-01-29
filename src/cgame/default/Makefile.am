cgamelibdir=$(libdir)/@PACKAGE@/default

noinst_HEADERS = \
	cg_hud.h \
	cg_local.h \
	cg_main.h \
	cg_scores.h \
	cg_types.h \
	cg_view.h

cgamelib_LTLIBRARIES = \
	cgame.la

cgame_la_SOURCES = \
	cg_hud.c \
	cg_main.c \
	cg_scores.c \
	cg_view.c

cgame_la_CFLAGS = \
	@SO_CFLAGS@ \
	-I../.. \
	-pipe \
	@BASE_CFLAGS@ \
	@OPENGL_CFLAGS@ \
	@SDL_CFLAGS@

cgame_la_LIBADD = \
	../../libshared.la \
	@OPENGL_LIBS@ \
	@SDL_LIBS@

cgame_la_LDFLAGS = \
	-module \
	-avoid-version \
	-rpath $(cgamelibdir)

