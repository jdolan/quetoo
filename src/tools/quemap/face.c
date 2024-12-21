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

#include "bsp.h"
#include "face.h"
#include "map.h"
#include "material.h"
#include "tree.h"
#include "qbsp.h"

/**
 * @brief
 */
face_t *AllocFace(void) {

	return Mem_TagMalloc(sizeof(face_t), MEM_TAG_FACE);
}

/**
 * @brief
 */
void FreeFace(face_t *f) {

	if (f->w) {
		Cm_FreeWinding(f->w);
	}

	Mem_Free(f);
}

/**
 * @brief Merges the two faces if they share a brush side, plane, and common edge.
 *
 * @return NULL if the faces couldn't be merged, or the new face.
 * @remark The originals will NOT be freed.
 */
face_t *MergeFaces(face_t *a, face_t *b) {

	if (a->brush_side != b->brush_side) {
		return NULL;
	}

	if (a->plane != b->plane) {
		return NULL;
	}

	const plane_t *plane = &planes[a->plane];
	cm_winding_t *w = Cm_MergeWindings(a->w, b->w, plane->normal);
	if (!w) {
		return NULL;
	}

	face_t *merged = AllocFace();
	merged->brush_side = a->brush_side;
	merged->plane = a->plane;
	merged->w = w;

	a->merged = merged;
	b->merged = merged;

	Cm_FreeWinding(a->w);
	Cm_FreeWinding(b->w);

	a->w = NULL;
	b->w = NULL;

	return merged;
}

static GHashTable* welding_spatial_hash;
static GSList* welding_hash_keys;

/**
 * @brief 
 */
static void WeldingSpatialHashValueDestroyFunc(GHashTable *ptr) {
	g_hash_table_destroy(ptr);
}

/**
 * @brief
 */
static vec3i_t GetWeldingPoint(const vec3_t p) {
	return Vec3i(roundf(p.x), roundf(p.y), roundf(p.z));
}

/**
 * @brief
 */
static guint WeldingSpatialHashFunc(const vec3i_t* ptr) {
	const uint16_t x = roundf((MAX_WORLD_COORD + ptr->x) * .5f);
	const uint16_t y = roundf((MAX_WORLD_COORD + ptr->y) * .5f);
	const uint16_t z = roundf((MAX_WORLD_COORD + ptr->z) * .25f);

	return x | (y << 11) | (z << 22);
}

/**
 * @brief
 */
static gboolean WeldingSpatialHashEqualFunc(const vec3i_t* a, const vec3i_t* b) {
	return a->x == b->x && a->y == b->y && a->z == b->z;
}

/**
 * @brief
 */
void ClearWeldingSpatialHash(void) {

	if (welding_spatial_hash) {
		g_hash_table_remove_all(welding_spatial_hash);
		g_slist_free_full(welding_hash_keys, Mem_Free);
		welding_hash_keys = NULL;
	} else {
		welding_spatial_hash = g_hash_table_new_full((GHashFunc) WeldingSpatialHashFunc,
													 (GEqualFunc) WeldingSpatialHashEqualFunc,
													 NULL,
													 (GDestroyNotify) WeldingSpatialHashValueDestroyFunc);
		welding_hash_keys = g_slist_alloc();
	}
}

/**
 * @brief
 */
static gboolean WeldingHashKeyEquals(gconstpointer a, gconstpointer b) {
	const int32_t key_a = GPOINTER_TO_INT(a);
	const int32_t key_b = GPOINTER_TO_INT(b);

	if (key_a == key_b) {
		return true;
	}

	return Vec3_Equal(bsp_file.vertexes[key_a].position, bsp_file.vertexes[key_b].position);
}

/**
 * @brief
 */
static guint WeldingHashKeyHash(gconstpointer a) {
	const int32_t key_a = GPOINTER_TO_INT(a);
	const vec3_t *v = &bsp_file.vertexes[key_a].position;

	return WeldingSpatialHashFunc(&(const vec3i_t) {
		.x = v->x * VERTEX_EPSILON,
		.y = v->y * VERTEX_EPSILON,
		.z = v->z * VERTEX_EPSILON
	});
}

