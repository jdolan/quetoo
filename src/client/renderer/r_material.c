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

static matrix4x4_t r_texture_matrix;

// Just a number used for the initial buffer size.
// It should fit most maps.
#define INITIAL_VERTEX_COUNT 512

// interleave constants
typedef struct {
	vec3_t		vertex;
	u8vec4_t	color;
	vec3_t		normal;
	vec4_t		tangent;
	vec2_t		diffuse;
	u16vec2_t	lightmap;
} r_material_interleave_vertex_t;

static r_buffer_layout_t r_material_buffer_layout[] = {
	{ .attribute = R_ARRAY_POSITION, .type = R_ATTRIB_FLOAT, .count = 3, .size = sizeof(vec3_t) },
	{ .attribute = R_ARRAY_COLOR, .type = R_ATTRIB_UNSIGNED_BYTE, .count = 4, .size = sizeof(u8vec4_t), .offset = 12, .normalized = true },
	{ .attribute = R_ARRAY_NORMAL, .type = R_ATTRIB_FLOAT, .count = 3, .size = sizeof(vec3_t), .offset = 16 },
	{ .attribute = R_ARRAY_TANGENT, .type = R_ATTRIB_FLOAT, .count = 4, .size = sizeof(vec4_t), .offset = 28 },
	{ .attribute = R_ARRAY_DIFFUSE, .type = R_ATTRIB_FLOAT, .count = 2, .size = sizeof(vec2_t), .offset = 44 },
	{ .attribute = R_ARRAY_LIGHTMAP, .type = R_ATTRIB_UNSIGNED_SHORT, .count = 2, .size = sizeof(u16vec2_t), .offset = 52, .normalized = true },
	{ .attribute = -1 }
};

typedef struct {
	GArray *vertex_array;
	r_buffer_t vertex_buffer;
	uint32_t vertex_len;

	GArray *element_array;
	uint32_t element_len;
} r_material_state_t;

#define VERTEX_ARRAY_INDEX(i) (g_array_index(r_material_state.vertex_array, r_material_interleave_vertex_t, i))
#define ELEMENT_ARRAY_INDEX(i) (g_array_index(r_material_state.element_array, u16vec_t, i))

static r_material_state_t r_material_state;

#define UPDATE_THRESHOLD 16

/**
 * @brief Materials "think" every few milliseconds to advance animations.
 */
static void R_UpdateMaterialStage(r_material_t *m, r_stage_t *s) {

	if (s->cm->flags & STAGE_PULSE) {
		s->pulse.dhz = (sin(r_view.ticks * s->cm->pulse.hz * 0.00628) + 1.0) / 2.0;
	}

	if (s->cm->flags & STAGE_STRETCH) {
		s->stretch.dhz = (sin(r_view.ticks * s->cm->stretch.hz * 0.00628) + 1.0) / 2.0;
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
		if (r_view.ticks - m->time < UPDATE_THRESHOLD) {
			return;
		}
	}

	m->time = r_view.ticks;
}

/**
 * @brief Manages state for stages supporting static, dynamic, and per-pixel lighting.
 */
static void R_StageLighting(const r_bsp_surface_t *surf, const r_stage_t *stage) {

	if (!surf) { // mesh materials don't support per-stage lighting
		return;
	}

	// if the surface has a lightmap, and the stage specifies lighting..

	if ((surf->flags & R_SURF_LIGHTMAP) && (stage->cm->flags & (STAGE_LIGHTMAP | STAGE_LIGHTING))) {

		R_EnableTexture(texunit_lightmap, true);

		if (r_stainmap->value && surf->stainmap) {
			R_BindLightmapTexture(surf->stainmap->texnum);
		} else {
			R_BindLightmapTexture(surf->lightmap->texnum);
		}

		if (stage->cm->flags & STAGE_LIGHTING) { // hardware lighting

			R_EnableLighting(program_default, true);

			if (r_state.lighting_enabled) {
				R_BindDeluxemapTexture(surf->deluxemap->texnum);

				if (surf->light_frame == r_locals.light_frame) { // dynamic light sources
					R_EnableLights(surf->light_mask);
				} else {
					R_EnableLights(0);
				}
			}
		} else {
			R_EnableLighting(NULL, false);
		}
	} else {
		R_EnableLighting(NULL, false);

		R_EnableTexture(texunit_lightmap, false);
	}

	R_UseMaterial(stage->material);
}

