bin_PROGRAMS = \
	quetoo-update

quetoo_update_SOURCES = \
	main.c

quetoo_update_CFLAGS = \
	-I$(top_srcdir)/src \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@

if WINDOWS

.rc.o:
	@WINDRES@ $^ -o $@

quetoo_update_SOURCES += \
	update-icon.rc

endif