/**
 * @brief
 */
static void AddVertexToWeldingSpatialHash(const vec3_t v, const int32_t index) {
	const vec3i_t spatial = GetWeldingPoint(v);
	GHashTable *array = g_hash_table_lookup(welding_spatial_hash, &spatial);

	if (!array) {
		array = g_hash_table_new(WeldingHashKeyHash, WeldingHashKeyEquals);

		gpointer key_copy = Mem_Malloc(sizeof(spatial));
		memcpy(key_copy, &spatial, sizeof(spatial));

		welding_hash_keys = g_slist_prepend(welding_hash_keys, key_copy);
		g_hash_table_insert(welding_spatial_hash, key_copy, array);
	}

	if (!g_hash_table_contains(array, GINT_TO_POINTER(index))) {
		g_hash_table_add(array, GINT_TO_POINTER(index));
	}
}

int32_t num_welds = 0;

/**
 * @brief
 */
static void FindWeldingSpatialHashPoint(const vec3_t in, vec3_t *out) {
	static const int32_t offsets[] = { 0, 1, -1 };

	for (int32_t z = 0; z < (int32_t) lengthof(offsets); z++) {
		for (int32_t y = 0; y < (int32_t) lengthof(offsets); y++) {
			for (int32_t x = 0; x < (int32_t) lengthof(offsets); x++) {
				const vec3i_t key = GetWeldingPoint(Vec3(in.x + offsets[x], in.y + offsets[y], in.z + offsets[z]));
				GHashTable *array = g_hash_table_lookup(welding_spatial_hash, &key);

				if (!array) {
					continue;
				}

				GHashTableIter iter;
				gpointer iter_key;
				g_hash_table_iter_init(&iter, array);

				while (g_hash_table_iter_next (&iter, &iter_key, NULL)) {
					const vec3_t pos = bsp_file.vertexes[GPOINTER_TO_INT(iter_key)].position;

					if (Vec3_DistanceSquared(pos, in) < VERTEX_EPSILON * VERTEX_EPSILON) {
						*out = pos;
						num_welds++;
						return;
					}
				}
			}
		}
	}

	*out = in;
}

/**
 * @brief Welds the specified winding, writing its welded points to the given array.
 * @remarks This attempts to fix hairline cracks in (usually) intricate brushes. Note
 * that the weld threshold here is significantly larger than that of WindingIsSmall.
 * This allows for small windings that act as "caulk" to not be collapsed, but instead
 * be welded to other geometry. We do not weld the points to each other; only to those
 * of other brushes.
 */
static int32_t WeldWinding(const cm_winding_t *w, vec3_t *points) {
	vec3_t *out = points;
	
	for (int32_t i = 0; i < w->num_points; i++, out++) {
		FindWeldingSpatialHashPoint(w->points[i], out);
	}

	return w->num_points;
}

/**
 * @brief Emits a vertex array for the given face.
 */
static int32_t EmitFaceVertexes(const face_t *face) {
	const brush_side_t *brush_side = face->brush_side;

	const vec3_t sdir = Vec4_XYZ(brush_side->axis[0]);
	const vec3_t tdir = Vec4_XYZ(brush_side->axis[1]);

	const SDL_Surface *diffusemap = materials[brush_side->material].diffusemap;
	
	vec3_t points[face->w->num_points];
	int32_t num_points = face->w->num_points;

	if (no_weld) {
		memcpy(points, face->w->points, face->w->num_points * sizeof(face->w->points[0]));
	} else {
		num_points = WeldWinding(face->w, points);
		if (num_points < 3) {
			Mon_SendWinding(MON_WARN, points, num_points, "Malformed face after welding");
			return 0;
		}
	}

	for (int32_t i = 0; i < num_points; i++) {

		if (bsp_file.num_vertexes == MAX_BSP_VERTEXES) {
			Com_Error(ERROR_FATAL, "MAX_BSP_VERTEXES");
		}

		bsp_vertex_t out = {
			.position = points[i],
			.normal = planes[face->plane].normal
		};

		const float s = Vec3_Dot(points[i], sdir) + brush_side->axis[0].w;
		const float t = Vec3_Dot(points[i], tdir) + brush_side->axis[1].w;

		out.diffusemap.x = s / (diffusemap ? diffusemap->w : 1.f);
		out.diffusemap.y = t / (diffusemap ? diffusemap->h : 1.f);

		switch (brush_side->surface & SURF_MASK_BLEND) {
			case SURF_BLEND_33:
				out.color = Color32(255, 255, 255, 255 * .33f);
				break;
			case SURF_BLEND_66:
				out.color = Color32(255, 255, 255, 255 * .66f);
				break;
			default:
				out.color = Color32(255, 255, 255, 255);
				break;
		}

		bsp_file.vertexes[bsp_file.num_vertexes] = out;
		AddVertexToWeldingSpatialHash(out.position, bsp_file.num_vertexes);
		bsp_file.num_vertexes++;
	}

	return num_points;
}

