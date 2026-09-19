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

#include <Objectively/Vector.h>

#include "bsp.h"
#include "face.h"
#include "map.h"
#include "material.h"
#include "patch.h"
#include "portal.h"
#include "qbsp.h"
#include "writebsp.h"

/**
 * @brief The portal faces emitted so far, and the draw elements each was emitted to.
 * @remarks Resolved into the portals lump by `EmitPortals`, once the entities it must target
 * are known to be complete.
 */
static struct {
  const BspFace *face;
  int32_t drawElements;
} portal_faces[MAX_BSP_PORTALS];

static int32_t num_portal_faces;

/**
 * @brief Writes all compiler planes to the BSP planes lump.
 */
void EmitPlanes(void) {

  bsp_file.numPlanes = 0;

  const Plane *p = planes;
  for (int32_t i = 0; i < num_planes; i++, p++) {
    BspPlane *out = &bsp_file.planes[bsp_file.numPlanes];

    out->normal = p->normal;
    out->dist = (float) p->dist;

    bsp_file.numPlanes++;

    Progress("Emitting planes", 100.f * i / num_planes);
  }
}

/**
 * @brief Writes all loaded materials to the BSP materials lump.
 */
void EmitMaterials(void) {

  const Material *m = materials;
  for (int32_t i = 0; i < num_materials; i++, m++) {
    BspMaterial *out = &bsp_file.materials[bsp_file.numMaterials];

    const char *name = m->cm->name;
    if (!q_strncmp(name, "textures/", 9)) {
      name += q_strlen("textures/");
    }
    q_strlcpy(out->name, name, sizeof(out->name));

    bsp_file.numMaterials++;

    Progress("Emitting materials", 100.f * i / num_materials);
  }
}

/**
 * @brief Resolves the material index for the given BSP face.
 */
static inline int32_t FaceMaterial(const BspFace *face) {
  if (face->brushSide >= 0) {
    return bsp_file.brushSides[face->brushSide].material;
  }
  return bsp_file.patches[face->patch].material;
}

/**
 * @brief Resolves the contents mask for the given BSP face.
 */
static inline int32_t FaceContents(const BspFace *face) {
  if (face->brushSide >= 0) {
    return bsp_file.brushSides[face->brushSide].contents;
  }
  return bsp_file.patches[face->patch].contents;
}

/**
 * @brief Resolves the surface mask for the given BSP face.
 */
static inline int32_t FaceSurface(const BspFace *face) {
  if (face->brushSide >= 0) {
    return bsp_file.brushSides[face->brushSide].surface;
  }
  return bsp_file.patches[face->patch].surface;
}

/**
 * @brief Emits brush faces for the given node.
 */
static int32_t EmitFaces(const Node *node, int32_t nodeNum) {

  const int32_t numFaces = bsp_file.numFaces;

  for (Face *face = node->faces; face; face = face->next) {

    if (face->merged) {
      continue;
    }

    face->out = EmitFace(face);
    face->out->node = nodeNum;
  }

  // Emit pre-tessellated patch faces assigned to this node
  for (PatchFace *pf = node->patchFaces; pf; pf = pf->next) {

    if (bsp_file.numFaces >= MAX_BSP_FACES) {
      Com_Error(ERROR_FATAL, "MAX_BSP_FACES\n");
    }

    BspFace *out = &bsp_file.faces[bsp_file.numFaces];
    memset(out, 0, sizeof(*out));
    bsp_file.numFaces++;

    pf->out = out;

    out->brushSide = -1;
    out->patch = -1;  // set by EmitPatches after BSP patch index is assigned
    out->plane = -1;
    out->node = nodeNum;
    out->block = -1;
    out->bounds = pf->bounds;

    // Copy vertexes to bsp_file
    out->firstVertex = bsp_file.numVertexes;
    out->numVertexes = pf->numVertexes;

    if (bsp_file.numVertexes + pf->numVertexes > MAX_BSP_VERTEXES) {
      Com_Error(ERROR_FATAL, "MAX_BSP_VERTEXES\n");
    }

    memcpy(&bsp_file.vertexes[bsp_file.numVertexes], pf->vertexes,
           pf->numVertexes * sizeof(BspVertex));
    bsp_file.numVertexes += pf->numVertexes;

    // Copy elements to bsp_file, adjusting indices
    out->firstElement = bsp_file.numElements;
    out->numElements = pf->numElements;

    if (bsp_file.numElements + pf->numElements > MAX_BSP_ELEMENTS) {
      Com_Error(ERROR_FATAL, "MAX_BSP_ELEMENTS\n");
    }

    for (int32_t e = 0; e < pf->numElements; e++) {
      bsp_file.elements[bsp_file.numElements++] = out->firstVertex + pf->elements[e];
    }
  }

  return bsp_file.numFaces - numFaces;
}

