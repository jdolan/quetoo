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

//
///**
// * @brief Calculate tangent vectors for the given Object model.
// *
// * http://www.terathon.com/code/tangent.html
// */
//static void R_LoadObjTangents(r_model_t *mod, r_obj_t *obj) {
//	vec3_t *tan1 = (vec3_t *) Mem_Malloc(obj->verts->len * sizeof(vec3_t));
//	vec3_t *tan2 = (vec3_t *) Mem_Malloc(obj->verts->len * sizeof(vec3_t));
//
//	uint32_t *tri = (uint32_t *) obj->tris;
//
//	// resolve the texture directional vectors
//
//	for (uint32_t i = 0; i < obj->num_tris; i++, tri += 3) {
//		vec3_t sdir, tdir;
//
//		const uint32_t i1 = tri[0];
//		const uint32_t i2 = tri[1];
//		const uint32_t i3 = tri[2];
//
//		const r_obj_vertex_t *vert1 = &g_array_index(obj->verts, r_obj_vertex_t, i1);
//		const r_obj_vertex_t *vert2 = &g_array_index(obj->verts, r_obj_vertex_t, i2);
//		const r_obj_vertex_t *vert3 = &g_array_index(obj->verts, r_obj_vertex_t, i3);
//
//		const vec_t *v1 = vert1->point;
//		const vec_t *v2 = vert2->point;
//		const vec_t *v3 = vert3->point;
//
//		const vec_t *w1 = vert1->texcoords;
//		const vec_t *w2 = vert2->texcoords;
//		const vec_t *w3 = vert3->texcoords;
//
//		vec_t x1 = v2[0] - v1[0];
//		vec_t x2 = v3[0] - v1[0];
//		vec_t y1 = v2[1] - v1[1];
//		vec_t y2 = v3[1] - v1[1];
//		vec_t z1 = v2[2] - v1[2];
//		vec_t z2 = v3[2] - v1[2];
//
//		vec_t s1 = w2[0] - w1[0];
//		vec_t s2 = w3[0] - w1[0];
//		vec_t t1 = w2[1] - w1[1];
//		vec_t t2 = w3[1] - w1[1];
//
//		vec_t r = 1.0 / (s1 * t2 - s2 * t1);
//
//		VectorSet(sdir,
//		          (t2 * x1 - t1 * x2),
//		          (t2 * y1 - t1 * y2),
//		          (t2 * z1 - t1 * z2)
//		         );
//
//		VectorScale(sdir, r, sdir);
//
//		VectorSet(tdir,
//		          (s1 * x2 - s2 * x1),
//		          (s1 * y2 - s2 * y1),
//		          (s1 * z2 - s2 * z1)
//		         );
//
//		VectorScale(tdir, r, tdir);
//
//		VectorAdd(tan1[i1], sdir, tan1[i1]);
//		VectorAdd(tan1[i2], sdir, tan1[i2]);
//		VectorAdd(tan1[i3], sdir, tan1[i3]);
//
//		VectorAdd(tan2[i1], tdir, tan2[i1]);
//		VectorAdd(tan2[i2], tdir, tan2[i2]);
//		VectorAdd(tan2[i3], tdir, tan2[i3]);
//	}
//
//	// calculate the tangents
//
//	for (uint32_t i = 0; i < obj->verts->len; i++) {
//		r_obj_vertex_t *v = &g_array_index(obj->verts, r_obj_vertex_t, i);
//		const vec_t *normal = v->normal;
//		TangentVectors(normal, tan1[i], tan2[i], v->tangent, v->bitangent);
//	}
//
//	Mem_Free(tan1);
//	Mem_Free(tan2);
//}

/**
 * @brief
 */
static GLuint R_FindOrAppendObjVertex(r_mesh_t *mesh, const r_mesh_vertex_t *v) {

	for (int32_t i = 0; i < mesh->num_vertexes; i++) {
		if (!memcmp(&v, mesh->vertexes + i, sizeof(*v))) {
			return i;
		}
	}

	mesh->num_vertexes++;
	mesh->vertexes = Mem_Realloc(mesh->vertexes, mesh->num_vertexes * sizeof(r_mesh_vertex_t));

	mesh->vertexes[mesh->num_vertexes - 1] = *v;
	return mesh->num_vertexes - 1;
}

/**
 * @brief
 */
