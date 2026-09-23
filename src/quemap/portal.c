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

#include "bsp.h"
#include "map.h"
#include "portal.h"
#include "qbsp.h"

static SDL_AtomicInt cActivePortals;

/**
 * @brief Allocates and returns a new portal.
 */
static Portal *AllocPortal(void) {

  SDL_AddAtomicInt(&cActivePortals, 1);

  return Mem_TagMalloc(sizeof(Portal), (MemTag) MEM_TAG_PORTAL);
}

/**
 * @brief Frees the portal's winding and the portal itself.
 */
void FreePortal(Portal *p) {

  if (p->winding) {
    Cm_FreeWinding(p->winding);
  }

  SDL_AddAtomicInt(&cActivePortals, -1);

  Mem_Free(p);
}

/**
 * @return The single content bit of the strongest visible content present.
 */
static int32_t VisibleContents(int32_t contents) {

  for (int32_t i = 1; i <= LAST_VISIBLE_CONTENTS; i <<= 1) {
    if (contents & i) {
      return i;
    }
  }

  return 0;
}

/**
 * @brief The entity flood determines which areas are "outside" of the map, which are then filled in.
 * Flowing from side s to side !s
 */
static bool Portal_EntityFlood(const Portal *p) {

  if (p->nodes[0]->plane != PLANE_LEAF || p->nodes[1]->plane != PLANE_LEAF) {
    Com_Error(ERROR_FATAL, "Not a leaf\n");
  }

  // can never cross to a solid
  if ((p->nodes[0]->contents & CONTENTS_SOLID) || (p->nodes[1]->contents & CONTENTS_SOLID)) {
    return false;
  }

  // can flood through everything else
  return true;
}

static int32_t cSmallPortals;

/**
 * @brief Links the portal into the portal lists of both front and back nodes.
 */
static void AddPortalToNodes(Portal *p, Node *front, Node *back) {

  if (p->nodes[0] || p->nodes[1]) {
    Com_Error(ERROR_FATAL, "Already included\n");
  }

  p->nodes[0] = front;
  p->next[0] = front->portals;
  front->portals = p;

  p->nodes[1] = back;
  p->next[1] = back->portals;
  back->portals = p;
}

/**
 * @brief Unlinks the portal from the given node's portal list.
 */
void RemovePortalFromNode(Portal *portal, Node *node) {

  // remove reference to the current portal
  Portal **pp = &node->portals;
  while (true) {
    Portal *p = *pp;
    if (!p) {
      Com_Error(ERROR_FATAL, "Portal not in leaf\n");
    }

    if (p == portal) {
      break;
    }

    if (p->nodes[0] == node) {
      pp = &p->next[0];
    } else if (p->nodes[1] == node) {
      pp = &p->next[1];
    } else {
      Com_Error(ERROR_FATAL, "Portal not bounding leaf\n");
    }
  }

  if (portal->nodes[0] == node) {
    *pp = portal->next[0];
    portal->nodes[0] = NULL;
  } else if (portal->nodes[1] == node) {
    *pp = portal->next[1];
    portal->nodes[1] = NULL;
  }
}

#define  SIDESPACE 8.f

/**
 * @brief The created portals will face the global outside node.
 */
void MakeHeadnodePortals(Tree *tree) {
  Box3 bounds;
  Portal *portals[6];

  // pad with some space so there will never be null volume leafs
  for (int32_t i = 0; i < 3; i++) {
    bounds.mins.xyz[i] = floorf(tree->bounds.mins.xyz[i]) - SIDESPACE;
    bounds.maxs.xyz[i] =  ceilf(tree->bounds.maxs.xyz[i]) + SIDESPACE;
  }

  tree->outsideNode.plane = PLANE_LEAF;
  tree->outsideNode.brushes = NULL;
  tree->outsideNode.portals = NULL;
  tree->outsideNode.contents = CONTENTS_NONE;

  for (int32_t i = 0; i < 3; i++) {
    for (int32_t j = 0; j < 2; j++) {
      const int32_t n = j * 3 + i;

      Portal *p = AllocPortal();
      portals[n] = p;

      Plane *plane = &p->plane;
      if (j) {
        plane->normal.xyz[i] = -1;
        plane->dist = -bounds.maxs.xyz[i];
      } else {
        plane->normal.xyz[i] = 1;
        plane->dist = bounds.mins.xyz[i];
      }
      p->winding = Cm_WindingForPlane(plane->normal, plane->dist);
      AddPortalToNodes(p, tree->headNode, &tree->outsideNode);
    }
  }

  // clip the basewindings by all the other planes
  for (int32_t i = 0; i < 6; i++) {
    for (int32_t j = 0; j < 6; j++) {
      if (j == i) {
        continue;
      }
      const Plane *plane = &portals[j]->plane;
      Cm_ClipWinding(&portals[i]->winding, plane->normal, plane->dist, SIDE_EPSILON);
    }
  }
}

