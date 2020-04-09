/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "r_local.h"

#if 0

static mat4_t r_texture_matrix;

#define UPDATE_TICKS 16

/**
 * @brief Materials "think" every few milliseconds to advance animations.
 */
static void R_UpdateMaterialStage(r_material_t *m, r_stage_t *s) {

	if (s->cm->flags & STAGE_PULSE) {
		s->pulse.dhz = (sinf(r_view.ticks * s->cm->pulse.hz * 0.00628) + 1.0) / 2.0;
	}

	if (s->cm->flags & STAGE_STRETCH) {
		s->stretch.dhz = (sinf(r_view.ticks * s->cm->stretch.hz * 0.00628) + 1.0) / 2.0;
		s->stretch.damp = 1.5 - s->stretch.dhz * s->cm->stretch.amp;
	}

	if (s->cm->flags & STAGE_ROTATE) {
		s->rotate.deg = r_view.ticks * s->cm->rotate.hz * 0.360;
	}

	if (s->cm->flags & STAGE_SCROLL_S) {
		s->scroll.ds = s->cm->scroll.s * r_view.ticks / 1000.0;
	}

	if (s->cm->flags & STAGE_SCROLL_T) {
		s->scroll.dt = s->cm->scroll.t * r_view.ticks / 1000.0;
	}

	if (s->cm->flags & STAGE_ANIM) {
		if (s->cm->anim.fps) {
			if (r_view.ticks >= s->anim.dtime) { // change frames
				s->anim.dtime = r_view.ticks + (1000 / s->cm->anim.fps);
				s->image = s->anim.frames[++s->anim.dframe % s->cm->anim.num_frames];
			}
		} else if (r_view.current_entity) {
			s->image = s->anim.frames[r_view.current_entity->frame % s->cm->anim.num_frames];
		}
	}
}

/**
 * @brief Materials "think" every few milliseconds to advance animations.
 */
static void R_UpdateMaterial(r_material_t *m) {

	if (!r_view.current_entity) {
		if (r_view.ticks - m->time < UPDATE_TICKS) {
			return;
		}
	}

	m->time = r_view.ticks;
}

/**
 * @brief Manages state for stages supporting static, dynamic, and per-pixel lighting.
 */
static void R_StageLighting(const r_bsp_face_t *surf, const r_stage_t *stage) {

	if (!surf) { // mesh materials don't support per-stage lighting
		return;
	}

	if (stage->cm->flags & (STAGE_LIGHTMAP | STAGE_LIGHTING)) {

		if (r_lightmap->value) {

			const r_image_t *atlas = surf->lightmap;

			R_EnableTexture(texunit_lightmap, true);

			R_BindLightmapTexture(atlas->lightmaps->texnum);

			if (r_deluxemap->integer) {
				R_EnableTexture(texunit_deluxemap, true);
			}

			if (r_stainmaps->value) {
				if (atlas->framebuffer) {
					R_EnableTexture(texunit_stainmap, true);

					R_BindStainmapTexture(surf->lightmap.atlas->stainmaps->texnum);
				}
			}
		}

		if (stage->cm->flags & STAGE_LIGHTING) { // hardware lighting

			R_EnableLighting(true);

			if (r_state.lighting_enabled) {

				if (surf->light_frame == r_locals.light_frame) { // dynamic light sources
					R_EnableLights(surf->light_mask);
				} else {
					R_EnableLights(0);
				}
			}
		} else {
			R_EnableLighting(false);
		}
	} else {
		R_EnableLighting(false);

		R_EnableTexture(texunit_lightmap, false);
		R_EnableTexture(texunit_deluxemap, false);
		R_EnableTexture(texunit_stainmap, false);
	}

	R_UseMaterial(stage->material);
}

/**
 * @brief Generates a single vertex for the specified stage.
 */
static void R_StageVertex(const r_bsp_face_t *surf, const r_stage_t *stage, const vec3_t in, vec3_t *out) {

	// TODO: vertex deformation
	*out = in;
}

/**
 * @brief Manages texture matrix manipulations for stages supporting rotations,
 * scrolls, and stretches (rotate, translate, scale).
 */
