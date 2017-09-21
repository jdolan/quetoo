noinst_HEADERS = \
	s_al_ext.h \
	s_local.h \
	s_main.h \
	s_media.h \
	s_mix.h \
	s_music.h \
	s_sample.h \
	s_types.h \
	sound.h

noinst_LTLIBRARIES = \
	libsound.la

libsound_la_SOURCES = \
	s_al_ext.c \
	s_main.c \
	s_media.c \
	s_mix.c \
	s_music.c \
	s_sample.c

libsound_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/client \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OPENAL_CFLAGS@ \
	@SDL2_CFLAGS@ \
	@SNDFILE_CFLAGS@

libsound_la_LDFLAGS = \
	-shared

libsound_la_LIBADD = \
	$(top_builddir)/src/client/renderer/librenderer.la \
	@OPENAL_LIBS@ \
	@SDL2_LIBS@ \
	@SNDFILE_LIBS@
