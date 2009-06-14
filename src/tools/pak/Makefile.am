bin_PROGRAMS = \
	pak

pak_SOURCES = \
	main.c

pak_CFLAGS = \
	-pipe \
	-I../.. \
	@WARN_CFLAGS@ \
	@DEBUG_CFLAGS@

pak_LDADD = \
	../../libfiles.la \
	../../libhash.la \
	../../libpak.la \
	@DEBUG_LIBS@