/**
 * @brief Returns the full-plane winding for the node clipped by all of its ancestors.
 */
static CmWinding *BaseWindingForNode(const Node *node) {

  const Plane *plane = &planes[node->plane];
  CmWinding *w = Cm_WindingForPlane(plane->normal, plane->dist);

  // clip by all the parents
  for (const Node *n = node->parent; n && w;) {
    plane = &planes[n->plane];

    if (n->children[0] == node) { // take front
      Cm_ClipWinding(&w, plane->normal, plane->dist, SIDE_EPSILON);
    } else { // take back
      const Vec3 normal = Vec3_Negate(plane->normal);
      Cm_ClipWinding(&w, normal, -plane->dist, SIDE_EPSILON);
    }
    node = n;
    n = n->parent;
  }

  return w;
}

/**
 * @brief Create the new portal by taking the full plane winding for the cutting plane
 * and clipping it by all of parents of this node.
 */
void MakeNodePortal(Node *node) {
  Vec3 normal;
  double dist;
  int32_t side;

  CmWinding *w = BaseWindingForNode(node);

  // clip the portal by all the other portals in the node
  for (const Portal *p = node->portals; p && w; p = p->next[side]) {
    if (p->nodes[0] == node) {
      side = 0;
      normal = p->plane.normal;
      dist = p->plane.dist;
    } else if (p->nodes[1] == node) {
      side = 1;
      normal = Vec3_Negate(p->plane.normal);
      dist = -p->plane.dist;
    } else {
      Com_Error(ERROR_FATAL, "Mis-linked portal\n");
    }

    Cm_ClipWinding(&w, normal, dist, SIDE_EPSILON);
  }

  if (!w) {
    return;
  }

  if (WindingIsSmall(w)) {
    cSmallPortals++;
    Cm_FreeWinding(w);
    return;
  }

  Portal *portal = AllocPortal();
  portal->plane = planes[node->plane];
  portal->onNode = node;
  portal->winding = w;
  AddPortalToNodes(portal, node->children[0], node->children[1]);
}

/**
 * @brief Move or split the portals that bound the node so that its children have portals instead of node.
 */
void SplitNodePortals(Node *node) {
  Portal *next;

  Plane *plane = &planes[node->plane];

  for (Portal *p = node->portals; p; p = next) {
    int32_t side;
    if (p->nodes[0] == node) {
      side = 0;
    } else if (p->nodes[1] == node) {
      side = 1;
    } else {
      Com_Error(ERROR_FATAL, "Mis-linked portal\n");
    }

    next = p->next[side];
    Node *other = p->nodes[!side];

    RemovePortalFromNode(p, p->nodes[0]);
    RemovePortalFromNode(p, p->nodes[1]);

    // cut the portal into two portals, one on each side of the cut plane

    CmWinding *frontWinding, *backWinding;
    Cm_SplitWinding(p->winding, plane->normal, plane->dist, SIDE_EPSILON, &frontWinding, &backWinding);

    if (frontWinding && WindingIsSmall(frontWinding)) {
      Cm_FreeWinding(frontWinding);
      frontWinding = NULL;
      cSmallPortals++;
    }

    if (backWinding && WindingIsSmall(backWinding)) {
      Cm_FreeWinding(backWinding);
      backWinding = NULL;
      cSmallPortals++;
    }

    if (!frontWinding && !backWinding) { // tiny windings on both sides
      continue;
    }

    if (!frontWinding) { // only back
      Cm_FreeWinding(backWinding);
      if (side == 0) {
        AddPortalToNodes(p, node->children[1], other);
      } else {
        AddPortalToNodes(p, other, node->children[1]);
      }
      continue;
    }
    if (!backWinding) { // only front
      Cm_FreeWinding(frontWinding);
      if (side == 0) {
        AddPortalToNodes(p, node->children[0], other);
      } else {
        AddPortalToNodes(p, other, node->children[0]);
      }
      continue;
    }

    // both sides remain after the split, allocate a new portal for the back side

    Portal *q = AllocPortal();
    *q = *p;
    q->winding = backWinding;
    Cm_FreeWinding(p->winding);
    p->winding = frontWinding;

    if (side == 0) {
      AddPortalToNodes(p, node->children[0], other);
      AddPortalToNodes(q, node->children[1], other);
    } else {
      AddPortalToNodes(p, other, node->children[0]);
      AddPortalToNodes(q, other, node->children[1]);
    }
  }

  node->portals = NULL;
}

/**
 * @brief Calculates mins and maxs for both leafs and nodes.
 */
static void CalcNodeBounds(Node *node) {
  int32_t s;

  node->bounds = Box3_Null();

  for (const Portal *p = node->portals; p; p = p->next[s]) {
    s = (p->nodes[1] == node);
    node->bounds = Box3_Union(node->bounds, Cm_WindingBounds(p->winding));
  }
}

