/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#include "qbsp.h"

static int c_active_portals;
static int c_peak_portals;

/*
 * ===========
 * AllocPortal
 * ===========
 */
static portal_t *AllocPortal(void) {
	portal_t *p;

	if (!threads->integer)
		c_active_portals++;
	if (c_active_portals > c_peak_portals)
		c_peak_portals = c_active_portals;

	p = Z_Malloc(sizeof(*p));
	memset(p, 0, sizeof(*p));

	return p;
}

void FreePortal(portal_t * p) {
	if (p->winding)
		FreeWinding(p->winding);
	if (!threads->integer)
		c_active_portals--;
	Z_Free(p);
}

//==============================================================

/*
 * ==============
 * VisibleContents
 *
 * Returns the single content bit of the
 * strongest visible content present
 * ==============
 */
int VisibleContents(int contents) {
	int i;

	for (i = 1; i <= LAST_VISIBLE_CONTENTS; i <<= 1)
		if (contents & i)
			return i;

	return 0;
}

/*
 * ===============
 * ClusterContents
 * ===============
 */
static int ClusterContents(const node_t * node) {
	int c1, c2, c;

	if (node->plane_num == PLANENUM_LEAF)
		return node->contents;

	c1 = ClusterContents(node->children[0]);
	c2 = ClusterContents(node->children[1]);
	c = c1 | c2;

	// a cluster may include some solid detail areas, but
	// still be seen into
	if (!(c1 & CONTENTS_SOLID) || !(c2 & CONTENTS_SOLID))
		c &= ~CONTENTS_SOLID;
	return c;
}

/*
 * =============
 * Portal_VisFlood
 *
 * Returns true if the portal is empty or translucent, allowing
 * the PVS calculation to see through it.
 * The nodes on either side of the portal may actually be clusters,
 * not leafs, so all contents should be ored together
 * =============
 */
boolean_t Portal_VisFlood(const portal_t * p) {
	int c1, c2;

	if (!p->onnode)
		return false; // to global outsideleaf

	c1 = ClusterContents(p->nodes[0]);
	c2 = ClusterContents(p->nodes[1]);

	if (!VisibleContents(c1 ^ c2))
		return true;

	if (c1 & (CONTENTS_TRANSLUCENT | CONTENTS_DETAIL))
		c1 = 0;
	if (c2 & (CONTENTS_TRANSLUCENT | CONTENTS_DETAIL))
		c2 = 0;

	if ((c1 | c2) & CONTENTS_SOLID)
		return false; // can't see through solid

	if (!(c1 ^ c2))
		return true; // identical on both sides

	if (!VisibleContents(c1 ^ c2))
		return true;
	return false;
}

/*
 * ===============
 * Portal_EntityFlood
 *
 * The entity flood determines which areas are
 * "outside" on the map, which are then filled in.
 * Flowing from side s to side !s
 * ===============
 */
static boolean_t Portal_EntityFlood(const portal_t * p, int s) {
	if (p->nodes[0]->plane_num != PLANENUM_LEAF || p->nodes[1]->plane_num
			!= PLANENUM_LEAF)
		Com_Error(ERR_FATAL, "Portal_EntityFlood: not a leaf\n");

	// can never cross to a solid
	if ((p->nodes[0]->contents & CONTENTS_SOLID) || (p->nodes[1]->contents
			& CONTENTS_SOLID))
		return false;

	// can flood through everything else
	return true;
}

//=============================================================================

static int c_tinyportals;

/*
 * =============
 * AddPortalToNodes
 * =============
 */
static void AddPortalToNodes(portal_t * p, node_t * front, node_t * back) {
	if (p->nodes[0] || p->nodes[1])
		Com_Error(ERR_FATAL, "AddPortalToNode: already included\n");

	p->nodes[0] = front;
	p->next[0] = front->portals;
	front->portals = p;

	p->nodes[1] = back;
	p->next[1] = back->portals;
	back->portals = p;
}

/*
 * =============
 * RemovePortalFromNode
 * =============
 */
