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

#include <glib.h>

#include "r_local.h"

typedef struct {
	GHashTable *models;

	r_model_t *world;
} r_model_state_t;

static r_model_state_t r_model_state;

typedef struct {
	r_model_type_t type;
	const char *name;
	void (*Load)(r_model_t *mod, void *buffer);
} r_model_format_t;

static r_model_format_t r_model_formats[] = {
	{ MOD_OBJ, ".obj", R_LoadObjModel },
	{ MOD_MD3, ".md3", R_LoadMd3Model },
	{ MOD_BSP, ".bsp", R_LoadBspModel }
};

/*
 * @brief
 */
void R_AllocVertexArrays(r_model_t *mod) {
	uint16_t i, j;

	mod->num_verts = 0;

	// first resolve the vertex count
	if (mod->type == MOD_BSP) {
		const r_bsp_leaf_t *leaf = mod->bsp->leafs;
		for (i = 0; i < mod->bsp->num_leafs; i++, leaf++) {

			r_bsp_surface_t **s = leaf->first_leaf_surface;
			for (j = 0; j < leaf->num_leaf_surfaces; j++, s++) {
				mod->num_verts += (*s)->num_edges;
			}
		}
	} else if (mod->type == MOD_MD3) {
		const r_md3_t *md3 = (r_md3_t *) mod->mesh->data;
		const r_md3_mesh_t *mesh = md3->meshes;

		for (i = 0; i < md3->num_meshes; i++, mesh++) {
			mod->num_verts += mesh->num_tris * 3;
		}
	} else if (mod->type == MOD_OBJ) {
		const r_obj_t *obj = (r_obj_t *) mod->mesh->data;
		mod->num_verts = obj->num_tris * 3;
	}

	// allocate the arrays, static models get verts, normals and tangents
	if (mod->bsp || mod->mesh->num_frames == 1) {
		mod->verts = Z_LinkMalloc(mod->num_verts * sizeof(vec3_t), mod);
		mod->normals = Z_LinkMalloc(mod->num_verts * sizeof(vec3_t), mod);
		mod->tangents = Z_LinkMalloc(mod->num_verts * sizeof(vec4_t), mod);
	}

	// all models get texcoords
	mod->texcoords = Z_LinkMalloc(mod->num_verts * sizeof(vec2_t), mod);

	if (mod->type != MOD_BSP)
		return;

	// and bsp models get lightmap texcoords
	mod->lightmap_texcoords = Z_LinkMalloc(mod->num_verts * sizeof(vec2_t), mod);
}

/*
 * @brief
 */