/**
 * @brief Emits a leaf node into the BSP leaves lump, writing its contents, bounds, and leaf-brush references.
 * @return The index of the new leaf in `bsp_file`.leafs`.
 */
static int32_t EmitLeaf(Node *node) {

  if (bsp_file.numLeafs == MAX_BSP_LEAFS) {
    Com_Error(ERROR_FATAL, "MAX_BSP_LEAFS\n");
  }

  BspLeaf *out = &bsp_file.leafs[bsp_file.numLeafs];
  bsp_file.numLeafs++;

  out->contents = node->contents;
  out->bounds = node->bounds;

  // write the leaf_brushes
  out->firstLeafBrush = bsp_file.numLeafBrushes;

  for (const CsgBrush *brush = node->brushes; brush; brush = brush->next) {

    if (bsp_file.numLeafBrushes >= MAX_BSP_LEAF_BRUSHES) {
      Com_Error(ERROR_FATAL, "MAX_BSP_LEAF_BRUSHES\n");
    }

    assert(brush->original);
    assert(brush->original->out);

    const int32_t brushNum = (int32_t) (ptrdiff_t) (brush->original->out - bsp_file.brushes);

    int32_t i;
    for (i = out->firstLeafBrush; i < bsp_file.numLeafBrushes; i++) {
      if (bsp_file.leafBrushes[i] == brushNum) {
        break;
      }
    }

    if (i == bsp_file.numLeafBrushes) {
      bsp_file.leafBrushes[bsp_file.numLeafBrushes] = brushNum;
      bsp_file.numLeafBrushes++;
    }
  }

  out->numLeafBrushes = bsp_file.numLeafBrushes - out->firstLeafBrush;

  return (int32_t) (ptrdiff_t) (out - bsp_file.leafs);
}

/**
 * @brief Recursively emits a BSP node and its children into the BSP nodes and leafs lumps.
 * @return The index of the emitted node in `bsp_file`.nodes`.
 */
static int32_t EmitNode(const Node *node) {

  if (node->plane == PLANE_LEAF) {
    Com_Error(ERROR_FATAL, "Node does not reference a plane\n");
  }

  if (node->plane & 1) {
    Com_Error(ERROR_FATAL, "Node referencing negative plane\n");
  }

  if (bsp_file.numNodes == MAX_BSP_NODES) {
    Com_Error(ERROR_FATAL, "MAX_BSP_NODES\n");
  }

  Progress("Emitting nodes", -1);

  const int32_t nodeNum = bsp_file.numNodes++;
  BspNode *out = &bsp_file.nodes[nodeNum];

  out->plane = node->plane;
  out->contents = node->contents;
  out->bounds = node->bounds;
  out->visibleBounds = node->visibleBounds;

  out->firstFace = bsp_file.numFaces;
  out->numFaces = EmitFaces(node, nodeNum);

  // recursively output the other nodes
  for (int32_t i = 0; i < 2; i++) {
    if (node->children[i]->plane == PLANE_LEAF) {
      out->children[i] = -(bsp_file.numLeafs + 1);
      EmitLeaf(node->children[i]);
    } else {
      out->children[i] = bsp_file.numNodes;
      EmitNode(node->children[i]);
    }
  }

  return nodeNum;
}

