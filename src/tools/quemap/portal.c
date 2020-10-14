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

	if (node->plane_num == PLANE_NUM_LEAF) {
		return node->contents;
	}

	const int32_t c1 = ClusterContents(node->children[0]);
	const int32_t c2 = ClusterContents(node->children[1]);

	int32_t c = c1 | c2;

	// a cluster may include some solid detail areas, but still be seen into
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
		return false;    // to global outsideleaf
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

	if (p->nodes[0]->plane_num != PLANE_NUM_LEAF || p->nodes[1]->plane_num != PLANE_NUM_LEAF) {
		Com_Error(ERROR_FATAL, "Not a leaf\n");
	}

	// can never cross to a solid
	if ((p->nodes[0]->contents & CONTENTS_SOLID) || (p->nodes[1]->contents & CONTENTS_SOLID)) {
		return false;
	}

	// can flood through everything else
	return true;
}

static int32_t c_tinyportals;

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
	vec3_t bounds[2];
	portal_t *portals[6];

	// pad with some space so there will never be null volume leafs
	for (int32_t i = 0; i < 3; i++) {
		bounds[0].xyz[i] = floorf(tree->mins.xyz[i]) - SIDESPACE;
		bounds[1].xyz[i] = ceilf(tree->maxs.xyz[i]) + SIDESPACE;
	}

	tree->outside_node.plane_num = PLANE_NUM_LEAF;
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
				plane->dist = -bounds[j].xyz[i];
			} else {
				plane->normal.xyz[i] = 1;
				plane->dist = bounds[j].xyz[i];
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
			Cm_ClipWinding(&portals[i]->winding, plane->normal, plane->dist, CLIP_EPSILON);
		}
	}
}

/**
 * @brief
 */
