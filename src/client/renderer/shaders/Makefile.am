shadersdir = @PKGLIBDIR@/default/shaders

shaders_DATA = \
	bsp_fs.glsl \
	bsp_vs.glsl \
	common.glsl \
	decal_fs.glsl \
	decal_vs.glsl \
	depth_pass_fs.glsl \
	depth_pass_vs.glsl \
	depth_resolve_fs.glsl \
	draw_2d_fs.glsl \
	draw_2d_vs.glsl \
	draw_3d_fs.glsl \
	draw_3d_vs.glsl \
  light.glsl \
	material.glsl \
	mesh_fs.glsl \
	mesh_vs.glsl \
	post_fs.glsl \
	post_vs.glsl \
	shadow_fs.glsl \
	shadow_vs.glsl \
	sky_fs.glsl \
	sky_vs.glsl \
	soften_fs.glsl \
	sprite_fs.glsl \
	sprite_vs.glsl \
	uniforms.glsl \
	version.glsl \
	voxel.glsl \
	rtx.rgen.spv \
	rtx.rmiss.spv \
	rtx_shadow.rmiss.spv \
	rtx.rchit.spv \
	vk_2d.vert.spv \
	vk_2d.frag.spv

# Source for the precompiled SPIR-V shaders above. Regenerate with:
#   glslangValidator --target-env vulkan1.2 -V rtx.rgen -o rtx.rgen.spv  (etc.)
#   glslangValidator --target-env vulkan1.2 -V vk_2d.vert -o vk_2d.vert.spv
EXTRA_DIST = \
	rtx.rgen \
	rtx.rmiss \
	rtx_shadow.rmiss \
	rtx.rchit \
	vk_2d.vert \
	vk_2d.frag