/**
 * @brief Emits the entire BSP tree rooted at `tree->`head_node` into the BSP file.
 * @return The index of the head node in `bsp_file`.nodes`.
 */
int32_t EmitNodes(const Tree *tree) {

  const uint32_t start = (uint32_t) SDL_GetTicks();

  // block nodes define additional planes, ensure they make it into the bsp
  EmitPlanes();

  num_welds = 0;
  ClearWeldingSpatialHash();

  const int32_t node = EmitNode(tree->headNode);

  Com_Verbose("%5i welded vertices\n", num_welds);

  Com_Print("\r%-24s [100%%] %d ms\n", "Emitting nodes", (uint32_t) SDL_GetTicks() - start);

  return node;
}

/**
 * @brief Emits a single brush side into the BSP brush sides lump and returns a pointer to it.
 */
static BspBrushSide *EmitBrushSide(const BrushSide *side) {

  BspBrushSide *out = bsp_file.brushSides + bsp_file.numBrushSides;

  out->plane = side->plane;
  out->material = side->material;

  for (size_t i = 0; i < lengthof(out->axis); i++) {
    out->axis[i] = side->axis[i];
  }

  out->contents = side->contents;
  out->surface = side->surface;
  out->value = side->value;

  return out;
}

/**
 * @brief Emits all sides of a brush into the BSP brush sides lump.
 * @return The number of brush sides emitted.
 */
static int32_t EmitBrushSides(const Brush *brush) {

  BrushSide *side = brush->brushSides;
  for (int32_t i = 0; i < brush->numBrushSides; i++, side++) {

    side->out = EmitBrushSide(side);
    bsp_file.numBrushSides++;
  }

  return brush->numBrushSides;
}

/**
 * @brief Emits a single brush and its sides into the BSP brushes lump and returns a pointer to the emitted brush.
 */
static BspBrush *EmitBrush(const Brush *brush) {

  BspBrush *out = bsp_file.brushes + bsp_file.numBrushes;

  out->entity = brush->entity;
  out->contents = brush->contents;

  out->firstBrushSide = bsp_file.numBrushSides;
  out->numBrushSides = EmitBrushSides(brush);

  out->bounds = brush->bounds;

  return out;
}

/**
 * @brief Emits all brushes to the BSP brushes and brush sides lumps.
 */
void EmitBrushes(void) {

  Brush *brush = brushes;
  for (int32_t i = 0; i < num_brushes; i++, brush++) {

    if (!brush->numBrushSides) {
      continue;
    }

    brush->out = EmitBrush(brush);
    bsp_file.numBrushes++;

    Progress("Emitting brushes", 100.f * i / num_brushes);
  }
}

/**
 * @brief Generates the entity string from all retained entities.
 */
void EmitEntities(void) {

  const uint32_t start = (uint32_t) SDL_GetTicks();

  Bsp_AllocLump(&bsp_file, BSP_LUMP_ENTITIES, MAX_BSP_ENTITIES_SIZE);

  char *out = bsp_file.entityString;
  *out = '\0';

  for (int32_t i = 0; i < num_entities; i++) {
    const EntityKeyValue *e = entities[i].values;
    if (e) {
      q_strlcat(out, "{\n", MAX_BSP_ENTITIES_SIZE);
      while (e) {
        q_strlcat(out, va(" \"%s\" \"%s\"\n", e->key, e->value), MAX_BSP_ENTITIES_SIZE);
        e = e->next;
      }
      q_strlcat(out, "}\n", MAX_BSP_ENTITIES_SIZE);
    }

    Progress("Emitting entities", 100.f * i / num_entities);
  }

  const size_t len = q_strlen(out);

  if (len == MAX_BSP_ENTITIES_SIZE - 1) {
    Com_Error(ERROR_FATAL, "MAX_BSP_ENTITIES_SIZE\n");
  }

  bsp_file.entityStringSize = (int32_t) len + 1;

  Com_Print("\r%-24s [100%%] %d ms\n\n", "Emitting entities", (uint32_t) SDL_GetTicks() - start);
}

