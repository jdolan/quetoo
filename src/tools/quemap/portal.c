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
#include "map.h"
#include "portal.h"
#include "qbsp.h"

static SDL_atomic_t c_active_portals;

/**
 * @brief
 */
static portal_t *AllocPortal(void) {

	SDL_AtomicAdd(&c_active_portals, 1);

	return Mem_TagMalloc(sizeof(portal_t), MEM_TAG_PORTAL);
}

/**
 * @brief
 */
void FreePortal(portal_t *p) {

	if (p->winding) {
		Cm_FreeWinding(p->winding);
	}

	SDL_AtomicAdd(&c_active_portals, -1);

	Mem_Free(p);
}

/**
 * @return The single content bit of the strongest visible content present
 */
int32_t VisibleContents(int32_t contents) {

	for (int32_t i = 1; i <= LAST_VISIBLE_CONTENTS; i <<= 1)
		if (contents & i) {
			return i;
		}

	return 0;
}

/**
 * @return The bitwise OR of all contents within node.
 */
static int32_t ClusterContents(const node_t *node) {

	if (node->plane == PLANE_LEAF) {
		return node->contents;
	}

	const int32_t c1 = ClusterContents(node->children[0]);
	const int32_t c2 = ClusterContents(node->children[1]);

	int32_t c = c1 | c2;

	// a cluster may include some solid details, but still be seen into
	if (!(c1 & CONTENTS_SOLID) || !(c2 & CONTENTS_SOLID)) {
		c &= ~CONTENTS_SOLID;
	}

	return c;
}

/**
 * @return True if the portal is empty or translucent, allowing the PVS calculation to see through it.
 * @remark The nodes on either side of the portal may actually be clusters, not leafs, so all
 * contents should be OR'ed together.
 */
_Bool Portal_VisFlood(const portal_t *p) {

	if (!p->on_node) {
		return false; // to global outsideleaf
	}

	int32_t c1 = ClusterContents(p->nodes[0]);
	int32_t c2 = ClusterContents(p->nodes[1]);

	if (!VisibleContents(c1 ^ c2)) {
		return true;
	}

	if (c1 & (CONTENTS_TRANSLUCENT | CONTENTS_DETAIL)) {
		c1 = 0;
	}
	if (c2 & (CONTENTS_TRANSLUCENT | CONTENTS_DETAIL)) {
		c2 = 0;
	}

	if ((c1 | c2) & CONTENTS_SOLID) {
		return false; // can't see through solid
	}

	if (!(c1 ^ c2)) {
		return true; // identical on both sides
	}

	if (!VisibleContents(c1 ^ c2)) {
		return true;
	}

	return false;
}

/**
 * @brief The entity flood determines which areas are "outside" of the map, which are then filled in.
 * Flowing from side s to side !s
 */
