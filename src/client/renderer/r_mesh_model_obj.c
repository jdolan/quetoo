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

typedef struct {
	uint32_t v;
	uint32_t vt;
	uint32_t vn;
	uint32_t el;
} r_obj_face_vertex_t;

typedef struct {
	r_obj_face_vertex_t fv[4];
} r_obj_face_t;

typedef struct {
	char name[MAX_QPATH];
	GArray *f;
} r_obj_group_t;

typedef struct {
	GArray *v;
	GArray *vt;
	GArray *vn;
	GArray *g;
} r_obj_t;

/**
 * @brief
 */
static GLuint R_FindOrAppendObjVertex(r_mesh_face_t *face, const r_mesh_vertex_t *v) {

	for (int32_t i = 0; i < face->num_vertexes; i++) {
		if (!memcmp(&v, face->vertexes + i, sizeof(*v))) {
			return i;
		}
	}

	face->num_vertexes++;
	face->vertexes = Mem_Realloc(face->vertexes, face->num_vertexes * sizeof(r_mesh_vertex_t));

	face->vertexes[face->num_vertexes - 1] = *v;
	return face->num_vertexes - 1;
}

/**
 * @brief
 */
static void R_AppendObjElements(r_mesh_face_t *face, GLuint a, GLuint b, GLuint c) {

	face->num_elements += 3;
	face->elements = Mem_Realloc(face->elements, face->num_elements * sizeof(GLuint));

	GLuint *elements = ((GLuint *) face->elements) + face->num_elements - 3;

	elements[0] = a;
	elements[1] = b;
	elements[2] = c;
}

/**
 * @brief
 */
void R_LoadObjModel(r_model_t *mod, void *buffer) {
	r_mesh_model_t *out;

	R_LoadModelMaterials(mod);

	mod->mesh = out = Mem_LinkMalloc(sizeof(r_mesh_model_t), mod);
	out->num_frames = 1;

	r_obj_t obj = {
		.v = g_array_new(FALSE, FALSE, sizeof(vec3_t)),
		.vt = g_array_new(FALSE, FALSE, sizeof(vec2_t)),
		.vn = g_array_new(FALSE, FALSE, sizeof(vec3_t)),
		.g = g_array_new(FALSE, FALSE, sizeof(r_obj_group_t)),
	};

	r_obj_group_t group = {
		.name = "default",
		.f = g_array_new(FALSE, FALSE, sizeof(r_obj_face_t))
	};

	char *file = buffer;

	for (char *line = strtok(file, "\r\n"); line; line = strtok(NULL, "\r\n")) {

		vec3_t vec;
		if (strncmp("v ", line, strlen("v ")) == 0) {
			if (sscanf(line, "v %f %f %f", &vec.x, &vec.z, &vec.y) == 3) {
				mod->mins = Vec3_Minf(mod->mins, vec);
				mod->maxs = Vec3_Maxf(mod->maxs, vec);
				g_array_append_val(obj.v, vec);
			}
		} else if (strncmp("vt ", line, strlen("vt ")) == 0) {
			if (sscanf(line, "vt %f %f", &vec.x, &vec.y) == 2) {
				vec.y = -vec.y;
				g_array_append_val(obj.vt, vec);
			}
		} else if (strncmp("vn ", line, strlen("vn ")) == 0) {
			if (sscanf(line, "vn %f %f %f", &vec.x, &vec.z, &vec.y) == 3) {
				vec = Vec3_Normalize(vec);
				g_array_append_val(obj.vn, vec);
			}
		} else if (strncmp("g ", line, strlen("g ")) == 0) {
			if (group.f->len) {
				g_array_append_val(obj.g, group);
			}
			g_strlcpy(group.name, line + strlen("g "), sizeof(group.name));
			group.f = g_array_new(FALSE, FALSE, sizeof(r_obj_face_t));
		} else if (strncmp("f ", line, strlen("f ")) == 0) {

			r_obj_face_t face;
			memset(&face, 0, sizeof(face));

			int32_t i = 0;

			char *token = line + 1;
			while (*token) {

				if (i == lengthof(face.fv)) {
					Com_Error(ERROR_DROP, "%s uses complex faces, try triangles\n", mod->media.name);
				}

				r_obj_face_vertex_t *fv = &face.fv[i++];
				fv->v = (int) strtol(token + 1, &token, 10);
				if (*token == '/') {
					fv->vt = (int) strtol(token + 1, &token, 10);
					if (*token == '/') {
						fv->vn = (int) strtol(token + 1, &token, 10);
					}
				} else if (fv->v == 0) {
					break;
				}
			}

			g_array_append_val(group.f, face);
		}
	}

	if (group.f->len) {
		g_array_append_val(obj.g, group);
	}

	out->num_faces = obj.g->len;
	out->faces = Mem_LinkMalloc(out->num_faces * sizeof(r_mesh_face_t), out);

	for (int32_t i = 0; i < out->num_faces; i++) {
		const r_obj_group_t *group = &g_array_index(obj.g, r_obj_group_t, i);
		r_mesh_face_t *face = out->faces + i;

		g_strlcpy(face->name, group->name, sizeof(face->name));
		face->material = R_ResolveMeshMaterial(mod, face, NULL);

		for (int32_t j = 0; j < (int32_t) group->f->len; j++) {
			r_obj_face_t *f = &g_array_index(group->f, r_obj_face_t, j);

			for (size_t k = 0; k < lengthof(f->fv); k++) {
				r_obj_face_vertex_t *fv = f->fv + k;

				if (fv->v == 0) {
					break;
				}

				const r_mesh_vertex_t v = {
					.position = g_array_index(obj.v, vec3_t, fv->v - 1),
					.diffusemap = g_array_index(obj.vt, vec2_t, fv->vt - 1),
					.normal = g_array_index(obj.vn, vec3_t, fv->vn - 1),
				};

				fv->el = R_FindOrAppendObjVertex(face, &v);
			}

			for (size_t k = 2; k < lengthof(f->fv); k++) {

				const r_obj_face_vertex_t *a = &f->fv[0];
				const r_obj_face_vertex_t *b = &f->fv[k - 1];
				const r_obj_face_vertex_t *c = &f->fv[k];

				if (c->v == 0) {
					break;
				}

				R_AppendObjElements(face, c->el, b->el, a->el);
			}
		}
	}

	// and configs
	R_LoadMeshConfigs(mod);

	// and finally the arrays
	R_LoadMeshVertexArray(mod);

	g_array_free(obj.v, TRUE);
	g_array_free(obj.vt, TRUE);
	g_array_free(obj.vn, TRUE);
	g_array_free(obj.g, TRUE);

	Com_Debug(DEBUG_RENDERER, "!================================\n");
	Com_Debug(DEBUG_RENDERER, "!R_LoadObjModel:   %s\n", mod->media.name);
	Com_Debug(DEBUG_RENDERER, "!  Vertexes:       %d\n", mod->mesh->num_vertexes);
	Com_Debug(DEBUG_RENDERER, "!  Elements:       %d\n", mod->mesh->num_elements);
	Com_Debug(DEBUG_RENDERER, "!  Frames:         %d\n", mod->mesh->num_frames);
	Com_Debug(DEBUG_RENDERER, "!  Tags:           %d\n", mod->mesh->num_tags);
	Com_Debug(DEBUG_RENDERER, "!  Faces:          %d\n", mod->mesh->num_faces);
	Com_Debug(DEBUG_RENDERER, "!  Animations:     %d\n", mod->mesh->num_animations);
	Com_Debug(DEBUG_RENDERER, "!================================\n");
}

