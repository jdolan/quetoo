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

static int32_t numPortalFaces;

/**
 * @brief The most reflective draw elements one model may emit, before they are grouped by plane.
 * @remarks Far above `MAX_BSP_REFLECTIONS`, which bounds the planes rather than the blocks they
 * are cut into.
 */
#define MAX_BSP_REFLECT_ELEMENTS 0x400

/**
 * @brief The reflective draw elements of the model being emitted.
 * @remarks Resolved into the reflections lump by `EmitReflections`, once all of the model's
 * blocks are known, since a plane spanning several of them is one reflection.
 */
static struct {
  Vec3 normal;
  float dist;
  int32_t drawElements;
} reflect_elements[MAX_BSP_REFLECT_ELEMENTS];

static int32_t numReflectElements;

/**
 * @brief The distance of each reflection's plane, which the lump does not store: it bakes the
 * normal, and the distance follows from an origin that lies on the plane.
 */
static float reflect_dists[MAX_BSP_REFLECTIONS];

/**
 * @brief Writes all compiler planes to the BSP planes lump.
 */
void EmitPlanes(void) {

  bspFile.numPlanes = 0;

  const Plane *p = planes;
  for (int32_t i = 0; i < numPlanes; i++, p++) {
    BspPlane *out = &bspFile.planes[bspFile.numPlanes];

    out->normal = p->normal;
    out->dist = (float) p->dist;

    bspFile.numPlanes++;

    Progress("Emitting planes", 100.f * i / numPlanes);
  }
}

/**
 * @brief Writes all loaded materials to the BSP materials lump.
 */
void EmitMaterials(void) {

  const Material *m = materials;
  for (int32_t i = 0; i < numMaterials; i++, m++) {
    BspMaterial *out = &bspFile.materials[bspFile.numMaterials];

    const char *name = m->cm->name;
    if (!q_strncmp(name, "textures/", 9)) {
      name += q_strlen("textures/");
    }
    q_strlcpy(out->name, name, sizeof(out->name));

    bspFile.numMaterials++;

    Progress("Emitting materials", 100.f * i / numMaterials);
  }
}

/**
 * @brief Resolves the material index for the given BSP face.
 */
static inline int32_t FaceMaterial(const BspFace *face) {
  if (face->brushSide >= 0) {
    return bspFile.brushSides[face->brushSide].material;
  }
  return bspFile.patches[face->patch].material;
}

/**
 * @brief Resolves the contents mask for the given BSP face.
 */
static inline int32_t FaceContents(const BspFace *face) {
  if (face->brushSide >= 0) {
    return bspFile.brushSides[face->brushSide].contents;
  }
  return bspFile.patches[face->patch].contents;
}

/**
 * @brief Resolves the surface mask for the given BSP face.
 */
static inline int32_t FaceSurface(const BspFace *face) {
  if (face->brushSide >= 0) {
    return bspFile.brushSides[face->brushSide].surface;
  }
  return bspFile.patches[face->patch].surface;
}

/**
 * @brief Emits brush faces for the given node.
 */