static _Bool Portal_EntityFlood(const portal_t *p) {

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

static int32_t c_small_portals;

/**
 * @brief
 */
static void AddPortalToNodes(portal_t *p, node_t *front, node_t *back) {

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
 * @brief
 */
void RemovePortalFromNode(portal_t *portal, node_t *node) {

	// remove reference to the current portal
	portal_t **pp = &node->portals;
	while (true) {
		portal_t *p = *pp;
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

#define	SIDESPACE 8.f

/**
 * @brief The created portals will face the global outside node.
 */
void MakeHeadnodePortals(tree_t *tree) {
	box3_t bounds;
	portal_t *portals[6];

	// pad with some space so there will never be null volume leafs
	for (int32_t i = 0; i < 3; i++) {
		bounds.mins.xyz[i] = floorf(tree->bounds.mins.xyz[i]) - SIDESPACE;
		bounds.maxs.xyz[i] =  ceilf(tree->bounds.maxs.xyz[i]) + SIDESPACE;
	}

	tree->outside_node.plane = PLANE_LEAF;
	tree->outside_node.brushes = NULL;
	tree->outside_node.portals = NULL;
	tree->outside_node.contents = 0;

	for (int32_t i = 0; i < 3; i++) {
		for (int32_t j = 0; j < 2; j++) {
			const int32_t n = j * 3 + i;

			portal_t *p = AllocPortal();
			portals[n] = p;

			plane_t *plane = &p->plane;
			if (j) {
				plane->normal.xyz[i] = -1;
				plane->dist = -bounds.maxs.xyz[i];
			} else {
				plane->normal.xyz[i] = 1;
				plane->dist = bounds.mins.xyz[i];
			}
			p->winding = Cm_WindingForPlane(plane->normal, plane->dist);
			AddPortalToNodes(p, tree->head_node, &tree->outside_node);
		}
	}

	// clip the basewindings by all the other planes
	for (int32_t i = 0; i < 6; i++) {
		for (int32_t j = 0; j < 6; j++) {
			if (j == i) {
				continue;
			}
			const plane_t *plane = &portals[j]->plane;
			Cm_ClipWinding(&portals[i]->winding, plane->normal, plane->dist, SIDE_EPSILON);
		}
	}
}

/**
 * @brief
 */
static cm_winding_t *BaseWindingForNode(const node_t *node) {

	const plane_t *plane = &planes[node->plane];
	cm_winding_t *w = Cm_WindingForPlane(plane->normal, plane->dist);

	// clip by all the parents
	for (const node_t *n = node->parent; n && w;) {
		plane = &planes[n->plane];

		if (n->children[0] == node) { // take front
			Cm_ClipWinding(&w, plane->normal, plane->dist, SIDE_EPSILON);
		} else { // take back
			const vec3_t normal = Vec3_Negate(plane->normal);
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
void MakeNodePortal(node_t *node) {
	vec3_t normal;
	double dist;
	int32_t side;

	cm_winding_t *w = BaseWindingForNode(node);

	// clip the portal by all the other portals in the node
	for (const portal_t *p = node->portals; p && w; p = p->next[side]) {
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
		c_small_portals++;
		Cm_FreeWinding(w);
		return;
	}

	portal_t *portal = AllocPortal();
	portal->plane = planes[node->plane];
	portal->on_node = node;
	portal->winding = w;
	AddPortalToNodes(portal, node->children[0], node->children[1]);
}

/**
 * @brief Move or split the portals that bound the node so that its children have portals instead of node.
 */
void SplitNodePortals(node_t *node) {
	portal_t *next;

	plane_t *plane = &planes[node->plane];

	for (portal_t *p = node->portals; p; p = next) {
		int32_t side;
		if (p->nodes[0] == node) {
			side = 0;
		} else if (p->nodes[1] == node) {
			side = 1;
		} else {
			Com_Error(ERROR_FATAL, "Mis-linked portal\n");
		}

		next = p->next[side];
		node_t *other = p->nodes[!side];

		RemovePortalFromNode(p, p->nodes[0]);
		RemovePortalFromNode(p, p->nodes[1]);

		// cut the portal into two portals, one on each side of the cut plane

		cm_winding_t *front_winding, *back_winding;
		Cm_SplitWinding(p->winding, plane->normal, plane->dist, SIDE_EPSILON, &front_winding, &back_winding);

		if (front_winding && WindingIsSmall(front_winding)) {
			Cm_FreeWinding(front_winding);
			front_winding = NULL;
			c_small_portals++;
		}

		if (back_winding && WindingIsSmall(back_winding)) {
			Cm_FreeWinding(back_winding);
			back_winding = NULL;
			c_small_portals++;
		}

		if (!front_winding && !back_winding) { // tiny windings on both sides
			continue;
		}

		if (!front_winding) { // only back
			Cm_FreeWinding(back_winding);
			if (side == 0) {
				AddPortalToNodes(p, node->children[1], other);
			} else {
				AddPortalToNodes(p, other, node->children[1]);
			}
			continue;
		}
		if (!back_winding) { // only front
			Cm_FreeWinding(front_winding);
			if (side == 0) {
				AddPortalToNodes(p, node->children[0], other);
			} else {
				AddPortalToNodes(p, other, node->children[0]);
			}
			continue;
		}

		// both sides remain after the split, allocate a new portal for the back side

		portal_t *q = AllocPortal();
		*q = *p;
		q->winding = back_winding;
		Cm_FreeWinding(p->winding);
		p->winding = front_winding;

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
static void CalcNodeBounds(node_t *node) {
	int32_t s;

	node->bounds = Box3_Null();

	for (portal_t *p = node->portals; p; p = p->next[s]) {
		s = (p->nodes[1] == node);
		node->bounds = Box3_Union(node->bounds, Cm_WindingBounds(p->winding));
	}
}

/**
 * @brief
 */
static void MakeTreePortals_r(node_t *node) {

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
 * @brief
 */
void MakeTreePortals(tree_t *tree) {

	MakeHeadnodePortals(tree);

	MakeTreePortals_r(tree->head_node);
}

/**
 * @brief
 */
static void FloodPortals_r(node_t *node, int32_t occupied) {
	int32_t s;

	node->occupied = occupied;

	for (portal_t *p = node->portals; p; p = p->next[s]) {
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
 * @return True if the entity can be placed in a valid leaf beneath head_node, false otherwise.
 */
static _Bool PlaceOccupant(node_t *head_node, const vec3_t origin, const entity_t *occupant) {

	node_t *node = head_node;
	while (node->plane != PLANE_LEAF) {
		const plane_t *plane = &planes[node->plane];
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
_Bool FloodEntities(tree_t *tree) {

	Com_Debug(DEBUG_ALL, "--- FloodEntities ---\n");

	_Bool inside_occupied = false;

	const entity_t *ent = &entities[1];
	for (int32_t i = 1; i < num_entities; i++, ent++) {

		if (!ValueForKey(ent, "origin", NULL)) {
			continue;
		}

		vec3_t origin = VectorForKey(ent, "origin", Vec3_Zero());
		origin = Vec3_Add(origin, Vec3_Up());

		if (PlaceOccupant(tree->head_node, origin, ent)) {
			inside_occupied = true;
		} else {
			const char *classname = ValueForKey(ent, "classname", NULL);
			Mon_SendSelect(MON_WARN, i, 0, va("%s resides outside map", classname));
		}
	}

	if (!inside_occupied) {
		Mon_SendMessage(MON_WARN, "No entities inside map");
	}

	return inside_occupied && !tree->outside_node.occupied;
}



static int32_t c_outside;
static int32_t c_inside;
static int32_t c_solid;

static void FillOutside_r(node_t *node) {

	if (node->plane != PLANE_LEAF) {
		FillOutside_r(node->children[0]);
		FillOutside_r(node->children[1]);
		return;
	}

	// anything not reachable by an entity can be filled away
	if (!node->occupied) {
		if (node->contents != CONTENTS_SOLID) {
			c_outside++;
			node->contents = CONTENTS_SOLID;
		} else {
			c_solid++;
		}
	} else {
		c_inside++;
	}
}

/**
 * @brief Fill all nodes that can't be reached by entities.
 */
void FillOutside(tree_t *tree) {

	c_outside = 0;
	c_inside = 0;
	c_solid = 0;

	Com_Verbose("--- FillOutside ---\n");

	FillOutside_r(tree->head_node);
	
	Com_Verbose("%5i solid leafs\n", c_solid);
	Com_Verbose("%5i leafs filled\n", c_outside);
	Com_Verbose("%5i inside leafs\n", c_inside);
}

/**
 * @brief Finds an original brush side to use for texturing the given portal.
 */
static void FindPortalBrushSide(portal_t *portal) {

	// decide which content change is strongest, solid > lava > water, etc
	const int32_t c = VisibleContents(portal->nodes[0]->contents ^ portal->nodes[1]->contents);
	if (!c) {
		return;
	}

	float best_dot = 0.0;

	for (int32_t j = 0; j < 2; j++) {
		const node_t *n = portal->nodes[j];

		for (const csg_brush_t *brush = n->brushes; brush; brush = brush->next) {
			const brush_t *original = brush->original;

			if (!(original->contents & c)) {
				continue;
			}

			for (int32_t i = 0; i < original->num_brush_sides; i++) {
				brush_side_t *side = &original->brush_sides[i];
				if (side->surface & SURF_BEVEL) {
					continue;
				}
				if (side->surface & SURF_NODE) {
					continue;
				}

				if ((side->plane & ~1) == portal->on_node->plane) { // exact match
					portal->side = side;
					return;
				}

				// see how close the match is
				const plane_t *p1 = &planes[portal->on_node->plane];
				const plane_t *p2 = &planes[side->plane & ~1];

				const float dot = Vec3_Dot(p1->normal, p2->normal);
				if (dot > best_dot) {
					portal->side = side;
					best_dot = dot;
				}
			}
		}
	}

	if (!portal->side && !leaked) {
		Mon_SendWinding(MON_WARN, portal->winding->points, portal->winding->num_points,
						"Brush side not found for portal");
	}
}

/**
 * @brief
 */
static void FindPortalBrushSides_r(const node_t *node) {
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
	for (portal_t *p = node->portals; p; p = p->next[!s]) {
		s = (p->nodes[0] == node);
		if (!p->on_node) {
			continue; // edge of world
		}
		FindPortalBrushSide(p);
	}
}

/**
 * @brief
 */
void FindPortalBrushSides(tree_t *tree) {

	Com_Verbose("--- MarkVisibleSides ---\n");

	FindPortalBrushSides_r(tree->head_node);
}

/**
 * @brief
 */
static face_t *FaceFromPortal(portal_t *p, int32_t pside) {

	const brush_side_t *side = p->side;
	if (!side) {
		return NULL; // portal does not bridge different visible contents
	}

	face_t *f = AllocFace();

	f->brush_side = side;
	f->portal = p;

	if (pside) {
		f->w = Cm_ReverseWinding(p->winding);
	} else {
		f->w = Cm_CopyWinding(p->winding);
	}

	return f;
}

static int32_t c_faces;

/**
 * @brief If a portal will make a visible face, mark the side that originally created it.
 *
 *   solid / empty : solid
 *   solid / water : solid
 *   water / empty : water
 *   water / water : none
 */
static void MakeFaces_r(node_t *node) {
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
	for (portal_t *p = node->portals; p; p = p->next[s]) {
		s = (p->nodes[1] == node);

		face_t *f = FaceFromPortal(p, s);
		if (f) {
			f->next = p->on_node->faces;
			p->on_node->faces = f;
			p->face[s] = f;
			c_faces++;
		}
	}
}

/**
 * @brief
 */
void MakeTreeFaces(tree_t *tree) {
	Com_Verbose("--- MakeFaces ---\n");

	c_faces = 0;

	MakeFaces_r(tree->head_node);

	Com_Verbose("%5i faces\n", c_faces);
}
