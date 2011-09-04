noinst_HEADERS = \
	r_gl.h \
	r_image.h \
	r_material.h \
	r_matrix.h \
	r_model.h \
	r_program.h \
	r_state.h \
	renderer.h

noinst_LTLIBRARIES = \
	librenderer.la

if BUILD_CLIENT

librenderer_la_SOURCES = \
	r_array.c \
	r_bsp_light.c \
	r_bsp_model.c \
	r_bsp.c \
	r_capture.c \
	r_context.c \
	r_corona.c \
	r_draw.c \
	r_entity.c \
	r_flare.c \
	r_gl.c \
	r_image.c \
	r_light.c \
	r_lighting.c \
	r_lightmap.c \
	r_main.c \
	r_material.c \
	r_matrix.c \
	r_mesh_model.c \
	r_mesh.c \
	r_model.c \
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
	@BASE_CFLAGS@ \
	@OPENGL_CFLAGS@ \
	@SDL_CFLAGS@
librenderer_la_LIBADD = \
	../../libimages.la

else

librenderer_la_SOURCES = \
	../../null.c
	
librenderer_la_CFLAGS = \
	-D__LIBTOOL_LIBRARY__=librenderer_la
	
endif