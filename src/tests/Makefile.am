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
	$(top_builddir)/src/common/libcommon.la

TESTS = \
	check_atlas \
	check_box \
	check_cm_polylib \
	check_cm_test \
	check_cmd \
	check_color \
	check_cvar \
	check_filesystem \
	check_master \
	check_mem \
	check_r_media \
	check_shared \
	check_thread \
	check_vector

noinst_PROGRAMS = $(TESTS)

check_atlas_SOURCES = \
	check_atlas.c
check_atlas_CFLAGS = \
	$(TESTS_CFLAGS)
check_atlas_LDADD = \
	$(TESTS_LIBS)

check_box_SOURCES = \
	check_box.c
check_box_CFLAGS = \
	$(TESTS_CFLAGS)
check_box_LDADD = \
	$(TESTS_LIBS)

check_cmd_SOURCES = \
	check_cmd.c
check_cmd_CFLAGS = \
	$(TESTS_CFLAGS)
check_cmd_LDADD = \
	$(TESTS_LIBS)

check_cm_polylib_SOURCES = \
	check_cm_polylib.c
check_cm_polylib_CFLAGS = \
	$(TESTS_CFLAGS)
check_cm_polylib_LDADD = \
	$(TESTS_LIBS) \
	$(top_builddir)/src/collision/libcollision.la

check_cm_test_SOURCES = \
	check_cm_test.c
check_cm_test_CFLAGS = \
	$(TESTS_CFLAGS)
check_cm_test_LDADD = \
	$(TESTS_LIBS) \
	$(top_builddir)/src/collision/libcollision.la

check_color_SOURCES = \
	check_color.c
check_color_CFLAGS = \
	$(TESTS_CFLAGS)
check_color_LDADD = \
	$(TESTS_LIBS)

check_cvar_SOURCES = \
	check_cvar.c
check_cvar_CFLAGS = \
	$(TESTS_CFLAGS)
check_cvar_LDADD = \
	$(TESTS_LIBS)

check_filesystem_SOURCES = \
	check_filesystem.c
check_filesystem_CFLAGS = \
	$(TESTS_CFLAGS)
check_filesystem_LDADD = \
	$(TESTS_LIBS)

check_master_SOURCES = \
	check_master.c
check_master_CFLAGS = \
	$(TESTS_CFLAGS)
check_master_LDADD = \
	$(TESTS_LIBS)

check_mem_SOURCES = \
	check_mem.c
check_mem_CFLAGS = \
	$(TESTS_CFLAGS)
check_mem_LDADD = \
	$(TESTS_LIBS)

check_r_media_SOURCES = \
	check_r_media.c
check_r_media_CFLAGS = \
	-I$(top_srcdir)/src/client/renderer \
	$(TESTS_CFLAGS) \
	@OPENGL_CFLAGS@
check_r_media_LDADD = \
	$(TESTS_LIBS) \
	$(top_builddir)/src/client/renderer/librenderer.la

check_shared_SOURCES = \
	check_shared.c
check_shared_CFLAGS = \
	-I$(top_srcdir)/src \
	$(TESTS_CFLAGS)
check_shared_LDADD = \
	$(TESTS_LIBS)

check_thread_SOURCES = \
	check_thread.c
check_thread_CFLAGS = \
	-I$(top_srcdir)/src \
	$(TESTS_CFLAGS)
check_thread_LDADD = \
	$(TESTS_LIBS)

check_vector_SOURCES = \
	check_vector.c
check_vector_CFLAGS = \
	$(TESTS_CFLAGS)
check_vector_LDADD = \
	$(TESTS_LIBS)

endif
