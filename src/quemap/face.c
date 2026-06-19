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

#include <Objectively/HashTable.h>
#include <Objectively/Vector.h>

#include "bsp.h"
#include "face.h"
#include "map.h"
#include "material.h"
#include "tree.h"
#include "qbsp.h"

/**
 * @brief Allocates a new face.
 */
face_t *AllocFace(void) {

  return Mem_TagMalloc(sizeof(face_t), (mem_tag_t) MEM_TAG_FACE);
}

/**
 * @brief Frees the face and its winding.
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
 * @return `NULL` if the faces couldn't be merged, or the new face.
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

static HashTable *welding_spatial_hash;

/**
 * @brief Destroys a Vector stored as a value in the welding spatial hash.
 */
static void WeldingSpatialHashValueDestroyFunc(ident ptr) {
  release(ptr);
}

/**
 * @brief Rounds a floating-point position to the nearest integer grid cell for spatial hashing.
 */
static vec3i_t GetWeldingPoint(const vec3_t p) {
  return Vec3i(roundf(p.x), roundf(p.y), roundf(p.z));
}

/**
 * @brief Hash function for integer grid-cell keys used by the welding spatial hash.
 */
static size_t WeldingSpatialHashFunc(const ident ptr_) {
  const vec3i_t *ptr = ptr_;
  const uint32_t x = (uint32_t) roundf((MAX_WORLD_COORD + ptr->x) * .5f);
  const uint32_t y = (uint32_t) roundf((MAX_WORLD_COORD + ptr->y) * .5f);
  const uint32_t z = (uint32_t) roundf((MAX_WORLD_COORD + ptr->z) * .25f);

  return x | (y << 12) | (z << 24);
}

/**
 * @brief Equality function for integer grid-cell keys used by the welding spatial hash.
 */
static bool WeldingSpatialHashEqualFunc(const ident a_, const ident b_) {
  const vec3i_t *a = a_;
  const vec3i_t *b = b_;
  return a->x == b->x && a->y == b->y && a->z == b->z;
}

/**
 * @brief Clears all entries from the welding spatial hash, or initializes it on first call.
 */
void ClearWeldingSpatialHash(void) {

  if (welding_spatial_hash) {
    release(welding_spatial_hash);
  }

  welding_spatial_hash = $(alloc(HashTable), init, WeldingSpatialHashFunc, WeldingSpatialHashEqualFunc);
  welding_spatial_hash->destroyKey = (HashTableDestroyFunc) Mem_Free;
  welding_spatial_hash->destroyValue = (HashTableDestroyFunc) WeldingSpatialHashValueDestroyFunc;
}

/**
 * @brief Returns true if the bucket already contains a vertex with the same BSP position.
 */
static bool WeldingBucketContainsIndex(const Vector *array, int32_t index) {
  for (size_t i = 0; i < array->count; i++) {
    const int32_t existing = *VectorElement((Vector *) array, int32_t, i);
    if (existing == index) {
      return true;
    }

    if (Vec3_Equal(bsp_file.vertexes[existing].position, bsp_file.vertexes[index].position)) {
      return true;
    }
  }

  return false;
}

/**
 * @brief Inserts a vertex position and its BSP index into the welding spatial hash.
 */
static void AddVertexToWeldingSpatialHash(const vec3_t v, const int32_t index) {
  const vec3i_t spatial = GetWeldingPoint(v);
  Vector *array = $(welding_spatial_hash, get, (ident) &spatial);

  if (!array) {
    array = $(alloc(Vector), initWithSize, sizeof(int32_t));

    vec3i_t *key_copy = Mem_Malloc(sizeof(*key_copy));
    *key_copy = spatial;

    $(welding_spatial_hash, set, key_copy, array);
  }

  if (!WeldingBucketContainsIndex(array, index)) {
    int32_t element = index;
    $(array, addElement, &element);
  }
}

int32_t num_welds = 0;

/**
 * @brief Searches the welding spatial hash for an existing vertex within `VERTEX_EPSILON` of the input, returning it in out.
 */
