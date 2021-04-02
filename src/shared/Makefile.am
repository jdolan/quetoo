noinst_HEADERS = \
	box.h \
	color.h \
	matrix.h \
	parse.h \
	shared.h \
	swap.h \
	vector.h

noinst_LTLIBRARIES = \
	libshared.la

libshared_la_SOURCES = \
	parse.c \
	shared.c \
	shared-anorms.c \
	swap.c
libshared_la_CFLAGS = \
	-I$(top_srcdir)/src \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@SDL2_CFLAGS@
libshared_la_LDFLAGS = \
	-shared
libshared_la_LIBADD = \
	@BASE_LIBS@ \
	@GLIB_LIBS@ \
	@SDL2_LIBS@
