noinst_HEADERS = \
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
	@SDL2_CFLAGS@

libsound_la_LDFLAGS = \
	-shared

libsound_la_LIBADD = \
	../../libshared.la \
	@SDL2_LIBS@