static void R_StageTextureMatrix(const r_bsp_face_t *surf, const r_stage_t *stage) {
	static _Bool identity = true;
	float s, t;

	if (!(stage->cm->flags & STAGE_TEXTURE_MATRIX)) {

		if (!identity) {
			Matrix4x4_CreateIdentity(&r_texture_matrix);
		}

		identity = true;
		return;
	}

	Matrix4x4_CreateIdentity(&r_texture_matrix);

	if (surf) { // for BSP surfaces, add stretch and rotate

		s = (surf->st_mins[0] + surf->st_maxs[0]) * 0.5;
		t = (surf->st_mins[1] + surf->st_maxs[1]) * 0.5;

		if (stage->cm->flags & STAGE_STRETCH) {
			Matrix4x4_ConcatTranslate(&r_texture_matrix, -s, -t, 0.0);
			Matrix4x4_ConcatScale3(&r_texture_matrix, stage->stretch.damp, stage->stretch.damp, 1.0);
			Matrix4x4_ConcatTranslate(&r_texture_matrix, -s, -t, 0.0);
		}

		if (stage->cm->flags & STAGE_ROTATE) {
			Matrix4x4_ConcatTranslate(&r_texture_matrix, -s, -t, 0.0);
			Matrix4x4_ConcatRotate(&r_texture_matrix, stage->rotate.deg, 0.0, 0.0, 1.0);
			Matrix4x4_ConcatTranslate(&r_texture_matrix, -s, -t, 0.0);
		}
	}

	if (stage->cm->flags & STAGE_SCALE_S) {
		Matrix4x4_ConcatScale3(&r_texture_matrix, stage->cm->scale.s, 1.0, 1.0);
	}

	if (stage->cm->flags & STAGE_SCALE_T) {
		Matrix4x4_ConcatScale3(&r_texture_matrix, 1.0, stage->cm->scale.t, 1.0);
	}

	if (stage->cm->flags & STAGE_SCROLL_S) {
		Matrix4x4_ConcatTranslate(&r_texture_matrix, stage->scroll.ds, 0.0, 0.0);
	}

	if (stage->cm->flags & STAGE_SCROLL_T) {
		Matrix4x4_ConcatTranslate(&r_texture_matrix, 0.0, stage->scroll.dt, 0.0);
	}

	identity = false;
}

/**
 * @brief Generates a single texture coordinate for the specified stage and vertex.
 */
static inline void R_StageTexCoord(const r_stage_t *stage, const vec3_t v, const vec2_t in,
                                   vec2_t out) {

	vec3_t tmp;

	if (stage->cm->flags & STAGE_ENVMAP) { // generate texcoords

		tmp = Vec3_Subtract(v, r_view.origin);
		tmp = Vec3_Normalize(tmp);

		out[0] = tmp[0];
		out[1] = tmp[1];
	} else { // or use the ones we were given
		out[0] = in[0];
		out[1] = in[1];
	}

	Matrix4x4_Transform2(&r_texture_matrix, out, out);
}

#define NUM_DIRTMAP_ENTRIES 16
static const float dirtmap[NUM_DIRTMAP_ENTRIES] = {
	0.6,
	0.5,
	0.3,
	0.4,
	0.7,
	0.3,
	0.0,
	0.4,
	0.5,
	0.2,
	0.8,
	0.5,
	0.3,
	0.2,
	0.5,
	0.3
};

/**
 * @brief Generates a single color for the specified stage and vertex.
 */
static inline void R_StageColor(const r_stage_t *stage, const vec3_t v, u8vec4_t color) {

	float a;

	if (stage->cm->flags & STAGE_TERRAIN) {

		if (stage->cm->flags & STAGE_COLOR) { // honor stage color
			ColorDecompose3(stage->cm->color, color);
		} else { // or use white
			color = vec3(255, 255, 255);
		}

		// resolve alpha for vert based on z axis height
		if (v[2] < stage->cm->terrain.floor) {
			a = 0.0;
		} else if (v[2] > stage->cm->terrain.ceil) {
			a = 1.0;
		} else {
			a = (v[2] - stage->cm->terrain.floor) / stage->cm->terrain.height;
		}

		color[3] = (u8float) (a * 255.0);
	} else if (stage->cm->flags & STAGE_DIRTMAP) {

		// resolve dirtmap based on vertex position
		const int32_t index = (int32_t) (v[0] + v[1]) % NUM_DIRTMAP_ENTRIES;
		if (stage->cm->flags & STAGE_COLOR) { // honor stage color
			ColorDecompose3(stage->cm->color, color);
		} else
			// or use white
		{
			color = vec3(255, 255, 255);
		}

		color[3] = (u8float) (dirtmap[index] * stage->cm->dirt.intensity);
	} else { // simply use white
		color[0] = color[1] = color[2] = color[3] = 255;
	}
}

