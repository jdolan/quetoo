noinst_HEADERS = \
	box.h \
	color.h \
	matrix.h \
	md5.h \
	parse.h \
	shared.h \
	swap.h \
	vector.h

noinst_LTLIBRARIES = \
	libshared.la

libshared_la_SOURCES = \
	md5.c \
	parse.c \
	shared.c \
	shared-anorms.c \
	swap.c
libshared_la_CFLAGS = \
	-I$(top_srcdir)/src \
	@BASE_CFLAGS@ \
	@OBJECTIVELY_CFLAGS@ \
	@SDL3_CFLAGS@
libshared_la_LDFLAGS = \
	-shared
libshared_la_LIBADD = \
	@BASE_LIBS@ \
	@OBJECTIVELY_LIBS@ \
	@SDL3_LIBS@
