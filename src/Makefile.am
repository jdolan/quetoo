SUBDIRS = . game tools @MASTER_SERVER@

SUFFIXES = .rc
.rc.o:
	@WINDRES@ $< -o $@

COMMON_H = anorms.h binds.h console.h con_curses.h hash.h keys.h shared.h \
	common.h files.h pak.h win32.h

CLIENT_H = client.h

CLIENT = cl_cmd.c cl_console.c cl_emit.c cl_entity.c cl_effect.c cl_http.c \
	cl_input.c cl_keys.c cl_loc.c cl_main.c cl_parse.c cl_pred.c cl_screen.c \
	cl_server.c cl_tentity.c cl_view.c

RENDERER_H = images.h r_gl.h r_image.h r_material.h r_model.h r_program.h \
	r_state.h renderer.h

RENDERER = r_bsp.c r_context.c r_corona.c r_draw.c r_entity.c r_flare.c \
	r_image.c r_light.c r_lightmap.c r_main.c r_material.c r_mesh.c r_model.c \
	r_model_bsp.c r_model_mesh.c r_particle.c r_program.c r_sky.c r_state.c \
	r_surface.c r_surface_pro.c r_thread.c

SOUND_H = sound.h

SOUND = s_device.c s_main.c s_mix.c s_sample.c

SERVER_H = server.h

SERVER = cmd.c console.c con_curses.c cvar.c main.c net_chan.c net_udp.c pmove.c \
	\
	sv_ccmds.c sv_ents.c sv_game.c sv_init.c sv_main.c sv_send.c sv_user.c \
	sv_world.c \
	\
	win32.c

bin_PROGRAMS = quake2world

if BUILD_CLIENT

noinst_HEADERS = $(COMMON_H) $(SERVER_H) $(CLIENT_H) $(RENDERER_H) $(SOUND_H)

noinst_LTLIBRARIES = libcmodel.la libcommon.la libfiles.la libhash.la \
	libimages.la libpak.la libshared.la libsys.la

quake2world_SOURCES = $(SERVER) $(CLIENT) $(RENDERER) $(SOUND)

EXTRA_quake2world_SOURCES = quake2world-icon.rc

quake2world_DEPENDENCIES = ${ICON}

quake2world_CFLAGS = -pipe @WARN_CFLAGS@ @SDL_CFLAGS@ @CURL_CFLAGS@ \
	@CURSES_CFLAGS@

quake2world_LDADD = @ICON@ libcmodel.la libcommon.la libfiles.la libhash.la \
	libimages.la libpak.la libshared.la @SDL_LIBS@ @CURL_LIBS@ @CURSES_LIBS@ \
	@HOST_LIBS@ @HOST_OPENGL_LIBS@ -lm -lz

else

noinst_HEADERS = $(COMMON_H) $(SERVER_H)

noinst_LTLIBRARIES = libcmodel.la libcommon.la libfiles.la libhash.la \
	libpak.la libshared.la libsys.la

quake2world_SOURCES = $(SERVER) sv_dummy.c

EXTRA_quake2world_SOURCES = quake2world-icon.rc

quake2world_DEPENDENCIES = ${ICON}

quake2world_CFLAGS = -pipe @WARN_CFLAGS@ @CURSES_CFLAGS@

quake2world_LDADD = @ICON@ libcmodel.la libcommon.la libfiles.la libhash.la \
	libpak.la libshared.la @CURSES_LIBS@ @HOST_LIBS@ -lm -lz

endif

libcmodel_la_SOURCES = cmodel.c
libcmodel_la_CFLAGS = -pipe @WARN_CFLAGS@

libcommon_la_SOURCES = common.c
libcommon_la_CFLAGS = -pipe @WARN_CFLAGS@ @SDL_CFLAGS@
libcommon_la_LIBADD = @SDL_LIBS@

libfiles_la_SOURCES = files.c
libfiles_la_CFLAGS = -pipe @WARN_CFLAGS@

libimages_la_SOURCES = images.c
libimages_la_CFLAGS = -pipe @WARN_CFLAGS@ @SDL_CFLAGS@

libhash_la_SOURCES = hash.c
libhash_la_CFLAGS = -pipe @WARN_CFLAGS@

libpak_la_SOURCES = pak.c files.c
libpak_la_CFLAGS = -pipe -D__LIBPAK_LA__ @WARN_CFLAGS@
libpak_la_LIBADD = libhash.la

libshared_la_SOURCES = shared.c
libshared_la_CFLAGS = -pipe @WARN_CFLAGS@

libsys_la_SOURCES = main.c
libsys_la_CFLAGS = -pipe -D__LIBSYS_LA__ @WARN_CFLAGS@
