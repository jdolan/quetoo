SUBDIRS = \
	. \
	node

noinst_HEADERS = \
	m_actions.h \
	m_data.h \
	mem.h \
	m_icon.h \
	m_internal.h \
	m_menus.h \
	m_parse.h \
	m_render.h \
	scripts.h \
	m_condition.h \
	m_draw.h \
	m_font.h \
	m_input.h \
	m_main.h \
	m_nodes.h \
	m_popup.h \
	m_timer.h \
	utf8.h

noinst_LTLIBRARIES = \
	libmenu.la

if BUILD_CLIENT
libmenu_la_SOURCES = \
	m_actions.c \
	m_condition.c \
	m_data.c \
	m_draw.c \
	mem.c \
	m_font.c \
	m_icon.c \
	m_input.c \
	m_main.c \
	m_menus.c \
	m_nodes.c \
	m_parse.c \
	m_popup.c \
	m_render.c \
	m_timer.c \
	scripts.c \
	utf8.c

libmenu_la_CFLAGS = \
	-I.. -I../.. \
	-pipe \
	@DEBUG_CFLAGS@ \
	@WARN_CFLAGS@ \
	@SDL_CFLAGS@
endif
