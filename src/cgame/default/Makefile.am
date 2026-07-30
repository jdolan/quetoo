CGUI_DIR = $(top_srcdir)/src/cgame/common/ui
CGCOMMON_DIR = $(top_srcdir)/src/cgame/common
include $(top_srcdir)/src/cgame/common/ui/sources.mk
include $(top_srcdir)/src/cgame/common/sources.mk
GCOMMON_DIR = $(top_srcdir)/src/game/common
include $(top_srcdir)/src/game/common/sources.mk

CGAME_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/cgame \
	-I$(top_srcdir)/src/game/default \
	-I$(top_srcdir)/src/game/common \
	-I$(srcdir) \
	$(CGCOMMON_CFLAGS) \
	$(CGUI_CFLAGS) \
	@SO_CFLAGS@ \
	@BASE_CFLAGS@ \
	@OBJECTIVELY_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@ \
	@OPENAL_CFLAGS@ \
	@SDL3_CFLAGS@ \
	@SNDFILE_CFLAGS@

noinst_HEADERS = \
	$(CGCOMMON_HDR) \
	$(CGUI_HDR)

# The common UI and client game sources are compiled here rather than linked
# from a prebuilt library, because cg_state_t and the item roster resolve from
# this module's game directory. This library's name MUST stay unique across
# modules: with subdir-objects automake derives object file names from it, so
# two modules sharing a prefix would collide in the common directories.
noinst_LTLIBRARIES = \
	libcgcommondefault.la

libcgcommondefault_la_SOURCES = \
	$(CGCOMMON_SRC) \
	$(CGUI_SRC)

libcgcommondefault_la_CFLAGS = \
	$(CGAME_CFLAGS)

cgamelibdir = \
	@PKGLIBDIR@/default

cgamelib_LTLIBRARIES = \
	cgame.la

cgame_la_SOURCES = \
	cg_team_mode.c

cgame_la_CFLAGS = \
	$(CGAME_CFLAGS)

cgame_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-shared

cgame_la_LIBADD = \
	libcgcommondefault.la \
	$(top_builddir)/src/game/default/libitem.la \
	$(top_builddir)/src/game/default/libpmovedefault.la \
	$(top_builddir)/deps/discord-rpc/src/libdiscord-rpc.la \
	@OBJECTIVELYMVC_LIBS@

if APPLE
cgame_la_LIBADD += -lc++
cgame_la_LDFLAGS += -framework AppKit
else
cgame_la_LIBADD += -lstdc++
endif