/**
 * @brief Manages all state for the specified surface and stage. The surface will be
 * NULL in the case of mesh stages.
 */
static void R_SetStageState(const r_bsp_face_t *surf, const r_stage_t *stage) {
	vec4_t color;

	// bind the texture
	R_BindDiffuseTexture(stage->image->texnum);

	// resolve all static, dynamic, and per-pixel lighting
	R_StageLighting(surf, stage);

	// set the blend function, ensuring a sane default
	if (stage->cm->flags & STAGE_BLEND) {
		R_BlendFunc(stage->cm->blend.src, stage->cm->blend.dest);
	} else {
		R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	// for meshes, see if we want to use the mesh color
	if (stage->cm->flags & (STAGE_TERRAIN | STAGE_DIRTMAP)) {
		// for terrain, enable the color array
		R_EnableColorArray(true);
	} else {
		R_EnableColorArray(false);

		// resolve the shade color
		if (stage->cm->mesh_color) { // explicit
			color = r_mesh_state.color;
		} else if (stage->cm->flags & STAGE_COLOR) { // explicit
			color = stage->cm->color;
		} else if (stage->cm->flags & STAGE_ENVMAP) { // implicit
			color = surf->texinfo->material->diffuse->color;
		} else {
			color = vec3(1.0, 1.0, 1.0);
		}

		// modulate the alpha value for pulses
		if (stage->cm->flags & STAGE_PULSE) {
			color[3] = stage->pulse.dhz;
		} else {
			color[3] = 1.0;
		}
	}

	if (stage->cm->flags & STAGE_FOG) {
		R_EnableFog(true);
	} else {
		R_EnableFog(false);
	}
}

static uint32_t r_material_vertex_count;

/**
 * @brief Render the specified stage for the surface.
 */
static void R_DrawBspSurfaceMaterialStage(const r_bsp_face_t *surf, const r_stage_t *stage) {

	// expand array if we're gonna eat it
	if (r_material_state.vertex_len <= (r_material_vertex_count + surf->num_vertexes)) {
		r_material_state.vertex_len *= 2;
		r_material_state.vertex_array = g_array_set_size(r_material_state.vertex_array, r_material_state.vertex_len);
		Com_Debug(DEBUG_RENDERER, "Expanded material vertex array to %u\n", r_material_state.vertex_len);
	}

	const r_bsp_vertex_t *in = r_model_state.world->bsp->vertexes + surf->first_vertex;
	for (int32_t i = 0; i < surf->num_vertexes; i++, in++) {

		r_material_vertex_t *out = &VERTEX_ARRAY_INDEX(r_material_vertex_count + i);

		R_StageVertex(surf, stage, in->position, out->position);

		R_StageTexCoord(stage, in->position, in->texcoords.diffuse, out->diffuse);

		if (texunit_lightmap->enabled) { // lightmap texcoords
			PackTexcoords(in->texcoords.lightmap, out->lightmap);
		}

		if (r_state.color_array_enabled) { // colors
			R_StageColor(stage, in->position, out->color);
		}

		if (r_state.lighting_enabled) { // normals and tangents
			out->normal = in->normal;
			out->tangent = in->tangent;
			out->bitangent = in->bitangent;
		}
	}

	r_material_vertex_count += surf->num_vertexes;
}

/**
 * @brief Iterates the specified surfaces list, updating materials as they are
 * encountered, and rendering all visible stages. State is lazily managed
 * throughout the iteration, so there is a concerted effort to restore the
 * state after all surface stages have been rendered.
 */
void R_DrawMaterialBspFaces(const r_bsp_faces_t *surfs) {

	if (!r_materials->value || r_draw_wireframe->value) {
		return;
	}

	if (!surfs->count) {
		return;
	}

	// first pass compiles vertices
	r_material_vertex_count = 0;

	for (uint32_t i = 0; i < surfs->count; i++) {

		const r_bsp_face_t *surf = surfs->surfaces[i];

		if (surf->frame != r_locals.frame) {
			continue;
		}

		r_material_t *m = surf->texinfo->material;

		R_UpdateMaterial(m);

		float j = -10.0;
		for (r_stage_t *s = m->stages; s; s = s->next, j -= 10.0) {

			if (!(s->cm->flags & STAGE_DIFFUSE)) {
				continue;
			}

			R_UpdateMaterialStage(m, s);

			R_StageTextureMatrix(surf, s);

			R_SetStageState(surf, s);

			R_DrawBspSurfaceMaterialStage(surf, s);
		}
	}

	if (!r_material_vertex_count) {
		return;
	}

	R_EnableTexture(texunit_lightmap, true);

	if (r_deluxemap->integer) {
		R_EnableTexture(texunit_deluxemap, true);
	}

	if (r_stainmaps->integer) {
		R_EnableTexture(texunit_stainmap, true);
	}

	R_EnableLighting(true);

	R_EnableColorArray(true);

	R_ResetArrayState();

	R_BindAttributeInterleaveBuffer(&r_material_state.vertex_buffer, R_ATTRIB_MASK_ALL);

	R_EnableColorArray(false);

	R_EnableLighting(false);

	R_EnableTexture(texunit_lightmap, false);
	R_EnableTexture(texunit_deluxemap, false);

	R_EnablePolygonOffset(true);

	R_UploadToSubBuffer(&r_material_state.vertex_buffer, 0,
	                    r_material_vertex_count * sizeof(r_material_vertex_t),
	                    r_material_state.vertex_array->data, false);

	// second pass draws
	for (uint32_t i = 0, index = 0; i < surfs->count; i++) {

		r_bsp_face_t *surf = surfs->surfaces[i];

		if (surf->frame != r_locals.frame) {
			continue;
		}

		r_material_t *m = surf->texinfo->material;

		float j = R_OFFSET_UNITS;
		for (const r_stage_t *s = m->stages; s; s = s->next, j += R_OFFSET_UNITS) {

			if (!(s->cm->flags & STAGE_DIFFUSE)) {
				continue;
			}

			R_PolygonOffset(R_OFFSET_FACTOR, j); // increase depth offset for each stage

			R_SetStageState(surf, s);

			R_DrawArrays(GL_TRIANGLE_FAN, index, surf->num_vertexes);

			index += surf->num_vertexes;
		}
	}

	R_UnbindAttributeBuffers();

	R_EnablePolygonOffset(false);

	Matrix4x4_CreateIdentity(&r_texture_matrix);

	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	R_EnableColorArray(false);

	R_EnableTexture(texunit_lightmap, false);
	R_EnableTexture(texunit_deluxemap, false);
	R_EnableTexture(texunit_stainmap, false);

	R_EnableLighting(true);

	R_EnableLights(0);

	R_UseMaterial(NULL);

	R_EnableLighting(false);
}

/**
 * @brief Re-draws the currently bound arrays from the given offset to count after
 * setting GL state for the stage.
 */
void R_DrawMeshMaterial(r_material_t *m, const GLuint offset, const GLuint count) {
	const _Bool blend = r_state.blend_enabled;

	if (!r_materials->value || r_draw_wireframe->value) {
		return;
	}

	if (!(m->cm->flags & STAGE_DIFFUSE)) {
		return;
	}

	R_UpdateMaterial(m);

	if (!blend) {
		R_EnableBlend(true);
		R_EnableDepthMask(false);
	}

	R_EnablePolygonOffset(true);

	r_stage_t *s = m->stages;
	for (float j = R_OFFSET_UNITS; s; s = s->next, j += R_OFFSET_UNITS) {

		if (!(s->cm->flags & STAGE_DIFFUSE)) {
			continue;
		}

		R_UpdateMaterialStage(m, s);

		R_PolygonOffset(R_OFFSET_FACTOR, j); // increase depth offset for each stage

		R_SetStageState(NULL, s);

		R_DrawArrays(GL_TRIANGLES, offset, count);

		r_view.num_mesh_tris += count;
		r_view.num_mesh_models++;
	}

	R_EnablePolygonOffset(false);

	if (!blend) {
		R_EnableBlend(false);
		R_EnableDepthMask(true);
	}

	Matrix4x4_CreateIdentity(&r_texture_matrix);

	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	R_EnableColorArray(false);
}
#endif

/**
 * @brief Register event listener for materials.
 */
static void R_RegisterMaterial(r_media_t *self) {
	r_material_t *mat = (r_material_t *) self;

	R_RegisterDependency(self, (r_media_t *) mat->texture);

	r_stage_t *s = mat->stages;
	while (s) {

		if (s->material) {
			R_RegisterDependency(self, (r_media_t *) s->material);
		}

		if (s->image) {
			R_RegisterDependency(self, (r_media_t *) s->image);
		}

		for (int32_t i = 0; i < s->cm->anim.num_frames; i++) {
			R_RegisterDependency(self, (r_media_t *) s->anim.frames[i]);
		}

		s = s->next;
	}
}

/**
 * @brief Free event listener for materials.
 */
static void R_FreeMaterial(r_media_t *self) {

	Cm_FreeMaterial(((r_material_t *) self)->cm);
}

/**
 * @return The number of frames resolved, or -1 on error.
 */
static int32_t R_ResolveStageAnimation(r_stage_t *stage, cm_asset_context_t context) {

	const size_t size = sizeof(r_image_t *) * stage->cm->anim.num_frames;
	stage->anim.frames = Mem_LinkMalloc(size, stage);

	int32_t i;
	for (i = 0; i < stage->cm->anim.num_frames; i++) {

		cm_asset_t *frame = &stage->cm->anim.frames[i];
		if (*frame->path) {
			stage->anim.frames[i] = R_LoadImage(frame->path, IT_MATERIAL);
			if (stage->anim.frames[i]->type == IT_NULL) {
				break;
			}
		} else {
			break;
		}
	}

	if (i < stage->cm->anim.num_frames) {
		Com_Warn("Failed to resolve frame: %d: %s\n", i, stage->cm->asset.name);
		return -1;
	}

	return stage->cm->anim.num_frames;
}

/**
 * @brief Resolves assets for the specified stage, within the given context.
 */
static int32_t R_ResolveStage(r_stage_t *stage, cm_asset_context_t context) {

	if (*stage->cm->asset.path) {

		if (stage->cm->flags & (STAGE_TEXTURE | STAGE_ENVMAP | STAGE_FLARE)) {
			stage->image = R_LoadImage(stage->cm->asset.path, IT_MATERIAL);
			if (stage->image->type == IT_NULL) {
				Com_Warn("Failed to resolve stage: %s\n", stage->cm->asset.name);
				return -1;
			}
		}

		if (stage->cm->flags & STAGE_LIGHTING) {
			stage->material = R_LoadMaterial(stage->cm->asset.name, context);
		}

		if (stage->cm->flags & STAGE_ANIM) {
			return R_ResolveStageAnimation(stage, context);
		}
	}
	
	return 0;
}

/**
 * @brief
 */
static void R_AppendStage(r_material_t *m, r_stage_t *s) {

	if (m->stages == NULL) {
		m->stages = s;
	} else {
		r_stage_t *stages = m->stages;
		while (stages->next) {
			stages = stages->next;
		}
		stages->next = s;
	}
}

/**
 * @brief Normalizes material names to their context, returning the unique media key for subsequent
 * material lookups.
 */
static void R_MaterialKey(const char *name, char *key, size_t len, cm_asset_context_t context) {

	*key = '\0';

	if (*name) {
		if (*name == '#') {
			name++;
		} else {
			switch (context) {
				case ASSET_CONTEXT_NONE:
					break;
				case ASSET_CONTEXT_TEXTURES:
					if (!g_str_has_prefix(name, "textures/")) {
						g_strlcat(key, "textures/", len);
					}
					break;
				case ASSET_CONTEXT_MODELS:
					if (!g_str_has_prefix(name, "models/")) {
						g_strlcat(key, "models/", len);
					}
					break;
				case ASSET_CONTEXT_PLAYERS:
					if (!g_str_has_prefix(name, "players/")) {
						g_strlcat(key, "players/", len);
					}
					break;
				case ASSET_CONTEXT_ENVMAPS:
					if (!g_str_has_prefix(name, "envmaps/")) {
						g_strlcat(key, "envmaps/", len);
					}
					break;
				case ASSET_CONTEXT_FLARES:
					if (!g_str_has_prefix(name, "flares/")) {
						g_strlcat(key, "flares/", len);
					}
					break;
			}
		}

		g_strlcat(key, name, len);
		g_strlcat(key, "_mat", len);
	}
}

/**
 * @brief Loads the material asset, ensuring it is the specified dimensions for texture layering.
 */
static SDL_Surface *R_LoadMaterialSurface(int32_t w, int32_t h, const char *path) {

	SDL_Surface *surface = Img_LoadSurface(path);
	if (surface) {
		if (w || h) {
			if (surface->w != w || surface->h != h) {
				Com_Warn("Material asset %s is not %dx%d\n", path, w, h);
				SDL_FreeSurface(surface);
				surface = NULL;
			}
		}
	}

	return surface;
}

/**
 * @brief
 */
static SDL_Surface *R_CreateMaterialSurface(int32_t w, int32_t h, color32_t color) {

	SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);

	SDL_memset4(surface->pixels, color.rgba, w * h);

	return surface;
}