/**
 * @brief Generates a single vertex for the specified stage.
 */
static void R_StageVertex(const r_bsp_surface_t *surf, const r_stage_t *stage, const vec3_t in, vec3_t out) {

	// TODO: vertex deformation
	VectorCopy(in, out);
}

/**
 * @brief Manages texture matrix manipulations for stages supporting rotations,
 * scrolls, and stretches (rotate, translate, scale).
 */
static void R_StageTextureMatrix(const r_bsp_surface_t *surf, const r_stage_t *stage) {
	static _Bool identity = true;
	vec_t s, t;

	if (!(stage->cm->flags & STAGE_TEXTURE_MATRIX)) {

		if (!identity) {
			Matrix4x4_CreateIdentity(&r_texture_matrix);
		}

		identity = true;
		return;
	}

	Matrix4x4_CreateIdentity(&r_texture_matrix);

	if (surf) { // for BSP surfaces, add stretch and rotate

		s = surf->st_center[0] / surf->texinfo->material->diffuse->width;
		t = surf->st_center[1] / surf->texinfo->material->diffuse->height;

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

		VectorSubtract(v, r_view.origin, tmp);
		VectorNormalize(tmp);

		out[0] = tmp[0];
		out[1] = tmp[1];
	} else { // or use the ones we were given
		out[0] = in[0];
		out[1] = in[1];
	}

	Matrix4x4_Transform2(&r_texture_matrix, out, out);
}

#define NUM_DIRTMAP_ENTRIES 16
static const vec_t dirtmap[NUM_DIRTMAP_ENTRIES] = {
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

	vec_t a;

	if (stage->cm->flags & STAGE_TERRAIN) {

		if (stage->cm->flags & STAGE_COLOR) { // honor stage color
			ColorDecompose3(stage->cm->color, color);
		} else
			// or use white
		{
			VectorSet(color, 255, 255, 255);
		}

		// resolve alpha for vert based on z axis height
		if (v[2] < stage->cm->terrain.floor) {
			a = 0.0;
		} else if (v[2] > stage->cm->terrain.ceil) {
			a = 1.0;
		} else {
			a = (v[2] - stage->cm->terrain.floor) / stage->cm->terrain.height;
		}

		color[3] = (u8vec_t) (a * 255.0);
	} else if (stage->cm->flags & STAGE_DIRTMAP) {

		// resolve dirtmap based on vertex position
		const uint16_t index = (uint16_t) (v[0] + v[1]) % NUM_DIRTMAP_ENTRIES;
		if (stage->cm->flags & STAGE_COLOR) { // honor stage color
			ColorDecompose3(stage->cm->color, color);
		} else
			// or use white
		{
			VectorSet(color, 255, 255, 255);
		}

		color[3] = (u8vec_t) (dirtmap[index] * stage->cm->dirt.intensity);
	} else { // simply use white
		color[0] = color[1] = color[2] = color[3] = 255;
	}
}

/**
 * @brief Manages all state for the specified surface and stage. The surface will be
 * NULL in the case of mesh stages.
 */
static void R_SetStageState(const r_bsp_surface_t *surf, const r_stage_t *stage) {
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
			VectorCopy(r_mesh_state.color, color);
		} else if (stage->cm->flags & STAGE_COLOR) { // explicit
			VectorCopy(stage->cm->color, color);
		} else if (stage->cm->flags & STAGE_ENVMAP) { // implicit
			VectorCopy(surf->texinfo->material->diffuse->color, color);
		} else {
			VectorSet(color, 1.0, 1.0, 1.0);
		}

		// modulate the alpha value for pulses
		if (stage->cm->flags & STAGE_PULSE) {
			color[3] = stage->pulse.dhz;
		} else {
			color[3] = 1.0;
		}

		R_Color(color);
	}

	if (stage->cm->flags & STAGE_FOG) {
		R_EnableFog(true);
	} else {
		R_EnableFog(false);
	}
}

static uint32_t r_material_vertex_count, r_material_index_count;

/**
 * @brief Render the specified stage for the surface.
 */
