noinst_HEADERS = \
	m_node_abstractnode.h \
	m_node_abstractscrollbar.h \
	m_node_abstractvalue.h \
	m_node_button.h \
	m_node_checkbox.h \
	m_node_controls.h \
	m_node_custombutton.h \
	m_node_editor.h \
	m_node_image.h \
	m_node_panel.h \
	m_node_radiobutton.h \
	m_node_rows.h \
	m_node_special.h \
	m_node_string.h \
	m_node_textentry.h \
	m_node_text.h \
	m_node_vscrollbar.h \
	m_node_window.h \
	m_node_zone.h

noinst_LTLIBRARIES = \
	libmenunode.la

if BUILD_CLIENT
libmenunode_la_SOURCES = \
	m_node_abstractnode.c \
	m_node_abstractscrollbar.c \
	m_node_abstractvalue.c \
	m_node_button.c \
	m_node_checkbox.c \
	m_node_controls.c \
	m_node_custombutton.c \
	m_node_editor.c \
	m_node_image.c \
	m_node_panel.c \
	m_node_radiobutton.c \
	m_node_rows.c \
	m_node_special.c \
	m_node_string.c \
	m_node_text.c \
	m_node_textentry.c \
	m_node_vscrollbar.c \
	m_node_window.c \
	m_node_zone.c

libmenunode_la_CFLAGS = \
	-I.. -I../.. -I../../.. \
	-pipe \
	@DEBUG_CFLAGS@ \
	@WARN_CFLAGS@ \
	@SDL_CFLAGS@

libmenunode_la_LIBADD = \
	../libmenu.la
endif
