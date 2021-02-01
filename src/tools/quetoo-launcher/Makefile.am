bin_PROGRAMS = \
	quetoo-launcher

quetoo_launcher_SOURCES = \
	main.c \
	windows.c

quetoo_launcher_CFLAGS = \
	-I$(top_srcdir)/src \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OBJECTIVELY_CFLAGS@

quetoo_launcher_LDADD = \
	@BASE_LIBS@ \
	@GLIB_LIBS@ \
	@OBJECTIVELY_LIBS@

if MINGW

.rc.o:
	@WINDRES@ $^ -o $@

quetoo_launcher_SOURCES += \
	quetoo-launcher-icon.rc

endif
