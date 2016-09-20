noinst_HEADERS = \
	ai.h \
	ai_goal.h \
	ai_local.h \
	ai_main.h \
	ai_types.h

noinst_LTLIBRARIES = \
	libai.la

libai_la_CFLAGS = \
	-I$(top_srcdir)/src \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@
	
libai_la_SOURCES = \
	ai_goal.c \
	ai_main.c

libai_la_LDFLAGS = \
	-shared
	
libai_la_LIBADD = \
	../collision/libcmodel.la