static int32_t EmitFaces(const Node *node, int32_t nodeNum) {

  const int32_t numFaces = bspFile.numFaces;

  for (Face *face = node->faces; face; face = face->next) {

    if (face->merged) {
      continue;
    }

    face->out = EmitFace(face);
    face->out->node = nodeNum;
  }

  // Emit pre-tessellated patch faces assigned to this node
  for (PatchFace *pf = node->patchFaces; pf; pf = pf->next) {

    if (bspFile.numFaces >= MAX_BSP_FACES) {
      Com_Error(ERROR_FATAL, "MAX_BSP_FACES\n");
    }

    BspFace *out = &bspFile.faces[bspFile.numFaces];
    memset(out, 0, sizeof(*out));
    bspFile.numFaces++;

    pf->out = out;

    out->brushSide = -1;
    out->patch = -1;  // set by EmitPatches after BSP patch index is assigned
    out->plane = -1;
    out->node = nodeNum;
    out->block = -1;
    out->bounds = pf->bounds;

    // Copy vertexes to bspFile
    out->firstVertex = bspFile.numVertexes;
    out->numVertexes = pf->numVertexes;

    if (bspFile.numVertexes + pf->numVertexes > MAX_BSP_VERTEXES) {
      Com_Error(ERROR_FATAL, "MAX_BSP_VERTEXES\n");
    }

    memcpy(&bspFile.vertexes[bspFile.numVertexes], pf->vertexes,
           pf->numVertexes * sizeof(BspVertex));
    bspFile.numVertexes += pf->numVertexes;

    // Copy elements to bspFile, adjusting indices
    out->firstElement = bspFile.numElements;
    out->numElements = pf->numElements;

    if (bspFile.numElements + pf->numElements > MAX_BSP_ELEMENTS) {
      Com_Error(ERROR_FATAL, "MAX_BSP_ELEMENTS\n");
    }

    for (int32_t e = 0; e < pf->numElements; e++) {
      bspFile.elements[bspFile.numElements++] = out->firstVertex + pf->elements[e];
    }
  }

  return bspFile.numFaces - numFaces;
}

/**
 * @brief Emits a leaf node into the BSP leaves lump, writing its contents, bounds, and leaf-brush references.
 * @return The index of the new leaf in `bspFile`.leafs`.
 */
static int32_t EmitLeaf(Node *node) {

  if (bspFile.numLeafs == MAX_BSP_LEAFS) {
    Com_Error(ERROR_FATAL, "MAX_BSP_LEAFS\n");
  }

  BspLeaf *out = &bspFile.leafs[bspFile.numLeafs];
  bspFile.numLeafs++;

  out->contents = node->contents;
  out->bounds = node->bounds;

  // write the leafBrushes
  out->firstLeafBrush = bspFile.numLeafBrushes;

  for (const CsgBrush *brush = node->brushes; brush; brush = brush->next) {

    if (bspFile.numLeafBrushes >= MAX_BSP_LEAF_BRUSHES) {
      Com_Error(ERROR_FATAL, "MAX_BSP_LEAF_BRUSHES\n");
    }

    assert(brush->original);
    assert(brush->original->out);

    const int32_t brushNum = (int32_t) (ptrdiff_t) (brush->original->out - bspFile.brushes);

    int32_t i;
    for (i = out->firstLeafBrush; i < bspFile.numLeafBrushes; i++) {
      if (bspFile.leafBrushes[i] == brushNum) {
        break;
      }
    }

    if (i == bspFile.numLeafBrushes) {
      bspFile.leafBrushes[bspFile.numLeafBrushes] = brushNum;
      bspFile.numLeafBrushes++;
    }
  }

  out->numLeafBrushes = bspFile.numLeafBrushes - out->firstLeafBrush;

  return (int32_t) (ptrdiff_t) (out - bspFile.leafs);
}

/**
 * @brief Recursively emits a BSP node and its children into the BSP nodes and leafs lumps.
 * @return The index of the emitted node in `bspFile`.nodes`.
 */
static int32_t EmitNode(const Node *node) {

  if (node->plane == PLANE_LEAF) {
    Com_Error(ERROR_FATAL, "Node does not reference a plane\n");
  }

  if (node->plane & 1) {
    Com_Error(ERROR_FATAL, "Node referencing negative plane\n");
  }

  if (bspFile.numNodes == MAX_BSP_NODES) {
    Com_Error(ERROR_FATAL, "MAX_BSP_NODES\n");
  }

  Progress("Emitting nodes", -1);

  const int32_t nodeNum = bspFile.numNodes++;
  BspNode *out = &bspFile.nodes[nodeNum];

  out->plane = node->plane;
  out->contents = node->contents;
  out->bounds = node->bounds;
  out->visibleBounds = node->visibleBounds;

  out->firstFace = bspFile.numFaces;
  out->numFaces = EmitFaces(node, nodeNum);

  // recursively output the other nodes
  for (int32_t i = 0; i < 2; i++) {
    if (node->children[i]->plane == PLANE_LEAF) {
      out->children[i] = -(bspFile.numLeafs + 1);
      EmitLeaf(node->children[i]);
    } else {
      out->children[i] = bspFile.numNodes;
      EmitNode(node->children[i]);
    }
  }

  return nodeNum;
}

