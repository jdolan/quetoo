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

static int32_t EmitDrawFaceElements(Vector *drawFaces);

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
void EmitPortals(void) {

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
 * @brief Emits depth-pass draw elements for the model: all opaque, non-liquid, non-sky faces
 * are lumped into a single draw elements, with a sentinel material of -1, since the shadow
 * pass and Z pre-pass do not sample any texture for them. Alpha-tested faces (foliage, fences,
 * grates) are grouped by material, so their diffuse texture can be sampled and discarded
 * per-pixel, letting them cast per-pixel holes rather than solid silhouettes.
 * @details These are emitted from the same draw faces as the draw elements, since the Z pre-pass
 * MUST rasterize the same triangles as the passes that test against it.
 */
void EmitDepthPassElements(BspModel *mod) {

  mod->firstDepthPassElements = bspFile.numDrawElements;

  if (bspFile.numDrawElements == MAX_BSP_DRAW_ELEMENTS) {
    Com_Error(ERROR_FATAL, "MAX_BSP_DRAW_ELEMENTS\n");
  }

  BspDrawElements *opaque = bspFile.drawElements + bspFile.numDrawElements;
  opaque->material = -1;
  opaque->reflection = -1;
  opaque->bounds = Box3_Null();
  opaque->firstElement = bspFile.numElements;

  Vector *alphaTestFaces = $(alloc(Vector), initWithSize, sizeof(DrawFace *));

  const DrawFace *drawFace = drawFaces;
  for (int32_t i = 0; i < numDrawFaces; i++, drawFace++) {

    const BspFace *face = drawFace->face;

    const int32_t surface = FaceSurface(face);
    if (surface & SURF_ALPHA_TEST) {
      $(alphaTestFaces, add, &drawFace);
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

    if (bspFile.numElements + drawFace->numElements >= MAX_BSP_ELEMENTS) {
      Com_Error(ERROR_FATAL, "MAX_BSP_ELEMENTS\n");
    }

    memcpy(bspFile.elements + bspFile.numElements,
           drawFace->elements,
           sizeof(int32_t) * drawFace->numElements);

    bspFile.numElements += drawFace->numElements;

    opaque->numElements += drawFace->numElements;
    opaque->bounds = Box3_Union(opaque->bounds, drawFace->bounds);
  }

  if (opaque->numElements) {
    bspFile.numDrawElements++;
  }

  if (alphaTestFaces->count) {
    EmitDrawFaceElements(alphaTestFaces);
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
 * The distance also comes from the brush side, since each face is made from its side and lies on
 * its plane.
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

  const BspPlane *plane = &bspFile.planes[bspFile.brushSides[face->brushSide].plane];

  *normal = plane->normal;
  *dist = plane->dist;

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

static Order DrawFaceCmpOrder(const ident a, const ident b) {
  const DrawFace *const *aFace = a;
  const DrawFace *const *bFace = b;
  const int32_t cmp = FaceCmp((*aFace)->face, (*bFace)->face);
  return cmp < 0 ? OrderAscending : cmp > 0 ? OrderDescending : OrderSame;
}

/**
 * @brief Emits glDrawElements commands for the given draw face list.
 * @details Sorts opaque and alpha test draw faces by material, and emits glDrawElements commands
 * for each unique material. The BSP face ordering is not modified, as this would break the
 * references that the nodes hold to them.
 * @return The number of draw elements commands emitted.
 */
static int32_t EmitDrawFaceElements(Vector *drawFaces) {

  const int32_t numDrawElements = bspFile.numDrawElements;

  $(drawFaces, sort, DrawFaceCmpOrder);

  for (size_t i = 0; i < drawFaces->count; i++) {

    if (bspFile.numDrawElements == MAX_BSP_DRAW_ELEMENTS) {
      Com_Error(ERROR_FATAL, "MAX_BSP_LEAF_ELEMENTS\n");
    }

    const DrawFace *a = VectorValue(drawFaces, DrawFace *, i);
    const int32_t aSurface = FaceSurface(a->face);

    if (aSurface & SURF_MASK_NO_DRAW_ELEMENTS) {
      continue;
    }

    BspDrawElements *out = bspFile.drawElements + bspFile.numDrawElements;
    bspFile.numDrawElements++;

    out->material = FaceMaterial(a->face);
    out->surface = aSurface & SURF_MASK_DRAW_ELEMENTS_CMP;
    out->reflection = -1;

    if (out->surface & SURF_REFLECT) {
      out->surface &= ~SURF_REFLECT;

      Vec3 normal;
      float dist;

      if (ReflectiveFacePlane(a->face, &normal, &dist)) {

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
      portal_faces[numPortalFaces].face = a->face;
      portal_faces[numPortalFaces].drawElements = (int32_t) (out - bspFile.drawElements);
      numPortalFaces++;
    }

    out->bounds = Box3_Null();

    out->firstElement = bspFile.numElements;

    for (size_t j = i; j < drawFaces->count; j++) {

      const DrawFace *b = VectorValue(drawFaces, DrawFace *, j);

      if (FaceCmp(a->face, b->face)) {
        break;
      }

      if (bspFile.numElements + b->numElements >= MAX_BSP_ELEMENTS) {
        Com_Error(ERROR_FATAL, "MAX_BSP_ELEMENTS\n");
      }

      memcpy(bspFile.elements + bspFile.numElements,
             b->elements,
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
 * @brief Emits glDrawElements commands for the given face list, one draw face for each face.
 * @return The number of draw elements commands emitted.
 */
int32_t EmitDrawElements(Vector *faces) {

  DrawFace *faceDrawFaces = Mem_Malloc(faces->count * sizeof(DrawFace));
  Vector *drawFacesVector = $(alloc(Vector), initWithSize, sizeof(DrawFace *));

  for (size_t i = 0; i < faces->count; i++) {

    const BspFace *face = VectorValue(faces, BspFace *, i);

    faceDrawFaces[i] = (DrawFace) {
      .face = face,
      .blockNode = -1,
      .elements = bspFile.elements + face->firstElement,
      .numElements = face->numElements,
      .bounds = face->bounds
    };

    const DrawFace *drawFace = &faceDrawFaces[i];
    $(drawFacesVector, add, &drawFace);
  }

  const int32_t count = EmitDrawFaceElements(drawFacesVector);

  release(drawFacesVector);
  Mem_Free(faceDrawFaces);

  return count;
}

/**
 * @brief Recursively emits block draw-element groups for all `CONTENTS_BLOCK` nodes in the BSP tree.
 */
static void EmitBlocks_r(BspModel *mod, BspNode *node) {

  if (node->contents == CONTENTS_BLOCK) {

    const int32_t nodeNum = (int32_t) (ptrdiff_t) (node - bspFile.nodes);

    Vector *blockDrawFaces = $(alloc(Vector), initWithSize, sizeof(DrawFace *));

    const DrawFace *drawFace = drawFaces;
    for (int32_t i = 0; i < numDrawFaces; i++, drawFace++) {
      if (drawFace->blockNode == nodeNum) {
        $(blockDrawFaces, add, &drawFace);
      }
    }

    if (blockDrawFaces->count == 0) {
      release(blockDrawFaces);
      node->contents = CONTENTS_NODE;
      return;
    }

    BspBlock *out = &bspFile.blocks[bspFile.numBlocks++];
    out->node = nodeNum;

    out->visibleBounds = Box3_Null();

    BspFace *face = bspFile.faces + mod->firstFace;
    for (int32_t i = 0; i < mod->numFaces; i++, face++) {
      if (DrawFaceBlockNode(i) == nodeNum) {
        face->block = (int32_t) (ptrdiff_t) (out - bspFile.blocks);
        out->visibleBounds = Box3_Union(out->visibleBounds, face->bounds);
      }
    }

    out->firstDrawElement = bspFile.numDrawElements;
    out->numDrawElements = EmitDrawFaceElements(blockDrawFaces);

    release(blockDrawFaces);
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
void EmitReflections(BspModel *mod) {

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
void EmitBlocks(BspModel *mod) {
  EmitBlocks_r(mod, bspFile.nodes + mod->headNode);
}
