bin_PROGRAMS = \
	quetoo

quetoo_SOURCES = \
	main.c

quetoo_CFLAGS = \
	-I.. \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@SDL2_CFLAGS@

quetoo_LDADD = \
	../client/libclient.la \
	../server/libserver.la