/**
 * @brief Allocates BSP lumps and initializes the BSP file structure for writing.
 */
void BeginBSPFile(void) {

  memset(&bsp_file, 0, sizeof(bsp_file));

  Bsp_AllocLump(&bsp_file, BSP_LUMP_MATERIALS, MAX_BSP_MATERIALS);
  Bsp_AllocLump(&bsp_file, BSP_LUMP_PLANES, MAX_BSP_PLANES);
  Bsp_AllocLump(&bsp_file, BSP_LUMP_BRUSH_SIDES, MAX_BSP_BRUSH_SIDES);
  Bsp_AllocLump(&bsp_file, BSP_LUMP_BRUSHES, MAX_BSP_BRUSHES);
  Bsp_AllocLump(&bsp_file, BSP_LUMP_VERTEXES, MAX_BSP_VERTEXES);
  Bsp_AllocLump(&bsp_file, BSP_LUMP_ELEMENTS, MAX_BSP_ELEMENTS);
  Bsp_AllocLump(&bsp_file, BSP_LUMP_FACES, MAX_BSP_FACES);
  Bsp_AllocLump(&bsp_file, BSP_LUMP_NODES, MAX_BSP_NODES);
  Bsp_AllocLump(&bsp_file, BSP_LUMP_LEAF_BRUSHES, MAX_BSP_LEAF_BRUSHES);
  Bsp_AllocLump(&bsp_file, BSP_LUMP_LEAFS, MAX_BSP_LEAFS);
  Bsp_AllocLump(&bsp_file, BSP_LUMP_DRAW_ELEMENTS, MAX_BSP_DRAW_ELEMENTS);
  Bsp_AllocLump(&bsp_file, BSP_LUMP_BLOCKS, MAX_BSP_BLOCKS);
  Bsp_AllocLump(&bsp_file, BSP_LUMP_MODELS, MAX_BSP_MODELS);
  Bsp_AllocLump(&bsp_file, BSP_LUMP_PATCHES, MAX_BSP_PATCHES);

  /*
   * jdolan 2019-01-01
   *
   * Leafs are referenced by their parents as child nodes, but with negative indices.
   * Because zero can not be negated, the first leaf in the map must be padded here.
   * You can choose to ignore this comment if you want to lose 3 days of your life
   * to debugging PVS, like I did.
   */
  bsp_file.numLeafs = 1;
  bsp_file.leafs[0].contents = CONTENTS_SOLID;
}

/**
 * @brief Called after all BSP data has been emitted; reserved for any final BSP file finalization.
 */
/**
 * @return The index of the entity that defined @p brush_side, or `-1`.
 */
static int32_t BrushSideEntity(const int32_t brushSide) {

  const BspBrush *brush = bsp_file.brushes;
  for (int32_t i = 0; i < bsp_file.numBrushes; i++, brush++) {
    if (brushSide >= brush->firstBrushSide &&
      brushSide < brush->firstBrushSide + brush->numBrushSides) {
      return brush->entity;
    }
  }

  return -1;
}

/**
 * @brief Resolves the frame of a portal face.
 * @details The face is the one the mapper approaches, so its outward normal points back at them
 * and travel through the portal runs the other way. Up comes from the way the texture reads,
 * which is the mapper's say over roll on a face whose normal leaves it undefined, as on a floor
 * or ceiling.
 */
