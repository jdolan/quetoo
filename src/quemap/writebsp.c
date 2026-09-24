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
#include "drawelements.h"
#include "face.h"
#include "map.h"
#include "material.h"
#include "patch.h"
#include "portal.h"
#include "qbsp.h"
#include "writebsp.h"

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
 * @brief Emits brush faces for the given node.
 */
static int32_t EmitFaces(const Node *node, int32_t nodeNum) {

  const int32_t numFaces = bspFile.numFaces;

  for (Face *face = node->faces; face; face = face->next) {

    if (face->merged) {
      continue;
    }

    face->out = EmitFace(face);
    if (face->out) {
      face->out->node = nodeNum;
    }
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

  const int32_t node = EmitNode(tree->headNode);

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

  EmitDrawFaces(mod);

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

  FreeDrawFaces();

  mod->numDrawElements = bspFile.numDrawElements - mod->firstDrawElements;
  mod->numBlocks = bspFile.numBlocks - mod->firstBlock;
}
