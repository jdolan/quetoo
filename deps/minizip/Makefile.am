
noinst_LTLIBRARIES = \
	libminizip.la

noinst_HEADERS = \
	ioapi.h \
	miniz.h \
	zip.h

libminizip_la_SOURCES = \
	ioapi.c \
	miniz.c \
	zip.c

libminizip_la_CFLAGS = \
	-DNOCRYPT \
	-DUSE_FILE32API \
	@BASE_CFLAGS@

libminizip_la_LIBADD = \
	-lz
