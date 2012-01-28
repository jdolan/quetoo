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

#include "sv_local.h"

/*
 * ENTITY AREA CHECKING
 *
 * Note that this use of "area" is different from the BSP file use.
 */

#define	STRUCT_FROM_LINK(l, t ,m) ((t *)((byte *)l - (ptrdiff_t)&(((t *)0)->m)))
#define EDICT_FROM_AREA(l) STRUCT_FROM_LINK(l, g_edict_t, area)

typedef struct sv_area_node_s {
	int axis; // -1 = leaf node
	float dist;
	struct sv_area_node_s *children[2];
	link_t trigger_edicts;
	link_t solid_edicts;
} sv_area_node_t;

#define AREA_DEPTH	4
#define AREA_NODES	32

// the server's view of the world, by areas
typedef struct sv_world_s {

	sv_area_node_t area_nodes[AREA_NODES];
	int num_area_nodes;

	float *area_mins, *area_maxs;

	g_edict_t **area_edicts;

	int num_area_edicts, max_area_edicts;
	int area_type;
} sv_world_t;

sv_world_t sv_world;

/*
 * Sv_ClearLink
 */
static void Sv_ClearLink(link_t *l) {
	l->prev = l->next = l;
}

/*
 * Sv_RemoveLink
 */
static void Sv_RemoveLink(link_t *l) {
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

/*
 * Sv_InsertLink
 */
static void Sv_InsertLink(link_t *l, link_t *before) {
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}

/*
 * Sv_CreateAreaNode
 *
 * Builds a uniformly subdivided tree for the given world size.
 */
static sv_area_node_t *Sv_CreateAreaNode(int depth, vec3_t mins, vec3_t maxs) {
	sv_area_node_t *anode;
	vec3_t size;
	vec3_t mins1, maxs1, mins2, maxs2;

	anode = &sv_world.area_nodes[sv_world.num_area_nodes];
	sv_world.num_area_nodes++;

	Sv_ClearLink(&anode->trigger_edicts);
	Sv_ClearLink(&anode->solid_edicts);

	if (depth == AREA_DEPTH) {
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}

	VectorSubtract(maxs, mins, size);
	if (size[0] > size[1])
		anode->axis = 0;
	else
		anode->axis = 1;

	anode->dist = 0.5 * (maxs[anode->axis] + mins[anode->axis]);
	VectorCopy(mins, mins1);
	VectorCopy(mins, mins2);
	VectorCopy(maxs, maxs1);
	VectorCopy(maxs, maxs2);

	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;

	anode->children[0] = Sv_CreateAreaNode(depth + 1, mins2, maxs2);
	anode->children[1] = Sv_CreateAreaNode(depth + 1, mins1, maxs1);

	return anode;
}

/*
 * Sv_InitWorld
 *
 * Resolve our area nodes for a newly loaded level. This is called prior to
 * linking any entities.
 */
void Sv_InitWorld(void) {

	memset(&sv_world, 0, sizeof(sv_world));

	Sv_CreateAreaNode(0, sv.models[1]->mins, sv.models[1]->maxs);
}

/*
 * Sv_UnlinkEdict
 *
 * Called before moving or freeing an entity to remove it from the clipping
 * hull.
 */
void Sv_UnlinkEdict(g_edict_t *ent) {

	if (!ent->area.prev)
		return; // not linked in anywhere

	Sv_RemoveLink(&ent->area);
	ent->area.prev = ent->area.next = NULL;
}

#define MAX_TOTAL_ENT_LEAFS 128

/*
 * Sv_LinkEdict
 *
 * Called whenever an entity changes origin, mins, maxs, or solid to add it to
 * the clipping hull.
 */
void Sv_LinkEdict(g_edict_t *ent) {
	sv_area_node_t *node;
	int leafs[MAX_TOTAL_ENT_LEAFS];
	int clusters[MAX_TOTAL_ENT_LEAFS];
	int num_leafs;
	int i, j, k;
	int area;
	int top_node;

	if (ent == svs.game->edicts) // never bother with the world
		return;

	if (ent->area.prev) // unlink from its previous area
		Sv_UnlinkEdict(ent);

	if (!ent->in_use) // and if its free, we're done
		return;

	// set the size
	VectorSubtract(ent->maxs, ent->mins, ent->size);

	// encode the size into the entity_state for client prediction
	if (ent->solid == SOLID_BOX) { // assume that x/y are equal and symetric
		i = ent->maxs[0] / 8;
		if (i < 1)
			i = 1;
		if (i > 31)
			i = 31;

		// z is not symmetric
		j = (-ent->mins[2]) / 8;
		if (j < 1)
			j = 1;
		if (j > 31)
			j = 31;

		// and z maxs can be negative...
		k = (ent->maxs[2] + 32) / 8;
		if (k < 1)
			k = 1;
		if (k > 63)
			k = 63;

		ent->s.solid = (k << 10) | (j << 5) | i;
	} else if (ent->solid == SOLID_BSP) {
		ent->s.solid = 31; // a solid_bbox will never create this value
	} else
		ent->s.solid = 0;

	// set the absolute bounding box
	if (ent->solid == SOLID_BSP && (ent->s.angles[0] || ent->s.angles[1]
			|| ent->s.angles[2])) { // expand for rotation
		float max, v;
		int i;

		max = 0;
		for (i = 0; i < 3; i++) {
			v = fabsf(ent->mins[i]);
			if (v > max)
				max = v;
			v = fabsf(ent->maxs[i]);
			if (v > max)
				max = v;
		}
		for (i = 0; i < 3; i++) {
			ent->abs_mins[i] = ent->s.origin[i] - max;
			ent->abs_maxs[i] = ent->s.origin[i] + max;
		}
	} else { // normal
		VectorAdd(ent->s.origin, ent->mins, ent->abs_mins);
		VectorAdd(ent->s.origin, ent->maxs, ent->abs_maxs);
	}

	// because movement is clipped an epsilon away from an actual edge,
	// we must fully check even when bounding boxes don't quite touch
	ent->abs_mins[0] -= 1;
	ent->abs_mins[1] -= 1;
	ent->abs_mins[2] -= 1;
	ent->abs_maxs[0] += 1;
	ent->abs_maxs[1] += 1;
	ent->abs_maxs[2] += 1;

	// link to PVS leafs
	ent->num_clusters = 0;
	ent->area_num = 0;
	ent->area_num2 = 0;

	// get all leafs, including solids
	num_leafs = Cm_BoxLeafnums(ent->abs_mins, ent->abs_maxs, leafs,
			MAX_TOTAL_ENT_LEAFS, &top_node);

	// set areas
	for (i = 0; i < num_leafs; i++) {
		clusters[i] = Cm_LeafCluster(leafs[i]);
		area = Cm_LeafArea(leafs[i]);
		if (area) { // doors may legally occupy two areas,
			// but nothing should ever need more than that
			if (ent->area_num && ent->area_num != area) {
				if (ent->area_num2 && ent->area_num2 != area && sv.state
						== SV_LOADING) {
					Com_Debug("Object touching 3 areas at %f %f %f\n",
							ent->abs_mins[0], ent->abs_mins[1],
							ent->abs_mins[2]);
				}
				ent->area_num2 = area;
			} else
				ent->area_num = area;
		}
	}

	if (num_leafs >= MAX_TOTAL_ENT_LEAFS) { // assume we missed some leafs, and mark by head_node
		ent->num_clusters = -1;
		ent->head_node = top_node;
	} else {
		ent->num_clusters = 0;
		for (i = 0; i < num_leafs; i++) {

			if (clusters[i] == -1)
				continue; // not a visible leaf

			for (j = 0; j < i; j++)
				if (clusters[j] == clusters[i])
					break;

			if (j == i) {
				if (ent->num_clusters == MAX_ENT_CLUSTERS) { // assume we missed some leafs, and mark by head_node
					ent->num_clusters = -1;
					ent->head_node = top_node;
					break;
				}

				ent->cluster_nums[ent->num_clusters++] = clusters[i];
			}
		}
	}

	// if first time, make sure old_origin is valid
	if (!ent->link_count) {
		VectorCopy(ent->s.origin, ent->s.old_origin);
	}
	ent->link_count++;

	if (ent->solid == SOLID_NOT)
		return;

	// find the first node that the ent's box crosses
	node = sv_world.area_nodes;
	while (true) {

		if (node->axis == -1)
			break;

		if (ent->abs_mins[node->axis] > node->dist)
			node = node->children[0];
		else if (ent->abs_maxs[node->axis] < node->dist)
			node = node->children[1];
		else
			break; // crosses the node
	}

	// link it in
	if (ent->solid == SOLID_TRIGGER)
		Sv_InsertLink(&ent->area, &node->trigger_edicts);
	else
		Sv_InsertLink(&ent->area, &node->solid_edicts);
}

/*
 * Sv_AreaEdicts_r
 */
static void Sv_AreaEdicts_r(sv_area_node_t *node) {
	link_t *l, *next, *start;
	g_edict_t *check;

	// touch linked edicts
	if (sv_world.area_type == AREA_SOLID)
		start = &node->solid_edicts;
	else
		start = &node->trigger_edicts;

	for (l = start->next; l != start; l = next) {
		next = l->next;
		check = EDICT_FROM_AREA(l);

		if (check->solid == SOLID_NOT)
			continue; // skip it

		if (check->abs_mins[0] > sv_world.area_maxs[0] || check->abs_mins[1]
				> sv_world.area_maxs[1] || check->abs_mins[2]
				> sv_world.area_maxs[2] || check->abs_maxs[0]
				< sv_world.area_mins[0] || check->abs_maxs[1]
				< sv_world.area_mins[1] || check->abs_maxs[2]
				< sv_world.area_mins[2])
			continue; // not touching

		if (sv_world.num_area_edicts == sv_world.max_area_edicts) {
			Com_Warn("Sv_AreaEdicts: MAXCOUNT\n");
			return;
		}

		sv_world.area_edicts[sv_world.num_area_edicts] = check;
		sv_world.num_area_edicts++;
	}

	if (node->axis == -1)
		return; // terminal node

	// recurse down both sides
	if (sv_world.area_maxs[node->axis] > node->dist)
		Sv_AreaEdicts_r(node->children[0]);

	if (sv_world.area_mins[node->axis] < node->dist)
		Sv_AreaEdicts_r(node->children[1]);
}

/*
 * Sv_AreaEdicts
 *
 * Fills in a table of edict pointers with those which have bounding boxes
 * that intersect the given area. It is possible for a non-axial bsp model
 * to be returned that doesn't actually intersect the area.
 *
 * Returns the number of entities found.
 */
int Sv_AreaEdicts(vec3_t mins, vec3_t maxs, g_edict_t **area_edicts,
		int max_area_edicts, int area_type) {

	sv_world.area_mins = mins;
	sv_world.area_maxs = maxs;
	sv_world.area_edicts = area_edicts;
	sv_world.num_area_edicts = 0;
	sv_world.max_area_edicts = max_area_edicts;
	sv_world.area_type = area_type;

	Sv_AreaEdicts_r(sv_world.area_nodes);

	return sv_world.num_area_edicts;
}

/*
 * Sv_HullForEntity
 *
 * Returns a head_node that can be used for testing or clipping an
 * object of mins/maxs size.
 *
 * Offset is filled in to contain the adjustment that must be added to the
 * testing object's origin to get a point to use with the returned hull.
 */
static int Sv_HullForEntity(const g_edict_t *ent) {
	c_model_t *model;

	// decide which clipping hull to use, based on the size
	if (ent->solid == SOLID_BSP) { // explicit hulls in the BSP model
		model = sv.models[ent->s.model1];

		if (!model)
			Com_Error(ERR_FATAL, "Sv_HullForEntity: SOLID_BSP with no model.\n");

		return model->head_node;
	}

	// create a temporary hull from bounding box sizes
	return Cm_HeadnodeForBox(ent->mins, ent->maxs);
}

/*
 * Sv_PointContents
 *
 * Returns the contents mask for the specified point.  This includes world
 * contents as well as contents for any entities this point intersects.
 */
int Sv_PointContents(vec3_t point) {
	g_edict_t *touched[MAX_EDICTS];
	int i, contents, num;

	// get base contents from world
	contents = Cm_PointContents(point, sv.models[1]->head_node);

	// as well as contents from all intersected entities
	num = Sv_AreaEdicts(point, point, touched, MAX_EDICTS, AREA_SOLID);

	for (i = 0; i < num; i++) {

		const g_edict_t *touch = touched[i];
		const vec_t *angles;

		// might intersect, so do an exact clip
		const int head_node = Sv_HullForEntity(touch);

		if (touch->solid == SOLID_BSP) // bsp models can rotate
			angles = touch->s.angles;
		else
			angles = vec3_origin;

		contents |= Cm_TransformedPointContents(point, head_node,
				touch->s.origin, angles);
	}

	return contents;
}

// an entity's movement, with allowed exceptions and other info
typedef struct sv_move_s {
	vec3_t box_mins, box_maxs; // enclose the test object along entire move
	float *mins, *maxs; // size of the moving object
	float *start, *end;
	c_trace_t trace;
	g_edict_t *skip;
	int contentmask;
} sv_trace_t;

/*
 * Sv_ClipTraceToEntities
 *
 * Clips the specified trace to other entities in its area.  This is the basis
 * of ALL collision and interaction for the server.  Tread carefully.
 */
static void Sv_ClipTraceToEntities(sv_trace_t *trace) {
	g_edict_t *touched[MAX_EDICTS];
	vec_t *angles;
	c_trace_t tr;
	int i, num, head_node;

	// first resolve the entities found within our desired trace
	num = Sv_AreaEdicts(trace->box_mins, trace->box_maxs, touched, MAX_EDICTS,
			AREA_SOLID);

	// then iterate them, determining if they have any bearing on our trace
	for (i = 0; i < num; i++) {

		g_edict_t *touch = touched[i];

		if (touch->solid == SOLID_NOT) // can't actually touch us
			continue;

		if (trace->skip) { // see if we can skip it

			if (touch == trace->skip)
				continue; // explicitly (ourselves)

			if (touch->owner == trace->skip)
				continue; // or via ownership (we own it)

			if (trace->skip->owner) {

				if (touch == trace->skip->owner)
					continue; // which is bi-directional (inverse of previous case)

				if (touch->owner == trace->skip->owner)
					continue; // and communitive (we are both owned by the same)
			}
		}

		// we couldn't skip it, so trace to it and see if we hit
		head_node = Sv_HullForEntity(touch);

		if (touch->solid == SOLID_BSP) // bsp entities can rotate
			angles = touch->s.angles;
		else
			angles = vec3_origin;

		// perform the trace against this particular entity
		tr = Cm_TransformedBoxTrace(trace->start, trace->end, trace->mins,
				trace->maxs, head_node, trace->contentmask, touch->s.origin,
				angles);

		// check for a full or partial intersection
		if (tr.all_solid || tr.start_solid || tr.fraction
				< trace->trace.fraction) {

			trace->trace = tr;
			trace->trace.ent = touch;

			if (trace->trace.all_solid) // we were actually blocked
				return;
		}
	}
}

/*
 * Sv_TraceBounds
 */
static void Sv_TraceBounds(sv_trace_t *trace) {
	int i;

	for (i = 0; i < 3; i++) {
		if (trace->end[i] > trace->start[i]) {
			trace->box_mins[i] = trace->start[i] + trace->mins[i] - 1.0;
			trace->box_maxs[i] = trace->end[i] + trace->maxs[i] + 1.0;
		} else {
			trace->box_mins[i] = trace->end[i] + trace->mins[i] - 1.0;
			trace->box_maxs[i] = trace->start[i] + trace->maxs[i] + 1.0;
		}
	}
}

/*
 * Sv_Trace
 *
 * Moves the given box volume through the world from start to end.
 *
 * The skipped edict, and edicts owned by him, are explicitly not checked.
 * This prevents players from clipping against their own projectiles, etc.
 */
c_trace_t Sv_Trace(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end,
		g_edict_t *skip, int contentmask) {

	sv_trace_t trace;

	memset(&trace, 0, sizeof(sv_trace_t));

	if (!mins)
		mins = vec3_origin;
	if (!maxs)
		maxs = vec3_origin;

	// clip to world
	trace.trace = Cm_BoxTrace(start, end, mins, maxs, 0, contentmask);
	trace.trace.ent = svs.game->edicts;

	if (trace.trace.fraction == 0)
		return trace.trace; // blocked by the world

	trace.start = start;
	trace.end = end;
	trace.mins = mins;
	trace.maxs = maxs;
	trace.skip = skip;
	trace.contentmask = contentmask;

	// create the bounding box of the entire move
	Sv_TraceBounds(&trace);

	// clip to other solid entities
	Sv_ClipTraceToEntities(&trace);

	return trace.trace;
}
