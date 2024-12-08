bin_PROGRAMS = \
	norm2height

norm2height_SOURCES = \
	main.c

norm2height_CFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	@BASE_CFLAGS@

norm2height_LDADD = \
	$(top_builddir)/src/common/libcommon.la \
	@FFTW_LIBS@

if MINGW

norm2height_LDFLAGS = \
	-mconsole

endif
