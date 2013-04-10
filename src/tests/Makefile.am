if BUILD_TESTS

noinst_LTLIBRARIES = \
	libtests.la

noinst_HEADERS = \
	tests.h

libtests_la_SOURCES = \
	tests.c
libtests_la_CFLAGS = \
	-I.. \
	@BASE_CFLAGS@ \
	@CHECK_CFLAGS@ \
	@GLIB_CFLAGS@

TESTS = check_mem
check_PROGRAMS = check_mem

check_mem_SOURCES = \
	check_mem.c

check_mem_CFLAGS = \
	-I.. \
	@BASE_CFLAGS@ \
	@CHECK_CFLAGS@ \
	@GLIB_CFLAGS@

check_mem_LDADD = \
	libtests.la \
	../libmem.la \
	../libcommon.la \
	../libshared.la \
	@CHECK_LIBS@ \
	@GLIB_LIBS@ \
	@SDL_LIBS@

endif