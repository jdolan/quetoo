
bin_PROGRAMS = \
	quetoo-update

quetoo_update_SOURCES = \
	main.c

quetoo_update_CFLAGS = \
	-I$(top_srcdir)/src \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@

quetoo_update_LDADD = \
	@BASE_LIBS@ \
	@GLIB_LIBS@

if MINGW

.rc.o:
	@WINDRES@ $^ -o $@

quetoo_update_SOURCES += \
	quetoo-update-icon.rc

endif
