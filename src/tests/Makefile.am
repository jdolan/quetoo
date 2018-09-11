if BUILD_TESTS

TESTS_CFLAGS = \
	-I$(top_srcdir)/src \
	@BASE_CFLAGS@ \
	@CHECK_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@SDL2_CFLAGS@
	
TESTS_LIBS = \
	libtests.la \
	@BASE_LIBS@ \
	@CHECK_LIBS@ \
	@GLIB_LIBS@ \
	@SDL2_LIBS@

noinst_LTLIBRARIES = \
	libtests.la

noinst_HEADERS = \
	tests.h

libtests_la_SOURCES = \
	tests.c
libtests_la_CFLAGS = \
	$(TESTS_CFLAGS)
libtests_la_LDFLAGS = \
	-shared
libtests_la_LIBADD = \
	$(top_builddir)/src/libcommon.la

TESTS = \
	check_cmd \
	check_cvar \
	check_filesystem \
	check_master \
	check_mem \
	check_r_media \
	check_thread

noinst_PROGRAMS = $(TESTS)

check_cmd_SOURCES = \
	check_cmd.c
check_cmd_CFLAGS = \
	$(TESTS_CFLAGS)
check_cmd_LDADD = \
	$(TESTS_LIBS) \
	$(top_builddir)/src/libconsole.la

check_cvar_SOURCES = \
	check_cvar.c
check_cvar_CFLAGS = \
	$(TESTS_CFLAGS)
check_cvar_LDADD = \
	$(TESTS_LIBS) \
	$(top_builddir)/src/libconsole.la

check_filesystem_SOURCES = \
	check_filesystem.c
check_filesystem_CFLAGS = \
	$(TESTS_CFLAGS)
check_filesystem_LDADD = \
	$(TESTS_LIBS) \
	$(top_builddir)/src/libfilesystem.la

check_master_SOURCES = \
	check_master.c
check_master_CFLAGS = \
	$(TESTS_CFLAGS)
check_master_LDADD = \
	$(TESTS_LIBS) \
	$(top_builddir)/src/libfilesystem.la

check_mem_SOURCES = \
	check_mem.c
check_mem_CFLAGS = \
	$(TESTS_CFLAGS)
check_mem_LDADD = \
	$(TESTS_LIBS) \
	$(top_builddir)/src/libmem.la

check_r_media_SOURCES = \
	check_r_media.c \
	$(top_srcdir)/src/client/renderer/r_media.c
check_r_media_CFLAGS = \
	-I$(top_srcdir)/src/client/renderer \
	$(TESTS_CFLAGS) \
	@OPENGL_CFLAGS@
check_r_media_LDADD = \
	$(TESTS_LIBS) \
	$(top_builddir)/src/libfilesystem.la

check_thread_SOURCES = \
	check_thread.c
check_thread_CFLAGS = \
	-I$(top_srcdir)/src \
	$(TESTS_CFLAGS)
check_thread_LDADD = \
	$(TESTS_LIBS) \
	$(top_builddir)/src/libthread.la

endif
