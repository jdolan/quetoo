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
		R_BindLightmapTexture(surf->lightmap->texnum);

		if (stage->cm->flags & STAGE_LIGHTING) { // hardware lighting

			R_EnableLighting(program_default, true);

			if (r_state.lighting_enabled) {
				R_BindDeluxemapTexture(surf->deluxemap->texnum);

				R_UseMaterial(stage->material);

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

	// for terrain, enable the color array
	if (stage->cm->flags & (STAGE_TERRAIN | STAGE_DIRTMAP)) {
		R_EnableColorArray(true);
	} else {
		R_EnableColorArray(false);

		// resolve the shade color
		if (stage->cm->flags & STAGE_COLOR) { // explicit
			VectorCopy(stage->cm->color, color);
		} else if (stage->cm->flags & STAGE_ENVMAP) { // implicit
			VectorCopy(surf->texinfo->material->diffuse->color, color);
		} else {
			VectorSet(color, 1.0, 1.0, 1.0);
		}

		// modulate the alpha value for pulses
		if (stage->cm->flags & STAGE_PULSE) {
			R_EnableFog(false); // disable fog, since it also sets alpha
			color[3] = stage->pulse.dhz;
		} else {
			R_EnableFog(true); // ensure fog is available
			color[3] = 1.0;
		}

		R_Color(color);
	}
}

static uint32_t r_material_vertex_count, r_material_index_count;

/**
 * @brief Render the specified stage for the surface.
 */
static void R_DrawBspSurfaceMaterialStage(r_bsp_surface_t *surf, const r_stage_t *stage) {
	int32_t i;

	// expand array if we're gonna eat it
	if (r_material_state.vertex_len <= (r_material_vertex_count + surf->num_edges)) {
		r_material_state.vertex_len *= 2;
		r_material_state.vertex_array = g_array_set_size(r_material_state.vertex_array, r_material_state.vertex_len);
		Com_Debug(DEBUG_RENDERER, "Expanded material vertex array to %u\n", r_material_state.vertex_len);
	}

	for (i = 0; i < surf->num_edges; i++) {

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

		r_bsp_surface_t *surf = surfs->surfaces[i];

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

		vec_t j = -10.0;
		for (const r_stage_t *s = m->stages; s; s = s->next, j -= 10.0) {

			if (!(s->cm->flags & STAGE_DIFFUSE)) {
				continue;
			}

			R_PolygonOffset(-0.125, j); // increase depth offset for each stage

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

	// some stages will manipulate texcoords

	r_stage_t *s = m->stages;
	for (vec_t j = -1.0; s; s = s->next, j--) {

		if (!(s->cm->flags & STAGE_DIFFUSE)) {
			continue;
		}

		R_UpdateMaterialStage(m, s);

		R_PolygonOffset(j, 1.0); // increase depth offset for each stage

		R_SetStageState(NULL, s);

		R_DrawArrays(GL_TRIANGLES, offset, count);
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

	Cm_UnrefMaterial((cm_material_t *) mat->cm);
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
 * @brief Loads the r_material_t from the specified cm_material_t. Optionally unreferences
 * the cm reference we passed into it if you don't intend on using it again.
 */
r_material_t *R_ConvertMaterial(cm_material_t *cm, const _Bool unref) {
	r_material_t *mat;

	if (!cm || !cm->diffuse[0]) {
		Com_Error(ERR_DROP, "NULL diffuse name\n");
	}

	if (!(mat = (r_material_t *) R_FindMedia(cm->key))) {
		mat = (r_material_t *) R_AllocMedia(cm->key, sizeof(r_material_t), MEDIA_MATERIAL);

		mat->cm = cm;
		
		mat->media.Register = R_RegisterMaterial;
		mat->media.Free = R_FreeMaterial;

		mat->diffuse = R_LoadImage(cm->diffuse, IT_DIFFUSE);
		if (mat->diffuse->type == IT_DIFFUSE) {
			R_LoadNormalmap(mat, cm->base);
			if (mat->normalmap) {
				R_LoadSpecularmap(mat, cm->base);
			}
		}

		R_RegisterMedia((r_media_t *) mat);
		Cm_RefMaterial(cm);
	}

	if (unref) {
		Cm_UnrefMaterial(cm);
	}

	return mat;
}

/**
 * @brief Loads the r_material_t from the specified texture and unreferences the cm_ it loads once.
 */
r_material_t *R_LoadMaterial(const char *name) {

	return R_ConvertMaterial(Cm_LoadMaterial(name), true);
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
static int32_t R_ParseStage(r_stage_t *s, cm_stage_t *cm) {

	s->cm = cm;

	if (*cm->image) {

		if (cm->flags & STAGE_TEXTURE) {
			s->image = R_LoadImage(cm->image, IT_DIFFUSE);

			if (s->image->type == IT_NULL) {
				Com_Warn("Failed to resolve texture: %s\n", cm->image);
				return -1;
			}
		} else if (cm->flags & STAGE_ENVMAP) {
			s->image = R_LoadImage(cm->image, IT_ENVMAP);

			if (s->image->type == IT_NULL) {
				Com_Warn("Failed to resolve envmap: %s\n", cm->image);
				return -1;
			}
		} else if (cm->flags & STAGE_FLARE) {
			s->image = R_LoadImage(cm->image, IT_FLARE);

			if (s->image->type == IT_NULL) {
				Com_Warn("Failed to resolve flare: %s\n", cm->image);
				return -1;
			}
		}
	}

	// load material if lighting
	if (cm->flags & STAGE_LIGHTING) {
		s->material = R_ConvertMaterial(cm->material, false);
	}

	return 0;
}

/**
 * @brief Loads all materials for the specified model. This is accomplished by
 * parsing the material definitions in ${model_name}.mat for mesh models, and
 * materials/${model_name}.mat for BSP models.
 */
void R_LoadMaterials(r_model_t *mod) {
	char path[MAX_QPATH];

	memset(path, 0, sizeof(path));

	// load the materials file for parsing
	if (mod->type == MOD_BSP) {
		g_snprintf(path, sizeof(path), "materials/%s", Basename(mod->media.name));
	} else {
		g_snprintf(path, sizeof(path), "%s", mod->media.name);
	}

	strcat(path, ".mat");

	GArray *materials = Cm_LoadMaterials(path);

	if (!materials) {
		return;
	}

	for (size_t i = 0; i < materials->len; i++) {
		cm_material_t *cm_mat = g_array_index(materials, cm_material_t *, i);
		r_material_t *r_mat = R_ConvertMaterial(cm_mat, true);

		if (!r_mat) {
			continue;
		}

		if (r_mat->diffuse->type == IT_NULL) {
			Com_Warn("Failed to resolve %s\n", cm_mat->diffuse);
			continue;
		}

		if (*cm_mat->normalmap && r_bumpmap->value) {
			r_mat->normalmap = R_LoadImage(cm_mat->normalmap, IT_NORMALMAP);

			if (r_mat->normalmap->type == IT_NULL) {
				Com_Warn("Failed to resolve normalmap: %s\n", cm_mat->normalmap);
				r_mat->normalmap = NULL;
			}
		}

		if (*cm_mat->specularmap && r_bumpmap->value) { // FIXME: r_specular->value?
			r_mat->specularmap = R_LoadImage(cm_mat->specularmap, IT_SPECULARMAP);

			if (r_mat->specularmap->type == IT_NULL) {
				Com_Warn("Failed to resolve specularmap: %s\n", cm_mat->specularmap);
				r_mat->specularmap = NULL;
			}
		}

		for (cm_stage_t *cm_stage = cm_mat->stages; cm_stage; cm_stage = cm_stage->next) {
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

			// append the stage to the chain
			if (!r_mat->stages) {
				r_mat->stages = r_stage;
			} else {
				r_stage_t *ss = r_mat->stages;
				while (ss->next) {
					ss = ss->next;
				}
				ss->next = r_stage;
			}

			r_mat->flags |= cm_stage->flags;
			continue;
		}

		Com_Debug(DEBUG_RENDERER, "Parsed material %s with %d stages\n", r_mat->diffuse->media.name, cm_mat->num_stages);
	}
}

#define MAX_SAVE_MATERIALS 1000

/**
 * @brief Comparator for R_SaveMaterials.
 */
static gint R_SaveMaterials_Compare(gconstpointer m1, gconstpointer m2) {

	const char *d1 = ((r_material_t *) m1)->diffuse->media.name;
	const char *d2 = ((r_material_t *) m2)->diffuse->media.name;

	return g_strcmp0(d1, d2);
}

/**
 * @brief Saves the materials properties for the current map.
 */
void R_SaveMaterials_f(void) {
	const r_model_t *world = r_model_state.world;

	if (!world) {
		Com_Print("No map loaded\n");
		return;
	}

	char filename[MAX_QPATH];
	uint16_t i;

	if (Cmd_Argc() == 2) { // write the requested file
		g_snprintf(filename, sizeof(filename), "materials/%s", Cmd_Argv(1));

		if (!g_str_has_suffix(filename, ".mat")) {
			g_strlcat(filename, ".mat", sizeof(filename));
		}
	} else { // or the next available one

		const char *map = Basename(world->media.name);

		for (i = 0; i < MAX_SAVE_MATERIALS; i++) {
			g_snprintf(filename, sizeof(filename), "materials/%s%03u.mat", map, i);

			if (!Fs_Exists(filename)) {
				break;
			}
		}

		if (i == MAX_SAVE_MATERIALS) {
			Com_Warn("Failed to create %s\n", filename);
			return;
		}
	}

	file_t *file;
	if (!(file = Fs_OpenWrite(filename))) {
		Com_Warn("Failed to write %s\n", filename);
		return;
	}

	GHashTable *materials = g_hash_table_new(g_direct_hash, g_direct_equal);

	const r_bsp_texinfo_t *tex = world->bsp->texinfo;
	for (i = 0; i < world->bsp->num_texinfo; i++, tex++) {
		if (tex->material->normalmap) {
			g_hash_table_insert(materials, tex->material, tex->material);
		}
	}

	GList *list = g_list_sort(g_hash_table_get_keys(materials), R_SaveMaterials_Compare);

	GList *e = list;
	while (e) {
		const r_material_t *m = (r_material_t *) e->data;

		const char *diffuse = m->diffuse->media.name + strlen("textures/");
		const char *normalmap = m->normalmap->media.name + strlen("textures/");

		const char *s = va("{\n"
		                   "\tmaterial %s\n"
		                   "\tnormalmap %s\n"
		                   "\tbump %01.1f\n"
		                   "\thardness %01.1f\n"
		                   "\tspecular %01.1f\n"
		                   "\tparallax %01.1f\n"
		                   "}\n", diffuse, normalmap, m->cm->bump, m->cm->hardness, m->cm->specular, m->cm->parallax);

		Fs_Write(file, s, 1, strlen(s));
		e = e->next;
	}

	g_list_free(list);
	g_hash_table_destroy(materials);

	Fs_Close(file);

	Com_Print("Saved %s\n", Basename(filename));
}

/**
 * @brief
 */
void R_InitMaterials(void) {

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