void RemovePortalFromNode(portal_t * portal, node_t * l) {
	portal_t **pp, *t;

	// remove reference to the current portal
	pp = &l->portals;
	while (true) {
		t = *pp;
		if (!t)
			Com_Error(ERR_FATAL, "RemovePortalFromNode: portal not in leaf\n");

		if (t == portal)
			break;

		if (t->nodes[0] == l)
			pp = &t->next[0];
		else if (t->nodes[1] == l)
			pp = &t->next[1];
		else
			Com_Error(ERR_FATAL,
					"RemovePortalFromNode: portal not bounding leaf\n");
	}

	if (portal->nodes[0] == l) {
		*pp = portal->next[0];
		portal->nodes[0] = NULL;
	} else if (portal->nodes[1] == l) {
		*pp = portal->next[1];
		portal->nodes[1] = NULL;
	}
}

/*
 * MakeHeadnodePortals
 *
 * The created portals will face the global outside_node
 */
#define	SIDESPACE	8
void MakeHeadnodePortals(tree_t * tree) {
	vec3_t bounds[2];
	int i, j, n;
	portal_t *p, *portals[6];
	map_plane_t bplanes[6], *pl;
	node_t *node;

	node = tree->head_node;

	// pad with some space so there will never be null volume leafs
	for (i = 0; i < 3; i++) {
		bounds[0][i] = tree->mins[i] - SIDESPACE;
		bounds[1][i] = tree->maxs[i] + SIDESPACE;
	}

	tree->outside_node.plane_num = PLANENUM_LEAF;
	tree->outside_node.brushes = NULL;
	tree->outside_node.portals = NULL;
	tree->outside_node.contents = 0;

	for (i = 0; i < 3; i++)
		for (j = 0; j < 2; j++) {
			n = j * 3 + i;

			p = AllocPortal();
			portals[n] = p;

			pl = &bplanes[n];
			memset(pl, 0, sizeof(*pl));
			if (j) {
				pl->normal[i] = -1;
				pl->dist = -bounds[j][i];
			} else {
				pl->normal[i] = 1;
				pl->dist = bounds[j][i];
			}
			p->plane = *pl;
			p->winding = BaseWindingForPlane(pl->normal, pl->dist);
			AddPortalToNodes(p, node, &tree->outside_node);
		}

	// clip the basewindings by all the other planes
	for (i = 0; i < 6; i++) {
		for (j = 0; j < 6; j++) {
			if (j == i)
				continue;
			ChopWindingInPlace(&portals[i]->winding, bplanes[j].normal,
					bplanes[j].dist, ON_EPSILON);
		}
	}
}

//===================================================


/*
 * ================
 * BaseWindingForNode
 * ================
 */
#define	BASE_WINDING_EPSILON	0.001
#define	SPLIT_WINDING_EPSILON	0.001

static winding_t *BaseWindingForNode(const node_t * node) {
	winding_t *w;
	const node_t *n;
	const map_plane_t *plane;
	vec3_t normal;
	vec_t dist;

	w = BaseWindingForPlane(map_planes[node->plane_num].normal,
			map_planes[node->plane_num].dist);

	// clip by all the parents
	for (n = node->parent; n && w;) {
		plane = &map_planes[n->plane_num];

		if (n->children[0] == node) { // take front
			ChopWindingInPlace(&w, plane->normal, plane->dist,
					BASE_WINDING_EPSILON);
		} else { // take back
			VectorSubtract(vec3_origin, plane->normal, normal);
			dist = -plane->dist;
			ChopWindingInPlace(&w, normal, dist, BASE_WINDING_EPSILON);
		}
		node = n;
		n = n->parent;
	}

	return w;
}

/*
 * MakeNodePortal
 *
 * create the new portal by taking the full plane winding for the cutting plane
 * and clipping it by all of parents of this node
 */