/**
 * @brief Recursively generates portals for all nodes in the tree, then propagates bounds upward.
 */
static void MakeTreePortals_r(Node *node) {

  CalcNodeBounds(node);

  if (node->bounds.mins.x >= node->bounds.maxs.x) {
    Com_Warn("Node without a volume, is your map centered?\n");
  }

  for (int32_t i = 0; i < 3; i++) {
    if (node->bounds.mins.xyz[i] < -MAX_WORLD_COORD || node->bounds.maxs.xyz[i] > MAX_WORLD_COORD) {
      Com_Warn("Node with unbounded volume, is your map centered?\n");
      break;
    }
  }

  if (node->plane == PLANE_LEAF) {
    return;
  }

  MakeNodePortal(node);
  SplitNodePortals(node);

  MakeTreePortals_r(node->children[0]);
  MakeTreePortals_r(node->children[1]);
}

/**
 * @brief Generates the initial bounding portals at the head node and recursively creates portals for the full tree.
 */
void MakeTreePortals(Tree *tree) {

  MakeHeadnodePortals(tree);

  MakeTreePortals_r(tree->headNode);
}

/**
 * @brief Recursively flood-fills reachable nodes from the given node, marking each with its flood distance.
 */
static void FloodPortals_r(Node *node, int32_t occupied) {
  int32_t s;

  node->occupied = occupied;

  for (Portal *p = node->portals; p; p = p->next[s]) {
    s = (p->nodes[1] == node);

    if (p->nodes[!s]->occupied) {
      continue;
    }

    if (!Portal_EntityFlood(p)) {
      continue;
    }

    FloodPortals_r(p->nodes[!s], occupied + 1);
  }
}

/**
 * @return True if the entity can be placed in a valid leaf beneath `headNode`, false otherwise.
 */
static bool PlaceOccupant(Node *headNode, const Vec3 origin, const Entity *occupant) {

  Node *node = headNode;
  while (node->plane != PLANE_LEAF) {
    const Plane *plane = &planes[node->plane];
    const double d = Vec3_Dot(origin, plane->normal) - plane->dist;
    if (d >= 0.0) {
      node = node->children[0];
    } else {
      node = node->children[1];
    }
  }

  if (node->contents == CONTENTS_SOLID) {
    return false;
  }

  node->occupant = occupant;

  FloodPortals_r(node, 1);
  return true;
}

/**
 * @brief Marks all nodes that can be reached by entites.
 */
bool FloodEntities(Tree *tree) {

  Com_Debug(DEBUG_ALL, "--- FloodEntities ---\n");

  bool insideOccupied = false;

  const Entity *ent = &entities[1];
  for (int32_t i = 1; i < numEntities; i++, ent++) {

    // Skip brush entities, we're only interested in point entities for flooding
    if (ent->numBrushes || ent->numPatches) {
      continue;
    }

    if (!ValueForKey(ent, "origin", NULL)) {
      continue;
    }

    Vec3 origin = VectorForKey(ent, "origin", Vec3_Zero());
    origin = Vec3_Add(origin, Vec3_Up());

    if (PlaceOccupant(tree->headNode, origin, ent)) {
      insideOccupied = true;
    } else {
      const char *classname = ValueForKey(ent, "classname", NULL);
      Com_Warn("%s @ %s resides outside map\n", classname, vtos(origin));
    }
  }

  if (!insideOccupied) {
    Com_Warn("No entities inside map.\n");
  }

  return insideOccupied && !tree->outsideNode.occupied;
}

static int32_t cOutside;
static int32_t cInside;
static int32_t cSolid;

static void FillOutside_r(Node *node) {

  if (node->plane != PLANE_LEAF) {
    FillOutside_r(node->children[0]);
    FillOutside_r(node->children[1]);
    return;
  }

  // anything not reachable by an entity can be filled away
  if (!node->occupied) {
    if (node->contents != CONTENTS_SOLID) {
      cOutside++;
      node->contents = CONTENTS_SOLID;
    } else {
      cSolid++;
    }
  } else {
    cInside++;
  }
}

/**
 * @brief Fill all nodes that can't be reached by entities.
 */
void FillOutside(Tree *tree) {

  cOutside = 0;
  cInside = 0;
  cSolid = 0;

  Com_Verbose("--- FillOutside ---\n");

  FillOutside_r(tree->headNode);
  
  Com_Verbose("%5i solid leafs\n", cSolid);
  Com_Verbose("%5i leafs filled\n", cOutside);
  Com_Verbose("%5i inside leafs\n", cInside);
}

/**
 * @brief Finds an original brush side to use for texturing the given portal.
 */