/**
 * @brief Merges the average of src's channels into the alpha channel of dest.
 */
static void R_MergeMaterialSurfaces(SDL_Surface *dest, const SDL_Surface *src) {

	assert(src->w == dest->w);
	assert(src->h == dest->h);

	const byte *in = src->pixels;
	byte *out = dest->pixels;

	const int32_t pixels = dest->w * dest->h;
	for (int32_t i = 0; i < pixels; i++, in += 4, out += 4) {
		out[3] = (in[0] + in[1] + in[2]) / 3.0;
	}
}

/**
 * @brief Resolves all asset references in the specified collision material, yielding a usable
 * renderer material.
 */
static r_material_t *R_ResolveMaterial(cm_material_t *cm, cm_asset_context_t context) {
	char key[MAX_QPATH];

	R_MaterialKey(cm->name, key, sizeof(key), context);

	r_material_t *material = (r_material_t *) R_FindMedia(key);
	
	if (material) {
		Cm_FreeMaterial(cm);
		return material;
	}

	material = (r_material_t *) R_AllocMedia(key, sizeof(r_material_t), MEDIA_MATERIAL);
	material->cm = cm;

	material->media.Register = R_RegisterMaterial;
	material->media.Free = R_FreeMaterial;

	material->texture = (r_image_t *) R_AllocMedia(va("%s_texture", material->cm->basename), sizeof(r_image_t), MEDIA_IMAGE);

	material->texture->type = IT_MATERIAL;

	Cm_ResolveMaterial(cm, context);

	SDL_Surface *diffusemap = NULL;
	if (*cm->diffusemap.path) {
		if ((diffusemap = Img_LoadSurface(cm->diffusemap.path))) {
			Com_Debug(DEBUG_RENDERER, "Loaded diffusemap %s for %s\n", cm->diffusemap.path, cm->basename);
		} else {
			Com_Warn("Failed to load diffusemap %s for %s\n", cm->diffusemap.path, cm->basename);
			diffusemap = R_CreateMaterialSurface(1, 1, Color32(255, 255, 255, 255));
		}
	} else {
		diffusemap = R_CreateMaterialSurface(1, 1, Color32(255, 255, 255, 255));
	}

	const int32_t w = material->texture->width = diffusemap->w;
	const int32_t h = material->texture->height = diffusemap->h;

	const size_t layer_size = w * h * 4;

	switch (context) {
		case ASSET_CONTEXT_TEXTURES:
		case ASSET_CONTEXT_MODELS: {

			SDL_Surface *normalmap = NULL;
			if (*cm->normalmap.path) {
				normalmap = R_LoadMaterialSurface(w, h, cm->normalmap.path);
				if (normalmap == NULL) {
					Com_Warn("Failed to load normalmap %s for %s\n", cm->normalmap.path, cm->basename);
					normalmap = R_CreateMaterialSurface(w, h, Color32(127, 127, 255, 127));
				}
			} else {
				normalmap = R_CreateMaterialSurface(w, h, Color32(127, 127, 255, 127));
			}

			if (*cm->heightmap.path) {
				SDL_Surface *heightmap = R_LoadMaterialSurface(w, h, cm->heightmap.path);
				if (heightmap) {
					R_MergeMaterialSurfaces(normalmap, heightmap);
					SDL_FreeSurface(heightmap);
				} else {
					Com_Warn("Failed to load heightmap %s for %s\n", cm->heightmap.path, cm->basename);
				}
			}

			SDL_Surface *glossmap = NULL;
			if (*cm->glossmap.path) {
				glossmap = R_LoadMaterialSurface(w, h, cm->glossmap.path);
				if (glossmap == NULL) {
					Com_Warn("Failed to load glossmap %s for %s\n", cm->glossmap.path, cm->basename);
					glossmap = R_CreateMaterialSurface(w, h, Color32(127, 127, 127, 127));
				}
			} else {
				glossmap = R_CreateMaterialSurface(w, h, Color32(127, 127, 127, 127));
			}

			if (*cm->specularmap.path) {
				SDL_Surface *specularmap = R_LoadMaterialSurface(w, h, cm->specularmap.path);
				if (specularmap) {
					R_MergeMaterialSurfaces(glossmap, specularmap);
					SDL_FreeSurface(specularmap);
				} else {
					Com_Warn("Failed to load specularmap %s for %s\n", cm->specularmap.path, cm->basename);
				}
			}

			material->texture->depth = 3;

			byte *data = malloc(layer_size * material->texture->depth);

			memcpy(data + 0 * layer_size, diffusemap->pixels, layer_size);
			memcpy(data + 1 * layer_size, normalmap->pixels, layer_size);
			memcpy(data + 2 * layer_size, glossmap->pixels, layer_size);

			R_UploadImage(material->texture, GL_RGBA, data);

			free(data);

			SDL_FreeSurface(normalmap);
			SDL_FreeSurface(glossmap);
		}
			break;

		case ASSET_CONTEXT_PLAYERS: {

			SDL_Surface *tintmap = NULL;
			if (*cm->tintmap.path) {
				tintmap = R_LoadMaterialSurface(w, h, cm->tintmap.path);
				if (tintmap == NULL) {
					Com_Warn("Failed to load tintmap %s for %s\n", cm->tintmap.path, cm->basename);
					tintmap = R_CreateMaterialSurface(diffusemap->w, diffusemap->h, Color32(0, 0, 0, 0));
				}
			} else {
				tintmap = R_CreateMaterialSurface(diffusemap->w, diffusemap->h, Color32(0, 0, 0, 0));
			}

			material->texture->depth = 2;

			byte *data = malloc(layer_size * material->texture->depth);

			memcpy(data + 0 * layer_size, diffusemap->pixels, layer_size);
			memcpy(data + 1 * layer_size, tintmap->pixels, layer_size);

			R_UploadImage(material->texture, GL_RGBA, data);

			free(data);

			SDL_FreeSurface(tintmap);
		}
			break;

		default:
			R_UploadImage(material->texture, GL_RGBA, diffusemap->pixels);
			break;
	}

	SDL_FreeSurface(diffusemap);

	return material;
}