static void R_LoadVertexBuffers(r_model_t *mod) {

	if (!qglGenBuffers)
		return;

	if (IS_MESH_MODEL(mod) && mod->mesh->num_frames > 1) // animated models don't use VBO
		return;

	const GLsizei v = mod->num_verts * 3 * sizeof(GLfloat);
	const GLsizei st = mod->num_verts * 2 * sizeof(GLfloat);
	const GLsizei t = mod->num_verts * 4 * sizeof(GLfloat);

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

	if (mod->type == MOD_BSP) { // including lightmap texcords for bsp

		qglGenBuffers(1, &mod->lightmap_texcoord_buffer);
		qglBindBuffer(GL_ARRAY_BUFFER, mod->lightmap_texcoord_buffer);
		qglBufferData(GL_ARRAY_BUFFER, st, mod->lightmap_texcoords, GL_STATIC_DRAW);

		qglBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	R_GetError(mod->name);
}

/*
 * @brief Loads the model by the specified name.
 */
r_model_t *R_LoadModel(const char *name) {
	r_model_t *mod;
	char key[MAX_QPATH];
	size_t i;

	if (!name || !name[0]) {
		Com_Error(ERR_DROP, "R_LoadModel: NULL name.\n");
	}

	StripExtension(name, key);

	// search the currently loaded models
	if ((mod = g_hash_table_lookup(r_model_state.models, key))) {
		mod->media_count = r_locals.media_count;
		return mod;
	}

	void *buf;

	// load the file
	r_model_format_t *format = r_model_formats;
	for (i = 0; i < lengthof(r_model_formats); i++, format++) {

		StripExtension(name, key);
		strcat(key, format->name);

		if (Fs_LoadFile(key, &buf) != -1)
			break;
	}

	if (i == lengthof(r_model_formats)) { // not found
		if (strstr(name, "players/")) {
			Com_Debug("R_LoadModel: Failed to load player %s.\n", name);
		} else {
			Com_Warn("R_LoadModel: Failed to load %s.\n", name);
		}
		return NULL;
	}

	mod = Z_TagMalloc(sizeof(r_model_t), Z_TAG_RENDERER);

	StripExtension(key, mod->name);
	mod->type = format->type;

	// load the materials first, so that we can resolve surfaces lists
	R_LoadMaterials(mod);

	format->Load(mod, buf);

	Fs_FreeFile(buf);

	// assemble vertex buffer objects from static arrays
	R_LoadVertexBuffers(mod);

	// calculate an approximate radius from the bounding box
	vec3_t tmp;

	VectorSubtract(mod->maxs, mod->mins, tmp);
	mod->radius = VectorLength(tmp) / 2.0;

	R_RegisterModel(mod);

	return mod;
}

/*
 * @brief Returns the currently loaded world model (BSP).
 */
r_model_t *R_WorldModel(void) {
	return r_model_state.world;
}

/*
 * @brief Prints information for the specified model to the console.
 */
static void R_ListModels_f_(gpointer key __attribute__((unused)), gpointer value, gpointer data __attribute__((unused))) {
	r_model_t *mod = (r_model_t *) value;

	Com_Print("%6i: %s\n", mod->num_verts, mod->name);
}

/*
 * @brief Prints information about all currently loaded models to the console.
 */
void R_ListModels_f(void) {

	Com_Print("Loaded models:\n");

	g_hash_table_foreach(r_model_state.models, R_ListModels_f_, NULL);
}

/*
 * @brief Inserts the specified model into the shared table.
 */
void R_RegisterModel(r_model_t *mod) {
	r_model_t *m;

	mod->media_count = r_locals.media_count;

	// save a reference to the world model
	if (mod->type == MOD_BSP) {
		r_model_state.world = mod;
	}

	// check for a model with the same name (e.g. inline models)
	if ((m = g_hash_table_lookup(r_model_state.models, mod->name))) {
		if (m == mod) {
			return;
		}
		// remove stale models
		g_hash_table_remove(r_model_state.models, mod->name);
	}

	// and insert the new one
	g_hash_table_insert(r_model_state.models, mod->name, mod);
}

/*
 * @brief GHRFunc for freeing models. If data is non-NULL, then all models are
 * freed (deleted from GL as well as Z_Free'd). Otherwise, only those with
 * stale media counts are released.
 */
static gboolean R_FreeModel(gpointer key __attribute__((unused)), gpointer value, gpointer data) {
	r_model_t *mod = (r_model_t *) value;

	if (!data) {
		if (mod->media_count == r_locals.media_count) {
			return false;
		}
	}

	if (mod->vertex_buffer)
		qglDeleteBuffers(1, &mod->vertex_buffer);

	if (mod->texcoord_buffer)
		qglDeleteBuffers(1, &mod->texcoord_buffer);

	if (mod->lightmap_texcoord_buffer)
		qglDeleteBuffers(1, &mod->lightmap_texcoord_buffer);

	if (mod->normal_buffer)
		qglDeleteBuffers(1, &mod->normal_buffer);

	if (mod->tangent_buffer)
		qglDeleteBuffers(1, &mod->tangent_buffer);

	R_GetError(mod->name);

	return true;
}

/*
 * @brief Frees any stale models.
 */
void R_FreeModels(void) {
	g_hash_table_foreach_remove(r_model_state.models, R_FreeModel, NULL);
}

/*
 * @brief
 */
void R_InitModels(void) {

	memset(&r_model_state, 0, sizeof(r_model_state));

	r_model_state.models = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, Z_Free);
}

/*
 * @brief
 */
void R_ShutdownModels(void) {

	g_hash_table_foreach_remove(r_model_state.models, R_FreeModel, (void *) true);

	g_hash_table_destroy(r_model_state.models);
}
