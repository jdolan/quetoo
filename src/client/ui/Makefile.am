noinst_HEADERS = \
	ui.h \
	ui_controls.h \
	ui_data.h \
	ui_local.h \
	ui_main.h \
	ui_player.h \
	ui_servers.h \
	ui_system.h

noinst_LTLIBRARIES = \
	libui.la

if BUILD_CLIENT

libui_la_SOURCES = \
	ui_controls.c \
	ui_data.c \
	ui_main.c \
	ui_misc.c \
	ui_player.c \
	ui_servers.c \
	ui_system.c

libui_la_CFLAGS = \
	-I.. -I../.. \
	-pipe \
	@BASE_CFLAGS@ \
	@OPENGL_CFLAGS@ \
	@SDL_CFLAGS@

else

libui_la_SOURCES = \
	../../null.c
	
libui_la_CFLAGS = \
	-D__LIBTOOL_LIBRARY__=libui_la

endif