void MakeNodePortal(node_t * node) {
	portal_t *new_portal, *p;
	winding_t *w;
	vec3_t normal;
	float dist;
	int side;

	w = BaseWindingForNode(node);

	side = 0;
	dist = 0;

	// clip the portal by all the other portals in the node
	for (p = node->portals; p && w; p = p->next[side]) {
		if (p->nodes[0] == node) {
			side = 0;
			VectorCopy(p->plane.normal, normal);
			dist = p->plane.dist;
		} else if (p->nodes[1] == node) {
			side = 1;
			VectorSubtract(vec3_origin, p->plane.normal, normal);
			dist = -p->plane.dist;
		} else
			Com_Error(ERR_FATAL, "CutNodePortals_r: mislinked portal\n");

		ChopWindingInPlace(&w, normal, dist, 0.1);
	}

	if (!w) {
		return;
	}

	if (WindingIsTiny(w)) {
		c_tinyportals++;
		FreeWinding(w);
		return;
	}

	new_portal = AllocPortal();
	new_portal->plane = map_planes[node->plane_num];
	new_portal->onnode = node;
	new_portal->winding = w;
	AddPortalToNodes(new_portal, node->children[0], node->children[1]);
}

/*
 * ==============
 * SplitNodePortals
 *
 * Move or split the portals that bound node so that the node's
 * children have portals instead of node.
 * ==============
 */
void SplitNodePortals(node_t * node) {
	portal_t *p, *next_portal, *new_portal;
	node_t *f, *b, *other_node;
	int side;
	map_plane_t *plane;
	winding_t *frontwinding, *backwinding;

	plane = &map_planes[node->plane_num];
	f = node->children[0];
	b = node->children[1];

	side = 0;

	for (p = node->portals; p; p = next_portal) {
		if (p->nodes[0] == node)
			side = 0;
		else if (p->nodes[1] == node)
			side = 1;
		else
			Com_Error(ERR_FATAL, "CutNodePortals_r: mislinked portal\n");
		next_portal = p->next[side];

		other_node = p->nodes[!side];
		RemovePortalFromNode(p, p->nodes[0]);
		RemovePortalFromNode(p, p->nodes[1]);

		//
		// cut the portal into two portals, one on each side of the cut plane
		//
		ClipWindingEpsilon(p->winding, plane->normal, plane->dist,
				SPLIT_WINDING_EPSILON, &frontwinding, &backwinding);

		if (frontwinding && WindingIsTiny(frontwinding)) {
			FreeWinding(frontwinding);
			frontwinding = NULL;
			c_tinyportals++;
		}

		if (backwinding && WindingIsTiny(backwinding)) {
			FreeWinding(backwinding);
			backwinding = NULL;
			c_tinyportals++;
		}

		if (!frontwinding && !backwinding) { // tiny windings on both sides
			continue;
		}

		if (!frontwinding) {
			FreeWinding(backwinding);
			if (side == 0)
				AddPortalToNodes(p, b, other_node);
			else
				AddPortalToNodes(p, other_node, b);
			continue;
		}
		if (!backwinding) {
			FreeWinding(frontwinding);
			if (side == 0)
				AddPortalToNodes(p, f, other_node);
			else
				AddPortalToNodes(p, other_node, f);
			continue;
		}
		// the winding is split
		new_portal = AllocPortal();
		*new_portal = *p;
		new_portal->winding = backwinding;
		FreeWinding(p->winding);
		p->winding = frontwinding;

		if (side == 0) {
			AddPortalToNodes(p, f, other_node);
			AddPortalToNodes(new_portal, b, other_node);
		} else {
			AddPortalToNodes(p, other_node, f);
			AddPortalToNodes(new_portal, other_node, b);
		}
	}

	node->portals = NULL;
}

/*
 * ================
 * CalcNodeBounds
 * ================
 */
static void CalcNodeBounds(node_t * node) {
	portal_t *p;
	int i, s;

	// calc mins/maxs for both leafs and nodes
	ClearBounds(node->mins, node->maxs);
	for (p = node->portals; p; p = p->next[s]) {
		s = (p->nodes[1] == node);
		for (i = 0; i < p->winding->numpoints; i++)
			AddPointToBounds(p->winding->p[i], node->mins, node->maxs);
	}
}

/*
 * MakeTreePortals_r
 */
