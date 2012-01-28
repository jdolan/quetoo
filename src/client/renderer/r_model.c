/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

r_model_t r_models[MAX_MOD_KNOWN];
unsigned short r_num_models;

r_model_t r_inline_models[MAX_BSP_MODELS];

r_model_t *r_world_model;
r_model_t *r_load_model;

typedef struct r_hunk_s {
	byte *base;
	size_t size;
	ptrdiff_t offset;
} r_hunk_t;

static r_hunk_t r_hunk;

/*
 * R_HunkBegin
 */
static void *R_HunkBegin() {
	return r_hunk.base + r_hunk.offset;
}

/*
 * R_HunkEnd
 */
static size_t R_HunkEnd(void *buf) {
	return r_hunk.base + r_hunk.offset - (byte *) buf;
}

/*
 * R_HunkAlloc
 */
void *R_HunkAlloc(size_t size) {
	byte *b;

	if (r_hunk.offset + size > r_hunk.size) {
		Com_Error(ERR_DROP, "R_HunkAlloc: Overflow.");
	}

	b = r_hunk.base + r_hunk.offset;
	r_hunk.offset += size;

	return b;
}

/*
 * R_AllocVertexArrays
 */
void R_AllocVertexArrays(r_model_t *mod) {
	int i, j;
	int v, st, t, c;

	mod->num_verts = 0;

	// first resolve the vertex count
	if (mod->type == mod_bsp) {
		r_bsp_surface_t *surf;

		for (i = 0, surf = mod->surfaces; i < mod->num_surfaces; i++, surf++) {
			for (j = 0; j < surf->num_edges; j++)
				mod->num_verts++;
		}
	} else if (mod->type == mod_md3) {
		const r_md3_t *md3 = (r_md3_t *) mod->extra_data;
		const r_md3_mesh_t *mesh = md3->meshes;

		for (i = 0; i < md3->num_meshes; i++, mesh++)
			mod->num_verts += mesh->num_tris * 3;
	} else if (mod->type == mod_obj) {
		const r_obj_t *obj = (r_obj_t *) mod->extra_data;
		mod->num_verts = obj->num_tris * 3;
	}

	v = mod->num_verts * 3 * sizeof(GLfloat);
	st = mod->num_verts * 2 * sizeof(GLfloat);
	t = mod->num_verts * 4 * sizeof(GLfloat);
	c = mod->num_verts * 4 * sizeof(GLfloat);

	// allocate the arrays, static models get verts and normals
	if (mod->num_frames < 2) {
		mod->verts = (GLfloat *) R_HunkAlloc(v);
		mod->normals = (GLfloat *) R_HunkAlloc(v);
	}

	// all models get texcoords
	mod->texcoords = (GLfloat *) R_HunkAlloc(st);

	if (mod->type != mod_bsp)
		return;

	// and bsp models get lightmap texcoords, tangents, and colors
	mod->lmtexcoords = (GLfloat *) R_HunkAlloc(st);
	mod->tangents = (GLfloat *) R_HunkAlloc(t);
	mod->colors = (GLfloat *) R_HunkAlloc(c);
}

/*
 * R_LoadVertexBuffers
 */
static void R_LoadVertexBuffers(r_model_t *mod) {
	int v, st, t, c;

	if (!qglGenBuffers)
		return;

	if (mod->num_frames > 1) // animated models don't use VBO
		return;

	v = mod->num_verts * 3 * sizeof(GLfloat);
	st = mod->num_verts * 2 * sizeof(GLfloat);
	t = mod->num_verts * 4 * sizeof(GLfloat);
	c = mod->num_verts * 4 * sizeof(GLfloat);

	// load the vertex buffer objects
	qglGenBuffers(1, &mod->vertex_buffer);
	qglBindBuffer(GL_ARRAY_BUFFER, mod->vertex_buffer);
	qglBufferData(GL_ARRAY_BUFFER, v, mod->verts, GL_STATIC_DRAW);

	qglGenBuffers(1, &mod->texcoord_buffer);
	qglBindBuffer(GL_ARRAY_BUFFER, mod->texcoord_buffer);
	qglBufferData(GL_ARRAY_BUFFER, st, mod->texcoords, GL_STATIC_DRAW);

	qglGenBuffers(1, &mod->normal_buffer);
	qglBindBuffer(GL_ARRAY_BUFFER, mod->normal_buffer);
	qglBufferData(GL_ARRAY_BUFFER, v, mod->normals, GL_STATIC_DRAW);

	qglBindBuffer(GL_ARRAY_BUFFER, 0);

	if (mod->type != mod_bsp)
		return;

	// including lightmap coords, tangents, and colors for bsp
	qglGenBuffers(1, &mod->lmtexcoord_buffer);
	qglBindBuffer(GL_ARRAY_BUFFER, mod->lmtexcoord_buffer);
	qglBufferData(GL_ARRAY_BUFFER, st, mod->lmtexcoords, GL_STATIC_DRAW);

	qglGenBuffers(1, &mod->tangent_buffer);
	qglBindBuffer(GL_ARRAY_BUFFER, mod->tangent_buffer);
	qglBufferData(GL_ARRAY_BUFFER, t, mod->tangents, GL_STATIC_DRAW);

	qglGenBuffers(1, &mod->color_buffer);
	qglBindBuffer(GL_ARRAY_BUFFER, mod->color_buffer);
	qglBufferData(GL_ARRAY_BUFFER, c, mod->colors, GL_STATIC_DRAW);

	qglBindBuffer(GL_ARRAY_BUFFER, 0);
}

typedef struct r_model_format_s {
	const char *name;
	void (*load)(r_model_t *mod, void *buffer);
} r_model_format_t;