static void FindWeldingSpatialHashPoint(const vec3_t in, vec3_t *out) {
  static const int32_t offsets[] = { 0, 1, -1 };

  float best_dist = VERTEX_EPSILON * VERTEX_EPSILON;
  int32_t best_index = -1;

  for (int32_t z = 0; z < (int32_t) lengthof(offsets); z++) {
    for (int32_t y = 0; y < (int32_t) lengthof(offsets); y++) {
      for (int32_t x = 0; x < (int32_t) lengthof(offsets); x++) {
        const vec3i_t key = GetWeldingPoint(Vec3(in.x + offsets[x], in.y + offsets[y], in.z + offsets[z]));
        Vector *array = $(welding_spatial_hash, get, (ident) &key);

        if (!array) {
          continue;
        }

        for (size_t i = 0; i < array->count; i++) {
          const int32_t idx = *VectorElement(array, int32_t, i);
          const vec3_t pos = bsp_file.vertexes[idx].position;
          const float dist = Vec3_DistanceSquared(pos, in);

          if (dist < best_dist || (dist == best_dist && idx < best_index)) {
            best_dist = dist;
            best_index = idx;
          }
        }
      }
    }
  }

  if (best_index >= 0) {
    *out = bsp_file.vertexes[best_index].position;
    num_welds++;
  } else {
    *out = in;
  }
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

  const vec3_t sdir = brush_side->axis[0].xyz;
  const vec3_t tdir = brush_side->axis[1].xyz;

  const SDL_Surface *diffusemap = materials[brush_side->material].diffusemap;
  
  vec3_t points[face->w->num_points];
  int32_t num_points = face->w->num_points;

  if (no_weld) {
    memcpy(points, face->w->points, face->w->num_points * sizeof(face->w->points[0]));
  } else {
    num_points = WeldWinding(face->w, points);
    if (num_points < 3) {
      const material_t *mat = &materials[face->brush_side->material];
      const vec3_t center = Cm_WindingCenter(face->w);
      Com_Warn("Malformed face %s @ %s after welding\n", mat->cm->name, vtos(center));
      return 0;
    }
  }

  for (int32_t i = 0; i < num_points; i++) {

    if (bsp_file.num_vertexes == MAX_BSP_VERTEXES) {
      Com_Error(ERROR_FATAL, "MAX_BSP_VERTEXES\n");
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
    const material_t *mat = &materials[face->brush_side->material];
    const vec3_t center = Cm_WindingCenter(face->w);
    Com_Warn("Face %s @ %s has degenerate winding\n", mat->cm->name, vtos(center));
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
 * @brief Emits the given face into the BSP file, writing its vertex array and element indices.
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
  out->patch = -1;
  out->plane = face->plane;
  out->block = -1;
  
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

#define MAX_VERTEX_FACES 64

static const bsp_model_t *phong_model;
static float phong_cosine;

// Pre-built for PhongShading: position -> Vector*(bsp_face_t*) and brush_side* -> winding*
static HashTable *phong_vertex_faces;
static HashTable *phong_brush_side_windings;

static void ReleaseObject(ident object) {
  release(object);
}

/**
 * @brief Builds lookup tables used by Phong shading for O(1) vertex-to-face and brush-side-to-winding queries.
 */
static void BuildPhongMaps(const bsp_model_t *mod) {

  phong_vertex_faces = $(alloc(HashTable), init, WeldingSpatialHashFunc, WeldingSpatialHashEqualFunc);
  phong_vertex_faces->destroyKey = (HashTableDestroyFunc) Mem_Free;
  phong_vertex_faces->destroyValue = ReleaseObject;

  const bsp_face_t *f = &bsp_file.faces[mod->first_face];
  for (int32_t i = 0; i < mod->num_faces; i++, f++) {
    if (f->plane == -1) {
      continue;
    }
    const bsp_vertex_t *v = &bsp_file.vertexes[f->first_vertex];
    for (int32_t j = 0; j < f->num_vertexes; j++, v++) {
      const vec3i_t key = GetWeldingPoint(v->position);
      Vector *arr = $(phong_vertex_faces, get, (ident) &key);
      if (!arr) {
        arr = $(alloc(Vector), initWithSize, sizeof(bsp_face_t *));
        vec3i_t *key_copy = Mem_Malloc(sizeof(*key_copy));
        *key_copy = key;
        $(phong_vertex_faces, set, key_copy, arr);
      }
      const bsp_face_t *face = f;
      $(arr, addElement, &face);
    }
  }

  phong_brush_side_windings = $(alloc(HashTable), init, HashTableHashDirect, HashTableEqualDirect);
  const brush_side_t *map_side = brush_sides;
  for (int32_t j = 0; j < num_brush_sides; j++, map_side++) {
    if (map_side->out && map_side->winding) {
      $(phong_brush_side_windings, set, (void *) map_side->out, map_side->winding);
    }
  }
}

static void FreePhongMaps(void) {
  release(phong_vertex_faces);
  phong_vertex_faces = NULL;
  release(phong_brush_side_windings);
  phong_brush_side_windings = NULL;
}

/**
 * @brief Populates faces with pointers to all those referencing the vertex.
 * @return The count of faces referencing the vertex.
 */
static size_t FacesForVertex(const bsp_face_t *face, const bsp_vertex_t *vertex, const bsp_face_t **faces) {

  const vec3i_t key = GetWeldingPoint(vertex->position);
  const Vector *arr = $(phong_vertex_faces, get, (ident) &key);
  if (!arr) {
    return 0;
  }

  size_t count = 0;
  for (size_t i = 0; i < arr->count; i++) {
    faces[count++] = *VectorElement((Vector *) arr, const bsp_face_t *, i);
    if (count == MAX_VERTEX_FACES) {
      Com_Warn("Vertex @ %s is shared by too many faces.\n", vtos(vertex->position));
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
        if (faces[j]->brush_side == (*f)->brush_side && faces[j]->plane == (*f)->plane) {
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

      cm_winding_t *w = $(phong_brush_side_windings, get, (ident) s);
      if (!w) {
        continue;
      }

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
static void PhongFace(int32_t model_face_num) {

  const int32_t face_num = phong_model->first_face + model_face_num;
  const bsp_face_t *face = bsp_file.faces + face_num;

  // Skip patch faces (they have correct normals from Bézier evaluation)
  if (face->plane == -1) {
    return;
  }

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
 * @brief Calculates Phong shading for the brush faces of the given model.
 */
void PhongShading(const bsp_model_t *mod) {

  if (no_phong) {
    return;
  }

  if (mod->num_faces == 0) {
    return;
  }

  phong_model = mod;

  const entity_t *entity = &entities[mod->entity];
  const float phong_angle = atof(ValueForKey(entity, "phong", "60"));

  phong_cosine = cosf(Radians(phong_angle));

  BuildPhongMaps(mod);
  Work("Phong shading", PhongFace, mod->num_faces);
  FreePhongMaps();

  phong_model = NULL;
  phong_cosine = 0.f;
}

/**
 * @brief Computes per-vertex tangent and bitangent vectors for all faces in the BSP file.
 */
static void TangentVectors_(bsp_model_t *model) {

  if (model->num_faces == 0) {
    return;
  }

  bsp_face_t *face = bsp_file.faces + model->first_face;
  int32_t base_vertex = face->first_vertex;

  bsp_vertex_t *vertexes = bsp_file.vertexes + base_vertex;
  int32_t *elements = bsp_file.elements + face->first_element;

  int32_t num_vertexes = 0, num_elements = 0;
  for (int32_t i = 0; i < model->num_faces; i++, face++) {
    num_vertexes += face->num_vertexes;
    num_elements += face->num_elements;
  }

  cm_vertex_t *cm = Mem_Malloc(sizeof(cm_vertex_t) * num_vertexes);

  bsp_vertex_t *v = vertexes;
  for (int32_t i = 0; i < num_vertexes; i++, v++) {
    cm[i] = (cm_vertex_t) {
      .position = &v->position,
      .normal = &v->normal,
      .tangent = &v->tangent,
      .bitangent = &v->bitangent,
      .st = &v->diffusemap
    };
  }

  Cm_Tangents(cm, base_vertex, num_vertexes, elements, num_elements);

  int32_t num_bad_vertexes = 0;

  v = vertexes;
  for (int32_t i = 0; i < num_vertexes; i++, v++) {

    if (cm[i].num_tris == 0) {
      continue;
    }

    if (Vec3_Length(v->tangent) < .9f || Vec3_Length(v->bitangent) < .9f) {
      Com_Warn("Vertex at %s has invalid tangents\n", vtos(v->position));
      num_bad_vertexes++;
    }
  }

  Com_Debug(DEBUG_ALL, "%d bad vertexes\n", num_bad_vertexes);

  Mem_Free(cm);
}

/**
 * @brief Calculates tangent and bitangent vectors from Phong-interpolated normals.
 */
void TangentVectors(void) {

  bsp_model_t *model = bsp_file.models;
  for (int32_t i = 0; i < bsp_file.num_models; i++, model++) {
    TangentVectors_(model);
  }
}
