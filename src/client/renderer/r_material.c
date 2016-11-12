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
	if (s->flags & STAGE_PULSE) {
		s->pulse.dhz = (sin(r_view.time * s->pulse.hz * 0.00628) + 1.0) / 2.0;
	}

	if (s->flags & STAGE_STRETCH) {
		s->stretch.dhz = (sin(r_view.time * s->stretch.hz * 0.00628) + 1.0) / 2.0;
		s->stretch.damp = 1.5 - s->stretch.dhz * s->stretch.amp;
	}

	if (s->flags & STAGE_ROTATE) {
		s->rotate.deg = r_view.time * s->rotate.hz * 0.360;
	}

	if (s->flags & STAGE_SCROLL_S) {
		s->scroll.ds = s->scroll.s * r_view.time / 1000.0;
	}

	if (s->flags & STAGE_SCROLL_T) {
		s->scroll.dt = s->scroll.t * r_view.time / 1000.0;
	}

	if (s->flags & STAGE_ANIM) {
		if (s->anim.fps) {
			if (r_view.time >= s->anim.dtime) { // change frames
				s->anim.dtime = r_view.time + (1000 / s->anim.fps);
				s->image = s->anim.frames[++s->anim.dframe % s->anim.num_frames];
			}
		} else if (r_view.current_entity) {
			s->image = s->anim.frames[r_view.current_entity->frame % s->anim.num_frames];
		}
	}
}

/**
 * @brief Materials "think" every few milliseconds to advance animations.
 */
static void R_UpdateMaterial(r_material_t *m) {

	if (!r_view.current_entity) {
		if (r_view.time - m->time < UPDATE_THRESHOLD) {
			return;
		}
	}

	m->time = r_view.time;
}

/**
 * @brief Manages state for stages supporting static, dynamic, and per-pixel lighting.
 */