/**
 * @brief Emits a vertex elements array of triangles for the given face.
 */
static int32_t EmitFaceElements(const face_t *face, int32_t first_vertex) {

	const int32_t num_triangles = (face->w->num_points - 2);
	const int32_t num_elements = num_triangles * 3;

	int32_t elements[num_elements];
	const int32_t count = Cm_ElementsForWinding(face->w, elements);

	if (num_elements != count) {
		Mon_SendWinding(MON_WARN, face->w->points, face->w->num_points, "Face has degenerate winding");
	}

	for (int32_t i = 0; i < count; i++) {

		if (bsp_file.num_elements == MAX_BSP_ELEMENTS) {
			Com_Error(ERROR_FATAL, "MAX_BSP_ELEMENTS\n");
		}

		bsp_file.elements[bsp_file.num_elements] = first_vertex + elements[i];
		bsp_file.num_elements++;
	}

	return num_elements;
}

/**
 * @brief
 */
bsp_face_t *EmitFace(const face_t *face) {

	assert(face->w->num_points > 2);
	assert(face->brush_side->material >= 0);
	assert(face->brush_side->out);

	if (bsp_file.num_faces == MAX_BSP_FACES) {
		Com_Error(ERROR_FATAL, "MAX_BSP_FACES\n");
	}

	bsp_face_t *out = &bsp_file.faces[bsp_file.num_faces];
	bsp_file.num_faces++;

	out->brush_side = (int32_t) (ptrdiff_t) (face->brush_side->out - bsp_file.brush_sides);
	out->plane = face->plane;
	
	out->bounds = Box3_Null();

	out->first_vertex = bsp_file.num_vertexes;
	out->num_vertexes = EmitFaceVertexes(face);

	assert(out->num_vertexes);

	const bsp_vertex_t *v = bsp_file.vertexes + out->first_vertex;
	for (int32_t i = 0; i < out->num_vertexes; i++, v++) {
		out->bounds = Box3_Append(out->bounds, v->position);
	}

	out->first_element = bsp_file.num_elements;
	out->num_elements = EmitFaceElements(face, out->first_vertex);

	return out;
}

#define MAX_VERTEX_FACES 32

static entity_t **face_entities;

/**
 * @brief Populates faces with pointers to all those referencing the vertex.
 * @return The count of faces referencing the vertex.
 */
static size_t FacesForVertex(const bsp_face_t *face, const bsp_vertex_t *vertex, const bsp_face_t **faces) {

	size_t count = 0;

	const entity_t *face_entity = face_entities[face - bsp_file.faces];

	const bsp_face_t *f = bsp_file.faces;
	for (int32_t i = 0; i < bsp_file.num_faces; i++, f++) {

		if (face_entity != face_entities[i]) {
			continue;
		}

		const bsp_vertex_t *v = &bsp_file.vertexes[f->first_vertex];
		for (int32_t j = 0; j < f->num_vertexes; j++, v++) {

			if (Vec3_Distance(vertex->position, v->position) < VERTEX_EPSILON) {
				faces[count++] = f;
				break;
			}
		}

		if (count == MAX_VERTEX_FACES) {
			Mon_SendPoint(MON_ERROR, vertex->position, "MAX_VERTEX_FACES");
			break;
		}
	}

	return count;
}