static void R_AppendObjElements(r_mesh_t *mesh, GLuint a, GLuint b, GLuint c) {

	mesh->num_elements += 3;
	mesh->elements = Mem_Realloc(mesh->elements, mesh->num_elements * sizeof(GLuint));

	GLuint *elements = ((GLuint *) mesh->elements) + mesh->num_elements - 3;

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

	ClearBounds(mod->mins, mod->maxs);

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
			if (sscanf(line, "v %f %f %f", &vec[0], &vec[1], &vec[2]) == 3) {
				AddPointToBounds(vec, mod->mins, mod->maxs);
				g_array_append_val(obj.v, vec);
			}
		} else if (strncmp("vt ", line, strlen("vt ")) == 0) {
			if (sscanf(line, "vt %f %f", &vec[0], &vec[1]) == 2) {
				vec[1] = -vec[1];
				g_array_append_val(obj.vt, vec);
			}
		} else if (strncmp("vn ", line, strlen("vn ")) == 0) {
			if (sscanf(line, "vn %f %f %f", &vec[0], &vec[1], &vec[2]) == 3) {
				VectorNormalize(vec);
				g_array_append_val(obj.vn, vec);
			}
		} else if (strncmp("g ", line, strlen("g ")) == 0) {
			if (group.f->len) {
				g_strlcpy(group.name, line + strlen("g "), sizeof(group.name));
				group.f = g_array_new(FALSE, FALSE, sizeof(r_obj_face_t));
				g_array_append_val(obj.g, group);
			}
		} else if (strncmp("f ", line, strlen("f ")) == 0) {

			r_obj_face_t face = { 0 };
			int32_t i = 0;

			char *token = line + 1;
			while (*token) {

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

	out->num_meshes = obj.g->len;
	out->meshes = Mem_LinkMalloc(out->num_meshes * sizeof(r_mesh_t), out);

	for (int32_t i = 0; i < out->num_meshes; i++) {
		const r_obj_group_t *group = &g_array_index(obj.g, r_obj_group_t, i);
		r_mesh_t *mesh = out->meshes + i;

		g_strlcpy(mesh->name, group->name, sizeof(mesh->name));

		for (int32_t j = 0; j < (int32_t) group->f->len; j++) {
			r_obj_face_t *f = &g_array_index(group->f, r_obj_face_t, j);

			for (size_t k = 0; k < lengthof(f->fv); k++) {
				r_obj_face_vertex_t *fv = f->fv + k;

				if (fv->v == 0) {
					break;
				}

				r_mesh_vertex_t v = { 0 };

				VectorCopy(g_array_index(obj.v, vec3_t, fv->v - 1), v.position);
				Vector2Copy(g_array_index(obj.vt, vec2_t, fv->vt - 1), v.diffuse);
				VectorCopy(g_array_index(obj.vn, vec3_t, fv->vn - 1), v.normal);

				fv->el = R_FindOrAppendObjVertex(mesh, &v);
			}

			for (size_t k = 2; k < lengthof(f->fv); k++) {

				const r_obj_face_vertex_t *a = &f->fv[0];
				const r_obj_face_vertex_t *b = &f->fv[k - 1];
				const r_obj_face_vertex_t *c = &f->fv[k];

				if (c->v == 0) {
					break;
				}

				R_AppendObjElements(mesh, a->el, b->el, c->el);
			}
		}

		out->num_vertexes += mesh->num_vertexes;
		out->num_elements += mesh->num_elements;
	}

	// we've loaded all the meshes with their own vertex arrays, now we need to combine them

	mod->mesh->vertexes = Mem_LinkMalloc(mod->mesh->num_vertexes * sizeof(r_mesh_vertex_t), mod->mesh);
	mod->mesh->elements = Mem_LinkMalloc(mod->mesh->num_elements * sizeof(GLuint), mod->mesh);

	r_mesh_vertex_t *vertex = mod->mesh->vertexes;
	GLuint *elements = mod->mesh->elements;

	r_mesh_t *mesh = mod->mesh->meshes;
	for (int32_t i = 0; i < mod->mesh->num_meshes; i++, mesh++) {

		memcpy(vertex, mesh->vertexes, mesh->num_vertexes * sizeof(r_mesh_vertex_t));
		Mem_Free(mesh->vertexes);

		mesh->vertexes = vertex;
		vertex += mesh->num_vertexes * mod->mesh->num_frames;

		memcpy(elements, mesh->elements, mesh->num_elements * sizeof(GLuint));
		Mem_Free(mesh->elements);

		mesh->elements = (GLvoid *) ((elements - mod->mesh->elements) * sizeof(GLuint));
		elements += mesh->num_elements;
	}

	// and configs
	R_LoadMeshConfigs(mod);

	// and finally the arrays
	R_LoadMeshVertexArray(mod);

	g_array_free(obj.v, TRUE);
	g_array_free(obj.vt, TRUE);
	g_array_free(obj.vn, TRUE);
	g_array_free(obj.g, TRUE);
}