static void PortalFaceFrame(const BspFace *face, const BspDrawElements *draw,
                            Vec3 *origin, Vec3 *forward, Vec3 *up) {

  const BspBrushSide *side = &bsp_file.brushSides[face->brushSide];
  const BspPlane *plane = &bsp_file.planes[side->plane];

  // the BSP may have split the portal face into several, all of which share this brush side and
  // were grouped into these draw elements. Their bounds union to the bounds of the face the
  // mapper drew, where averaging their vertexes would lean toward whichever fragment came away
  // with more of the vertexes the split introduced
  *origin = Box3_Center(draw->bounds);

  // that center lies on the face's plane for a rectangle, but not for every shape it could be
  *origin = Vec3_Subtract(*origin, Vec3_Scale(plane->normal, Vec3_Dot(*origin, plane->normal) - plane->dist));

  *forward = Vec3_Negate(plane->normal);

  Vec3 u = Vec3_Negate(side->axis[1].xyz);
  u = Vec3_Subtract(u, Vec3_Scale(plane->normal, Vec3_Dot(u, plane->normal)));

  if (Vec3_Length(u) > FLT_EPSILON) {
    *up = Vec3_Normalize(u);
  } else {
    Vec3_Vectors(Vec3_Euler(*forward), NULL, NULL, up);
  }
}

/**
 * @brief Emits the portals lump, resolving each portal face to the entity it views from.
 * @details A portal that names no target, or names one that does not exist, is dropped with a
 * warning: it has nothing to show, and the renderer would draw a hole in the world.
 */
static void EmitPortals(void) {

  for (int32_t i = 0; i < num_portal_faces; i++) {

    const BspFace *face = portal_faces[i].face;

    const BspDrawElements *draw = &bsp_file.drawElements[portal_faces[i].drawElements];

    Vec3 entryOrigin, entryForward, entryUp;
    PortalFaceFrame(face, draw, &entryOrigin, &entryForward, &entryUp);

    const int32_t e = BrushSideEntity(face->brushSide);
    if (e == -1) {
      Com_Warn("Portal @ %s belongs to no brush, skipping\n", vtos(entryOrigin));
      continue;
    }

    // not "target", which the entity carrying the portal face may already owe to its own class:
    // a func_train reads it as the first path_corner of its route, a func_button as what it fires
    const char *target = ValueForKey(&entities[e], "portal", NULL);
    if (!target) {
      Com_Warn("Portal @ %s has no portal key, skipping\n", vtos(entryOrigin));
      continue;
    }

    const Entity *exit = NULL;
    for (int32_t j = 0; j < num_entities; j++) {
      const char *targetname = ValueForKey(&entities[j], "targetname", NULL);
      if (targetname && !q_strcmp(targetname, target)) {
        exit = &entities[j];
        break;
      }
    }

    if (!exit) {
      Com_Warn("Portal @ %s names missing \"%s\", skipping\n", vtos(entryOrigin), target);
      continue;
    }

    Vec3 angles = VectorForKey(exit, "angles", Vec3_Zero());

    const char *angle = ValueForKey(exit, "angle", NULL);
    if (angle) {
      angles = MakeVec3(0.f, (float) atof(angle), 0.f);
    }

    Vec3 exitForward, exitUp;
    Vec3_Vectors(angles, &exitForward, NULL, &exitUp);

    const Vec3 exitOrigin = VectorForKey(exit, "origin", Vec3_Zero());

    BspPortal *out = &bsp_file.portals[bsp_file.numPortals];
    bsp_file.numPortals++;

    out->brushSide = face->brushSide;
    out->drawElements = portal_faces[i].drawElements;
    out->entryOrigin = entryOrigin;
    out->entryForward = entryForward;
    out->entryUp = entryUp;
    out->exitOrigin = exitOrigin;
    out->exitForward = exitForward;
    out->exitUp = exitUp;
  }

  Com_Verbose("Emitted %d portals\n", bsp_file.numPortals);
}