static void R_DrawBspSurfaceMaterialStage(const r_bsp_surface_t *surf, const r_stage_t *stage) {

	// expand array if we're gonna eat it
	if (r_material_state.vertex_len <= (r_material_vertex_count + surf->num_edges)) {
		r_material_state.vertex_len *= 2;
		r_material_state.vertex_array = g_array_set_size(r_material_state.vertex_array, r_material_state.vertex_len);
		Com_Debug(DEBUG_RENDERER, "Expanded material vertex array to %u\n", r_material_state.vertex_len);
	}

	for (int32_t i = 0; i < surf->num_edges; i++) {

		const vec_t *v = &r_model_state.world->bsp->verts[surf->elements[i]][0];
		const vec_t *st = &r_model_state.world->bsp->texcoords[surf->elements[i]][0];

		r_material_interleave_vertex_t *vertex = &VERTEX_ARRAY_INDEX(r_material_vertex_count + i);

		R_StageVertex(surf, stage, v, &vertex->vertex[0]);

		R_StageTexCoord(stage, v, st, &vertex->diffuse[0]);

		if (texunit_lightmap->enabled) { // lightmap texcoords
			st = &r_model_state.world->bsp->lightmap_texcoords[surf->elements[i]][0];
			PackTexcoords(st, vertex->lightmap);
		}

		if (r_state.color_array_enabled) { // colors
			R_StageColor(stage, v, &vertex->color[0]);
		}

		if (r_state.lighting_enabled) { // normals and tangents

			const vec_t *n = &r_model_state.world->bsp->normals[surf->elements[i]][0];
			VectorCopy(n, vertex->normal);

			const vec_t *t = &r_model_state.world->bsp->tangents[surf->elements[i]][0];
			Vector4Copy(t, vertex->tangent);
		}
	}

	// expand array if we're gonna eat it
	if (r_material_state.element_len <= r_material_index_count) {
		r_material_state.element_len *= 2;
		r_material_state.element_array = g_array_set_size(r_material_state.element_array, r_material_state.element_len);
		Com_Debug(DEBUG_RENDERER, "Expanded material index array to %u\n", r_material_state.element_len);
	}

	// first # to render
	ELEMENT_ARRAY_INDEX(r_material_index_count) = r_material_vertex_count;

	r_material_vertex_count += surf->num_edges;
	r_material_index_count++;
}

/**
 * @brief Iterates the specified surfaces list, updating materials as they are
 * encountered, and rendering all visible stages. State is lazily managed
 * throughout the iteration, so there is a concerted effort to restore the
 * state after all surface stages have been rendered.
 */
