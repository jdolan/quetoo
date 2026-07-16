shadersdir = @PKGLIBDIR@/default/shaders

shaders_DATA = \
	bsp_fs.glsl \
	bsp_vs.glsl \
	common.glsl \
	decal_fs.glsl \
	decal_vs.glsl \
	depth_pass_fs.glsl \
	depth_pass_vs.glsl \
	draw_2d_fs.glsl \
	draw_2d_vs.glsl \
	draw_3d_fs.glsl \
	draw_3d_vs.glsl \
	light.glsl \
	light_types.glsl \
	material.glsl \
	mesh_fs.glsl \
	mesh_vs.glsl \
	occlude_vs.glsl \
	post_fs.glsl \
	post_vs.glsl \
	shadow_clear_fs.glsl \
	shadow_clear_vs.glsl \
	shadow_fs.glsl \
	shadow_vs.glsl \
	sky_fs.glsl \
	sky_vs.glsl \
	soften.glsl \
	sprite_fs.glsl \
	sprite_vs.glsl \
	uniforms.glsl \
 	voxel.glsl

# SDL_gpu shader transpilation via glslc and SDL_shadercross. The output
# (.dxil, .metal, .spv) are all versioned, so your typical build does not
# need to worry about these. But for users who modify the .glsl sources,
# run `make -C src/client/renderer/shaders shaders` to transpile.

VERTEX_SHADERS = depth_pass_vs draw_2d_vs draw_3d_vs post_vs bsp_vs mesh_vs shadow_vs shadow_clear_vs sky_vs sprite_vs decal_vs occlude_vs
FRAGMENT_SHADERS = depth_pass_fs draw_2d_fs draw_3d_fs post_fs bsp_fs mesh_fs shadow_fs shadow_clear_fs sky_fs sprite_fs decal_fs

# Fragment shaders with an ALPHA_TEST compile-time variant due to the implications of discard.
ALPHA_TEST_FRAGMENT_SHADERS = bsp_fs mesh_fs

shaders_DATA += \
	$(VERTEX_SHADERS:=.spv) \
	$(VERTEX_SHADERS:=.metal) \
	$(VERTEX_SHADERS:=.dxil) \
	$(FRAGMENT_SHADERS:=.spv) \
	$(FRAGMENT_SHADERS:=.metal) \
	$(FRAGMENT_SHADERS:=.dxil) \
	$(ALPHA_TEST_FRAGMENT_SHADERS:=_alpha_test.spv) \
	$(ALPHA_TEST_FRAGMENT_SHADERS:=_alpha_test.metal) \
	$(ALPHA_TEST_FRAGMENT_SHADERS:=_alpha_test.dxil)

EXTRA_DIST = $(shaders_DATA)

.PHONY: shaders
shaders:
	for s in $(VERTEX_SHADERS); do \
		$(GLSLC) -DVERTEX_SHADER -I$(srcdir) -fshader-stage=vertex $(srcdir)/$$s.glsl -o $$s.spv || exit 1; \
		$(SHADERCROSS) $$s.spv -s SPIRV -d MSL -t vertex --msl-version 2.2.0 -o $$s.metal || exit 1; \
		$(SHADERCROSS) $$s.spv -s SPIRV -d DXIL -t vertex -o $$s.dxil || exit 1; \
	done
	for s in $(FRAGMENT_SHADERS); do \
		$(GLSLC) -DFRAGMENT_SHADER -I$(srcdir) -fshader-stage=fragment $(srcdir)/$$s.glsl -o $$s.spv || exit 1; \
		$(SHADERCROSS) $$s.spv -s SPIRV -d MSL -t fragment --msl-version 2.2.0 -o $$s.metal || exit 1; \
		$(SHADERCROSS) $$s.spv -s SPIRV -d DXIL -t fragment -o $$s.dxil || exit 1; \
	done
	for s in $(ALPHA_TEST_FRAGMENT_SHADERS); do \
		$(GLSLC) -DFRAGMENT_SHADER -DALPHA_TEST -I$(srcdir) -fshader-stage=fragment $(srcdir)/$$s.glsl -o $${s}_alpha_test.spv || exit 1; \
		$(SHADERCROSS) $${s}_alpha_test.spv -s SPIRV -d MSL -t fragment --msl-version 2.2.0 -o $${s}_alpha_test.metal || exit 1; \
		$(SHADERCROSS) $${s}_alpha_test.spv -s SPIRV -d DXIL -t fragment -o $${s}_alpha_test.dxil || exit 1; \
	done
