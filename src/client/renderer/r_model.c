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

r_model_state_t r_model_state;

typedef struct {
	const char *extension;
	r_model_type_t type;
	void (*Load)(r_model_t *mod, void *buffer);
} r_model_format_t;

static const r_model_format_t r_model_formats[] = {
	{ ".obj", MOD_OBJ, R_LoadObjModel },
	{ ".md3", MOD_MD3, R_LoadMd3Model },
	{ ".bsp", MOD_BSP, R_LoadBspModel }
};

/*
 * @brief Allocates client-side vertex arrays for the specified r_model_t.
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

	// and BSP models get lightmap texcoords
	mod->lightmap_texcoords = Z_LinkMalloc(mod->num_verts * sizeof(vec2_t), mod);
}

/*
 * @brief Allocates and populates static VBO's for the specified r_model_t.
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

	R_GetError(mod->media.name);
}

/*
 * @brief Register event listener for models.
 */
static void R_RegisterModel(r_media_t *self) {
	r_model_t *mod = (r_model_t *) self;

	if (mod->type == MOD_BSP) {
		const r_bsp_surface_t *s = mod->bsp->surfaces;
		uint16_t i;

		for (i = 0; i < mod->bsp->num_surfaces; i++, s++) {
			R_RegisterDependency(self, (r_media_t *) s->texinfo->material);

			R_RegisterDependency(self, (r_media_t *) s->lightmap);
			R_RegisterDependency(self, (r_media_t *) s->deluxemap);
		}

		// keep a reference to the world model
		r_model_state.world = mod;

	} else if (IS_MESH_MODEL(mod)) {
		R_RegisterDependency(self, (r_media_t *) mod->mesh->material);
	}
}

/*
 * @brief Free event listener for models.
 */
static void R_FreeModel(r_media_t *self) {
	r_model_t *mod = (r_model_t *) self;

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

	R_GetError(mod->media.name);
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

	if (*name == '*') {
		g_snprintf(key, sizeof(key), "%s#%s", r_model_state.world->media.name, name + 1);
	} else {
		StripExtension(name, key);
	}

	if (!(mod = (r_model_t *) R_FindMedia(key))) {

		void *buf;
		const r_model_format_t *format = r_model_formats;
		for (i = 0; i < lengthof(r_model_formats); i++, format++) {

			StripExtension(name, key);
			strcat(key, format->extension);

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

		StripExtension(name, key);

		mod = (r_model_t *) R_MallocMedia(key, sizeof(r_model_t));

		mod->media.Register = R_RegisterModel;
		mod->media.Free = R_FreeModel;

		mod->type = format->type;

		// load the materials first, so that we can resolve surfaces lists
		R_LoadMaterials(mod);

		// load it
		format->Load(mod, buf);

		// free the file
		Fs_FreeFile(buf);

		// assemble vertex buffer objects from static arrays
		R_LoadVertexBuffers(mod);

		// calculate an approximate radius from the bounding box
		vec3_t tmp;

		VectorSubtract(mod->maxs, mod->mins, tmp);
		mod->radius = VectorLength(tmp) / 2.0;

		R_RegisterMedia((r_media_t *) mod);
	}

	return mod;
}

/*
 * @brief Returns the currently loaded world model (BSP).
 */
r_model_t *R_WorldModel(void) {
	return r_model_state.world;
}

/*
 * @brief Initializes the model facilities.
 */
void R_InitModels(void) {
	memset(&r_model_state, 0, sizeof(r_model_state));
}
