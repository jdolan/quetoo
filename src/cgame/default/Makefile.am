SUBDIRS = \
	ui

noinst_HEADERS = \
	cg_client.h \
	cg_effect.h \
	cg_entity.h \
	cg_entity_effect.h \
	cg_entity_event.h \
	cg_entity_misc.h \
	cg_entity_trail.h \
	cg_flare.h \
	cg_hud.h \
	cg_input.h \
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
	cg_weapon.h

cgamelibdir = \
	@PKGLIBDIR@/default

cgamelib_LTLIBRARIES = \
	cgame.la

cgame_la_SOURCES = \
	cg_client.c \
	cg_discord.c \
	cg_effect.c \
	cg_entity.c \
	cg_entity_effect.c \
	cg_entity_event.c \
	cg_entity_misc.c \
	cg_entity_trail.c \
	cg_flare.c \
	cg_hud.c \
	cg_input.c \
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
	-I$(top_srcdir)/src \
	@SO_CFLAGS@ \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OPENAL_CFLAGS@ \
	@SDL2_CFLAGS@

cgame_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-shared

cgame_la_LIBADD = \
	ui/common/libcguicommon.la \
	ui/controls/libcguicontrols.la \
	ui/home/libcguihome.la \
	ui/main/libcguimain.la \
	ui/play/libcguiplay.la \
	ui/settings/libcguisettings.la \
	ui/teams/libcguiteams.la \
	$(top_builddir)/src/game/default/libpmove.la \
	$(top_builddir)/deps/discord-rpc/src/libdiscord-rpc.la

if APPLE
cgame_la_LIBADD += -lc++
cgame_la_LDFLAGS += -framework AppKit
else
cgame_la_LIBADD += -lstdc++
endif