static cm_winding_t *BaseWindingForNode(const node_t *node) {

	const plane_t *plane = &planes[node->plane_num];
	cm_winding_t *w = Cm_WindingForPlane(plane->normal, plane->dist);

	// clip by all the parents
	for (const node_t *n = node->parent; n && w;) {
		plane = &planes[n->plane_num];

		if (n->children[0] == node) { // take front
			Cm_ClipWinding(&w, plane->normal, plane->dist, CLIP_EPSILON);
		} else { // take back
			const vec3_t normal = Vec3_Negate(plane->normal);
			Cm_ClipWinding(&w, normal, -plane->dist, CLIP_EPSILON);
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

		Cm_ClipWinding(&w, normal, dist, CLIP_EPSILON);
	}

	if (!w) {
		return;
	}

	if (WindingIsSmall(w)) {
		c_tinyportals++;
		Cm_FreeWinding(w);
		return;
	}

	portal_t *portal = AllocPortal();
	portal->plane = planes[node->plane_num];
	portal->on_node = node;
	portal->winding = w;
	AddPortalToNodes(portal, node->children[0], node->children[1]);
}

/**
 * @brief Move or split the portals that bound the node so that its children have portals instead of node.
 */
void SplitNodePortals(node_t *node) {
	portal_t *next;

	plane_t *plane = &planes[node->plane_num];

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
		Cm_SplitWinding(p->winding, plane->normal, plane->dist, CLIP_EPSILON, &front_winding, &back_winding);

		if (front_winding && WindingIsSmall(front_winding)) {
			Cm_FreeWinding(front_winding);
			front_winding = NULL;
			c_tinyportals++;
		}

		if (back_winding && WindingIsSmall(back_winding)) {
			Cm_FreeWinding(back_winding);
			back_winding = NULL;
			c_tinyportals++;
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

	node->mins = Vec3_Mins();
	node->maxs = Vec3_Maxs();

	for (portal_t *p = node->portals; p; p = p->next[s]) {
		s = (p->nodes[1] == node);
		for (int32_t i = 0; i < p->winding->num_points; i++) {
			node->mins = Vec3_Minf(node->mins, p->winding->points[i]);
			node->maxs = Vec3_Maxf(node->maxs, p->winding->points[i]);
		}
	}
}

/**
 * @brief
 */
static void MakeTreePortals_r(node_t *node) {

	CalcNodeBounds(node);

	if (node->mins.x >= node->maxs.x) {
		Com_Warn("Node without a volume, is your map centered?\n");
	}

	for (int32_t i = 0; i < 3; i++) {
		if (node->mins.xyz[i] < -MAX_WORLD_COORD || node->maxs.xyz[i] > MAX_WORLD_COORD) {
			Com_Warn("Node with unbounded volume, is your map centered?\n");
			break;
		}
	}

	if (node->plane_num == PLANE_NUM_LEAF) {
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
	while (node->plane_num != PLANE_NUM_LEAF) {
		const plane_t *plane = &planes[node->plane_num];
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
			Mon_SendSelect(MON_WARN, ent - entities, 0, va("%s resides outside map", classname));
		}
	}

	if (!inside_occupied) {
		Mon_SendMessage(MON_WARN, "No entities inside map");
	}

	return inside_occupied && !tree->outside_node.occupied;
}

static int32_t c_areas;

/**
 * @brief
 */
static void FloodAreas_r(node_t *node) {

	if (node->contents == CONTENTS_AREA_PORTAL) {
		// this node is part of an area portal
		const csg_brush_t *b = node->brushes;
		entity_t *e = &entities[b->original->entity_num];

		// if the current area has already touched this
		// portal, we are done
		if (e->portal_areas[0] == c_areas || e->portal_areas[1] == c_areas) {
			return;
		}

		// note the current area as bounding the portal
		if (e->portal_areas[1]) {
			Com_Verbose("WARNING: areaportal entity %i touches > 2 areas\n", b->original->entity_num);
			return;
		}
		if (e->portal_areas[0]) {
			e->portal_areas[1] = c_areas;
		} else {
			e->portal_areas[0] = c_areas;
		}

		return;
	}

	if (node->area) {
		return; // already got it
	}

	node->area = c_areas;

	int32_t s;
	for (portal_t *p = node->portals; p; p = p->next[s]) {
		s = (p->nodes[1] == node);
		// TODO: why is this commented out?
#if 0
		if (points->nodes[!s]->occupied) {
			continue;
		}
#endif
		if (!Portal_EntityFlood(p)) {
			continue;
		}

		FloodAreas_r(p->nodes[!s]);
	}
}

/**
 * @brief Descend the tree, and for each node that hasn't had an area set,
 * flood fill out from there.
 */
static void FindAreas_r(node_t *node) {

	if (node->plane_num != PLANE_NUM_LEAF) {
		FindAreas_r(node->children[0]);
		FindAreas_r(node->children[1]);
		return;
	}

	if (node->area) {
		return; // already got it
	}

	if (node->contents & CONTENTS_SOLID) {
		return;
	}

	if (!node->occupied) {
		return; // not reachable by entities
	}

	// area portals are always only flooded into, never out of
	if (node->contents == CONTENTS_AREA_PORTAL) {
		return;
	}

	c_areas++;
	FloodAreas_r(node);
}

/**
 * @brief
 */
static void SetAreaPortalAreas_r(node_t *node) {

	if (node->plane_num != PLANE_NUM_LEAF) {
		SetAreaPortalAreas_r(node->children[0]);
		SetAreaPortalAreas_r(node->children[1]);
		return;
	}

	if (node->contents == CONTENTS_AREA_PORTAL) {
		if (node->area) {
			return; // already set
		}

		const csg_brush_t *brush = node->brushes;
		const entity_t *ent = &entities[brush->original->entity_num];

		node->area = ent->portal_areas[0];

		if (!ent->portal_areas[1]) {
			Mon_SendSelect(MON_WARN, brush->original->entity_num, brush->original->brush_num,
						   va("%s does not touch two areas", ValueForKey(ent, "classname", NULL)));
			return;
		}
	}
}

/**
 * @brief
 */
void EmitAreaPortals(void) {

	if (c_areas > MAX_BSP_AREAS) {
		Com_Error(ERROR_FATAL, "MAX_BSP_AREAS\n");
	}

	bsp_file.num_areas = c_areas + 1;
	bsp_file.num_area_portals = 1; // leave 0 as an error

	for (int32_t i = 1; i <= c_areas; i++) {
		bsp_file.areas[i].first_area_portal = bsp_file.num_area_portals;
		for (int32_t j = 0; j < num_entities; j++) {
			const entity_t *e = &entities[j];

			if (!e->area_portal_num) {
				continue;
			}

			bsp_area_portal_t *dp = &bsp_file.area_portals[bsp_file.num_area_portals];

			if (e->portal_areas[0] == i) {
				dp->portal_num = e->area_portal_num;
				dp->other_area = e->portal_areas[1];
				bsp_file.num_area_portals++;
			} else if (e->portal_areas[1] == i) {
				dp->portal_num = e->area_portal_num;
				dp->other_area = e->portal_areas[0];
				bsp_file.num_area_portals++;
			}
		}
		bsp_file.areas[i].num_area_portals = bsp_file.num_area_portals - bsp_file.areas[i].first_area_portal;
	}

	Com_Verbose("%5i num_areas\n", bsp_file.num_areas);
	Com_Verbose("%5i num_area_portals\n", bsp_file.num_area_portals);
}

/**
 * @brief Mark each leaf with an area, bounded by CONTENTS_AREA_PORTAL
 */
void FloodAreas(tree_t *tree) {

	Com_Verbose("--- FloodAreas ---\n");
	FindAreas_r(tree->head_node);

	SetAreaPortalAreas_r(tree->head_node);
	Com_Verbose("%5i areas\n", c_areas);
}

static int32_t c_outside;
static int32_t c_inside;
static int32_t c_solid;

static void FillOutside_r(node_t *node) {

	if (node->plane_num != PLANE_NUM_LEAF) {
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
void FillOutside(node_t *head_node) {
	c_outside = 0;
	c_inside = 0;
	c_solid = 0;
	Com_Verbose("--- FillOutside ---\n");
	FillOutside_r(head_node);
	Com_Verbose("%5i solid leafs\n", c_solid);
	Com_Verbose("%5i leafs filled\n", c_outside);
	Com_Verbose("%5i inside leafs\n", c_inside);
}

/**
 * @brief Finds a brush side to use for texturing the given portal
 */
static void FindPortalSide(portal_t *portal) {

	portal->side_found = true;

	// decide which content change is strongest, solid > lava > water, etc
	const int32_t c = VisibleContents(portal->nodes[0]->contents ^ portal->nodes[1]->contents);
	if (!c) {
		return;
	}

	float best_dot = 0.0;

	for (int32_t j = 0; j < 2; j++) {
		const node_t *n = portal->nodes[j];

		for (const csg_brush_t *b = n->brushes; b; b = b->next) {
			const brush_t *brush = b->original;

			if (!(brush->contents & c)) {
				continue;
			}

			for (int32_t i = 0; i < brush->num_sides; i++) {
				brush_side_t *side = &brush->original_sides[i];
				if (side->bevel) {
					continue;
				}
				if (side->texinfo == TEXINFO_NODE) {
					continue; // non-visible
				}
				if ((side->plane_num & ~1) == portal->on_node->plane_num) { // exact match
					portal->side = &brush->sides[i];
					return;
				}

				// see how close the match is
				const plane_t *p1 = &planes[portal->on_node->plane_num];
				const plane_t *p2 = &planes[side->plane_num & ~1];

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
						"Side not found for portal");
	}
}

/**
 * @brief
 */
static void MarkVisibleSides_r(const node_t *node) {
	int32_t s;

	if (node->plane_num != PLANE_NUM_LEAF) {
		MarkVisibleSides_r(node->children[0]);
		MarkVisibleSides_r(node->children[1]);
		return;
	}
	// empty leafs are never boundary leafs
	if (!node->contents) {
		return;
	}

	// see if there is a visible face
	for (portal_t *p = node->portals; p; p = p->next[!s]) {
		s = (p->nodes[0] == node);
		if (!p->on_node) {
			continue; // edge of world
		}
		if (!p->side_found) {
			FindPortalSide(p);
		}
		if (p->side) {
			p->side->visible = true;
		}
	}
}

/**
 * @brief
 */
void MarkVisibleSides(tree_t *tree, int32_t start, int32_t end) {

	Com_Verbose("--- MarkVisibleSides ---\n");

	// clear all the visible flags
	for (int32_t i = start; i < end; i++) {
		brush_t *brush = &brushes[i];
		for (int32_t j = 0; j < brush->num_sides; j++) {
			brush->sides[j].visible = false;
		}
	}

	// set visible flags on the sides that are used by portals
	MarkVisibleSides_r(tree->head_node);
}

/**
 * @brief
 */
static face_t *FaceFromPortal(portal_t *p, int32_t pside) {

	brush_side_t *side = p->side;
	if (!side) {
		return NULL; // portal does not bridge different visible contents
	}

	// don't emit faces marked as no draw or skip
	if (side->surf & (SURF_NO_DRAW | SURF_SKIP) && !(side->surf & (SURF_SKY | SURF_HINT))) {
		return NULL;
	}

	face_t *f = AllocFace();

	f->texinfo = side->texinfo;
	f->plane_num = (side->plane_num & ~1) | pside;
	f->portal = p;

	if (pside) {
		f->w = Cm_ReverseWinding(p->winding);
		f->contents = p->nodes[1]->contents;
	} else {
		f->w = Cm_CopyWinding(p->winding);
		f->contents = p->nodes[0]->contents;
	}

	return f;
}

static int32_t c_nodefaces;

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
	if (node->plane_num != PLANE_NUM_LEAF) {
		MakeFaces_r(node->children[0]);
		MakeFaces_r(node->children[1]);

		if (!no_merge) {
			MergeNodeFaces(node);
		}

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
			c_nodefaces++;
		}
	}
}

/**
 * @brief
 */
void MakeTreeFaces(tree_t *tree) {
	Com_Verbose("--- MakeFaces ---\n");
	c_merge = 0;
	c_nodefaces = 0;

	MakeFaces_r(tree->head_node);

	Com_Verbose("%5i node faces\n", c_nodefaces);
	Com_Verbose("%5i merged\n", c_merge);
}
