CGUI_DIR = $(top_srcdir)/src/cgame/common/ui
include $(top_srcdir)/src/cgame/common/ui/sources.mk

CGAME_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/cgame \
	-I$(top_srcdir)/src/cgame/common \
	-I$(srcdir) \
	$(CGUI_CFLAGS) \
	@SO_CFLAGS@ \
	@BASE_CFLAGS@ \
	@OBJECTIVELY_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@ \
	@OPENAL_CFLAGS@ \
	@SDL3_CFLAGS@ \
	@SNDFILE_CFLAGS@

noinst_HEADERS = \
	cg_client.h \
	cg_editor.h \
	cg_effect.h \
	cg_entity.h \
	cg_entity_effect.h \
	cg_entity_event.h \
	cg_entity_misc.h \
	cg_entity_trail.h \
	cg_flare.h \
	cg_hud.h \
	cg_input.h \
	cg_inventory.h \
	cg_light.h \
	cg_local.h \
	cg_main.h \
	cg_media.h \
	cg_muzzle_flash.h \
	cg_predict.h \
	cg_score.h \
	cg_sound.h \
	cg_sprite.h \
	cg_temp_entity.h \
	cg_types.h \
	cg_ui.h \
	cg_view.h \
	cg_weapon.h \
	$(CGUI_HDR)

# The common UI is compiled here rather than linked from a prebuilt library,
# because cg_state_t's layout depends on this module's game/<module>/g_types.h.
# This library's name MUST stay unique across modules: with subdir-objects,
# automake derives object file names from it, so two modules both using the
# cgame_la- prefix would write colliding objects into src/cgame/common/ui.
noinst_LTLIBRARIES = \
	libcguictf.la

libcguictf_la_SOURCES = \
	$(CGUI_SRC)

libcguictf_la_CFLAGS = \
	$(CGAME_CFLAGS)

cgamelibdir = \
	@PKGLIBDIR@/ctf

cgamelib_LTLIBRARIES = \
	cgame.la

cgame_la_SOURCES = \
	cg_client.c \
	cg_discord.c \
	cg_editor.c \
	cg_effect.c \
	cg_entity.c \
	cg_entity_effect.c \
	cg_entity_event.c \
	cg_entity_misc.c \
	cg_entity_trail.c \
	cg_flare.c \
	cg_hud.c \
	cg_input.c \
	cg_inventory.c \
	cg_light.c \
	cg_main.c \
	cg_media.c \
	cg_muzzle_flash.c \
	cg_predict.c \
	cg_score.c \
	cg_sound.c \
	cg_sprite.c \
	cg_temp_entity.c \
	cg_ui.c \
	cg_view.c \
	cg_weapon.c

cgame_la_CFLAGS = \
	$(CGAME_CFLAGS)

cgame_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-shared

cgame_la_LIBADD = \
	libcguictf.la \
	$(top_builddir)/src/game/ctf/libitem.la \
	$(top_builddir)/src/game/ctf/libpmovectf.la \
	$(top_builddir)/deps/discord-rpc/src/libdiscord-rpc.la \
	@OBJECTIVELYMVC_LIBS@

if APPLE
cgame_la_LIBADD += -lc++
cgame_la_LDFLAGS += -framework AppKit
else
cgame_la_LIBADD += -lstdc++
endif