/**
 * @brief Calculate per-vertex (instead of per-plane) normal vectors. This is done by finding all of
 * the faces which share a given vertex, and calculating a weighted average of their normals.
 */
static void PhongVertex(const bsp_face_t *face, bsp_vertex_t *v, float phong_cosine) {
	const bsp_face_t *faces[MAX_VERTEX_FACES];

	const bsp_brush_side_t *side = &bsp_file.brush_sides[face->brush_side];
	const bsp_plane_t *plane = &bsp_file.planes[face->plane];

	const size_t count = FacesForVertex(face, v, faces);
	if (count > 1) {

		v->normal = Vec3_Zero();

		const bsp_face_t **f = faces;
		for (size_t i = 0; i < count; i++, f++) {

			size_t j;
			for (j = 0; j < i; j++) {
				if (faces[j]->brush_side == (*f)->brush_side &&
					faces[j]->plane == (*f)->plane) {
					break;
				}
			}

			if (j < i) {
				continue;
			}

			const bsp_brush_side_t *s = &bsp_file.brush_sides[(*f)->brush_side];
			const bsp_plane_t *p = &bsp_file.planes[(*f)->plane];

			const float dot = Vec3_Dot(plane->normal, p->normal);
			if (dot <= 0.f) {
				continue;
			}

			if (side->surface & SURF_PHONG) {
				if ((s->surface & SURF_PHONG) && (side->value == s->value)) {
					// phong enabled via surface flag and shading groups
				} else {
					continue;
				}
			} else {
				if (phong_cosine > 0.f && dot > phong_cosine) {
					// phong enabled via entity key and dot product
				} else {
					continue;
				}
			}

			/*
			 * Find the original brush side winding that the vertex came from, rather than the
			 * winding of the face itself. This is because faces are split by the BSP process,
			 * and so to get the correct normal vector weighting, the original side windings
			 * are more reliable.
			 */

			cm_winding_t *w = NULL;
			const brush_side_t *map_side = brush_sides;
			for (int32_t j = 0; j < num_brush_sides; j++, map_side++) {
				if (s == map_side->out) {
					w = map_side->winding;
					break;
				}
			}
			assert(w);

			v->normal = Vec3_Fmaf(v->normal, Cm_WindingArea(w), p->normal);
		}

		if (Vec3_LengthSquared(v->normal)) {
			v->normal = Vec3_Normalize(v->normal);
		} else {
			v->normal = plane->normal;
		}
	}
}

/**
 * @brief Phong shades the specified face.
 * @details Phong shading only applies to "superverts," or vertexes created from original geometry,
 * and not from BSP splits or T-junctions. That is, it only applies to corners of faces, not to
 * vertexes with colinear neighbors.
 */
