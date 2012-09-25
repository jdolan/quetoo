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

r_models_t r_models;

typedef struct r_hunk_s {
	byte *base;
	size_t size;
	ptrdiff_t offset;
} r_hunk_t;

static r_hunk_t r_hunk;

/*
 * @brief
 */
void *R_HunkBegin(void) {
	return r_hunk.base + r_hunk.offset;
}

/*
 * @brief
 */
size_t R_HunkEnd(void *buf) {
	return r_hunk.base + r_hunk.offset - (byte *) buf;
}

/*
 * @brief
 */
void *R_HunkAlloc(size_t size) {
	byte *b;

	if (r_hunk.offset + size > r_hunk.size) {
		Com_Error(ERR_DROP, "R_HunkAlloc: Overflow.\n");
	}

	b = r_hunk.base + r_hunk.offset;
	r_hunk.offset += size;

	return b;
}

/*
 * @brief
 */
void R_AllocVertexArrays(r_model_t *mod) {
	int32_t i, j, k, l, m;
	GLuint v, st, t;

	mod->num_verts = 0;

	// first resolve the vertex count
	if (mod->type == mod_bsp) {
		const r_bsp_cluster_t *cluster = mod->bsp->clusters;
		for (i = 0; i < mod->bsp->num_clusters; i++, cluster++) {

			const r_bsp_array_t *array = cluster->arrays;
			for (j = 0; j < cluster->num_arrays; j++, array++) {

				const r_bsp_leaf_t *leaf = mod->bsp->leafs;
				for (k = 0; k < mod->bsp->num_leafs; k++, leaf++) {

					if (i == leaf->cluster) {

						r_bsp_surface_t **s = leaf->first_leaf_surface;
						for (l = 0; l < leaf->num_leaf_surfaces; l++, s++) {

							if ((*s)->texinfo == array->texinfo) {

								for (m = 0; m < (*s)->num_edges; m++) {
									if (m > 2) {
										mod->num_verts += 2;
									}
									mod->num_verts++;
								}
							}
						}
					}
				}
			}
		}
	} else if (mod->type == mod_md3) {
		const r_md3_t *md3 = (r_md3_t *) mod->mesh->extra_data;
		const r_md3_mesh_t *mesh = md3->meshes;

		for (i = 0; i < md3->num_meshes; i++, mesh++) {
			mod->num_verts += mesh->num_tris * 3;
		}
	} else if (mod->type == mod_obj) {
		const r_obj_t *obj = (r_obj_t *) mod->mesh->extra_data;
		mod->num_verts = obj->num_tris * 3;
	}

	v = mod->num_verts * 3 * sizeof(GLfloat);
	st = mod->num_verts * 2 * sizeof(GLfloat);
	t = mod->num_verts * 4 * sizeof(GLfloat);

	// allocate the arrays, static models get verts, normals and tangents
	if (mod->bsp || mod->mesh->num_frames == 1) {
		mod->verts = (GLfloat *) R_HunkAlloc(v);
		mod->normals = (GLfloat *) R_HunkAlloc(v);
		mod->tangents = (GLfloat *) R_HunkAlloc(t);
	}

	// all models get texcoords
	mod->texcoords = (GLfloat *) R_HunkAlloc(st);

	if (mod->type != mod_bsp)
		return;

	// and bsp models get lightmap texcoords and colors
	mod->lightmap_texcoords = (GLfloat *) R_HunkAlloc(st);
}

/*
 * @brief
 */
static void R_LoadVertexBuffers(r_model_t *mod) {
	int32_t v, st, t;

	if (!qglGenBuffers)
		return;

	if (mod->mesh && mod->mesh->num_frames > 1) // animated models don't use VBO
		return;

	v = mod->num_verts * 3 * sizeof(GLfloat);
	st = mod->num_verts * 2 * sizeof(GLfloat);
	t = mod->num_verts * 4 * sizeof(GLfloat);

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

	qglGenBuffers(1, &mod->tangent_buffer);
	qglBindBuffer(GL_ARRAY_BUFFER, mod->tangent_buffer);
	qglBufferData(GL_ARRAY_BUFFER, t, mod->tangents, GL_STATIC_DRAW);

	qglBindBuffer(GL_ARRAY_BUFFER, 0);

	if (mod->type != mod_bsp)
		return;

	// including lightmap texcords for bsp
	qglGenBuffers(1, &mod->lightmap_texcoord_buffer);
	qglBindBuffer(GL_ARRAY_BUFFER, mod->lightmap_texcoord_buffer);
	qglBufferData(GL_ARRAY_BUFFER, st, mod->lightmap_texcoords, GL_STATIC_DRAW);

	qglBindBuffer(GL_ARRAY_BUFFER, 0);
}

typedef struct r_model_format_s {
	r_model_type_t type;
	const char *name;
	void (*load)(r_model_t *mod, void *buffer);
} r_model_format_t;

static r_model_format_t r_model_formats[] = {
		{ mod_obj, ".obj", R_LoadObjModel },
		{ mod_md3, ".md3", R_LoadMd3Model },
		{ mod_bsp, ".bsp", R_LoadBspModel } };

