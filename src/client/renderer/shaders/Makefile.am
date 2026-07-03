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
	soften.glsl \
	sprite_fs.glsl \
	sprite_vs.glsl \
	uniforms.glsl \
	version.glsl \
  voxel.glsl

# --- SDL_gpu shader transpilation -------------------------------------------
#
# Entry-point shaders ported to SDL_gpu. The compiled blobs (.spv for Vulkan,
# .metal for Metal) are versioned in the repository, so normal builds never
# invoke glslc or shadercross. Run 'make shaders' after editing a .glsl to
# regenerate them. Extend these lists as more shaders are ported (issue #864).

VERTEX_SHADERS = depth_pass_vs draw_2d_vs draw_3d_vs post_vs bsp_vs mesh_vs shadow_vs sky_vs sprite_vs decal_vs
FRAGMENT_SHADERS = depth_pass_fs draw_2d_fs draw_3d_fs post_fs depth_resolve_fs bsp_fs mesh_fs shadow_fs sky_fs sprite_fs decal_fs

# Second, MATERIAL_STAGES variants of the bsp and mesh programs, used only for
# material stage (and mesh shell) draws, compiled with -DMATERIAL_STAGES.
STAGE_SHADERS = bsp_stage_vs bsp_stage_fs mesh_stage_vs mesh_stage_fs

# Install the compiled blobs alongside the GLSL sources so the renderer's
# ResourceProvider (Fs_Load) can resolve them at runtime, e.g.
# loadShader("shaders/depth_pass_vs") -> shaders/depth_pass_vs.metal|.spv.
shaders_DATA += \
	$(VERTEX_SHADERS:=.spv) $(VERTEX_SHADERS:=.metal) \
	$(FRAGMENT_SHADERS:=.spv) $(FRAGMENT_SHADERS:=.metal) \
	$(STAGE_SHADERS:=.spv) $(STAGE_SHADERS:=.metal)

EXTRA_DIST = \
	$(VERTEX_SHADERS:=.spv) $(VERTEX_SHADERS:=.metal) \
	$(FRAGMENT_SHADERS:=.spv) $(FRAGMENT_SHADERS:=.metal) \
	$(STAGE_SHADERS:=.spv) $(STAGE_SHADERS:=.metal)

.PHONY: shaders
shaders:
	for s in $(VERTEX_SHADERS); do \
		$(GLSLC) -DVERTEX_SHADER -I$(srcdir) -fshader-stage=vertex $(srcdir)/$$s.glsl -o $$s.spv || exit 1; \
		$(SHADERCROSS) $$s.spv -s SPIRV -d MSL -t vertex --msl-version 2.2.0 -o $$s.metal || exit 1; \
	done
	for s in $(FRAGMENT_SHADERS); do \
		$(GLSLC) -DFRAGMENT_SHADER -I$(srcdir) -fshader-stage=fragment $(srcdir)/$$s.glsl -o $$s.spv || exit 1; \
		$(SHADERCROSS) $$s.spv -s SPIRV -d MSL -t fragment --msl-version 2.2.0 -o $$s.metal || exit 1; \
	done
	$(GLSLC) -DVERTEX_SHADER -DMATERIAL_STAGES -I$(srcdir) -fshader-stage=vertex $(srcdir)/bsp_vs.glsl -o bsp_stage_vs.spv || exit 1
	$(SHADERCROSS) bsp_stage_vs.spv -s SPIRV -d MSL -t vertex --msl-version 2.2.0 -o bsp_stage_vs.metal || exit 1
	$(GLSLC) -DFRAGMENT_SHADER -DMATERIAL_STAGES -I$(srcdir) -fshader-stage=fragment $(srcdir)/bsp_fs.glsl -o bsp_stage_fs.spv || exit 1
	$(SHADERCROSS) bsp_stage_fs.spv -s SPIRV -d MSL -t fragment --msl-version 2.2.0 -o bsp_stage_fs.metal || exit 1
	$(GLSLC) -DVERTEX_SHADER -DMATERIAL_STAGES -I$(srcdir) -fshader-stage=vertex $(srcdir)/mesh_vs.glsl -o mesh_stage_vs.spv || exit 1
	$(SHADERCROSS) mesh_stage_vs.spv -s SPIRV -d MSL -t vertex --msl-version 2.2.0 -o mesh_stage_vs.metal || exit 1
	$(GLSLC) -DFRAGMENT_SHADER -DMATERIAL_STAGES -I$(srcdir) -fshader-stage=fragment $(srcdir)/mesh_fs.glsl -o mesh_stage_fs.spv || exit 1
	$(SHADERCROSS) mesh_stage_fs.spv -s SPIRV -d MSL -t fragment --msl-version 2.2.0 -o mesh_stage_fs.metal || exit 1
