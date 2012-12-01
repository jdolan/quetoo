noinst_HEADERS = \
	r_array.h \
	r_bsp_light.h \
	r_bsp_model.h \
	r_bsp_surface.h \
	r_bsp.h \
	r_capture.h \
	r_context.h \
	r_corona.h \
	r_draw.h \
	r_entity.h \
	r_flare.h \
	r_gl.h \
	r_image.h \
	r_light.h \
	r_lighting.h \
	r_lightmap.h \
	r_local.h \
	r_main.h \
	r_material.h \
	r_matrix.h \
	r_media.h \
	r_mesh_model.h \
	r_mesh.h \
	r_model.h \
	r_particle.h \
	r_program.h \
	r_program_default.h \
	r_program_warp.h \
	r_sky.h \
	r_state.h \
	r_types.h \
	renderer.h

noinst_LTLIBRARIES = \
	librenderer.la

if BUILD_CLIENT

librenderer_la_SOURCES = \
	r_array.c \
	r_bsp_light.c \
	r_bsp_model.c \
	r_bsp_surface.c \
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
	r_media.c \
	r_mesh_model.c \
	r_mesh.c \
	r_model.c \
	r_particle.c \
	r_program.c \
	r_program_default.c \
	r_program_warp.c \
	r_sky.c \
	r_state.c

librenderer_la_CFLAGS = \
	-I.. -I../.. \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
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