/**
 * @brief Emits the entire BSP tree rooted at `tree->`headNode` into the BSP file.
 * @return The index of the head node in `bspFile`.nodes`.
 */
int32_t EmitNodes(const Tree *tree) {

  const uint32_t start = (uint32_t) SDL_GetTicks();

  // block nodes define additional planes, ensure they make it into the bsp
  EmitPlanes();

  numWelds = 0;
  ClearWeldingSpatialHash();

  const int32_t node = EmitNode(tree->headNode);

  Com_Verbose("%5i welded vertices\n", numWelds);

  Com_Print("\r%-24s [100%%] %d ms\n", "Emitting nodes", (uint32_t) SDL_GetTicks() - start);

  return node;
}

/**
 * @brief Emits a single brush side into the BSP brush sides lump and returns a pointer to it.
 */
static BspBrushSide *EmitBrushSide(const BrushSide *side) {

  BspBrushSide *out = bspFile.brushSides + bspFile.numBrushSides;

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
    bspFile.numBrushSides++;
  }

  return brush->numBrushSides;
}

/**
 * @brief Emits a single brush and its sides into the BSP brushes lump and returns a pointer to the emitted brush.
 */
static BspBrush *EmitBrush(const Brush *brush) {

  BspBrush *out = bspFile.brushes + bspFile.numBrushes;

  out->entity = brush->entity;
  out->contents = brush->contents;

  out->firstBrushSide = bspFile.numBrushSides;
  out->numBrushSides = EmitBrushSides(brush);

  out->bounds = brush->bounds;

  return out;
}

/**
 * @brief Emits all brushes to the BSP brushes and brush sides lumps.
 */
void EmitBrushes(void) {

  Brush *brush = brushes;
  for (int32_t i = 0; i < numBrushes; i++, brush++) {

    if (!brush->numBrushSides) {
      continue;
    }

    brush->out = EmitBrush(brush);
    bspFile.numBrushes++;

    Progress("Emitting brushes", 100.f * i / numBrushes);
  }
}

/**
 * @brief Generates the entity string from all retained entities.
 */
void EmitEntities(void) {

  const uint32_t start = (uint32_t) SDL_GetTicks();

  Bsp_AllocLump(&bspFile, BSP_LUMP_ENTITIES, MAX_BSP_ENTITIES_SIZE);

  char *out = bspFile.entityString;
  *out = '\0';

  for (int32_t i = 0; i < numEntities; i++) {
    const EntityKeyValue *e = entities[i].values;
    if (e) {
      q_strlcat(out, "{\n", MAX_BSP_ENTITIES_SIZE);
      while (e) {
        q_strlcat(out, va(" \"%s\" \"%s\"\n", e->key, e->value), MAX_BSP_ENTITIES_SIZE);
        e = e->next;
      }
      q_strlcat(out, "}\n", MAX_BSP_ENTITIES_SIZE);
    }

    Progress("Emitting entities", 100.f * i / numEntities);
  }

  const size_t len = q_strlen(out);

  if (len == MAX_BSP_ENTITIES_SIZE - 1) {
    Com_Error(ERROR_FATAL, "MAX_BSP_ENTITIES_SIZE\n");
  }

  bspFile.entityStringSize = (int32_t) len + 1;

  Com_Print("\r%-24s [100%%] %d ms\n\n", "Emitting entities", (uint32_t) SDL_GetTicks() - start);
}

/**
 * @brief Allocates BSP lumps and initializes the BSP file structure for writing.
 */
