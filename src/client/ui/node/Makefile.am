noinst_HEADERS = \
	ui_node_abstractnode.h \
	ui_node_abstractscrollbar.h \
	ui_node_abstractvalue.h \
	ui_node_button.h \
	ui_node_checkbox.h \
	ui_node_controls.h \
	ui_node_custombutton.h \
	ui_node_editor.h \
	ui_node_image.h \
	ui_node_panel.h \
	ui_node_radiobutton.h \
	ui_node_rows.h \
	ui_node_special.h \
	ui_node_string.h \
	ui_node_textentry.h \
	ui_node_text.h \
	ui_node_vscrollbar.h \
	ui_node_window.h \
	ui_node_zone.h

noinst_LTLIBRARIES = \
	libuinode.la

if BUILD_CLIENT
libuinode_la_SOURCES = \
	ui_node_abstractnode.c \
	ui_node_abstractscrollbar.c \
	ui_node_abstractvalue.c \
	ui_node_button.c \
	ui_node_checkbox.c \
	ui_node_controls.c \
	ui_node_custombutton.c \
	ui_node_editor.c \
	ui_node_image.c \
	ui_node_panel.c \
	ui_node_radiobutton.c \
	ui_node_rows.c \
	ui_node_special.c \
	ui_node_string.c \
	ui_node_text.c \
	ui_node_textentry.c \
	ui_node_vscrollbar.c \
	ui_node_window.c \
	ui_node_zone.c

libuinode_la_CFLAGS = \
	-I.. -I../.. -I../../.. \
	-pipe \
	@DEBUG_CFLAGS@ \
	@OPENGL_CFLAGS@ \
	@SDL_CFLAGS@ \
	@WARN_CFLAGS@

libuinode_la_LIBADD = \
	../libui.la
endif