static void MakeTreePortals_r(node_t * node) {
	int i;

	CalcNodeBounds(node);
	if (node->mins[0] >= node->maxs[0]) {
		Com_Verbose("WARNING: node without a volume\n");
	}

	for (i = 0; i < 3; i++) {
		if (node->mins[i] < -8000 || node->maxs[i] > 8000) {
			Com_Verbose("WARNING: node with unbounded volume\n");
			break;
		}
	}
	if (node->plane_num == PLANENUM_LEAF)
		return;

	MakeNodePortal(node);
	SplitNodePortals(node);

	MakeTreePortals_r(node->children[0]);
	MakeTreePortals_r(node->children[1]);
}

/*
 * MakeTreePortals
 */
void MakeTreePortals(tree_t * tree) {
	MakeHeadnodePortals(tree);
	MakeTreePortals_r(tree->head_node);
}

/*
 *
 * FLOOD ENTITIES
 *
 */

/*
 * FloodPortals_r
 */
static void FloodPortals_r(node_t * node, int dist) {
	portal_t *p;
	int s;

	node->occupied = dist;

	for (p = node->portals; p; p = p->next[s]) {
		s = (p->nodes[1] == node);

		if (p->nodes[!s]->occupied)
			continue;

		if (!Portal_EntityFlood(p, s))
			continue;

		FloodPortals_r(p->nodes[!s], dist + 1);
	}
}

/*
 * PlaceOccupant
 */
static boolean_t PlaceOccupant(node_t * head_node, vec3_t origin,
		entity_t * occupant) {
	node_t *node;

	// find the leaf to start in
	node = head_node;
	while (node->plane_num != PLANENUM_LEAF) {
		const map_plane_t *plane = &map_planes[node->plane_num];
		const vec_t d = DotProduct(origin, plane->normal) - plane->dist;
		if (d >= 0)
			node = node->children[0];
		else
			node = node->children[1];
	}

	if (node->contents == CONTENTS_SOLID)
		return false;
	node->occupant = occupant;

	FloodPortals_r(node, 1);

	return true;
}

/*
 * FloodEntities
 *
 * Marks all nodes that can be reached by entites
 */
boolean_t FloodEntities(tree_t *tree) {
	int i;
	vec3_t origin;
	const char *cl;
	boolean_t inside;
	node_t *head_node;

	head_node = tree->head_node;
	Com_Debug("--- FloodEntities ---\n");
	inside = false;
	tree->outside_node.occupied = 0;
	cl = "";

	for (i = 1; i < num_entities; i++) {
		GetVectorForKey(&entities[i], "origin", origin);
		if (VectorCompare(origin, vec3_origin))
			continue;

		cl = ValueForKey(&entities[i], "classname");
		origin[2] += 1; // so objects on floor are ok

		// nudge playerstart around if needed so clipping hulls always
		// have a valid point
		if (!strcmp(cl, "info_player_start")) {
			int x, y;

			for (x = -16; x <= 16; x += 16) {
				for (y = -16; y <= 16; y += 16) {
					origin[0] += x;
					origin[1] += y;
					if (PlaceOccupant(head_node, origin, &entities[i])) {
						inside = true;
						goto gotit;
					}
					origin[0] -= x;
					origin[1] -= y;
				}
			}
			gotit: ;
		} else {
			if (PlaceOccupant(head_node, origin, &entities[i]))
				inside = true;
		}
	}

	if (!inside) {
		Com_Debug("no entities in open -- no filling\n");
	} else if (tree->outside_node.occupied) {
		Com_Debug("entity %s reached from outside -- no filling\n", cl);
	}

	return inside && !tree->outside_node.occupied;
}

/*
 * FLOOD AREAS
 */

static int c_areas;

/*
 * FloodAreas_r
 */
static void FloodAreas_r(node_t * node) {
	portal_t *p;
	int s;

	if (node->contents == CONTENTS_AREA_PORTAL) {
		// this node is part of an area portal
		const bsp_brush_t *b = node->brushes;
		entity_t *e = &entities[b->original->entity_num];

		// if the current area has already touched this
		// portal, we are done
		if (e->portal_areas[0] == c_areas || e->portal_areas[1] == c_areas)
			return;

		// note the current area as bounding the portal
		if (e->portal_areas[1]) {
			Com_Verbose("WARNING: areaportal entity %i touches > 2 areas\n",
					b->original->entity_num);
			return;
		}
		if (e->portal_areas[0])
			e->portal_areas[1] = c_areas;
		else
			e->portal_areas[0] = c_areas;

		return;
	}

	if (node->area)
		return; // already got it
	node->area = c_areas;

	for (p = node->portals; p; p = p->next[s]) {
		s = (p->nodes[1] == node);
#if 0
		if(p->nodes[!s]->occupied)
		continue;
#endif
		if (!Portal_EntityFlood(p, s))
			continue;

		FloodAreas_r(p->nodes[!s]);
	}
}

