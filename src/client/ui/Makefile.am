noinst_HEADERS = \
	ui.h \
	ui_create_server.h \
	ui_credits.h \
	ui_controls.h \
	ui_data.h \
	ui_editor.h \
	ui_local.h \
	ui_main.h \
	ui_player.h \
	ui_servers.h \
	ui_system.h

noinst_LTLIBRARIES = \
	libui.la

libui_la_SOURCES = \
	ui_create_server.c \
	ui_credits.c \
	ui_controls.c \
	ui_data.c \
	ui_editor.c \
	ui_main.c \
	ui_misc.c \
	ui_player.c \
	ui_servers.c \
	ui_system.c

libui_la_CFLAGS = \
	-I.. -I../.. \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OPENGL_CFLAGS@ \
	@SDL2_CFLAGS@

libui_la_LDFLAGS = \
	-shared

libui_la_LIBADD = \
	../../../deps/AntTweakBar/src/libanttweakbar.la \
	../../libcommon.la
