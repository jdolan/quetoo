noinst_HEADERS = \
	cm_local.h \
	cm_model.h \
	cm_test.h \
	cm_types.h \
	cm_vis.h \
	cmodel.h

noinst_LTLIBRARIES = \
	libcmodel.la

libcmodel_la_CFLAGS = \
	-I$(top_srcdir)/src \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@

libcmodel_la_SOURCES = \
	cm_model.c \
	cm_test.c \
	cm_trace.c \
	cm_vis.c

libcmodel_la_LDFLAGS = \
	-shared

libcmodel_la_LIBADD = \
	../libfilesystem.la \
	../libmatrix.la