/*
 * =============
 * FindAreas_r
 *
 * Just decend the tree, and for each node that hasn't had an
 * area set, flood fill out from there
 * =============
 */
static void FindAreas_r(node_t * node) {
	if (node->plane_num != PLANENUM_LEAF) {
		FindAreas_r(node->children[0]);
		FindAreas_r(node->children[1]);
		return;
	}

	if (node->area)
		return; // already got it

	if (node->contents & CONTENTS_SOLID)
		return;

	if (!node->occupied)
		return; // not reachable by entities

	// area portals are always only flooded into, never
	// out of
	if (node->contents == CONTENTS_AREA_PORTAL)
		return;

	c_areas++;
	FloodAreas_r(node);
}

/*
 * =============
 * SetAreaPortalAreas_r
 *
 * Just decend the tree, and for each node that hasn't had an
 * area set, flood fill out from there
 * =============
 */
static void SetAreaPortalAreas_r(node_t * node) {
	bsp_brush_t *b;
	entity_t *e;

	if (node->plane_num != PLANENUM_LEAF) {
		SetAreaPortalAreas_r(node->children[0]);
		SetAreaPortalAreas_r(node->children[1]);
		return;
	}

	if (node->contents == CONTENTS_AREA_PORTAL) {
		if (node->area)
			return; // already set

		b = node->brushes;
		e = &entities[b->original->entity_num];
		node->area = e->portal_areas[0];
		if (!e->portal_areas[1]) {
			Com_Verbose(
					"WARNING: areaportal entity %i doesn't touch two areas\n",
					b->original->entity_num);
			return;
		}
	}
}

/*
 * EmitAreaPortals
 */
void EmitAreaPortals(node_t * head_node) {
	int i, j;
	d_bsp_area_portal_t *dp;

	if (c_areas > MAX_BSP_AREAS)
		Com_Error(ERR_FATAL, "MAX_BSP_AREAS\n");

	d_bsp.num_areas = c_areas + 1;
	d_bsp.num_area_portals = 1; // leave 0 as an error

	for (i = 1; i <= c_areas; i++) {
		d_bsp.areas[i].first_area_portal = d_bsp.num_area_portals;
		for (j = 0; j < num_entities; j++) {
			const entity_t *e = &entities[j];

			if (!e->area_portal_num)
				continue;

			dp = &d_bsp.area_portals[d_bsp.num_area_portals];

			if (e->portal_areas[0] == i) {
				dp->portal_num = e->area_portal_num;
				dp->other_area = e->portal_areas[1];
				d_bsp.num_area_portals++;
			} else if (e->portal_areas[1] == i) {
				dp->portal_num = e->area_portal_num;
				dp->other_area = e->portal_areas[0];
				d_bsp.num_area_portals++;
			}
		}
		d_bsp.areas[i].num_area_portals = d_bsp.num_area_portals
				- d_bsp.areas[i].first_area_portal;
	}

	Com_Verbose("%5i num_areas\n", d_bsp.num_areas);
	Com_Verbose("%5i num_area_portals\n", d_bsp.num_area_portals);
}

/*
 * FloodAreas
 *
 * Mark each leaf with an area, bounded by CONTENTS_AREA_PORTAL
 */
void FloodAreas(tree_t * tree) {
	Com_Verbose("--- FloodAreas ---\n");
	FindAreas_r(tree->head_node);
	SetAreaPortalAreas_r(tree->head_node);
	Com_Verbose("%5i areas\n", c_areas);
}

static int c_outside;
static int c_inside;
static int c_solid;