/**
 * @brief Resolves all asset references in the specified render material's stages
 */
static void R_ResolveMaterialStages(r_material_t *material, cm_asset_context_t context) {

	const cm_material_t *cm = material->cm;
	for (const cm_stage_t *s = cm->stages; s; s = s->next) {

		r_stage_t *stage = (r_stage_t *) Mem_LinkMalloc(sizeof(r_stage_t), material);
		stage->cm = s;

		if (R_ResolveStage(stage, context) == -1) {
			Mem_Free(stage);
		} else {
			R_AppendStage(material, stage);
		}
	}
}

/**
 * @brief Finds an existing r_material_t from the specified texture, and registers it again if it exists.
 */
r_material_t *R_FindMaterial(const char *name, cm_asset_context_t context) {
	char key[MAX_QPATH];
	char mat_name[MAX_QPATH];
	
	StripExtension(name, mat_name);

	R_MaterialKey(mat_name, key, sizeof(key), context);

	return (r_material_t *) R_FindMedia(key);
}

/**
 * @brief Loads the r_material_t from the specified texture.
 */
r_material_t *R_LoadMaterial(const char *name, cm_asset_context_t context) {
	r_material_t *material = R_FindMaterial(name, context);

	if (material == NULL) {
		cm_material_t *mat = Cm_AllocMaterial(name);

		material = R_ResolveMaterial(mat, context);

		R_ResolveMaterialStages(material, context);

		R_RegisterMedia((r_media_t *) material);
	}

	return material;
}