void BeginBSPFile(void) {

  memset(&bspFile, 0, sizeof(bspFile));

  Bsp_AllocLump(&bspFile, BSP_LUMP_MATERIALS, MAX_BSP_MATERIALS);
  Bsp_AllocLump(&bspFile, BSP_LUMP_PLANES, MAX_BSP_PLANES);
  Bsp_AllocLump(&bspFile, BSP_LUMP_BRUSH_SIDES, MAX_BSP_BRUSH_SIDES);
  Bsp_AllocLump(&bspFile, BSP_LUMP_BRUSHES, MAX_BSP_BRUSHES);
  Bsp_AllocLump(&bspFile, BSP_LUMP_VERTEXES, MAX_BSP_VERTEXES);
  Bsp_AllocLump(&bspFile, BSP_LUMP_ELEMENTS, MAX_BSP_ELEMENTS);
  Bsp_AllocLump(&bspFile, BSP_LUMP_FACES, MAX_BSP_FACES);
  Bsp_AllocLump(&bspFile, BSP_LUMP_NODES, MAX_BSP_NODES);
  Bsp_AllocLump(&bspFile, BSP_LUMP_LEAF_BRUSHES, MAX_BSP_LEAF_BRUSHES);
  Bsp_AllocLump(&bspFile, BSP_LUMP_LEAFS, MAX_BSP_LEAFS);
  Bsp_AllocLump(&bspFile, BSP_LUMP_DRAW_ELEMENTS, MAX_BSP_DRAW_ELEMENTS);
  Bsp_AllocLump(&bspFile, BSP_LUMP_BLOCKS, MAX_BSP_BLOCKS);
  Bsp_AllocLump(&bspFile, BSP_LUMP_MODELS, MAX_BSP_MODELS);
  Bsp_AllocLump(&bspFile, BSP_LUMP_PATCHES, MAX_BSP_PATCHES);
  Bsp_AllocLump(&bspFile, BSP_LUMP_REFLECTIONS, MAX_BSP_REFLECTIONS);

  /*
   * jdolan 2019-01-01
   *
   * Leafs are referenced by their parents as child nodes, but with negative indices.
   * Because zero can not be negated, the first leaf in the map must be padded here.
   * You can choose to ignore this comment if you want to lose 3 days of your life
   * to debugging PVS, like I did.
   */
  bspFile.numLeafs = 1;
  bspFile.leafs[0].contents = CONTENTS_SOLID;
}

/**
 * @brief Called after all BSP data has been emitted; reserved for any final BSP file finalization.
 */
/**
 * @return The index of the entity that defined @p brushSide, or `-1`.
 */