/**
 * @brief
 */
void EndBSPFile(void) {

  Bsp_AllocLump(&bsp_file, BSP_LUMP_PORTALS, MAX_BSP_PORTALS);

  EmitPortals();
}

/**
 * @brief Allocates a new BSP model entry for the given entity and initializes its face and element offsets.
 */
BspModel *BeginModel(const Entity *e) {

  if (bsp_file.numModels == MAX_BSP_MODELS) {
    Com_Error(ERROR_FATAL, "MAX_BSP_MODELS\n");
  }

  BspModel *mod = &bsp_file.models[bsp_file.numModels];
  bsp_file.numModels++;

  mod->entity = (int32_t) (ptrdiff_t) (e - entities);

  mod->firstFace = bsp_file.numFaces;
  mod->firstBlock = bsp_file.numBlocks;

  // bound the brushes
  const int32_t start = e->firstBrush;
  const int32_t end = start + e->numBrushes;

  mod->bounds = Box3_Null();

  const Brush *brush = &brushes[start];
  for (int32_t j = start; j < end; j++, brush++) {

    if (brush->numBrushSides) {
      mod->bounds = Box3_Union(mod->bounds, brush->bounds);
    }
  }

  return mod;
}

/**
 * @brief Emits depth-pass draw elements for the model: all opaque, non-liquid, non-sky faces
 * are lumped into a single draw elements, with a sentinel material of -1, since the shadow
 * pass and Z pre-pass do not sample any texture for them. Alpha-tested faces (foliage, fences,
 * grates) are grouped by material, so their diffuse texture can be sampled and discarded
 * per-pixel, letting them cast per-pixel holes rather than solid silhouettes.
 */
static void EmitDepthPassElements(BspModel *mod) {

  mod->firstDepthPassElements = bsp_file.numDrawElements;

  if (bsp_file.numDrawElements == MAX_BSP_DRAW_ELEMENTS) {
    Com_Error(ERROR_FATAL, "MAX_BSP_DRAW_ELEMENTS\n");
  }

  BspDrawElements *opaque = bsp_file.drawElements + bsp_file.numDrawElements;
  opaque->material = -1;
  opaque->bounds = Box3_Null();
  opaque->firstElement = bsp_file.numElements;

  Vector *alphaTestFaces = $(alloc(Vector), initWithSize, sizeof(BspFace *));

  const BspFace *face = &bsp_file.faces[mod->firstFace];
  for (int32_t i = 0; i < mod->numFaces; i++, face++) {

    const int32_t surface = FaceSurface(face);
    if (surface & SURF_ALPHA_TEST) {
      $(alphaTestFaces, add, &face);
      continue;
    }

    const int32_t contents = FaceContents(face);
    if (contents & CONTENTS_MIST) {
      continue;
    }

    if (surface & SURF_MASK_NO_DRAW_ELEMENTS) {
      continue;
    }

    if (surface & SURF_LIQUID) {
      continue;
    }

    if (surface & (SURF_MASK_BLEND | SURF_MATERIAL)) {
      continue;
    }

    if (bsp_file.numElements + face->numElements >= MAX_BSP_ELEMENTS) {
      Com_Error(ERROR_FATAL, "MAX_BSP_ELEMENTS\n");
    }

    memcpy(bsp_file.elements + bsp_file.numElements,
           bsp_file.elements + face->firstElement,
           sizeof(int32_t) * face->numElements);

    bsp_file.numElements += face->numElements;

    opaque->numElements += face->numElements;
    opaque->bounds = Box3_Union(opaque->bounds, face->bounds);
  }

  if (opaque->numElements) {
    bsp_file.numDrawElements++;
  }

  if (alphaTestFaces->count) {
    EmitDrawElements(alphaTestFaces);
  }

  release(alphaTestFaces);

  mod->numDepthPassElements = bsp_file.numDrawElements - mod->firstDepthPassElements;
}