/**
 * @brief Loads all materials defined in the given file.
 */
ssize_t R_LoadMaterials(const char *path, cm_asset_context_t context, GList **materials) {
	GList *source = NULL;
	const ssize_t count = Cm_LoadMaterials(path, &source);

	if (count > 0) {

		for (GList *list = source; list; list = list->next) {
			cm_material_t *cm = (cm_material_t *) list->data;

			r_material_t *material = R_ResolveMaterial(cm, context);

			*materials = g_list_prepend(*materials, material);
		}

		for (GList *list = *materials; list; list = list->next) {
			r_material_t *material = (r_material_t *) list->data;

			R_ResolveMaterialStages(material, context);

			Com_Debug(DEBUG_RENDERER, "Parsed material %s with %d stages\n", material->cm->name, material->cm->num_stages);

			R_RegisterMedia((r_media_t *) material);
		}
	}

	g_list_free(source);
	return count;
}

/**
 * @brief Loads all r_material_t for the specified BSP model.
 */
static void R_LoadBspMaterials(r_model_t *mod, GList **materials) {
	char path[MAX_QPATH];

	g_snprintf(path, sizeof(path), "%s.mat", mod->media.name);

	R_LoadMaterials(path, ASSET_CONTEXT_TEXTURES, materials);

	const bsp_texinfo_t *in = mod->bsp->cm->file.texinfo;
	for (int32_t i = 0; i < mod->bsp->cm->file.num_texinfo; i++, in++) {

		r_material_t *material = R_LoadMaterial(in->texture, ASSET_CONTEXT_TEXTURES);

		if (g_list_find(*materials, material) == NULL) {
			*materials = g_list_prepend(*materials, material);
		}
	}
}

