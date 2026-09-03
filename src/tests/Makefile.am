if BUILD_TESTS

TESTS_CFLAGS = \
	-I$(top_srcdir)/src \
	@BASE_CFLAGS@ \
	@CHECK_CFLAGS@ \
	@OBJECTIVELY_CFLAGS@ \
	@SDL3_CFLAGS@
	
TESTS_LIBS = \
	libtests.la \
	@BASE_LIBS@ \
	@CHECK_LIBS@ \
	@OBJECTIVELY_LIBS@ \
	@SDL3_LIBS@

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
	check_cm_entity \
	check_cm_manifest \
	check_cm_polylib \
	check_cm_test \
	check_cmd \
	check_color \
	check_cvar \
	check_editor_map \
	check_filesystem \
	check_http \
	check_master \
	check_mem \
	check_net_message \
	check_pmove \
	check_r_media \
	check_race \
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

check_cm_entity_SOURCES = \
	check_cm_entity.c
check_cm_entity_CFLAGS = \
	$(TESTS_CFLAGS)
check_cm_entity_LDADD = \
	$(TESTS_LIBS) \
	$(top_builddir)/src/collision/libcollision.la

check_cm_manifest_SOURCES = \
	check_cm_manifest.c
check_cm_manifest_CFLAGS = \
	$(TESTS_CFLAGS)
check_cm_manifest_LDADD = \
	$(TESTS_LIBS) \
	$(top_builddir)/src/collision/libcollision.la

check_editor_map_SOURCES = \
	check_editor_map.c
check_editor_map_CFLAGS = \
	$(TESTS_CFLAGS)
check_editor_map_LDADD = \
	$(TESTS_LIBS) \
	$(top_builddir)/src/collision/libcollision.la

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

check_http_SOURCES = \
	check_http.c
check_http_CFLAGS = \
	$(TESTS_CFLAGS) \
	@OBJECTIVELY_CFLAGS@
check_http_LDADD = \
	$(TESTS_LIBS) \
	$(top_builddir)/src/net/libnet.la

check_master_SOURCES = \
	check_master.c
check_master_CFLAGS = \
	$(TESTS_CFLAGS)
check_master_LDADD = \
	$(TESTS_LIBS) \
	$(top_builddir)/src/net/libnet.la

check_mem_SOURCES = \
	check_mem.c
check_mem_CFLAGS = \
	$(TESTS_CFLAGS)
check_mem_LDADD = \
	$(TESTS_LIBS)

check_net_message_SOURCES = \
	check_net_message.c
check_net_message_CFLAGS = \
	-I$(top_srcdir)/src \
	$(TESTS_CFLAGS)
check_net_message_LDADD = \
	$(TESTS_LIBS) \
	$(top_builddir)/src/net/libnet.la

check_pmove_SOURCES = \
	check_pmove.c \
	$(top_srcdir)/src/game/common/bg_pmove.c \
	$(top_srcdir)/src/game/common/bg_pmove_quake.c \
	$(top_srcdir)/src/game/common/bg_pmove_quake2.c \
	$(top_srcdir)/src/game/common/bg_pmove_quake3.c \
	$(top_srcdir)/src/game/common/bg_pmove_quetoo.c \
	$(top_srcdir)/src/game/common/bg_pmove_race.c
check_pmove_CFLAGS = \
	-I$(top_srcdir)/src \
	-fms-extensions \
	-Wno-microsoft-anon-tag \
	$(TESTS_CFLAGS)
check_pmove_LDADD = \
	$(TESTS_LIBS) \
	$(top_builddir)/src/collision/libcollision.la

check_r_media_SOURCES = \
	check_r_media.c
check_r_media_CFLAGS = \
	-I$(top_srcdir)/src/client/renderer \
	$(TESTS_CFLAGS)
check_r_media_LDADD = \
	$(TESTS_LIBS) \
	$(top_builddir)/src/client/renderer/librenderer.la \
	$(top_builddir)/src/collision/libcollision.la

check_race_SOURCES = \
	check_race.c \
	$(top_srcdir)/src/game/common/bg_pmove.c \
	$(top_srcdir)/src/game/common/bg_pmove_quake.c \
	$(top_srcdir)/src/game/common/bg_pmove_quake2.c \
	$(top_srcdir)/src/game/common/bg_pmove_quake3.c \
	$(top_srcdir)/src/game/common/bg_pmove_quetoo.c \
	$(top_srcdir)/src/game/common/bg_pmove_race.c \
	$(top_srcdir)/src/game/race/g_race_records.c \
	$(top_srcdir)/src/game/race/g_race_replay.c
check_race_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/game/common \
	-I$(top_srcdir)/src/game/race \
	-ffp-contract=off \
	-fms-extensions \
	-Wno-microsoft-anon-tag \
	$(TESTS_CFLAGS)
check_race_LDADD = \
	$(TESTS_LIBS) \
	$(top_builddir)/src/collision/libcollision.la

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
