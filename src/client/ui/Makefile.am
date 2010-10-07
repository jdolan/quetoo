SUBDIRS = \
	. \
	node

noinst_HEADERS = \
	ui_actions.h \
	ui_data.h \
	mem.h \
	ui_icon.h \
	ui_internal.h \
	ui_parse.h \
	ui_render.h \
	scripts.h \
	ui_condition.h \
	ui_draw.h \
	ui_font.h \
	ui_input.h \
	ui_main.h \
	ui_nodes.h \
	ui_popup.h \
	ui_timer.h \
	ui_windows.h \
	utf8.h

noinst_LTLIBRARIES = \
	libui.la

if BUILD_CLIENT
libui_la_SOURCES = \
	ui_actions.c \
	ui_condition.c \
	ui_data.c \
	ui_draw.c \
	mem.c \
	ui_font.c \
	ui_icon.c \
	ui_input.c \
	ui_main.c \
	ui_nodes.c \
	ui_parse.c \
	ui_popup.c \
	ui_render.c \
	ui_timer.c \
	ui_windows.c \
	scripts.c \
	utf8.c

libui_la_CFLAGS = \
	-I.. -I../.. \
	-pipe \
	@DEBUG_CFLAGS@ \
	@OPENGL_CFLAGS@ \
	@SDL_CFLAGS@ \
	@WARN_CFLAGS@
endif
