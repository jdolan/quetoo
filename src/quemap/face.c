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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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
Face *AllocFace(void) {
  return Mem_TagMalloc(sizeof(Face), (MemTag) MEM_TAG_FACE);
}

/**
 * @brief Frees the face and its winding.
 */
void FreeFace(Face *f) {

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
Face *MergeFaces(Face *a, Face *b) {

  if (a->brushSide != b->brushSide) {
    return NULL;
  }

  if (a->plane != b->plane) {
    return NULL;
  }

  const Plane *plane = &planes[a->plane];
  CmWinding *w = Cm_MergeWindings(a->w, b->w, plane->normal);
  if (!w) {
    return NULL;
  }

  Face *merged = AllocFace();
  merged->brushSide = a->brushSide;
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

/**
 * @brief Rounds a floating-point position to the nearest integer grid cell for spatial hashing.
 */
static Vec3i GetVertexGridPoint(const Vec3 p) {
  return MakeVec3i(roundf(p.x), roundf(p.y), roundf(p.z));
}

/**
 * @brief Hash function for integer grid-cell keys used by the vertex spatial hash.
 */
static size_t VertexGridHashFunc(const ident ptr_) {
  const Vec3i *ptr = ptr_;
  const uint32_t x = (uint32_t) roundf((MAX_WORLD_COORD + ptr->x) * .5f);
  const uint32_t y = (uint32_t) roundf((MAX_WORLD_COORD + ptr->y) * .5f);
  const uint32_t z = (uint32_t) roundf((MAX_WORLD_COORD + ptr->z) * .25f);

  return x | (y << 12) | (z << 24);
}

/**
 * @brief Equality function for integer grid-cell keys used by the vertex spatial hash.
 */
static bool VertexGridEqualFunc(const ident a_, const ident b_) {
  const Vec3i *a = a_;
  const Vec3i *b = b_;
  return a->x == b->x && a->y == b->y && a->z == b->z;
}

/**
 * @brief Emits a vertex array for the given face.
 */
static int32_t EmitFaceVertexes(const Face *face) {
  const BrushSide *brushSide = face->brushSide;

  const Vec3 sdir = brushSide->axis[0].xyz;
  const Vec3 tdir = brushSide->axis[1].xyz;

  const SDL_Surface *diffusemap = materials[brushSide->material].diffusemap;
  
  const Vec3 *points = face->w->points;
  const int32_t numPoints = face->w->numPoints;

  for (int32_t i = 0; i < numPoints; i++) {

    if (bspFile.numVertexes == MAX_BSP_VERTEXES) {
      Com_Error(ERROR_FATAL, "MAX_BSP_VERTEXES\n");
    }

    BspVertex out = {
      .position = points[i],
      .normal = planes[face->plane].normal
    };

    const float s = Vec3_Dot(points[i], sdir) + brushSide->axis[0].w;
    const float t = Vec3_Dot(points[i], tdir) + brushSide->axis[1].w;

    out.diffusemap.x = s / (diffusemap ? diffusemap->w : 1.f);
    out.diffusemap.y = t / (diffusemap ? diffusemap->h : 1.f);

    switch (brushSide->surface & SURF_MASK_BLEND) {
      case SURF_BLEND_33:
        out.color = MakeColor32(255, 255, 255, 255 * .33f);
        break;
      case SURF_BLEND_66:
        out.color = MakeColor32(255, 255, 255, 255 * .66f);
        break;
      default:
        out.color = MakeColor32(255, 255, 255, 255);
        break;
    }

    bspFile.vertexes[bspFile.numVertexes] = out;
    bspFile.numVertexes++;
  }

  return numPoints;
}

/**
 * @brief Emits the given face into the BSP file, writing its vertex array and element indices.
 * @details The winding is triangulated before anything is written, and a face with no triangles
 * is not emitted: decals and flares have no use for it.
 * @return The emitted face, or `NULL` if the face has no triangles.
 */
BspFace *EmitFace(const Face *face) {

  assert(face->w->numPoints > 2);
  assert(face->brushSide->material >= 0);
  assert(face->brushSide->out);

  int32_t elements[(face->w->numPoints - 2) * 3];
  const int32_t numElements = Cm_ElementsForWinding(face->w, elements);

  if (numElements != (int32_t) lengthof(elements)) {
    const Material *mat = &materials[face->brushSide->material];
    const Vec3 center = Cm_WindingCenter(face->w);
    Com_Warn("Face %s @ %s has degenerate winding\n", mat->cm->name, vtos(center));
  }

  if (numElements == 0) {
    return NULL;
  }

  if (bspFile.numFaces == MAX_BSP_FACES) {
    Com_Error(ERROR_FATAL, "MAX_BSP_FACES\n");
  }

  BspFace *out = &bspFile.faces[bspFile.numFaces];

  out->brushSide = (int32_t) (ptrdiff_t) (face->brushSide->out - bspFile.brushSides);
  out->patch = -1;
  out->plane = face->plane;
  out->block = -1;
  
  out->bounds = Box3_Null();

  out->firstVertex = bspFile.numVertexes;
  out->numVertexes = EmitFaceVertexes(face);

  if (out->numVertexes == 0) {
    return NULL;
  }

  bspFile.numFaces++;

  const BspVertex *v = bspFile.vertexes + out->firstVertex;
  for (int32_t i = 0; i < out->numVertexes; i++, v++) {
    out->bounds = Box3_Append(out->bounds, v->position);
  }

  out->firstElement = bspFile.numElements;
  out->numElements = numElements;

  if (bspFile.numElements + numElements > MAX_BSP_ELEMENTS) {
    Com_Error(ERROR_FATAL, "MAX_BSP_ELEMENTS\n");
  }

  for (int32_t i = 0; i < numElements; i++) {
    bspFile.elements[bspFile.numElements++] = out->firstVertex + elements[i];
  }

  return out;
}

#define MAX_VERTEX_FACES 64

static const BspModel *phongModel;
static float phongCosine;

// Pre-built for PhongShading: position -> Vector*(BspFace*) and brushSide* -> winding*
static HashTable *phongVertexFaces;
static HashTable *phongBrushSideWindings;

static void ReleaseObject(ident object) {
  release(object);
}

/**
 * @brief Builds lookup tables used by Phong shading for O(1) vertex-to-face and brush-side-to-winding queries.
 */
static void BuildPhongMaps(const BspModel *mod) {

  phongVertexFaces = $(alloc(HashTable), init, VertexGridHashFunc, VertexGridEqualFunc);
  phongVertexFaces->destroyKey = Mem_Free;
  phongVertexFaces->destroyValue = ReleaseObject;

  const BspFace *f = &bspFile.faces[mod->firstFace];
  for (int32_t i = 0; i < mod->numFaces; i++, f++) {
    if (f->plane == -1) {
      continue;
    }
    const BspVertex *v = &bspFile.vertexes[f->firstVertex];
    for (int32_t j = 0; j < f->numVertexes; j++, v++) {
      const Vec3i key = GetVertexGridPoint(v->position);
      Vector *arr = $(phongVertexFaces, get, (ident) &key);
      if (!arr) {
        arr = $(alloc(Vector), initWithSize, sizeof(BspFace *));
        Vec3i *keyCopy = Mem_Malloc(sizeof(*keyCopy));
        *keyCopy = key;
        $(phongVertexFaces, set, keyCopy, arr);
      }
      const BspFace *face = f;
      $(arr, add, &face);
    }
  }

  phongBrushSideWindings = $(alloc(HashTable), init, HashTableHashDirect, HashTableEqualDirect);
  const BrushSide *mapSide = brushSides;
  for (int32_t j = 0; j < numBrushSides; j++, mapSide++) {
    if (mapSide->out && mapSide->winding) {
      $(phongBrushSideWindings, set, (void *) mapSide->out, mapSide->winding);
    }
  }
}

static void FreePhongMaps(void) {
  phongVertexFaces = release(phongVertexFaces);
  phongBrushSideWindings = release(phongBrushSideWindings);
}

/**
 * @return True if @p face has a vertex within `VERTEX_EPSILON` of @p position.
 * @details The distance is the one that welding used, so that Phong shading still smooths across
 * a chamfer too narrow to see.
 */
static bool FaceHasVertex(const BspFace *face, const Vec3 position) {

  const BspVertex *v = bspFile.vertexes + face->firstVertex;
  for (int32_t i = 0; i < face->numVertexes; i++, v++) {
    if (Vec3_DistanceSquared(v->position, position) <= VERTEX_EPSILON * VERTEX_EPSILON) {
      return true;
    }
  }

  return false;
}

/**
 * @brief Populates faces with pointers to all those referencing the vertex.
 * @details The grid cell of the vertex and its 26 neighbors are searched, and a face counts only
 * if it has a vertex within `VERTEX_EPSILON` of this one. Two vertexes that are shared can round
 * to different cells, and two that round to one cell need not be shared.
 * @return The count of faces referencing the vertex.
 */
static size_t FacesForVertex(const BspFace *face, const BspVertex *vertex, const BspFace **faces) {

  const Vec3i cell = GetVertexGridPoint(vertex->position);

  size_t count = 0;

  for (int32_t z = -1; z <= 1; z++) {
    for (int32_t y = -1; y <= 1; y++) {
      for (int32_t x = -1; x <= 1; x++) {

        const Vec3i key = MakeVec3i(cell.x + x, cell.y + y, cell.z + z);
        const Vector *arr = $(phongVertexFaces, get, (ident) &key);
        if (!arr) {
          continue;
        }

        for (size_t i = 0; i < arr->count; i++) {
          const BspFace *f = VectorValue((Vector *) arr, const BspFace *, i);

          size_t j;
          for (j = 0; j < count; j++) {
            if (faces[j] == f) {
              break;
            }
          }

          if (j < count || !FaceHasVertex(f, vertex->position)) {
            continue;
          }

          if (count == MAX_VERTEX_FACES) {
            Com_Warn("Vertex @ %s is shared by too many faces.\n", vtos(vertex->position));
            return count;
          }

          faces[count++] = f;
        }
      }
    }
  }

  return count;
}

/**
 * @brief Calculate per-vertex (instead of per-plane) normal vectors. This is done by finding all of
 * the faces which share a given vertex, and calculating a weighted average of their normals.
 */
static void PhongVertex(const BspFace *face, BspVertex *v, float phongCosine) {
  const BspFace *faces[MAX_VERTEX_FACES];

  const BspBrushSide *side = &bspFile.brushSides[face->brushSide];
  const BspPlane *plane = &bspFile.planes[face->plane];

  const size_t count = FacesForVertex(face, v, faces);
  if (count > 1) {

    v->normal = Vec3_Zero();

    const BspFace **f = faces;
    for (size_t i = 0; i < count; i++, f++) {

      size_t j;
      for (j = 0; j < i; j++) {
        if (faces[j]->brushSide == (*f)->brushSide && faces[j]->plane == (*f)->plane) {
          break;
        }
      }

      if (j < i) {
        continue;
      }

      const BspBrushSide *s = &bspFile.brushSides[(*f)->brushSide];
      const BspPlane *p = &bspFile.planes[(*f)->plane];

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
        if (phongCosine > 0.f && dot > phongCosine) {
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

      CmWinding *w = $(phongBrushSideWindings, get, (ident) s);
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
static void PhongFace(int32_t modelFaceNum) {

  const int32_t faceNum = phongModel->firstFace + modelFaceNum;
  const BspFace *face = bspFile.faces + faceNum;

  // Skip patch faces (they have correct normals from Bézier evaluation)
  if (face->plane == -1) {
    return;
  }

  BspVertex *v = bspFile.vertexes + face->firstVertex;

  for (int32_t i = 0; i < face->numVertexes; i++) {

    BspVertex *a = v + ((i + 0) % face->numVertexes);
    BspVertex *b = v + ((i + 1) % face->numVertexes);
    BspVertex *c = v + ((i + 2) % face->numVertexes);

    const Vec3 ba = Vec3_Direction(b->position, a->position);
    const Vec3 cb = Vec3_Direction(c->position, b->position);

    const float dot = Vec3_Dot(ba, cb);
    if (dot > 1.f - COLINEAR_EPSILON) {
      continue;
    }

    PhongVertex(face, b, phongCosine);
  }

  // FIXME: There is a corner case here (get it?) where multiple colinear vertexes on a Phong
  // FIXME: shaded face will receive bad normals. A complete solution here would be to copy what
  // FIXME: Cm_ElementsForWinding does, and actually flag the corners of the winding, and then
  // FIXME: linear interpolate all non-corner vertex normals in this loop between their two
  // FIXME: bounding corners.

  for (int32_t i = 0; i < face->numVertexes; i++) {

    BspVertex *a = v + ((i + 0) % face->numVertexes);
    BspVertex *b = v + ((i + 1) % face->numVertexes);
    BspVertex *c = v + ((i + 2) % face->numVertexes);

    Vec3 ba, cb;

    const float baDist = fabsf(Vec3_DistanceDir(b->position, a->position, &ba));
    const float cbDist = fabsf(Vec3_DistanceDir(c->position, b->position, &cb));

    const float dot = Vec3_Dot(ba, cb);
    if (dot <= 1.f - COLINEAR_EPSILON) {
      continue;
    }

    b->normal = Vec3_Normalize(Vec3_Mix(a->normal, c->normal, baDist / (baDist + cbDist)));
  }
}

/**
 * @brief Calculates Phong shading for the brush faces of the given model.
 */
void PhongShading(const BspModel *mod) {

  if (noPhong) {
    return;
  }

  if (mod->numFaces == 0) {
    return;
  }

  phongModel = mod;

  const Entity *entity = &entities[mod->entity];
  const float phongAngle = atof(ValueForKey(entity, "phong", "60"));

  phongCosine = cosf(Radians(phongAngle));

  BuildPhongMaps(mod);
  Work("Phong shading", PhongFace, mod->numFaces);
  FreePhongMaps();

  phongModel = NULL;
  phongCosine = 0.f;
}

/**
 * @brief Computes per-vertex tangent and bitangent vectors for all faces in the BSP file.
 */
static void TangentVectors_(BspModel *model) {

  if (model->numFaces == 0) {
    return;
  }

  BspFace *face = bspFile.faces + model->firstFace;
  int32_t baseVertex = face->firstVertex;

  BspVertex *vertexes = bspFile.vertexes + baseVertex;
  int32_t *elements = bspFile.elements + face->firstElement;

  int32_t numVertexes = 0, numElements = 0;
  for (int32_t i = 0; i < model->numFaces; i++, face++) {
    numVertexes += face->numVertexes;
    numElements += face->numElements;
  }

  CmVertex *cm = Mem_Malloc(sizeof(CmVertex) * numVertexes);

  BspVertex *v = vertexes;
  for (int32_t i = 0; i < numVertexes; i++, v++) {
    cm[i] = (CmVertex) {
      .position = &v->position,
      .normal = &v->normal,
      .tangent = &v->tangent,
      .bitangent = &v->bitangent,
      .st = &v->diffusemap
    };
  }

  Cm_Tangents(cm, baseVertex, numVertexes, elements, numElements);

  int32_t numBadVertexes = 0;

  v = vertexes;
  for (int32_t i = 0; i < numVertexes; i++, v++) {

    if (cm[i].numTris == 0) {
      continue;
    }

    if (Vec3_Length(v->tangent) < .9f || Vec3_Length(v->bitangent) < .9f) {
      Com_Warn("Vertex at %s has invalid tangents\n", vtos(v->position));
      numBadVertexes++;
    }
  }

  Com_Debug(DEBUG_ALL, "%d bad vertexes\n", numBadVertexes);

  Mem_Free(cm);
}

/**
 * @brief Calculates tangent and bitangent vectors from Phong-interpolated normals.
 */
void TangentVectors(void) {

  BspModel *model = bspFile.models;
  for (int32_t i = 0; i < bspFile.numModels; i++, model++) {
    TangentVectors_(model);
  }
}
