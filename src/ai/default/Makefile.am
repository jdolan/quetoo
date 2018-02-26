ailibdir = @PKGLIBDIR@/default

noinst_HEADERS = \
	ai_goal.h \
	ai_item.h \
	ai_local.h \
	ai_main.h \
	ai_types.h

ailib_LTLIBRARIES = \
	ai.la

ai_la_SOURCES = \
	ai_goal.c \
	ai_item.c \
	ai_main.c

ai_la_CFLAGS = \
	-I$(top_srcdir)/src \
	@SO_CFLAGS@ \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@

ai_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-shared

ai_la_LIBADD = \
	$(top_builddir)/src/game/default/libbgame.la \
	@GLIB_LIBS@