static void R_StageLighting(const r_bsp_surface_t *surf, const r_stage_t *stage) {

	if (!surf) { // mesh materials don't support per-stage lighting
		return;
	}

	// if the surface has a lightmap, and the stage specifies lighting..

	if ((surf->flags & R_SURF_LIGHTMAP) && (stage->flags & (STAGE_LIGHTMAP | STAGE_LIGHTING))) {

		R_EnableTexture(&texunit_lightmap, true);
		R_BindLightmapTexture(surf->lightmap->texnum);

		if (stage->flags & STAGE_LIGHTING) { // hardware lighting

			R_EnableLighting(r_state.default_program, true);

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

		R_EnableTexture(&texunit_lightmap, false);
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

	if (!(stage->flags & STAGE_TEXTURE_MATRIX)) {

		if (!identity) {
			Matrix4x4_CreateIdentity(&texture_matrix);
		}

		identity = true;
		return;
	}

	Matrix4x4_CreateIdentity(&texture_matrix);

	if (surf) { // for BSP surfaces, add stretch and rotate

		s = surf->st_center[0] / surf->texinfo->material->diffuse->width;
		t = surf->st_center[1] / surf->texinfo->material->diffuse->height;

		if (stage->flags & STAGE_STRETCH) {
			Matrix4x4_ConcatTranslate(&texture_matrix, -s, -t, 0.0);
			Matrix4x4_ConcatScale3(&texture_matrix, stage->stretch.damp, stage->stretch.damp, 1.0);
			Matrix4x4_ConcatTranslate(&texture_matrix, -s, -t, 0.0);
		}

		if (stage->flags & STAGE_ROTATE) {
			Matrix4x4_ConcatTranslate(&texture_matrix, -s, -t, 0.0);
			Matrix4x4_ConcatRotate(&texture_matrix, stage->rotate.deg, 0.0, 0.0, 1.0);
			Matrix4x4_ConcatTranslate(&texture_matrix, -s, -t, 0.0);
		}
	}

	if (stage->flags & STAGE_SCALE_S) {
		Matrix4x4_ConcatScale3(&texture_matrix, stage->scale.s, 1.0, 1.0);
	}

	if (stage->flags & STAGE_SCALE_T) {
		Matrix4x4_ConcatScale3(&texture_matrix, 1.0, stage->scale.t, 1.0);
	}

	if (stage->flags & STAGE_SCROLL_S) {
		Matrix4x4_ConcatTranslate(&texture_matrix, stage->scroll.ds, 0.0, 0.0);
	}

	if (stage->flags & STAGE_SCROLL_T) {
		Matrix4x4_ConcatTranslate(&texture_matrix, 0.0, stage->scroll.dt, 0.0);
	}

	identity = false;
}

/**
 * @brief Generates a single texture coordinate for the specified stage and vertex.
 */
static inline void R_StageTexCoord(const r_stage_t *stage, const vec3_t v, const vec2_t in,
                                   vec2_t out) {

	vec3_t tmp;

	if (stage->flags & STAGE_ENVMAP) { // generate texcoords

		VectorSubtract(v, r_view.origin, tmp);
		VectorNormalize(tmp);

		out[0] = tmp[0];
		out[1] = tmp[1];
	} else { // or use the ones we were given
		out[0] = in[0];
		out[1] = in[1];
	}

	Matrix4x4_Transform2(&texture_matrix, out, out);
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

	if (stage->flags & STAGE_TERRAIN) {

		if (stage->flags & STAGE_COLOR) { // honor stage color
			ColorDecompose3(stage->color, color);
		} else
			// or use white
		{
			VectorSet(color, 255, 255, 255);
		}

		// resolve alpha for vert based on z axis height
		if (v[2] < stage->terrain.floor) {
			a = 0.0;
		} else if (v[2] > stage->terrain.ceil) {
			a = 1.0;
		} else {
			a = (v[2] - stage->terrain.floor) / stage->terrain.height;
		}

		color[3] = (u8vec_t) (a * 255.0);
	} else if (stage->flags & STAGE_DIRTMAP) {

		// resolve dirtmap based on vertex position
		const uint16_t index = (uint16_t) (v[0] + v[1]) % NUM_DIRTMAP_ENTRIES;
		if (stage->flags & STAGE_COLOR) { // honor stage color
			ColorDecompose3(stage->color, color);
		} else
			// or use white
		{
			VectorSet(color, 255, 255, 255);
		}

		color[3] = (u8vec_t) (dirtmap[index] * stage->dirt.intensity);
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
	R_BindTexture(stage->image->texnum);

	// resolve all static, dynamic, and per-pixel lighting
	R_StageLighting(surf, stage);

	// set the blend function, ensuring a sane default
	if (stage->flags & STAGE_BLEND) {
		R_BlendFunc(stage->blend.src, stage->blend.dest);
	} else {
		R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	// for terrain, enable the color array
	if (stage->flags & (STAGE_TERRAIN | STAGE_DIRTMAP)) {
		R_EnableColorArray(true);
	} else {
		R_EnableColorArray(false);

		// resolve the shade color
		if (stage->flags & STAGE_COLOR) { // explicit
			VectorCopy(stage->color, color);
		} else if (stage->flags & STAGE_ENVMAP) { // implicit
			VectorCopy(surf->texinfo->material->diffuse->color, color);
		} else {
			VectorSet(color, 1.0, 1.0, 1.0);
		}

		// modulate the alpha value for pulses
		if (stage->flags & STAGE_PULSE) {
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
		Com_Debug("Expanded material vertex array to %u\n", r_material_state.vertex_len);
	}

	for (i = 0; i < surf->num_edges; i++) {

		const vec_t *v = &r_model_state.world->bsp->verts[surf->elements[i]][0];
		const vec_t *st = &r_model_state.world->bsp->texcoords[surf->elements[i]][0];

		r_material_interleave_vertex_t *vertex = &VERTEX_ARRAY_INDEX(r_material_vertex_count + i);

		R_StageVertex(surf, stage, v, &vertex->vertex[0]);

		R_StageTexCoord(stage, v, st, &vertex->diffuse[0]);

		if (texunit_lightmap.enabled) { // lightmap texcoords
			st = &r_model_state.world->bsp->lightmap_texcoords[surf->elements[i]][0];
			Vector2Set(vertex->lightmap, (u16vec_t) (st[0] * USHRT_MAX), (u16vec_t) (st[1] * USHRT_MAX));
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
		Com_Debug("Expanded material index array to %u\n", r_material_state.element_len);
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

			if (!(s->flags & STAGE_DIFFUSE)) {
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

	R_EnableTexture(&texunit_lightmap, true);

	R_EnableLighting(r_state.default_program, true);

	R_EnableColorArray(true);

	R_ResetArrayState();

	R_BindAttributeInterleaveBuffer(&r_material_state.vertex_buffer);

	R_EnableColorArray(false);

	R_EnableLighting(NULL, false);

	R_EnableTexture(&texunit_lightmap, false);

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

			if (!(s->flags & STAGE_DIFFUSE)) {
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

	Matrix4x4_CreateIdentity(&texture_matrix);

	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	R_EnableColorArray(false);

	R_EnableTexture(&texunit_lightmap, false);

	R_EnableLighting(r_state.default_program, true);

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

		if (!(s->flags & STAGE_DIFFUSE)) {
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

	Matrix4x4_CreateIdentity(&texture_matrix);

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
		for (i = 0; i < s->anim.num_frames; i++) {
			R_RegisterDependency(self, (r_media_t *) s->anim.frames[i]);
		}

		R_RegisterDependency(self, (r_media_t *) s->material);
		s = s->next;
	}
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
 * @brief Loads the r_material_t with the specified diffuse texture.
 */
r_material_t *R_LoadMaterial(const char *diffuse) {
	r_material_t *mat;
	char name[MAX_QPATH], base[MAX_QPATH], key[MAX_QPATH];

	if (!diffuse || !diffuse[0]) {
		Com_Error(ERR_DROP, "NULL diffuse name\n");
	}

	StripExtension(diffuse, name);

	g_strlcpy(base, name, sizeof(base));

	if (g_str_has_suffix(base, "_d")) {
		base[strlen(base) - 2] = '\0';
	}

	g_snprintf(key, sizeof(key), "%s_mat", base);

	if (!(mat = (r_material_t *) R_FindMedia(key))) {
		mat = (r_material_t *) R_AllocMedia(key, sizeof(r_material_t));

		mat->media.Register = R_RegisterMaterial;

		mat->diffuse = R_LoadImage(name, IT_DIFFUSE);
		if (mat->diffuse->type == IT_DIFFUSE) {
			R_LoadNormalmap(mat, base);
			if (mat->normalmap) {
				R_LoadSpecularmap(mat, base);
			}
		}

		mat->bump = DEFAULT_BUMP;
		mat->hardness = DEFAULT_HARDNESS;
		mat->parallax = DEFAULT_PARALLAX;
		mat->specular = DEFAULT_SPECULAR;

		R_RegisterMedia((r_media_t *) mat);
	}

	return mat;
}

/**
 * @brief
 */
static inline GLenum R_ConstByName(const char *c) {

	if (!g_strcmp0(c, "GL_ONE")) {
		return GL_ONE;
	}
	if (!g_strcmp0(c, "GL_ZERO")) {
		return GL_ZERO;
	}
	if (!g_strcmp0(c, "GL_SRC_ALPHA")) {
		return GL_SRC_ALPHA;
	}
	if (!g_strcmp0(c, "GL_ONE_MINUS_SRC_ALPHA")) {
		return GL_ONE_MINUS_SRC_ALPHA;
	}
	if (!g_strcmp0(c, "GL_SRC_COLOR")) {
		return GL_SRC_COLOR;
	}
	if (!g_strcmp0(c, "GL_DST_COLOR")) {
		return GL_DST_COLOR;
	}
	if (!g_strcmp0(c, "GL_ONE_MINUS_SRC_COLOR")) {
		return GL_ONE_MINUS_SRC_COLOR;
	}

	// ...
	Com_Warn("Failed to resolve: %s\n", c);
	return GL_INVALID_ENUM;
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

	s->anim.frames = Mem_LinkMalloc(s->anim.num_frames * sizeof(r_image_t *), s);
	s->anim.frames[0] = s->image;

	// now load the rest
	name[len - 1] = '\0';
	for (j = 1, i = i + 1; j < s->anim.num_frames; j++, i++) {
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
static int32_t R_ParseStage(r_stage_t *s, const char **buffer) {
	int32_t i;

	while (true) {

		const char *c = ParseToken(buffer);

		if (*c == '\0') {
			break;
		}

		if (!g_strcmp0(c, "texture") || !g_strcmp0(c, "diffuse")) {

			c = ParseToken(buffer);
			if (*c == '#') {
				s->image = R_LoadImage(++c, IT_DIFFUSE);
			} else {
				s->image = R_LoadImage(va("textures/%s", c), IT_DIFFUSE);
			}

			if (s->image->type == IT_NULL) {
				Com_Warn("Failed to resolve texture: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_TEXTURE;
			continue;
		}

		if (!g_strcmp0(c, "envmap")) {

			c = ParseToken(buffer);
			i = (int32_t) strtol(c, NULL, 0);

			if (*c == '#') {
				s->image = R_LoadImage(++c, IT_ENVMAP);
			} else if (*c == '0' || i > 0) {
				s->image = R_LoadImage(va("envmaps/envmap_%d", i), IT_ENVMAP);
			} else {
				s->image = R_LoadImage(va("envmaps/%s", c), IT_ENVMAP);
			}

			if (s->image->type == IT_NULL) {
				Com_Warn("Failed to resolve envmap: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_ENVMAP;
			continue;
		}

		if (!g_strcmp0(c, "blend")) {

			c = ParseToken(buffer);
			s->blend.src = R_ConstByName(c);

			if (s->blend.src == GL_INVALID_ENUM) {
				Com_Warn("Failed to resolve blend src: %s\n", c);
				return -1;
			}

			c = ParseToken(buffer);
			s->blend.dest = R_ConstByName(c);

			if (s->blend.dest == GL_INVALID_ENUM) {
				Com_Warn("Failed to resolve blend dest: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_BLEND;
			continue;
		}

		if (!g_strcmp0(c, "color")) {

			for (i = 0; i < 3; i++) {

				c = ParseToken(buffer);
				s->color[i] = strtod(c, NULL);

				if (s->color[i] < 0.0 || s->color[i] > 1.0) {
					Com_Warn("Failed to resolve color: %s\n", c);
					return -1;
				}
			}

			s->flags |= STAGE_COLOR;
			continue;
		}

		if (!g_strcmp0(c, "pulse")) {

			c = ParseToken(buffer);
			s->pulse.hz = strtod(c, NULL);

			if (s->pulse.hz == 0.0) {
				Com_Warn("Failed to resolve frequency: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_PULSE;
			continue;
		}

		if (!g_strcmp0(c, "stretch")) {

			c = ParseToken(buffer);
			s->stretch.amp = strtod(c, NULL);

			if (s->stretch.amp == 0.0) {
				Com_Warn("Failed to resolve amplitude: %s\n", c);
				return -1;
			}

			c = ParseToken(buffer);
			s->stretch.hz = strtod(c, NULL);

			if (s->stretch.hz == 0.0) {
				Com_Warn("Failed to resolve frequency: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_STRETCH;
			continue;
		}

		if (!g_strcmp0(c, "rotate")) {

			c = ParseToken(buffer);
			s->rotate.hz = strtod(c, NULL);

			if (s->rotate.hz == 0.0) {
				Com_Warn("Failed to resolve rotate: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_ROTATE;
			continue;
		}

		if (!g_strcmp0(c, "scroll.s")) {

			c = ParseToken(buffer);
			s->scroll.s = strtod(c, NULL);

			if (s->scroll.s == 0.0) {
				Com_Warn("Failed to resolve scroll.s: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_SCROLL_S;
			continue;
		}

		if (!g_strcmp0(c, "scroll.t")) {

			c = ParseToken(buffer);
			s->scroll.t = strtod(c, NULL);

			if (s->scroll.t == 0.0) {
				Com_Warn("Failed to resolve scroll.t: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_SCROLL_T;
			continue;
		}

		if (!g_strcmp0(c, "scale.s")) {

			c = ParseToken(buffer);
			s->scale.s = strtod(c, NULL);

			if (s->scale.s == 0.0) {
				Com_Warn("Failed to resolve scale.s: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_SCALE_S;
			continue;
		}

		if (!g_strcmp0(c, "scale.t")) {

			c = ParseToken(buffer);
			s->scale.t = strtod(c, NULL);

			if (s->scale.t == 0.0) {
				Com_Warn("Failed to resolve scale.t: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_SCALE_T;
			continue;
		}

		if (!g_strcmp0(c, "terrain")) {

			c = ParseToken(buffer);
			s->terrain.floor = strtod(c, NULL);

			c = ParseToken(buffer);
			s->terrain.ceil = strtod(c, NULL);

			if (s->terrain.ceil <= s->terrain.floor) {
				Com_Warn("Invalid terrain ceiling and floor values for %s\n",
				         (s->image ? s->image->media.name : "NULL"));
				return -1;
			}

			s->terrain.height = s->terrain.ceil - s->terrain.floor;
			s->flags |= STAGE_TERRAIN;
			continue;
		}

		if (!g_strcmp0(c, "dirtmap")) {

			c = ParseToken(buffer);
			s->dirt.intensity = strtod(c, NULL);

			if (s->dirt.intensity <= 0.0 || s->dirt.intensity > 1.0) {
				Com_Warn("Invalid dirtmap intensity for %s\n",
				         (s->image ? s->image->media.name : "NULL"));
				return -1;
			}

			s->flags |= STAGE_DIRTMAP;
			continue;
		}

		if (!g_strcmp0(c, "anim")) {

			c = ParseToken(buffer);
			s->anim.num_frames = strtoul(c, NULL, 0);

			if (s->anim.num_frames < 1) {
				Com_Warn("Invalid number of anim frames for %s\n",
				         (s->image ? s->image->media.name : "NULL"));
				return -1;
			}

			c = ParseToken(buffer);
			s->anim.fps = strtod(c, NULL);

			if (s->anim.fps < 0.0) {
				Com_Warn("Invalid anim fps for %s\n",
				         (s->image ? s->image->media.name : "NULL"));
				return -1;
			}

			// the frame images are loaded once the stage is parsed completely

			s->flags |= STAGE_ANIM;
			continue;
		}

		if (!g_strcmp0(c, "lightmap")) {
			s->flags |= STAGE_LIGHTMAP;
			continue;
		}

		if (!g_strcmp0(c, "flare")) {

			c = ParseToken(buffer);
			i = (int32_t) strtol(c, NULL, 0);

			if (*c == '#') {
				s->image = R_LoadImage(++c, IT_FLARE);
			} else if (*c == '0' || i > 0) {
				s->image = R_LoadImage(va("flares/flare_%d", i), IT_FLARE);
			} else {
				s->image = R_LoadImage(va("flares/%s", c), IT_FLARE);
			}

			if (s->image->type == IT_NULL) {
				Com_Warn("Failed to resolve flare: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_FLARE;
			continue;
		}

		if (*c == '}') {

			// a texture, or envmap means render it
			if (s->flags & (STAGE_TEXTURE | STAGE_ENVMAP)) {
				s->flags |= STAGE_DIFFUSE;

				// a terrain blend or dirtmap means light it
				if (s->flags & (STAGE_TERRAIN | STAGE_DIRTMAP)) {
					s->material = R_LoadMaterial(s->image->media.name);
					s->flags |= STAGE_LIGHTING;
				}
			}

			Com_Debug("Parsed stage\n"
			          "  flags: %d\n"
			          "  texture: %s\n"
			          "   -> material: %s\n"
			          "  blend: %d %d\n"
			          "  color: %3f %3f %3f\n"
			          "  pulse: %3f\n"
			          "  stretch: %3f %3f\n"
			          "  rotate: %3f\n"
			          "  scroll.s: %3f\n"
			          "  scroll.t: %3f\n"
			          "  scale.s: %3f\n"
			          "  scale.t: %3f\n"
			          "  terrain.floor: %5f\n"
			          "  terrain.ceil: %5f\n"
			          "  anim.num_frames: %d\n"
			          "  anim.fps: %3f\n", s->flags, (s->image ? s->image->media.name : "NULL"),
			          (s->material ? s->material->diffuse->media.name : "NULL"), s->blend.src,
			          s->blend.dest, s->color[0], s->color[1], s->color[2], s->pulse.hz,
			          s->stretch.amp, s->stretch.hz, s->rotate.hz, s->scroll.s, s->scroll.t,
			          s->scale.s, s->scale.t, s->terrain.floor, s->terrain.ceil, s->anim.num_frames,
			          s->anim.fps);

			return 0;
		}
	}

	Com_Warn("Malformed stage\n");
	return -1;
}

/**
 * @brief Loads all materials for the specified model. This is accomplished by
 * parsing the material definitions in ${model_name}.mat for mesh models, and
 * materials/${model_name}.mat for BSP models.
 */
void R_LoadMaterials(const r_model_t *mod) {
	char path[MAX_QPATH];
	void *buf;

	memset(path, 0, sizeof(path));

	// load the materials file for parsing
	if (mod->type == MOD_BSP) {
		g_snprintf(path, sizeof(path), "materials/%s", Basename(mod->media.name));
	} else {
		g_snprintf(path, sizeof(path), "%s", mod->media.name);
	}

	strcat(path, ".mat");

	if (Fs_Load(path, &buf) == -1) {
		Com_Debug("Couldn't load %s\n", path);
		return;
	}

	const char *buffer = (char *) buf;

	_Bool in_material = false;
	r_material_t *m = NULL;

	while (true) {

		const char *c = ParseToken(&buffer);

		if (*c == '\0') {
			break;
		}

		if (*c == '{' && !in_material) {
			in_material = true;
			continue;
		}

		if (!g_strcmp0(c, "material")) {
			c = ParseToken(&buffer);
			if (*c == '#') {
				m = R_LoadMaterial(++c);
			} else {
				m = R_LoadMaterial(va("textures/%s", c));
			}

			if (m->diffuse->type == IT_NULL) {
				Com_Warn("Failed to resolve %s\n", c);
				m = NULL;
			}

			continue;
		}

		if (!m) {
			continue;
		}

		if (!g_strcmp0(c, "normalmap") && r_bumpmap->value) {
			c = ParseToken(&buffer);
			if (*c == '#') {
				m->normalmap = R_LoadImage(++c, IT_NORMALMAP);
			} else {
				m->normalmap = R_LoadImage(va("textures/%s", c), IT_NORMALMAP);
			}

			if (m->normalmap->type == IT_NULL) {
				Com_Warn("Failed to resolve normalmap: %s\n", c);
				m->normalmap = NULL;
			}
		}

		if (!g_strcmp0(c, "specularmap") && r_bumpmap->value) {
			c = ParseToken(&buffer);
			if (*c == '#') {
				m->specularmap = R_LoadImage(++c, IT_SPECULARMAP);
			} else {
				m->specularmap = R_LoadImage(va("textures/%s", c), IT_SPECULARMAP);
			}

			if (m->specularmap->type == IT_NULL) {
				Com_Warn("Failed to resolve specularmap: %s\n", c);
				m->specularmap = NULL;
			}
		}

		if (!g_strcmp0(c, "bump")) {

			m->bump = strtod(ParseToken(&buffer), NULL);

			if (m->bump < 0.0) {
				Com_Warn("Invalid bump value for %s\n", m->diffuse->media.name);
				m->bump = DEFAULT_BUMP;
			}
		}

		if (!g_strcmp0(c, "parallax")) {

			m->parallax = strtod(ParseToken(&buffer), NULL);

			if (m->parallax < 0.0) {
				Com_Warn("Invalid parallax value for %s\n", m->diffuse->media.name);
				m->parallax = DEFAULT_PARALLAX;
			}
		}

		if (!g_strcmp0(c, "hardness")) {

			m->hardness = strtod(ParseToken(&buffer), NULL);

			if (m->hardness < 0.0) {
				Com_Warn("Invalid hardness value for %s\n", m->diffuse->media.name);
				m->hardness = DEFAULT_HARDNESS;
			}
		}

		if (!g_strcmp0(c, "specular")) {
			m->specular = strtod(ParseToken(&buffer), NULL);

			if (m->specular < 0.0) {
				Com_Warn("Invalid specular value for %s\n", m->diffuse->media.name);
				m->specular = DEFAULT_SPECULAR;
			}
		}

		if (*c == '{' && in_material) {

			r_stage_t *s = (r_stage_t *) Mem_LinkMalloc(sizeof(*s), m);

			if (R_ParseStage(s, &buffer) == -1) {
				Mem_Free(s);
				continue;
			}

			// load animation frame images
			if (s->flags & STAGE_ANIM) {
				if (R_LoadStageFrames(s) == -1) {
					Mem_Free(s);
					continue;
				}
			}

			// append the stage to the chain
			if (!m->stages) {
				m->stages = s;
			} else {
				r_stage_t *ss = m->stages;
				while (ss->next) {
					ss = ss->next;
				}
				ss->next = s;
			}

			m->flags |= s->flags;
			m->num_stages++;
			continue;
		}

		if (*c == '}' && in_material) {
			Com_Debug("Parsed material %s with %d stages\n", m->diffuse->media.name, m->num_stages);
			in_material = false;
			m = NULL;
		}
	}

	Fs_Free(buf);
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
		                   "}\n", diffuse, normalmap, m->bump, m->hardness, m->specular, m->parallax);

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
}

/**
 * @brief
 */
void R_ShutdownMaterials(void) {

	g_array_free(r_material_state.vertex_array, true);
	R_DestroyBuffer(&r_material_state.vertex_buffer);

	g_array_free(r_material_state.element_array, true);
}