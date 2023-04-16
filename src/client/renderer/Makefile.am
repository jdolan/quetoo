SUBDIRS = \
	. \
	shaders

noinst_HEADERS = \
	r_animation.h \
	r_atlas.h \
	r_bsp.h \
	r_bsp_draw.h \
	r_bsp_model.h \
	r_context.h \
	r_cull.h \
	r_depth_pass.h \
	r_draw_2d.h \
	r_draw_3d.h \
	r_entity.h \
	r_framebuffer.h \
	r_gl.h \
	r_gl_types.h \
	r_image.h \
	r_light.h \
	r_local.h \
	r_main.h \
	r_material.h \
	r_media.h \
	r_mesh_draw.h \
	r_mesh_model.h \
	r_mesh_model_md3.h \
	r_mesh_model_obj.h \
	r_mesh.h \
	r_model.h \
	r_occlude.h \
	r_post.h \
	r_program.h \
	r_shadow.h \
	r_sky.h \
	r_sprite.h \
	r_stain.h \
	r_types.h \
	renderer.h

noinst_LTLIBRARIES = \
	librenderer.la

librenderer_la_SOURCES = \
	r_animation.c \
	r_atlas.c \
	r_bsp.c \
	r_bsp_draw.c \
	r_bsp_model.c \
	r_context.c \
	r_cull.c \
	r_depth_pass.c \
	r_draw_2d.c \
	r_draw_3d.c \
	r_entity.c \
	r_framebuffer.c \
	r_gl.c \
	r_image.c \
	r_light.c \
	r_main.c \
	r_material.c \
	r_media.c \
	r_mesh.c \
	r_mesh_draw.c \
	r_mesh_model.c \
	r_mesh_model_md3.c \
	r_mesh_model_obj.c \
	r_model.c \
	r_occlude.c \
	r_post.c \
	r_program.c \
	r_shadow.c \
	r_sky.c \
	r_sprite.c \
	r_stain.c

librenderer_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/client \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OPENGL_CFLAGS@ \
	@SDL2_CFLAGS@

librenderer_la_LDFLAGS = \
	-shared
	
librenderer_la_LIBADD = \
	$(top_builddir)/src/common/libcommon.la \
	@OPENGL_LIBS@