/**
 * @brief Draw elements comparator to sort model faces by material.
 * @details Opaque and blended faces are equal if they share material and contents.
 * @details Material faces equal if they share blend equality and brush side.
 */
static int32_t FaceCmp(const void * a, const void * b) {

  const BspFace *aFace = a;
  const BspFace *bFace = b;

  const int32_t aMaterial = FaceMaterial(aFace);
  const int32_t bMaterial = FaceMaterial(bFace);

  int32_t order = aMaterial - bMaterial;
  if (order == 0) {

    const int32_t aSurface = FaceSurface(aFace) & SURF_MASK_DRAW_ELEMENTS_CMP;
    const int32_t bSurface = FaceSurface(bFace) & SURF_MASK_DRAW_ELEMENTS_CMP;

    order = aSurface - bSurface;
    if (order == 0) {

      if (aSurface & (SURF_MATERIAL | SURF_PORTAL)) {
        // Brush side faces with SURF_MATERIAL are unique per brush side, and each SURF_PORTAL
        // face is its own portal, drawn with its own view
        return aFace->brushSide - bFace->brushSide;
      }
    }
  }

  return order;
}

static Order FaceCmpOrder(const ident a, const ident b) {
  const BspFace *const *aFace = a;
  const BspFace *const *bFace = b;
  const int32_t cmp = FaceCmp(*aFace, *bFace);
  return cmp < 0 ? OrderAscending : cmp > 0 ? OrderDescending : OrderSame;
}

/**
 * @brief Emits glDrawElements commands for the given face list.
 * @details Sorts opaque and alpha test faces in the given model by material, and emits
 * glDrawElements commands for each unique material. The BSP face ordering is not modified,
 * as this would break the references that the nodes hold to them.
 * @return The number of draw elements commands emitted.
 */
int32_t EmitDrawElements(Vector *faces) {

  const int32_t numDrawElements = bsp_file.numDrawElements;

  $(faces, sort, FaceCmpOrder);

  for (size_t i = 0; i < faces->count; i++) {

    if (bsp_file.numDrawElements == MAX_BSP_DRAW_ELEMENTS) {
      Com_Error(ERROR_FATAL, "MAX_BSP_LEAF_ELEMENTS\n");
    }

    const BspFace *a = VectorValue(faces, BspFace *, i);
    const int32_t aSurface = FaceSurface(a);

    if (aSurface & SURF_MASK_NO_DRAW_ELEMENTS) {
      continue;
    }

    BspDrawElements *out = bsp_file.drawElements + bsp_file.numDrawElements;
    bsp_file.numDrawElements++;

    out->material = FaceMaterial(a);
    out->surface = aSurface & SURF_MASK_DRAW_ELEMENTS_CMP;

    if (aSurface & SURF_PORTAL) {
      if (num_portal_faces == MAX_BSP_PORTALS) {
        Com_Error(ERROR_FATAL, "MAX_BSP_PORTALS\n");
      }
      portal_faces[num_portal_faces].face = a;
      portal_faces[num_portal_faces].drawElements = (int32_t) (out - bsp_file.drawElements);
      num_portal_faces++;
    }

    out->bounds = Box3_Null();

    out->firstElement = bsp_file.numElements;

    for (size_t j = i; j < faces->count; j++) {

      const BspFace *b = VectorValue(faces, BspFace *, j);

      if (FaceCmp(a, b)) {
        break;
      }

      if (bsp_file.numElements + b->numElements >= MAX_BSP_ELEMENTS) {
        Com_Error(ERROR_FATAL, "MAX_BSP_ELEMENTS\n");
      }

      memcpy(bsp_file.elements + bsp_file.numElements,
             bsp_file.elements + b->firstElement,
             sizeof(int32_t) * b->numElements);

      bsp_file.numElements += b->numElements;
      out->numElements += b->numElements;

      out->bounds = Box3_Union(out->bounds, b->bounds);

      i = j;
    }

    assert(out->numElements);
  }

  return bsp_file.numDrawElements - numDrawElements;
}

