bin_PROGRAMS = \
	pak

pak_SOURCES = \
	main.c

pak_DEPENDENCIES = \
	../../libcmd.la \
	../../libcommon.la \
	../../libcvar.la \
	../../libfilesystem.la \
	../../libhash.la \
	../../libmem.la \
	../../libpak.la \
	../../libshared.la \
	../../libsys.la \
	../../libwin32.la

pak_CFLAGS = \
	-I../.. \
	-pipe \
	@DEBUG_CFLAGS@ \
	@SDL_CFLAGS@ \
	@WARN_CFLAGS@

pak_LDADD = \
	${pak_DEPENDENCIES} \
	@DEBUG_LIBS@ \
	@HOST_LIBS@ \
	@SDL_LIBS@ \
	-lm