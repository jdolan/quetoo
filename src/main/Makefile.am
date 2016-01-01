bin_PROGRAMS = \
	quetoo \
	quetoo-dedicated

quetoo_SOURCES = \
	main.c

#if _WIN32

%.rc.o: %.rc
	@WINDRES@ $^ -o $@
        
quetoo_SOURCES += \
	quetoo.rc

#endif

quetoo_CFLAGS = \
	-I.. \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@SDL2_CFLAGS@

quetoo_LDADD = \
	../client/libclient.la \
	../server/libserver.la
	
quetoo_dedicated_SOURCES = \
	main.c

quetoo_dedicated_CFLAGS = \
	-I.. \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@SDL2_CFLAGS@

quetoo_dedicated_LDADD = \
	../client/libclient_null.la \
	../server/libserver.la
