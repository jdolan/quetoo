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

noinst_PROGRAMS = \
	check_cmd \
	check_cvar \
	check_filesystem \
	check_mem \
	check_r_media \
	check_threads

check_cmd_SOURCES = \
	check_cmd.c \
	../cmd.c \
	../cvar.c
check_cmd_CFLAGS = \
	$(TESTS_CFLAGS)
check_cmd_LDADD = \
	$(TESTS_LIBS) \
	../libfilesystem.la

check_cvar_SOURCES = \
	check_cvar.c \
	../cmd.c \
	../cvar.c
check_cvar_CFLAGS = \
	$(TESTS_CFLAGS)
check_cvar_LDADD = \
	$(TESTS_LIBS) \
	../libfilesystem.la

check_filesystem_SOURCES = \
	check_filesystem.c
check_filesystem_CFLAGS = \
	$(TESTS_CFLAGS)
check_filesystem_LDADD = \
	$(TESTS_LIBS) \
	../libfilesystem.la

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
	../libcommon.la \
	../libmem.la

check_threads_SOURCES = \
	check_threads.c
check_threads_CFLAGS = \
	-I../ \
	$(TESTS_CFLAGS)
check_threads_LDADD = \
	$(TESTS_LIBS) \
	../libthreads.la

endif