void R_DrawMaterialBspSurfaces(const r_bsp_surfaces_t *surfs) {

	if (!r_materials->value || r_draw_wireframe->value) {
		return;
	}

	if (!surfs->count) {
		return;
	}

	// first pass compiles vertices
	r_material_vertex_count = r_material_index_count = 0;

	for (uint32_t i = 0; i < surfs->count; i++) {

		const r_bsp_surface_t *surf = surfs->surfaces[i];

		if (surf->frame != r_locals.frame) {
			continue;
		}

		r_material_t *m = surf->texinfo->material;

		R_UpdateMaterial(m);

		vec_t j = -10.0;
		for (r_stage_t *s = m->stages; s; s = s->next, j -= 10.0) {

			if (!(s->cm->flags & STAGE_DIFFUSE)) {
				continue;
			}

			R_UpdateMaterialStage(m, s);

			// load the texture matrix for rotations, stretches, etc..
			R_StageTextureMatrix(surf, s);

			R_SetStageState(surf, s);

			R_DrawBspSurfaceMaterialStage(surf, s);
		}
	}

	if (!r_material_vertex_count) {
		return;
	}

	R_EnableTexture(texunit_lightmap, true);

	R_EnableLighting(program_default, true);

	R_EnableColorArray(true);

	R_ResetArrayState();

	R_BindAttributeInterleaveBuffer(&r_material_state.vertex_buffer, R_ARRAY_MASK_ALL);

	R_EnableColorArray(false);

	R_EnableLighting(NULL, false);

	R_EnableTexture(texunit_lightmap, false);

	R_EnablePolygonOffset(true);

	R_UploadToSubBuffer(&r_material_state.vertex_buffer, 0,
	                    r_material_vertex_count * sizeof(r_material_interleave_vertex_t),
	                    r_material_state.vertex_array->data, false);

	// second pass draws
	for (uint32_t i = 0, si = 0; i < surfs->count; i++) {

		r_bsp_surface_t *surf = surfs->surfaces[i];

		if (surf->frame != r_locals.frame) {
			continue;
		}

		r_material_t *m = surf->texinfo->material;

		vec_t j = R_OFFSET_UNITS;
		for (const r_stage_t *s = m->stages; s; s = s->next, j += R_OFFSET_UNITS) {

			if (!(s->cm->flags & STAGE_DIFFUSE)) {
				continue;
			}

			R_PolygonOffset(R_OFFSET_FACTOR, j); // increase depth offset for each stage

			R_SetStageState(surf, s);

			R_DrawArrays(GL_TRIANGLE_FAN, (GLint) ELEMENT_ARRAY_INDEX(si), surf->num_edges);

			++si;
		}
	}

	R_UnbindAttributeBuffers();

	R_EnablePolygonOffset(false);

	Matrix4x4_CreateIdentity(&r_texture_matrix);

	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	R_EnableColorArray(false);

	R_EnableTexture(texunit_lightmap, false);

	R_EnableLighting(program_default, true);

	R_EnableLights(0);

	R_UseMaterial(NULL);

	R_EnableLighting(NULL, false);

	R_Color(NULL);
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

	if (!(m->flags & STAGE_DIFFUSE)) {
		return;
	}

	R_UpdateMaterial(m);

	if (!blend) {
		R_EnableBlend(true);
		R_EnableDepthMask(false);
	}

	R_EnablePolygonOffset(true);

	r_stage_t *s = m->stages;
	for (vec_t j = R_OFFSET_UNITS; s; s = s->next, j += R_OFFSET_UNITS) {

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

	R_EnableFog(true);

	R_EnableColorArray(false);
}

/**
 * @brief Register event listener for materials.
 */
static void R_RegisterMaterial(r_media_t *self) {
	r_material_t *mat = (r_material_t *) self;

	R_RegisterDependency(self, (r_media_t *) mat->diffuse);
	R_RegisterDependency(self, (r_media_t *) mat->normalmap);
	R_RegisterDependency(self, (r_media_t *) mat->specularmap);
	R_RegisterDependency(self, (r_media_t *) mat->tintmap);

	r_stage_t *s = mat->stages;
	while (s) {
		R_RegisterDependency(self, (r_media_t *) s->image);

		uint16_t i;
		for (i = 0; i < s->cm->anim.num_frames; i++) {
			R_RegisterDependency(self, (r_media_t *) s->anim.frames[i]);
		}

		R_RegisterDependency(self, (r_media_t *) s->material);
		s = s->next;
	}
}

/**
 * @brief Free event listener for materials.
 */
static void R_FreeMaterial(r_media_t *self) {
	r_material_t *mat = (r_material_t *) self;

	Cm_FreeMaterial(mat->cm);
}

/**
 * @brief
 */
static void R_LoadNormalmap(r_material_t *mat, const char *base) {
	const char *suffix[] = { "_nm", "_norm", "_local", "_bump" };

	for (size_t i = 0; i < lengthof(suffix); i++) {
		mat->normalmap = R_LoadImage(va("%s%s", base, suffix[i]), IT_NORMALMAP);
		if (mat->normalmap->type == IT_NORMALMAP) {
			break;
		}
	}

	if (mat->normalmap->type == IT_NULL) {
		mat->normalmap = NULL;
	}
}

/**
 * @brief
 */
static void R_LoadSpecularmap(r_material_t *mat, const char *base) {
	const char *suffix[] = { "_s", "_gloss", "_spec" };

	for (size_t i = 0; i < lengthof(suffix); i++) {
		mat->specularmap = R_LoadImage(va("%s%s", base, suffix[i]), IT_SPECULARMAP);
		if (mat->specularmap->type == IT_SPECULARMAP) {
			break;
		}
	}

	if (mat->specularmap->type == IT_NULL) {
		mat->specularmap = NULL;
	}
}

/**
 * @brief
 */
static void R_LoadTintmap(r_material_t *mat, const char *base) {
	const char *suffix[] = { "_tint" };

	for (size_t i = 0; i < lengthof(suffix); i++) {
		mat->tintmap = R_LoadImage(va("%s%s", base, suffix[i]), IT_TINTMAP);
		if (mat->tintmap->type == IT_TINTMAP) {
			break;
		}
	}

	if (mat->tintmap->type == IT_NULL) {
		mat->tintmap = NULL;
	}
}

/**
 * @brief Loads the r_material_t from the specified cm_material_t. If the material is
 * already loaded, the cm_material_t will be freed, so don't use it after this function!
 * Use the "cm" member of the renderer material.
 * @returns False if the material is already loaded and shouldn't be re-parsed.
 */
static _Bool R_ConvertMaterial(cm_material_t *cm, r_material_t **mat) {
	char key[MAX_QPATH];

	if (!cm || !cm->diffuse[0]) {
		Com_Error(ERROR_DROP, "NULL diffuse name\n");
	}

	g_snprintf(key, sizeof(key), "%s_mat", cm->base);
	r_material_t *material = (r_material_t *) R_FindMedia(key);

	if (material == NULL) {
		material = (r_material_t *) R_AllocMedia(key, sizeof(r_material_t), MEDIA_MATERIAL);
		
		*mat = material;
		material->cm = cm;

		material->media.Register = R_RegisterMaterial;
		material->media.Free = R_FreeMaterial;

		material->diffuse = R_LoadImage(cm->diffuse, IT_DIFFUSE);

		if (material->diffuse->type == IT_DIFFUSE) {
			R_LoadNormalmap(material, cm->base);

			if (material->normalmap) {
				R_LoadSpecularmap(material, cm->base);
			}

			R_LoadTintmap(material, cm->base);
		}

		R_RegisterMedia((r_media_t *) material);
		return true;
	}
	
	*mat = material;
	Cm_FreeMaterial(cm);
	return false;
}

/**
 * @brief
 */
static void R_AttachStage(r_material_t *m, r_stage_t *s) {

	// append the stage to the chain
	if (!m->stages) {
		m->stages = s;
		return;
	}

	r_stage_t *ss = m->stages;
	while (ss->next) {
		ss = ss->next;
	}
	ss->next = s;
}

/**
 * @brief Loads the r_material_t from the specified texture.
 */
r_material_t *R_LoadMaterial(const char *name) {
	char key[MAX_QPATH];

	StripExtension(name, key);
	Cm_NormalizeMaterialName(key, key, sizeof(key));

	g_strlcat(key, "_mat", sizeof(key));
	r_material_t *mat = (r_material_t *) R_FindMedia(key);

	if (mat == NULL) {
		R_ConvertMaterial(Cm_AllocMaterial(name), &mat);
	}

	return mat;
}

/**
 * @brief
 */
static int32_t R_LoadStageFrames(r_stage_t *s) {
	char name[MAX_QPATH];
	int32_t i, j;

	if (!s->image) {
		Com_Warn("Texture not defined in anim stage\n");
		return -1;
	}

	g_strlcpy(name, s->image->media.name, sizeof(name));
	const size_t len = strlen(name);

	if ((i = (int32_t) strtol(&name[len - 1], NULL, 0)) < 0) {
		Com_Warn("Texture name does not end in numeric: %s\n", name);
		return -1;
	}

	// the first image was already loaded by the stage parse, so just copy
	// the pointer into the array

	s->anim.frames = Mem_LinkMalloc(s->cm->anim.num_frames * sizeof(r_image_t *), s);
	s->anim.frames[0] = s->image;

	// now load the rest
	name[len - 1] = '\0';
	for (j = 1, i = i + 1; j < s->cm->anim.num_frames; j++, i++) {
		char frame[MAX_QPATH];

		g_snprintf(frame, sizeof(frame), "%s%d", name, i);
		s->anim.frames[j] = R_LoadImage(frame, IT_DIFFUSE);

		if (s->anim.frames[j]->type == IT_NULL) {
			Com_Warn("Failed to resolve frame: %d: %s\n", j, frame);
			return -1;
		}
	}

	return 0;
}

/**
 * @brief
 */
static int32_t R_ParseStage(r_stage_t *s, const cm_stage_t *cm) {

	s->cm = cm;

	if (*cm->image) {

		if (cm->flags & STAGE_TEXTURE) {
			if (*cm->image == '#') {
				s->image = R_LoadImage(cm->image + 1, IT_DIFFUSE);
			} else {
				s->image = R_LoadImage(va("textures/%s", cm->image), IT_DIFFUSE);
			}

			if (s->image->type == IT_NULL) {
				Com_Warn("Failed to resolve texture: %s\n", cm->image);
				return -1;
			}
		} else if (cm->flags & STAGE_ENVMAP) {
			if (*cm->image == '#') {
				s->image = R_LoadImage(cm->image + 1, IT_ENVMAP);
			} else if (*cm->image == '0' || cm->image_index > 0) {
				s->image = R_LoadImage(va("envmaps/envmap_%d", cm->image_index), IT_ENVMAP);
			} else {
				s->image = R_LoadImage(va("envmaps/%s", cm->image), IT_ENVMAP);
			}

			if (s->image->type == IT_NULL) {
				Com_Warn("Failed to resolve envmap: %s\n", cm->image);
				return -1;
			}
		} else if (cm->flags & STAGE_FLARE) {
			s->image = R_LoadImage(cm->image, IT_FLARE);

			if (*cm->image == '#') {
				s->image = R_LoadImage(cm->image + 1, IT_FLARE);
			} else if (*cm->image == '0' || cm->image_index > 0) {
				s->image = R_LoadImage(va("flares/flare_%d", cm->image_index), IT_FLARE);
			} else {
				s->image = R_LoadImage(va("flares/%s", cm->image), IT_FLARE);
			}

			if (s->image->type == IT_NULL) {
				Com_Warn("Failed to resolve flare: %s\n", cm->image);
				return -1;
			}
		}
	}

	// load material if lighting
	if (cm->flags & STAGE_LIGHTING) {
		s->material = R_LoadMaterial(cm->image);
	}

	return 0;
}

/**
 * @brief Loads all materials for the specified model. This is accomplished by
 * parsing the material definitions in ${model_name}.mat for mesh models, and
 * materials/${model_name}.mat for BSP models.
 */
void R_LoadMaterials(r_model_t *mod) {
	cm_material_t **materials;
	size_t num_materials;

	// load the materials file for parsing
	if (mod->type == MOD_BSP) {
		materials = Cm_LoadMaterials(va("materials/%s.mat", Basename(mod->media.name)), &num_materials);
	} else {
		materials = Cm_LoadMaterials(va("%s.mat", mod->media.name), &num_materials);
	}

	if (!num_materials) {
		return;
	}

	for (size_t i = 0; i < num_materials; i++) {
		r_material_t *r_mat;
		
		if (!R_ConvertMaterial(materials[i], &r_mat)) {
			Com_Debug(DEBUG_RENDERER, "Retained material %s with %d stages\n", r_mat->diffuse->media.name, r_mat->cm->num_stages);
			continue;
		}

		if (!r_mat) {
			Com_Warn("Failed to convert %s\n", r_mat->cm->diffuse);
			continue;
		}

		if (r_mat->diffuse->type == IT_NULL) {
			Com_Warn("Failed to resolve %s\n", r_mat->cm->diffuse);
			continue;
		}

		if (r_bumpmap->value) { // if per-pixel lighting is enabled, resolve normal and specular

			if (*r_mat->cm->normalmap) {
				if (*r_mat->cm->normalmap == '#') {
					r_mat->normalmap = R_LoadImage(r_mat->cm->normalmap + 1, IT_NORMALMAP);
				} else {
					r_mat->normalmap = R_LoadImage(va("textures/%s", r_mat->cm->normalmap), IT_NORMALMAP);
				}

				if (r_mat->normalmap->type == IT_NULL) {
					Com_Warn("Failed to resolve normalmap: %s\n", r_mat->cm->normalmap);
					r_mat->normalmap = NULL;
				}
			}

			if (*r_mat->cm->specularmap) {
				if (*r_mat->cm->specularmap == '#') {
					r_mat->specularmap = R_LoadImage(r_mat->cm->specularmap + 1, IT_SPECULARMAP);
				} else {
					r_mat->specularmap = R_LoadImage(va("textures/%s", r_mat->cm->specularmap), IT_SPECULARMAP);
				}

				if (r_mat->specularmap->type == IT_NULL) {
					Com_Warn("Failed to resolve specularmap: %s\n", r_mat->cm->specularmap);
					r_mat->specularmap = NULL;
				}
			}
		}

		if (*r_mat->cm->tintmap) {
			if (*r_mat->cm->tintmap == '#') {
				r_mat->tintmap = R_LoadImage(r_mat->cm->tintmap + 1, IT_TINTMAP);
			} else {
				r_mat->tintmap = R_LoadImage(va("textures/%s", r_mat->cm->tintmap), IT_TINTMAP);
			}

			if (r_mat->tintmap->type == IT_NULL) {
				Com_Warn("Failed to resolve tintmap: %s\n", r_mat->cm->tintmap);
				r_mat->tintmap = NULL;
			}
		}

		for (cm_stage_t *cm_stage = r_mat->cm->stages; cm_stage; cm_stage = cm_stage->next) {
			r_stage_t *r_stage = (r_stage_t *) Mem_LinkMalloc(sizeof(r_stage_t), r_mat);

			if (R_ParseStage(r_stage, cm_stage) == -1) {
				Mem_Free(r_stage);
				continue;
			}

			// load animation frame images
			if (cm_stage->flags & STAGE_ANIM) {
				if (R_LoadStageFrames(r_stage) == -1) {
					Mem_Free(r_stage);
					continue;
				}
			}
			
			// attach stage
			R_AttachStage(r_mat, r_stage);

			r_mat->flags |= cm_stage->flags;
			continue;
		}

		R_RegisterDependency((r_media_t *) mod, (r_media_t *) r_mat);
		Com_Debug(DEBUG_RENDERER, "Parsed material %s with %d stages\n", r_mat->diffuse->media.name, r_mat->cm->num_stages);
	}
	
	Cm_FreeMaterialList(materials);
}

/**
 * @brief
 */
static void R_SaveMaterials_f(void) {
	const r_model_t *mod = r_model_state.world;

	if (!mod) {
		Com_Print("No map loaded\n");
		return;
	}

	char path[MAX_QPATH];
	g_snprintf(path, sizeof(path), "materials/%s.mat", Basename(mod->media.name));

	cm_material_t **materials = Mem_Malloc(sizeof(cm_material_t *) * mod->bsp->cm->num_materials);

	cm_material_t **c = mod->bsp->cm->materials;
	for (size_t i = 0; i < mod->bsp->cm->num_materials; i++, c++) {

		const r_material_t *mat = R_LoadMaterial((*c)->diffuse);
		if (mat) {
			materials[i] = mat->cm;
		} else {
			Com_Debug(DEBUG_RENDERER, "Failed to resolve renderer material %s\n", (*c)->diffuse);
		}
	}

	Cm_WriteMaterials(path, (const cm_material_t **) materials, mod->bsp->cm->num_materials);
	Com_Print("Saved %s\n", path);

	Mem_Free(materials);
}

/**
 * @brief
 */
void R_InitMaterials(void) {

	Cmd_Add("r_save_materials", R_SaveMaterials_f, CMD_RENDERER, "Write all of the loaded map materials to the disk.");

	r_material_state.vertex_len = INITIAL_VERTEX_COUNT;
	r_material_state.element_len = INITIAL_VERTEX_COUNT;

	r_material_state.vertex_array = g_array_sized_new(false, true, sizeof(r_material_interleave_vertex_t),
	                                r_material_state.vertex_len);
	R_CreateInterleaveBuffer(&r_material_state.vertex_buffer, sizeof(r_material_interleave_vertex_t),
	                         r_material_buffer_layout, GL_DYNAMIC_DRAW, sizeof(r_material_interleave_vertex_t) * r_material_state.vertex_len, NULL);

	r_material_state.element_array = g_array_sized_new(false, false, sizeof(u16vec_t), r_material_state.element_len);

	Matrix4x4_CreateIdentity(&r_texture_matrix);
}

/**
 * @brief
 */
void R_ShutdownMaterials(void) {

	g_array_free(r_material_state.vertex_array, true);

	R_DestroyBuffer(&r_material_state.vertex_buffer);

	g_array_free(r_material_state.element_array, true);
}
