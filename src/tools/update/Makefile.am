bin_PROGRAMS = \
	update

update_SOURCES = \
	main.c

update_CFLAGS = \
	-I../.. \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@

update_LDADD = \
	../../libfilesystem.la \
	../../libsys.la

if WINDOWS

.rc.o:
	@WINDRES@ $^ -o $@

update_SOURCES += \
	update-icon.rc

endif