static void FillOutside_r(node_t * node) {
	if (node->plane_num != PLANENUM_LEAF) {
		FillOutside_r(node->children[0]);
		FillOutside_r(node->children[1]);
		return;
	}
	// anything not reachable by an entity
	// can be filled away
	if (!node->occupied) {
		if (node->contents != CONTENTS_SOLID) {
			c_outside++;
			node->contents = CONTENTS_SOLID;
		} else
			c_solid++;
	} else
		c_inside++;

}

/*
 * =============
 * FillOutside
 *
 * Fill all nodes that can't be reached by entities
 * =============
 */
void FillOutside(node_t * head_node) {
	c_outside = 0;
	c_inside = 0;
	c_solid = 0;
	Com_Verbose("--- FillOutside ---\n");
	FillOutside_r(head_node);
	Com_Verbose("%5i solid leafs\n", c_solid);
	Com_Verbose("%5i leafs filled\n", c_outside);
	Com_Verbose("%5i inside leafs\n", c_inside);
}

//==============================================================

/*
 * ============
 * FindPortalSide
 *
 * Finds a brush side to use for texturing the given portal
 * ============
 */
static void FindPortalSide(portal_t * p) {
	int viscontents;
	bsp_brush_t *bb;
	int i, j;
	int plane_num;
	side_t *bestside;
	float dot, bestdot;
	map_plane_t *p2;

	// decide which content change is strongest
	// solid > lava > water, etc
	viscontents
			= VisibleContents(p->nodes[0]->contents ^ p->nodes[1]->contents);
	if (!viscontents)
		return;

	plane_num = p->onnode->plane_num;
	bestside = NULL;
	bestdot = 0;

	for (j = 0; j < 2; j++) {
		const node_t *n = p->nodes[j];
		const map_plane_t *p1 = &map_planes[p->onnode->plane_num];
		for (bb = n->brushes; bb; bb = bb->next) {
			const map_brush_t *brush = bb->original;
			if (!(brush->contents & viscontents))
				continue;
			for (i = 0; i < brush->num_sides; i++) {
				side_t *side = &brush->original_sides[i];
				if (side->bevel)
					continue;
				if (side->texinfo == TEXINFO_NODE)
					continue; // non-visible
				if ((side->plane_num & ~1) == plane_num) { // exact match
					bestside = &brush->original_sides[i];
					goto gotit;
				}
				// see how close the match is
				p2 = &map_planes[side->plane_num & ~1];
				dot = DotProduct(p1->normal, p2->normal);
				if (dot > bestdot) {
					bestdot = dot;
					bestside = side;
				}
			}
		}
	}

	gotit: if (!bestside)
		Com_Verbose("WARNING: side not found for portal\n");

	p->sidefound = true;
	p->side = bestside;
}

/*
 * ===============
 * MarkVisibleSides_r
 *
 * ===============
 */
static void MarkVisibleSides_r(const node_t * node) {
	portal_t *p;
	int s;

	if (node->plane_num != PLANENUM_LEAF) {
		MarkVisibleSides_r(node->children[0]);
		MarkVisibleSides_r(node->children[1]);
		return;
	}
	// empty leafs are never boundary leafs
	if (!node->contents)
		return;

	// see if there is a visible face
	for (p = node->portals; p; p = p->next[!s]) {
		s = (p->nodes[0] == node);
		if (!p->onnode)
			continue; // edge of world
		if (!p->sidefound)
			FindPortalSide(p);
		if (p->side)
			p->side->visible = true;
	}

}

/*
 * =============
 * MarkVisibleSides
 *
 * =============
 */
void MarkVisibleSides(tree_t * tree, int startbrush, int endbrush) {
	int i, j;

	Com_Verbose("--- MarkVisibleSides ---\n");

	// clear all the visible flags
	for (i = startbrush; i < endbrush; i++) {
		map_brush_t *mb = &map_brushes[i];
		const int num_sides = mb->num_sides;
		for (j = 0; j < num_sides; j++)
			mb->original_sides[j].visible = false;
	}

	// set visible flags on the sides that are used by portals
	MarkVisibleSides_r(tree->head_node);
}
