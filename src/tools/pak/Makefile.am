bin_PROGRAMS = \
	pak

pak_SOURCES = \
	main.c

pak_CFLAGS = \
	-pipe \
	-I../.. \
	@DEBUG_CFLAGS@ \
	@SDL_CFLAGS@ \
	@WARN_CFLAGS@

pak_LDADD = \
	../../libcmd.la \
	../../libcommon.la \
	../../libcvar.la \
	../../libfilesystem.la \
	../../libhash.la \
	../../libmem.la \
	../../libpak.la \
	../../libshared.la \
	../../libsys.la \
	../../libwin32.la \
	@SDL_LIBS@ \
	@DEBUG_LIBS@