static r_model_format_t r_model_formats[] = { { ".obj", R_LoadObjModel }, {
		".md3", R_LoadMd3Model }, { ".bsp", R_LoadBspModel } };

#define NUM_MODEL_FORMATS (sizeof(r_model_formats) / sizeof(r_model_format_t))

/*
 * R_LoadModel
 */
r_model_t *R_LoadModel(const char *name) {
	r_model_format_t *format;
	r_model_t *mod;
	char n[MAX_QPATH];
	void *buf;
	vec3_t tmp;
	unsigned short i;

	if (!name || !name[0]) {
		Com_Error(ERR_DROP, "R_LoadModel: NULL name.");
	}

	// inline models are fetched from a separate array
	if (name[0] == '*') {
		i = atoi(name + 1);
		if (i < 1 || !r_world_model || i >= r_world_model->num_submodels) {
			Com_Error(ERR_DROP, "R_LoadModel: Bad inline model number.");
		}
		return &r_inline_models[i];
	}

	StripExtension(name, n);

	// search the currently loaded models
	for (i = 0, mod = r_models; i < r_num_models; i++, mod++) {
		if (!strcmp(n, mod->name))
			return mod;
	}

	// find a free model slot spot
	for (i = 0, mod = r_models; i < r_num_models; i++, mod++) {
		if (!mod->name[0])
			break; // free spot
	}

	if (i == r_num_models) {
		if (r_num_models == MAX_MOD_KNOWN) {
			Com_Error(ERR_DROP, "R_LoadModel: MAX_MOD_KNOWN reached.");
		}
		r_num_models++;
	}

	// load the file
	format = r_model_formats;
	for (i = 0; i < NUM_MODEL_FORMATS; i++, format++) {

		StripExtension(name, n);
		strcat(n, format->name);

		if (Fs_LoadFile(n, &buf) != -1)
			break;
	}

	if (i == NUM_MODEL_FORMATS) { // not found
		Com_Warn("R_LoadModel: Failed to load %s.\n", name);
		return NULL;
	}

	StripExtension(n, mod->name);

	r_load_model = mod;
	r_load_model->extra_data = R_HunkBegin();

	format->load(mod, buf);

	r_load_model->extra_data_size = R_HunkEnd(r_load_model->extra_data);

	// assemble vertex buffer objects from static arrays
	R_LoadVertexBuffers(mod);

	// calculate an approximate radius from the bounding box
	VectorSubtract(mod->maxs, mod->mins, tmp);
	mod->radius = VectorLength(tmp) / 2.0;

	Fs_FreeFile(buf);

	return mod;
}

/*
 * R_ListModels_f
 */
void R_ListModels_f(void) {
	r_model_t *mod;
	int i, total;

	total = 0;
	Com_Print("Loaded models:\n");

	for (i = 0, mod = r_models; i < r_num_models; i++, mod++) {

		if (!mod->name[0])
			continue;

		Com_Print("%6i: %s\n", mod->num_verts, mod->name);

		total += mod->extra_data_size;
	}

	Com_Print("Total resident: %i\n", total);
}

/*
 * R_HunkStats_f
 */
void R_HunkStats_f(void) {
	Com_Print("Hunk usage: %.2f / %.2f MB\n", r_hunk.offset / 1024.0 / 1024.0,
			r_hunk_mb->value);
}

/*
 * R_FreeModels
 */
static void R_FreeModels(void) {
	int i;

	for (i = 0; i < r_num_models; i++) {
		r_model_t *mod = &r_models[i];

		if (mod->vertex_buffer)
			qglDeleteBuffers(1, &mod->vertex_buffer);
		if (mod->texcoord_buffer)
			qglDeleteBuffers(1, &mod->texcoord_buffer);
		if (mod->lmtexcoord_buffer)
			qglDeleteBuffers(1, &mod->lmtexcoord_buffer);
		if (mod->normal_buffer)
			qglDeleteBuffers(1, &mod->normal_buffer);
	}

	r_world_model = NULL;

	// reset the models array
	memset(r_models, 0, sizeof(r_models));
	r_num_models = 0;

	// reset inline models
	memset(r_inline_models, 0, sizeof(r_inline_models));

	// reset the hunk
	memset(r_hunk.base, 0, r_hunk.size);
	r_hunk.offset = 0;
}

/*
 * R_BeginLoading
 *
 * Loads the specified level after resetting all model data.
 */
void R_BeginLoading(const char *bsp_name, int bsp_size) {

	R_FreeModels(); // free all models

	// load bsp for collision detection (prediction)
	if (!Com_WasInit(Q2W_SERVER)) {
		int bs;

		Cm_LoadBsp(bsp_name, &bs);

		if (bs != bsp_size) {
			Com_Error(ERR_DROP, "Local map version differs from server: "
				"%i != %i.", bs, bsp_size);
		}
	}

	// then load materials
	R_LoadMaterials(bsp_name);

	// finally load the bsp for rendering (surface arrays)
	r_world_model = R_LoadModel(bsp_name);
}

/*
 * R_InitModels
 */
void R_InitModels(void) {

	// allocate the hunk
	r_hunk.size = (size_t) r_hunk_mb->value * 1024 * 1024;

	if (!(r_hunk.base = Z_Malloc(r_hunk.size))) { // malloc the new one
		Com_Error(ERR_FATAL, "R_HunkInit: Unable to allocate hunk.");
	}

	r_hunk.offset = 0;
}

/*
 * R_Shutdown
 */
void R_ShutdownModels(void) {

	R_FreeModels();

	if (r_hunk.base) // free the hunk
		Z_Free(r_hunk.base);
}
