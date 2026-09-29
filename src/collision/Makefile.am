noinst_HEADERS = \
	cm_bsp.h \
	cm_entity.h \
	cm_light.h \
	cm_local.h \
	cm_model.h \
	cm_test.h \
	cm_types.h \
	cm_voxel.h \
	collision.h

noinst_LTLIBRARIES = \
	libcollision.la

libcollision_la_CFLAGS = \
	-I$(top_srcdir)/src \
	@BASE_CFLAGS@ \
	@OBJECTIVELY_CFLAGS@ \
	@SDL3_CFLAGS@

libcollision_la_SOURCES = \
	cm_bsp.c \
	cm_entity.c \
	cm_light.c \
	cm_model.c \
	cm_test.c \
	cm_trace.c \
	cm_voxel.c

libcollision_la_LDFLAGS = \
	-shared

libcollision_la_LIBADD = \
	$(top_builddir)/src/common/libcommon.la
