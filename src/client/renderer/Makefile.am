SUBDIRS = \
	.

noinst_HEADERS = \
	../../images.h \
	r_gl.h \
	r_image.h \
	r_material.h \
	r_model.h \
	r_program.h \
	r_state.h \
	renderer.h

noinst_LTLIBRARIES = \
	librenderer.la
	
librenderer_la_SOURCES = \
	r_array.c \
	r_bsp.c \
	r_context.c \
	r_corona.c \
	r_draw.c \
	r_entity.c \
	r_flare.c \
	r_image.c \
	r_light.c \
	r_lightmap.c \
	r_main.c \
	r_material.c \
	r_mesh.c \
	r_model.c \
	r_model_bsp.c \
	r_model_mesh.c \
	r_particle.c \
	r_program.c \
	r_sky.c \
	r_state.c \
	r_surface.c \
	r_surface_pro.c \
	r_thread.c

librenderer_la_CFLAGS = \
	-I.. -I../.. \
	-pipe \
	@WARN_CFLAGS@ \
	@DEBUG_CFLAGS@ \
	@SDL_CFLAGS@