/**
 * @brief Loads all r_material_t for the specified mesh model.
 * @remarks Player models may optionally define materials, but are not required to.
 * @remarks Other mesh models must resolve at least one material. If no materials file is found,
 * we attempt to load ${model_dir}/skin.tga as the default material.
 */
static void R_LoadMeshMaterials(r_model_t *mod, GList **materials) {
	char path[MAX_QPATH];

	g_snprintf(path, sizeof(path), "%s.mat", mod->media.name);

	if (g_str_has_prefix(mod->media.name, "players/")) {
		R_LoadMaterials(path, ASSET_CONTEXT_PLAYERS, materials);
	} else {
		if (R_LoadMaterials(path, ASSET_CONTEXT_MODELS, materials) < 1) {

			Dirname(mod->media.name, path);
			g_strlcat(path, "skin", sizeof(path));

			*materials = g_list_prepend(*materials, R_LoadMaterial(path, ASSET_CONTEXT_MODELS));
		}

		assert(materials);
	}
}

/**
 * @brief Loads all r_material_t for the specified model, populating `mod->materials`.
 */
void R_LoadModelMaterials(r_model_t *mod) {
	GList *materials = NULL;

	switch (mod->type) {
		case MOD_BSP:
			R_LoadBspMaterials(mod, &materials);
			break;
		case MOD_MESH:
			R_LoadMeshMaterials(mod, &materials);
			break;
		default:
			Com_Debug(DEBUG_RENDERER, "Unsupported model: %s\n", mod->media.name);
			break;
	}

	mod->num_materials = g_list_length(materials);
	mod->materials = Mem_LinkMalloc(sizeof(r_material_t *) * mod->num_materials, mod);

	r_material_t **out = mod->materials;
	for (const GList *list = materials; list; list = list->next, out++) {
		*out = (r_material_t *) R_RegisterDependency((r_media_t *) mod, (r_media_t *) list->data);
	}

	Com_Debug(DEBUG_RENDERER, "Loaded %" PRIuPTR " materials for %s\n", mod->num_materials, mod->media.name);

	g_list_free(materials);
}

