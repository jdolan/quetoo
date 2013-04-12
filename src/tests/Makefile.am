if BUILD_TESTS

TESTS_CFLAGS = \
	-I.. \
	@BASE_CFLAGS@ \
	@CHECK_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@SDL_CFLAGS@
	
TESTS_LIBS = \
	libtests.la \
	@BASE_LIBS@ \
	@CHECK_LIBS@ \
	@GLIB_LIBS@ \
	@SDL_LIBS@

noinst_LTLIBRARIES = \
	libtests.la

noinst_HEADERS = \
	tests.h

libtests_la_SOURCES = \
	tests.c
libtests_la_CFLAGS = \
	$(TESTS_CFLAGS)

TESTS = check_mem check_r_media		
noinst_PROGRAMS = $(TESTS)

check_mem_SOURCES = \
	check_mem.c
check_mem_CFLAGS = \
	$(TESTS_CFLAGS)
check_mem_LDADD = \
	$(TESTS_LIBS) \
	../libmem.la

check_r_media_SOURCES = \
	check_r_media.c \
	../client/renderer/r_media.c

check_r_media_CFLAGS = \
	-I../client/renderer \
	$(TESTS_CFLAGS) \
	@OPENGL_CFLAGS@
check_r_media_LDADD = \
	$(TESTS_LIBS) \
	../libfilesystem.la

endif