/**
 * @brief Recursively emits block draw-element groups for all `CONTENTS_BLOCK` nodes in the BSP tree.
 */
static void EmitBlocks_r(BspModel *mod, BspNode *node) {

  if (node->contents == CONTENTS_BLOCK) {

    Vector *faces = $(alloc(Vector), initWithSize, sizeof(BspFace *));

    BspFace *face = bsp_file.faces + mod->firstFace;
    for (int32_t i = 0; i < mod->numFaces; i++, face++) {

      const BspBrushSide *side = bsp_file.brushSides + face->brushSide;
      const BspMaterial *material = bsp_file.materials + side->material;

      if (Box3_ContainsPoint(node->bounds, Box3_Center(face->bounds))) {
        if (face->block != -1) {
          Com_Verbose("Face %s @ %s resides in multiple blocks\n", material->name, vtos(Box3_Center(face->bounds)));
        }
        
        $(faces, add, &face);
      }
    }

    if (faces->count == 0) {
      release(faces);
      node->contents = CONTENTS_NODE;
      return;
    }

    BspBlock *out = &bsp_file.blocks[bsp_file.numBlocks++];
    out->node = (int32_t) (ptrdiff_t) (node - bsp_file.nodes);

    out->visibleBounds = Box3_Null();
    for (size_t i = 0; i < faces->count; i++) {

      BspFace *face = VectorValue(faces, BspFace *, i);
      face->block = (int32_t) (ptrdiff_t) (out - bsp_file.blocks);

      out->visibleBounds = Box3_Union(out->visibleBounds, face->bounds);
    }

    out->firstDrawElement = bsp_file.numDrawElements;
    out->numDrawElements = EmitDrawElements(faces);

    release(faces);
    return;
  }

  if (node->contents == CONTENTS_NODE) {
    EmitBlocks_r(mod, bsp_file.nodes + node->children[0]);
    EmitBlocks_r(mod, bsp_file.nodes + node->children[1]);
  }
}

/**
 * @brief Emits all block draw-element groups for the given model.
 */
static void EmitBlocks(BspModel *mod) {
  EmitBlocks_r(mod, bsp_file.nodes + mod->headNode);
}

/**
 * @brief Finalizes a BSP model: computes face counts, runs Phong shading, emits patches, depth-pass elements, and blocks.
 */
void EndModel(BspModel *mod) {

  const BspNode *headNode = &bsp_file.nodes[mod->headNode];

  mod->visibleBounds = headNode->visibleBounds;

  // Faces (brush + patch) were emitted during EmitNode
  mod->numFaces = bsp_file.numFaces - mod->firstFace;

  // Phong shade brush faces (skip patch faces via plane == -1)
  PhongShading(mod);

  // Emit patch definitions to the patches lump
  EmitPatches(mod);

  // Free pre-tessellated patch face data
  FreePatchFaces(mod->entity);

  EmitDepthPassElements(mod);

  // Captured here (not in BeginModel) since EmitDepthPassElements above also appends entries
  // to the shared draw_elements pool; this must exclude those from the block range below.
  mod->firstDrawElements = bsp_file.numDrawElements;

  EmitBlocks(mod);

  const BspFace *face = &bsp_file.faces[mod->firstFace];
  for (int32_t i = 0; i < mod->numFaces; i++, face++) {
    if (face->block == -1) {
      Com_Warn("Model %d face %d (%s) was not assigned to a CONTENTS_BLOCK node\n",
               mod->entity, i, materials[FaceMaterial(face)].cm->name);
    }
  }

  mod->numDrawElements = bsp_file.numDrawElements - mod->firstDrawElements;
  mod->numBlocks = bsp_file.numBlocks - mod->firstBlock;
}