/**
 * @brief Writes all r_material_t for the specified BSP model to disk.
 */
static ssize_t R_SaveBspMaterials(const r_model_t *mod) {
	char path[MAX_QPATH];

	g_snprintf(path, sizeof(path), "%s.mat", mod->media.name);

	GList *materials = NULL;
	for (size_t i = 0; i < mod->num_materials; i++) {
		materials = g_list_prepend(materials, mod->materials[i]->cm);
	}

	const ssize_t count = Cm_WriteMaterials(path, materials);

	g_list_free(materials);
	return count;
}

/**
 * @brief
 */
static void R_SaveMaterials_f(void) {

	if (!r_model_state.world) {
		Com_Print("No map loaded\n");
		return;
	}

	R_SaveBspMaterials(r_model_state.world);
}

/**
 * @brief
 */
void R_InitMaterials(void) {

	Cmd_Add("r_save_materials", R_SaveMaterials_f, CMD_RENDERER, "Write all of the loaded map materials to the disk.");

//	r_material_state.vertex_len = INITIAL_VERTEX_COUNT;
//
//	r_material_state.vertex_array = g_array_sized_new(false, true, sizeof(r_material_vertex_t),
//	                                r_material_state.vertex_len);
//
//	R_CreateInterleaveBuffer(&r_material_state.vertex_buffer, &(const r_create_interleave_t) {
//		.struct_size = sizeof(r_material_vertex_t),
//		.layout = r_material_buffer_layout,
//		.hint = GL_DYNAMIC_DRAW,
//		.size = sizeof(r_material_vertex_t) * r_material_state.vertex_len
//	});
//
//	Matrix4x4_CreateIdentity(&r_texture_matrix);
}

/**
 * @brief
 */
void R_ShutdownMaterials(void) {

//	g_array_free(r_material_state.vertex_array, true);
//
//	R_DestroyBuffer(&r_material_state.vertex_buffer);
}
