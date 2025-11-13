
bin_PROGRAMS = \
	quetoo-master

quetoo_master_SOURCES = \
	main.c

quetoo_master_CFLAGS = \
	-I$(top_srcdir)/src \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@SDL3_CFLAGS@

quetoo_master_LDADD = \
	$(top_builddir)/src/common/libcommon.la
