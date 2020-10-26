ailibdir = @PKGLIBDIR@/default

noinst_HEADERS = \
	ai_goal.h \
	ai_info.h \
	ai_item.h \
	ai_local.h \
	ai_main.h \
	ai_types.h

noinst_LTLIBRARIES = \
	libai.la

libai_la_SOURCES = \
	ai_goal.c \
	ai_info.c \
	ai_item.c \
	ai_main.c

libai_la_CFLAGS = \
	-I$(top_srcdir)/src \
	@SO_CFLAGS@ \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@

libai_la_LDFLAGS = \
	-shared

libai_la_LIBADD = \
	$(top_builddir)/deps/genann/libgenann.la \
	$(top_builddir)/src/game/default/libpmove.la \
	@GLIB_LIBS@

ailib_LTLIBRARIES = \
	ai.la

ai_la_SOURCES = \
	$(libai_la_SOURCES)

ai_la_CFLAGS = \
	$(libai_la_CFLAGS)

ai_la_LDFLAGS = \
	$(libai_la_LDFLAGS) \
	-avoid-version \
	-module \
	-no-undefined

ai_la_LIBADD = \
	$(libai_la_LIBADD)