static int32_t BrushSideEntity(const int32_t brushSide) {

  const BspBrush *brush = bspFile.brushes;
  for (int32_t i = 0; i < bspFile.numBrushes; i++, brush++) {
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

  const BspBrushSide *side = &bspFile.brushSides[face->brushSide];
  const BspPlane *plane = &bspFile.planes[side->plane];

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
 * @brief The height above a `misc_teleporter_dest` at which a portal targeting it is viewed.
 * @remarks Quetoo's standing eye. The other pmove modules stand at 26 and at 22, and quemap
 * cannot know which one a server will run.
 */
#define PORTAL_DEST_VIEW_HEIGHT 30.f

/**
 * @brief Emits the portals lump, resolving each portal face to the entity it views from.
 * @details A portal that names no target, or names one that does not exist, is dropped with a
 * warning: it has nothing to show, and the renderer would draw a hole in the world.
 */
static void EmitPortals(void) {

  for (int32_t i = 0; i < numPortalFaces; i++) {

    const BspFace *face = portal_faces[i].face;

    const BspDrawElements *draw = &bspFile.drawElements[portal_faces[i].drawElements];

    Vec3 entryOrigin, entryForward, entryUp;
    PortalFaceFrame(face, draw, &entryOrigin, &entryForward, &entryUp);

    const int32_t e = BrushSideEntity(face->brushSide);
    if (e == -1) {
      Com_Warn("Portal %s @ %s belongs to no brush, skipping\n",
               bspFile.materials[draw->material].name, vtos(entryOrigin));
      continue;
    }

    // not "target", which the entity carrying the portal face may already owe to its own class:
    // a func_train reads it as the first path_corner of its route, a func_button as what it fires
    const char *target = ValueForKey(&entities[e], "portal", NULL);
    if (!target) {
      Com_Warn("Portal %s @ %s has no portal key, skipping\n",
               bspFile.materials[draw->material].name, vtos(entryOrigin));
      continue;
    }

    const Entity *exit = NULL;
    for (int32_t j = 0; j < numEntities; j++) {
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

    Vec3 exitOrigin = VectorForKey(exit, "origin", Vec3_Zero());

    // a misc_teleporter_dest marks where a player arrives, not where a camera belongs, so the
    // viewpoint is raised to the eye. This is Quetoo's standing eye: a server running another
    // pmove module sees the portal from a little above or below its own
    const char *classname = ValueForKey(exit, "classname", NULL);
    if (classname && !q_strcmp(classname, "misc_teleporter_dest")) {
      exitOrigin.z += PORTAL_DEST_VIEW_HEIGHT;
    }

    BspPortal *out = &bspFile.portals[bspFile.numPortals];
    bspFile.numPortals++;

    out->brushSide = face->brushSide;
    out->drawElements = portal_faces[i].drawElements;
    out->entryOrigin = entryOrigin;
    out->entryForward = entryForward;
    out->entryUp = entryUp;
    out->exitOrigin = exitOrigin;
    out->exitForward = exitForward;
    out->exitUp = exitUp;
  }

  Com_Verbose("Emitted %d portals\n", bspFile.numPortals);
}

/**
 * @brief
 */
void EndBSPFile(void) {

  Bsp_AllocLump(&bspFile, BSP_LUMP_PORTALS, MAX_BSP_PORTALS);

  EmitPortals();
}

/**
 * @brief Allocates a new BSP model entry for the given entity and initializes its face and element offsets.
 */
BspModel *BeginModel(const Entity *e) {

  if (bspFile.numModels == MAX_BSP_MODELS) {
    Com_Error(ERROR_FATAL, "MAX_BSP_MODELS\n");
  }

  BspModel *mod = &bspFile.models[bspFile.numModels];
  bspFile.numModels++;

  mod->entity = (int32_t) (ptrdiff_t) (e - entities);

  mod->firstFace = bspFile.numFaces;
  mod->firstBlock = bspFile.numBlocks;

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

  mod->firstDepthPassElements = bspFile.numDrawElements;

  if (bspFile.numDrawElements == MAX_BSP_DRAW_ELEMENTS) {
    Com_Error(ERROR_FATAL, "MAX_BSP_DRAW_ELEMENTS\n");
  }

  BspDrawElements *opaque = bspFile.drawElements + bspFile.numDrawElements;
  opaque->material = -1;
  opaque->reflection = -1;
  opaque->bounds = Box3_Null();
  opaque->firstElement = bspFile.numElements;

  Vector *alphaTestFaces = $(alloc(Vector), initWithSize, sizeof(BspFace *));

  const BspFace *face = &bspFile.faces[mod->firstFace];
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

    if (bspFile.numElements + face->numElements >= MAX_BSP_ELEMENTS) {
      Com_Error(ERROR_FATAL, "MAX_BSP_ELEMENTS\n");
    }

    memcpy(bspFile.elements + bspFile.numElements,
           bspFile.elements + face->firstElement,
           sizeof(int32_t) * face->numElements);

    bspFile.numElements += face->numElements;

    opaque->numElements += face->numElements;
    opaque->bounds = Box3_Union(opaque->bounds, face->bounds);
  }

  if (opaque->numElements) {
    bspFile.numDrawElements++;
  }

  if (alphaTestFaces->count) {
    EmitDrawElements(alphaTestFaces);
  }

  release(alphaTestFaces);

  mod->numDepthPassElements = bspFile.numDrawElements - mod->firstDepthPassElements;
}

/**
 * @return The plane @p face reflects about, in @p normal and @p dist, or `false` if it cannot
 * carry a mirror.
 * @details The facing comes from the brush side, which is the only description of the surface
 * that every fragment of it agrees on. `face->plane` is the plane of the node that cut the face,
 * and for one surface that can be either member of an opposing pair -- cistern's pool arrives as
 * eight faces at one height recorded alternately as a plane and its twin. The winding is no
 * better: those fragments are not consistently wound either, so a cross product splits the pool
 * into an upward and a downward reflection, and standing over it only half of them have a camera
 * in front of them. The rest fall back to plain water along the BSP seams.
 *
 * The distance comes from the vertexes rather than from the brush side, whose plane a face need
 * not lie on: cistern has three-vertex slivers, left where terrain cuts the water, whose side is
 * a thousand units away. Every vertex is then checked against the plane that results.
 */
static bool ReflectiveFacePlane(const BspFace *face, Vec3 *normal, float *dist) {

  if (face->numVertexes < 3) {
    return false;
  }

  // a patch has no brush side, so nothing says which way its faces are meant to look
  if (face->brushSide == -1) {
    Com_Warn("Patch %s @ %s cannot reflect; a reflection needs a brush side\n",
             bspFile.materials[FaceMaterial(face)].name, vtos(Box3_Center(face->bounds)));
    return false;
  }

  const Vec3 n = bspFile.planes[bspFile.brushSides[face->brushSide].plane].normal;

  const BspVertex *v = &bspFile.vertexes[face->firstVertex];
  const float d = Vec3_Dot(v[0].position, n);

  for (int32_t i = 1; i < face->numVertexes; i++) {
    if (fabsf(Vec3_Dot(v[i].position, n) - d) > ON_EPSILON) {
      Com_Warn("Reflective %s @ %s is not planar and will not reflect\n",
               bspFile.materials[FaceMaterial(face)].name, vtos(Box3_Center(face->bounds)));
      return false;
    }
  }

  *normal = n;
  *dist = d;

  return true;
}

/**
 * @brief Draw elements comparator to sort model faces by material.
 * @details Opaque and blended faces are equal if they share material and contents.
 * @details Material faces equal if they share blend equality and brush side.
 * @details Subview faces are likewise unique per brush side, which is what makes each of them
 * planar: the renderer reads a reflective face's mirror plane off its own geometry, and a brush's
 * top and sides sharing a material would otherwise merge into one draw element spanning several
 * planes, with no way to say which of them is the one to reflect about.
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

      if (aSurface & (SURF_MATERIAL | SURF_MASK_SUBVIEW)) {
        // Brush side faces with SURF_MATERIAL are unique per brush side, and each subview face
        // is drawn with a view of its own, placed from the plane of the side that cut it
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

  const int32_t numDrawElements = bspFile.numDrawElements;

  $(faces, sort, FaceCmpOrder);

  for (size_t i = 0; i < faces->count; i++) {

    if (bspFile.numDrawElements == MAX_BSP_DRAW_ELEMENTS) {
      Com_Error(ERROR_FATAL, "MAX_BSP_LEAF_ELEMENTS\n");
    }

    const BspFace *a = VectorValue(faces, BspFace *, i);
    const int32_t aSurface = FaceSurface(a);

    if (aSurface & SURF_MASK_NO_DRAW_ELEMENTS) {
      continue;
    }

    BspDrawElements *out = bspFile.drawElements + bspFile.numDrawElements;
    bspFile.numDrawElements++;

    out->material = FaceMaterial(a);
    out->surface = aSurface & SURF_MASK_DRAW_ELEMENTS_CMP;
    out->reflection = -1;

    if (out->surface & SURF_REFLECT) {
      out->surface &= ~SURF_REFLECT;

      Vec3 normal;
      float dist;

      if (ReflectiveFacePlane(a, &normal, &dist)) {

        if (numReflectElements == MAX_BSP_REFLECT_ELEMENTS) {
          Com_Error(ERROR_FATAL, "MAX_BSP_REFLECT_ELEMENTS\n");
        }

        reflect_elements[numReflectElements].normal = normal;
        reflect_elements[numReflectElements].dist = dist;
        reflect_elements[numReflectElements].drawElements = (int32_t) (out - bspFile.drawElements);
        numReflectElements++;

        out->surface |= SURF_REFLECT;
      }
    }

    if (aSurface & SURF_PORTAL) {
      if (numPortalFaces == MAX_BSP_PORTALS) {
        Com_Error(ERROR_FATAL, "MAX_BSP_PORTALS\n");
      }
      portal_faces[numPortalFaces].face = a;
      portal_faces[numPortalFaces].drawElements = (int32_t) (out - bspFile.drawElements);
      numPortalFaces++;
    }

    out->bounds = Box3_Null();

    out->firstElement = bspFile.numElements;

    for (size_t j = i; j < faces->count; j++) {

      const BspFace *b = VectorValue(faces, BspFace *, j);

      if (FaceCmp(a, b)) {
        break;
      }

      if (bspFile.numElements + b->numElements >= MAX_BSP_ELEMENTS) {
        Com_Error(ERROR_FATAL, "MAX_BSP_ELEMENTS\n");
      }

      memcpy(bspFile.elements + bspFile.numElements,
             bspFile.elements + b->firstElement,
             sizeof(int32_t) * b->numElements);

      bspFile.numElements += b->numElements;
      out->numElements += b->numElements;

      out->bounds = Box3_Union(out->bounds, b->bounds);

      i = j;
    }

    assert(out->numElements);
  }

  return bspFile.numDrawElements - numDrawElements;
}

/**
 * @brief Recursively emits block draw-element groups for all `CONTENTS_BLOCK` nodes in the BSP tree.
 */
static void EmitBlocks_r(BspModel *mod, BspNode *node) {

  if (node->contents == CONTENTS_BLOCK) {

    Vector *faces = $(alloc(Vector), initWithSize, sizeof(BspFace *));

    BspFace *face = bspFile.faces + mod->firstFace;
    for (int32_t i = 0; i < mod->numFaces; i++, face++) {

      const BspBrushSide *side = bspFile.brushSides + face->brushSide;
      const BspMaterial *material = bspFile.materials + side->material;

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

    BspBlock *out = &bspFile.blocks[bspFile.numBlocks++];
    out->node = (int32_t) (ptrdiff_t) (node - bspFile.nodes);

    out->visibleBounds = Box3_Null();
    for (size_t i = 0; i < faces->count; i++) {

      BspFace *face = VectorValue(faces, BspFace *, i);
      face->block = (int32_t) (ptrdiff_t) (out - bspFile.blocks);

      out->visibleBounds = Box3_Union(out->visibleBounds, face->bounds);
    }

    out->firstDrawElement = bspFile.numDrawElements;
    out->numDrawElements = EmitDrawElements(faces);

    release(faces);
    return;
  }

  if (node->contents == CONTENTS_NODE) {
    EmitBlocks_r(mod, bspFile.nodes + node->children[0]);
    EmitBlocks_r(mod, bspFile.nodes + node->children[1]);
  }
}

/**
 * @brief Groups the reflective draw elements of @p mod by plane, and emits one reflection each.
 * @details Draw elements are cut per BSP block, so a pool spanning four of them arrives here as
 * four entries sharing one plane. A reflection is a whole scene rendered into a layer and the
 * renderer holds only a handful of layers, so they are grouped: one reflection per plane per
 * model, culled and scissored by the union of its faces.
 *
 * Grouped per model rather than per world, since two models holding the same plane part company
 * as soon as either moves.
 */
static void EmitReflections(BspModel *mod) {

  const int32_t firstReflection = bspFile.numReflections;

  for (int32_t i = 0; i < numReflectElements; i++) {

    BspDrawElements *draw = &bspFile.drawElements[reflect_elements[i].drawElements];

    BspReflection *out = NULL;

    for (int32_t j = firstReflection; j < bspFile.numReflections && out == NULL; j++) {
      if (fabsf(reflect_dists[j] - reflect_elements[i].dist) <= ON_EPSILON &&
          Vec3_Dot(bspFile.reflections[j].normal, reflect_elements[i].normal) >= 1.f - COLINEAR_EPSILON) {
        out = &bspFile.reflections[j];
      }
    }

    if (out == NULL) {

      if (bspFile.numReflections == MAX_BSP_REFLECTIONS) {
        Com_Error(ERROR_FATAL, "MAX_BSP_REFLECTIONS\n");
      }

      out = &bspFile.reflections[bspFile.numReflections];

      reflect_dists[bspFile.numReflections] = reflect_elements[i].dist;
      bspFile.numReflections++;

      out->model = (int32_t) (mod - bspFile.models);
      out->normal = reflect_elements[i].normal;
      out->bounds = Box3_Null();
    }

    out->bounds = Box3_Union(out->bounds, draw->bounds);

    draw->reflection = (int32_t) (out - bspFile.reflections);
  }

  // the origin must lie on the plane, which the center of the bounds does not for a plane that is
  // not axis aligned, so it is projected onto it
  for (int32_t i = firstReflection; i < bspFile.numReflections; i++) {

    BspReflection *out = &bspFile.reflections[i];

    const Vec3 center = Box3_Center(out->bounds);

    out->origin = Vec3_Fmaf(center, reflect_dists[i] - Vec3_Dot(center, out->normal), out->normal);
  }

  if (bspFile.numReflections > firstReflection) {
    Com_Verbose("Emitted %d reflections for model %d, from %d draw elements\n",
                bspFile.numReflections - firstReflection,
                (int32_t) (mod - bspFile.models), numReflectElements);
  }

  numReflectElements = 0;
}

/**
 * @brief Emits all block draw-element groups for the given model.
 */
static void EmitBlocks(BspModel *mod) {
  EmitBlocks_r(mod, bspFile.nodes + mod->headNode);
}

/**
 * @brief Finalizes a BSP model: computes face counts, runs Phong shading, emits patches, depth-pass elements, and blocks.
 */
void EndModel(BspModel *mod) {

  const BspNode *headNode = &bspFile.nodes[mod->headNode];

  mod->visibleBounds = headNode->visibleBounds;

  // Faces (brush + patch) were emitted during EmitNode
  mod->numFaces = bspFile.numFaces - mod->firstFace;

  // Phong shade brush faces (skip patch faces via plane == -1)
  PhongShading(mod);

  // Emit patch definitions to the patches lump
  EmitPatches(mod);

  // Free pre-tessellated patch face data
  FreePatchFaces(mod->entity);

  EmitDepthPassElements(mod);

  // Captured here (not in BeginModel) since EmitDepthPassElements above also appends entries
  // to the shared drawElements pool; this must exclude those from the block range below.
  mod->firstDrawElements = bspFile.numDrawElements;

  EmitBlocks(mod);

  EmitReflections(mod);

  const BspFace *face = &bspFile.faces[mod->firstFace];
  for (int32_t i = 0; i < mod->numFaces; i++, face++) {
    if (face->block == -1) {
      Com_Warn("Model %d face %d (%s) was not assigned to a CONTENTS_BLOCK node\n",
               mod->entity, i, materials[FaceMaterial(face)].cm->name);
    }
  }

  mod->numDrawElements = bspFile.numDrawElements - mod->firstDrawElements;
  mod->numBlocks = bspFile.numBlocks - mod->firstBlock;
}