static void PhongFace(int32_t face_num) {

	const entity_t *entity = face_entities[face_num];
	assert(entity);

	const float phong_angle = atof(ValueForKey(entity, "phong", "60"));
	const float phong_cosine = cosf(Radians(phong_angle));

	const bsp_face_t *face = bsp_file.faces + face_num;
	bsp_vertex_t *v = bsp_file.vertexes + face->first_vertex;

	for (int32_t i = 0; i < face->num_vertexes; i++) {

		bsp_vertex_t *a = v + ((i + 0) % face->num_vertexes);
		bsp_vertex_t *b = v + ((i + 1) % face->num_vertexes);
		bsp_vertex_t *c = v + ((i + 2) % face->num_vertexes);

		const vec3_t ba = Vec3_Direction(b->position, a->position);
		const vec3_t cb = Vec3_Direction(c->position, b->position);

		const float dot = Vec3_Dot(ba, cb);
		if (dot > 1.f - COLINEAR_EPSILON) {
			continue;
		}

		PhongVertex(face, b, phong_cosine);
	}

	// FIXME: There is a corner case here (get it?) where multiple colinear vertexes on a Phong
	// FIXME: shaded face will receive bad normals. A complete solution here would be to copy what
	// FIXME: Cm_ElementsForWinding does, and actually flag the corners of the winding, and then
	// FIXME: linear interpolate all non-corner vertex normals in this loop between their two
	// FIXME: bounding corners.

	for (int32_t i = 0; i < face->num_vertexes; i++) {

		bsp_vertex_t *a = v + ((i + 0) % face->num_vertexes);
		bsp_vertex_t *b = v + ((i + 1) % face->num_vertexes);
		bsp_vertex_t *c = v + ((i + 2) % face->num_vertexes);

		vec3_t ba, cb;

		const float ba_dist = fabsf(Vec3_DistanceDir(b->position, a->position, &ba));
		const float cb_dist = fabsf(Vec3_DistanceDir(c->position, b->position, &cb));

		const float dot = Vec3_Dot(ba, cb);
		if (dot <= 1.f - COLINEAR_EPSILON) {
			continue;
		}

		b->normal = Vec3_Normalize(Vec3_Mix(a->normal, c->normal, ba_dist / (ba_dist + cb_dist)));
	}
}

/**
 * @brief Calculates Phong shading, updating vertex normal vectors.
 */
void PhongShading(void) {

	if (no_phong) {
		return;
	}

	face_entities = Mem_Malloc(sizeof(entity_t *) * bsp_file.num_faces);

	const bsp_face_t *face = bsp_file.faces;
	for (int32_t i = 0; i < bsp_file.num_faces; i++, face++) {

		const bsp_brush_t *brush = bsp_file.brushes;
		for (int32_t j = 0; j < bsp_file.num_brushes; j++, brush++) {

			if (face->brush_side >= brush->first_brush_side
				&& face->brush_side < brush->first_brush_side + brush->num_brush_sides) {
				face_entities[i] = entities + brush->entity;
				break;
			}
		}

		assert(face_entities[i]);
	}

	Work("Phong shading", PhongFace, bsp_file.num_faces);

	Mem_Free(face_entities);
}

/**
 * @brief Calculates tangent and bitangent vectors from Phong-interpolated normals.
 */
void TangentVectors(void) {

	cm_vertex_t *vertexes = Mem_Malloc(sizeof(cm_vertex_t) * bsp_file.num_vertexes);

	bsp_vertex_t *v = bsp_file.vertexes;
	for (int32_t i = 0; i < bsp_file.num_vertexes; i++, v++) {
		vertexes[i] = (cm_vertex_t) {
			.position = &v->position,
			.normal = &v->normal,
			.tangent = &v->tangent,
			.bitangent = &v->bitangent,
			.st = &v->diffusemap
		};
	}

	Cm_Tangents(vertexes, bsp_file.num_vertexes, bsp_file.elements, bsp_file.num_elements);

	int32_t num_bad_vertexes = 0;

	v = bsp_file.vertexes;
	for (int32_t i = 0; i < bsp_file.num_vertexes; i++, v++) {

		if (vertexes[i].num_tris == 0) {
			continue;
		}

		if (Vec3_Length(v->tangent) < .9f || Vec3_Length(v->bitangent) < .9f) {

			const int32_t *e = bsp_file.elements;
			for (int32_t j = 0; j < bsp_file.num_elements; j += 3, e += 3) {

				if (e[0] == i || e[1] == i || e[2] == i) {

					const vec3_t tri[3] = {
						bsp_file.vertexes[e[0]].position,
						bsp_file.vertexes[e[1]].position,
						bsp_file.vertexes[e[2]].position
					};

					const char *msg = va("Vertex at %s has invalid tangents", vtos(v->position));
					Mon_SendWinding(MON_WARN, tri, 3, msg);

					break;
				}
			}

			num_bad_vertexes++;
		}
	}

	Com_Debug(DEBUG_ALL, "%d bad vertexes\n", num_bad_vertexes);

	Mem_Free(vertexes);
}