#define NUM_MODEL_FORMATS (sizeof(r_model_formats) / sizeof(r_model_format_t))

/*
 * @brief
 */
r_model_t *R_LoadModel(const char *name) {
	r_model_format_t *format;
	r_model_t *mod;
	char n[MAX_QPATH];
	void *buf;
	vec3_t tmp;
	uint16_t i;

	if (!name || !name[0]) {
		Com_Error(ERR_DROP, "R_LoadModel: NULL name.\n");
	}

	// inline models are fetched from a separate array
	if (name[0] == '*') {
		i = atoi(name + 1);
		if (i < 1 || !r_models.world || i >= r_models.world->bsp->num_submodels) {
			Com_Error(ERR_DROP, "R_LoadModel: Bad inline model number.\n");
		}
		return &r_models.bsp_models[i];
	}

	StripExtension(name, n);

	// search the currently loaded models
	for (i = 0, mod = r_models.models; i < r_models.num_models; i++, mod++) {
		if (!strcmp(n, mod->name))
			return mod;
	}

	if (i == r_models.num_models) {
		if (r_models.num_models == MAX_MOD_KNOWN) {
			Com_Error(ERR_DROP, "R_LoadModel: MAX_MOD_KNOWN reached.\n");
		}
		r_models.num_models++;
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
		if (strstr(name, "players/")) {
			Com_Debug("R_LoadModel: Failed to load player %s.\n", name);
		} else {
			Com_Warn("R_LoadModel: Failed to load %s.\n", name);
		}
		return NULL;
	}

	StripExtension(n, mod->name);
	mod->type = format->type;

	// load the materials first, so that we can resolve surfaces lists
	R_LoadMaterials(mod);

	r_models.load = mod;
	format->load(mod, buf);

	Fs_FreeFile(buf);

	// assemble vertex buffer objects from static arrays
	R_LoadVertexBuffers(mod);

	// calculate an approximate radius from the bounding box
	VectorSubtract(mod->maxs, mod->mins, tmp);
	mod->radius = VectorLength(tmp) / 2.0;

	return mod;
}

/*
 * @brief
 */
r_model_t *R_WorldModel(void) {
	return r_models.world;
}

/*
 * @brief
 */
void R_ListModels_f(void) {
	uint16_t i;

	Com_Print("Loaded models:\n");

	const r_model_t *mod = r_models.models;
	for (i = 0; i < r_models.num_models; i++, mod++) {
		Com_Print("%6i: %s\n", mod->num_verts, mod->name);
	}
}

/*
 * @brief
 */
void R_HunkStats_f(void) {
	Com_Print("Hunk usage: %.2f / %.2f MB\n", r_hunk.offset / 1024.0 / 1024.0, r_hunk_mb->value);
}

/*
 * @brief
 */
static void R_FreeModels(void) {
	int32_t i;

	R_FreeMaterials(); // first free materials

	r_model_t *mod = r_models.models;
	for (i = 0; i < r_models.num_models; i++, mod++) {
		if (mod->vertex_buffer)
			qglDeleteBuffers(1, &mod->vertex_buffer);
		if (mod->texcoord_buffer)
			qglDeleteBuffers(1, &mod->texcoord_buffer);
		if (mod->lightmap_texcoord_buffer)
			qglDeleteBuffers(1, &mod->lightmap_texcoord_buffer);
		if (mod->normal_buffer)
			qglDeleteBuffers(1, &mod->normal_buffer);
	}

	// reset the models
	memset(&r_models, 0, sizeof(r_models));

	// reset the hunk
	memset(r_hunk.base, 0, r_hunk.size);
	r_hunk.offset = 0;
}

/*
 * @brief Loads the specified level after resetting all model data.
 */
void R_BeginLoading(const char *bsp_name, int32_t bsp_size) {

	R_FreeModels(); // free all models

	// load bsp for collision detection (prediction)
	if (!Com_WasInit(Q2W_SERVER) || !Cm_NumModels()) {
		int32_t bs;

		Cm_LoadBsp(bsp_name, &bs);

		if (bs != bsp_size) {
			Com_Error(ERR_DROP, "Local map version differs from server: "
				"%i != %i.\n", bs, bsp_size);
		}
	}

	// and then load the bsp for rendering (surface arrays)
	r_models.world = R_LoadModel(bsp_name);
}

/*
 * @brief
 */
void R_InitModels(void) {

	// allocate the hunk
	r_hunk.size = (size_t) r_hunk_mb->value * 1024 * 1024;

	if (!(r_hunk.base = Z_Malloc(r_hunk.size))) { // malloc the new one
		Com_Error(ERR_FATAL, "R_HunkInit: Unable to allocate hunk.\n");
	}

	r_hunk.offset = 0;
}

/*
 * @brief
 */
void R_ShutdownModels(void) {

	R_FreeModels();

	if (r_hunk.base) // free the hunk
		Z_Free(r_hunk.base);
}
