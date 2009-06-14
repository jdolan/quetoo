noinst_HEADERS =\
	sound.h

noinst_LTLIBRARIES =\
	libsound.la

if BUILD_CLIENT
libsound_la_SOURCES =\
	s_main.c \
	s_mix.c \
	s_music.c \
	s_sample.c

libsound_la_CFLAGS = \
	-I.. -I../.. \
	-pipe \
	@WARN_CFLAGS@ \
	@DEBUG_CFLAGS@ \
	@SDL_CFLAGS@
endif