static void FindPortalBrushSide(Portal *portal) {

  // decide which content change is strongest, solid > lava > water, etc
  const int32_t c = VisibleContents(portal->nodes[0]->contents ^ portal->nodes[1]->contents);
  if (!c) {
    return;
  }

  float bestDot = 0.0;
  double bestDist = DBL_MAX;

  for (int32_t j = 0; j < 2; j++) {
    const Node *n = portal->nodes[j];

    for (const CsgBrush *brush = n->brushes; brush; brush = brush->next) {
      const Brush *original = brush->original;

      if (!(original->contents & c)) {
        continue;
      }

      BrushSide *side = original->brushSides;
      for (int32_t i = 0; i < original->numBrushSides; i++, side++) {

        if (side->surface & SURF_BEVEL) {
          continue;
        }

        if (side->surface & SURF_NODE) {
          continue;
        }

        if ((side->plane & ~1) == portal->onNode->plane) { // exact match
          portal->side = side;
          return;
        }

        // see how close the match is
        const Plane *p1 = &planes[portal->onNode->plane];
        const Plane *p2 = &planes[side->plane & ~1];

        const float dot = Vec3_Dot(p1->normal, p2->normal);
        const double dist = fabs(p1->dist - p2->dist);

        if (dot > 0.f && (dot > bestDot || (dot == bestDot && dist < bestDist))) {
          bestDot = dot;
          bestDist = dist;
          portal->side = side;
        }
      }
    }
  }

  // Only warn if the split side should have been findable
  // Don't warn for sides that are intentionally excluded from portal matching
  if (!portal->side && !leaked && portal->onNode->splitSide) {
    const int32_t surf = portal->onNode->splitSide->surface;
    // These surface types are intentionally excluded from portal matching
    if (!(surf & (SURF_NO_DRAW | SURF_SKIP))) {
      Com_Warn("Brush side not found for portal @ %s\n", vtos(Cm_WindingCenter(portal->winding)));
    }
  }
}

/**
 * @brief Traverses all portals in the tree to associate each portal with the nearest matching brush side.
 */
static void FindPortalBrushSides_r(const Node *node) {
  int32_t s;

  if (node->plane != PLANE_LEAF) {
    FindPortalBrushSides_r(node->children[0]);
    FindPortalBrushSides_r(node->children[1]);
    return;
  }

  // empty leafs are never boundary leafs
  if (node->contents == CONTENTS_NONE) {
    return;
  }

  // see if there is a visible face
  for (Portal *p = node->portals; p; p = p->next[!s]) {
    s = (p->nodes[0] == node);
    if (!p->onNode) {
      continue; // edge of world
    }
    FindPortalBrushSide(p);
  }
}

/**
 * @brief Walks the BSP tree and calls FindPortalBrushSide for every portal in every leaf.
 */
void FindPortalBrushSides(Tree *tree) {
  FindPortalBrushSides_r(tree->headNode);
}

/**
 * @brief Creates a face from the portal's winding for the given portal side, or `NULL` if the portal is not a visible boundary.
 */
static Face *FaceFromPortal(Portal *p, int32_t pside) {

  const BrushSide *side = p->side;
  if (!side) {
    return NULL; // portal does not bridge different visible contents
  }

  if (side->surface & (SURF_NO_DRAW | SURF_SKIP)) {
    return NULL; // not a visible face
  }

  Face *f = AllocFace();

  f->brushSide = side;
  f->plane = (side->plane & ~1) | pside;

  if (pside) {
    f->w = Cm_ReverseWinding(p->winding);
  } else {
    f->w = Cm_CopyWinding(p->winding);
  }

  return f;
}

static int32_t cFaces;

/**
 * @brief Create faces from portals and the brush sides they reference.
 *
 *   solid / empty : solid
 *   solid / water : solid
 *   water / empty : water
 *   water / water : none
 */
static void MakeFaces_r(Node *node) {
  int32_t s;

  // recurse down to leafs
  if (node->plane != PLANE_LEAF) {
    MakeFaces_r(node->children[0]);
    MakeFaces_r(node->children[1]);
    return;
  }

  // solid leafs never have visible faces
  if (node->contents & CONTENTS_SOLID) {
    return;
  }

  // see which portals are valid
  for (Portal *p = node->portals; p; p = p->next[s]) {
    s = (p->nodes[1] == node);

    Face *f = FaceFromPortal(p, s);
    if (f) {
      f->next = p->onNode->faces;
      p->onNode->faces = f;
      p->face[s] = f;
      cFaces++;
    }
  }
}

/**
 * @brief Creates faces for all visible leaf portals in the tree and attaches them to the portal's on-node.
 */
void MakeTreeFaces(Tree *tree) {
  Com_Verbose("--- MakeTreeFaces ---\n");

  cFaces = 0;

  MakeFaces_r(tree->headNode);

  Com_Verbose("%5i faces\n", cFaces);
}
