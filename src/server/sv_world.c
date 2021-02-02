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

#include "sv_local.h"

/**
 * @brief The world is divided into evenly sized sectors to aid in entity
 * management. This works like a meta-BSP tree, providing fast searches via
 * recursion to find entities within an arbitrary box.
 */
typedef struct sv_sector_s {
	int32_t axis; // -1 = leaf
	float dist;
	struct sv_sector_s *children[2];
	GList *entities;
} sv_sector_t;

#define SECTOR_DEPTH	4
#define SECTOR_NODES	32

/**
 * @brief The world structure contains all sectors and also the current query
 * context issued to Sv_BoxEntities.
 */
typedef struct {
	sv_sector_t sectors[SECTOR_NODES];
	uint16_t num_sectors;

	vec3_t box_mins, box_maxs;

	g_entity_t **box_entities;
	size_t num_box_entities, max_box_entities;

	uint32_t box_type; // BOX_SOLID, BOX_TRIGGER, ..
} sv_world_t;

static sv_world_t sv_world;

/**
 * @brief Builds a uniformly subdivided tree for the given world size.
 */
static sv_sector_t *Sv_CreateSector(int32_t depth, const vec3_t mins, const vec3_t maxs) {
	vec3_t size, mins1, maxs1, mins2, maxs2;

	sv_sector_t *sector = &sv_world.sectors[sv_world.num_sectors];
	sv_world.num_sectors++;

	if (depth == SECTOR_DEPTH) {
		sector->axis = -1;
		sector->children[0] = sector->children[1] = NULL;
		return sector;
	}

	size = Vec3_Subtract(maxs, mins);
	if (size.x > size.y) {
		sector->axis = 0;
	} else {
		sector->axis = 1;
	}

	sector->dist = 0.5f * (maxs.xyz[sector->axis] + mins.xyz[sector->axis]);
	mins1 = mins;
	mins2 = mins;
	maxs1 = maxs;
	maxs2 = maxs;

	maxs1.xyz[sector->axis] = mins2.xyz[sector->axis] = sector->dist;

	sector->children[0] = Sv_CreateSector(depth + 1, mins2, maxs2);
	sector->children[1] = Sv_CreateSector(depth + 1, mins1, maxs1);

	return sector;
}

/**
 * @brief Resolve our sectors for a newly loaded level. This is called prior to
 * linking any entities.
 */
void Sv_InitWorld(void) {

	for (uint16_t i = 0; i < sv_world.num_sectors; i++) {
		g_list_free(sv_world.sectors[i].entities);
	}

	memset(&sv_world, 0, sizeof(sv_world));

	Sv_CreateSector(0, sv.cm_models[0]->mins, sv.cm_models[0]->maxs);
}

/**
 * @brief Called before moving or freeing an entity to remove it from the clipping
 * hull.
 */
void Sv_UnlinkEntity(g_entity_t *ent) {

	sv_entity_t *sent = &sv.entities[NUM_FOR_ENTITY(ent)];

	if (sent->sector) {
		sv_sector_t *sector = (sv_sector_t *) sent->sector;
		sector->entities = g_list_remove(sector->entities, ent);

		memset(sent, 0, sizeof(*sent));
	}
}

/**
 * @brief Called whenever an entity changes origin, mins, maxs, or solid to add it to
 * the clipping hull.
 */
void Sv_LinkEntity(g_entity_t *ent) {
	int32_t leafs[MAX_ENT_LEAFS];
	int32_t top_node;

	if (ent == svs.game->entities) { // never bother with the world
		return;
	}

	// remove it from its current sector
	Sv_UnlinkEntity(ent);

	if (!ent->in_use) { // and if its free, we're done
		return;
	}

	// set the size
	ent->size = Vec3_Subtract(ent->maxs, ent->mins);

	// encode the size into the entity state for client prediction
	ent->s.solid = ent->solid;
	switch (ent->s.solid) {
		case SOLID_TRIGGER:
		case SOLID_PROJECTILE:
		case SOLID_DEAD:
		case SOLID_BOX:
			ent->s.mins = ent->mins;
			ent->s.maxs = ent->maxs;
			break;
		default:
			ent->s.mins = Vec3_Zero();
			ent->s.maxs = Vec3_Zero();
			break;
	}

	// set the absolute bounding box; ensure it is symmetrical
	Cm_EntityBounds(ent->solid, ent->s.origin, ent->s.angles, ent->mins, ent->maxs, &ent->abs_mins, &ent->abs_maxs);

	sv_entity_t *sent = &sv.entities[NUM_FOR_ENTITY(ent)];

	// link to leafs
	sent->num_clusters = 0;

	// get all leafs, including solids
	const size_t len = Cm_BoxLeafnums(ent->abs_mins, ent->abs_maxs, leafs, lengthof(leafs), &top_node, 0);

	if (len == MAX_ENT_LEAFS) { // use top_node
		sent->num_clusters = -1;
		sent->top_node = top_node;
	} else {
		sent->num_clusters = 0;
		for (size_t i = 0; i < len; i++) {

			const int32_t cluster = Cm_LeafCluster(leafs[i]);
			if (cluster == -1) {
				continue; // not a visible leaf
			}

			int32_t c;
			for (c = 0; c < sent->num_clusters; c++)
				if (sent->clusters[c] == cluster) {
					break;
				}

			if (c == sent->num_clusters) {
				if (sent->num_clusters == MAX_ENT_CLUSTERS) { // use top_node
					Com_Debug(DEBUG_SERVER, "%s exceeds MAX_ENT_CLUSTERS\n", etos(ent));
					sent->num_clusters = -1;
					sent->top_node = top_node;
					break;
				}

				sent->clusters[sent->num_clusters++] = cluster;
			}
		}
	}

	if (ent->solid == SOLID_NOT) {
		return;
	}

	// find the first sector that the ent's box crosses
	sv_sector_t *sector = sv_world.sectors;
	while (true) {

		if (sector->axis == -1) {
			break;
		}

		if (ent->abs_mins.xyz[sector->axis] > sector->dist) {
			sector = sector->children[0];
		} else if (ent->abs_maxs.xyz[sector->axis] < sector->dist) {
			sector = sector->children[1];
		} else {
			break;    // crosses the node
		}
	}

	// add it to the sector
	sent->sector = sector;
	sector->entities = g_list_prepend(sector->entities, ent);

	// and update its clipping matrices
	const vec3_t angles = ent->solid == SOLID_BSP ? ent->s.angles : Vec3_Zero();

	sent->matrix = Mat4_FromOriginAnglesScale(ent->s.origin, angles, 1.f);
	sent->inverse_matrix = Mat4_Invert(sent->matrix);
}

/**
 * @return True if the entity matches the current world filter, false otherwise.
 */
static _Bool Sv_BoxEntities_Filter(const g_entity_t *ent) {

	switch (ent->solid) {
		case SOLID_TRIGGER:
		case SOLID_PROJECTILE:
			if (sv_world.box_type & BOX_OCCUPY) {
				return true;
			}
			break;

		case SOLID_DEAD:
		case SOLID_BOX:
		case SOLID_BSP:
			if (sv_world.box_type & BOX_COLLIDE) {
				return true;
			}
			break;

		case SOLID_NOT:
			break;
	}

	return false;
}

/**
 * @brief
 */
static void Sv_BoxEntities_r(sv_sector_t *sector) {

	GList *e = sector->entities;
	while (e) {
		g_entity_t *ent = (g_entity_t *) e->data;

		if (Sv_BoxEntities_Filter(ent)) {

			if (Vec3_BoxIntersect(ent->abs_mins, ent->abs_maxs, sv_world.box_mins, sv_world.box_maxs)) {

				sv_world.box_entities[sv_world.num_box_entities] = ent;
				sv_world.num_box_entities++;

				if (sv_world.num_box_entities == sv_world.max_box_entities) {
					Com_Warn("sv_world.max_box_entities\n");
					return;
				}
			}
		}

		e = e->next;
	}

	if (sector->axis == -1) {
		return;    // terminal node
	}

	// recurse down both sides
	if (sv_world.box_maxs.xyz[sector->axis] > sector->dist) {
		Sv_BoxEntities_r(sector->children[0]);
	}

	if (sv_world.box_mins.xyz[sector->axis] < sector->dist) {
		Sv_BoxEntities_r(sector->children[1]);
	}
}

/**
 * @brief Populates an array of entities with those which have bounding boxes
 * that intersect the given box. It is possible for a non-axial BSP model to
 * be returned that doesn't actually intersect the box.
 *
 * @return The number of entities found.
 */
size_t Sv_BoxEntities(const vec3_t mins, const vec3_t maxs, g_entity_t **list, const size_t len,
                      const uint32_t type) {

	sv_world.box_mins = mins;
	sv_world.box_maxs = maxs;
	sv_world.box_entities = list;
	sv_world.num_box_entities = 0;
	sv_world.max_box_entities = len;
	sv_world.box_type = type;

	Sv_BoxEntities_r(sv_world.sectors);

	sv_world.box_mins = Vec3_Zero();
	sv_world.box_maxs = Vec3_Zero();
	sv_world.box_entities = NULL;

	return sv_world.num_box_entities;
}

/**
 * @brief Prepares the collision model to clip to the specified entity. For
 * mesh models, the box hull must be set to reflect the bounds of the entity.
 */
static int32_t Sv_HullForEntity(const g_entity_t *ent) {

	if (ent->solid == SOLID_BSP) {
		const cm_bsp_model_t *mod = sv.cm_models[ent->s.model1];

		if (!mod) {
			Com_Error(ERROR_DROP, "SOLID_BSP with no model\n");
		}

		return mod->head_node;
	}

	if (ent->solid == SOLID_BOX) {

		if (ent->client) {
			return Cm_SetBoxHull(ent->mins, ent->maxs, CONTENTS_MONSTER);
		}

		return Cm_SetBoxHull(ent->mins, ent->maxs, CONTENTS_SOLID);
	}

	if (ent->solid == SOLID_DEAD) {
		return Cm_SetBoxHull(ent->mins, ent->maxs, CONTENTS_DEAD_MONSTER);
	}

	return -1;
}

/**
 * @brief Returns the contents mask for the specified point. This includes world
 * contents as well as contents for any solid entities this point intersects.
 */
int32_t Sv_PointContents(const vec3_t point) {
	g_entity_t *entities[MAX_ENTITIES];

	// get base contents from world
	int32_t contents = Cm_PointContents(point, 0);

	// as well as contents from all intersected entities
	const size_t len = Sv_BoxEntities(point, point, entities, lengthof(entities), BOX_COLLIDE);

	// iterate the box entities, checking each one for an intersection
	for (size_t i = 0; i < len; i++) {
		const g_entity_t *ent = entities[i];

		const int32_t head_node = Sv_HullForEntity(ent);
		if (head_node != -1) {

			const sv_entity_t *sent = &sv.entities[NUM_FOR_ENTITY(ent)];
			contents |= Cm_TransformedPointContents(point, head_node, &sent->inverse_matrix);
		}
	}

	return contents;
}

// an entity's movement, with allowed exceptions and other info
typedef struct {
	vec3_t start, end;
	vec3_t mins, maxs; // size of the moving object
	vec3_t box_mins, box_maxs; // enclose the test object along entire move
	cm_trace_t trace;
	const g_entity_t *skip;
	int32_t contents;
} sv_trace_t;

/**
 * @brief Clips the specified trace to other entities in its bounds. This is the basis of all
 * collision and interaction for the server. Tread carefully.
 */
static void Sv_ClipTraceToEntities(sv_trace_t *trace) {
	g_entity_t *e[MAX_ENTITIES];

	const size_t len = Sv_BoxEntities(trace->box_mins, trace->box_maxs, e, lengthof(e), BOX_COLLIDE);

	for (size_t i = 0; i < len; i++) {
		g_entity_t *ent = e[i];

		if (trace->skip) { // see if we can skip it

			if (ent == trace->skip) {
				continue; // explicitly (ourselves)
			}

			if (ent->owner == trace->skip) {
				continue; // or via ownership (we own it)
			}

			if (trace->skip->owner) {

				if (ent == trace->skip->owner) {
					continue; // which is bidirectional (inverse of previous case)
				}

				if (ent->owner == trace->skip->owner) {
					continue; // and commutative (we are both owned by the same)
				}
			}

			// triggers only clip to the world (while other entities can occupy triggers)
			if (trace->skip->solid == SOLID_TRIGGER) {

				if (ent->solid != SOLID_BSP) {
					continue;
				}
			}
		}

		const int32_t head_node = Sv_HullForEntity(ent);
		if (head_node != -1) {

			const sv_entity_t *sent = &sv.entities[NUM_FOR_ENTITY(ent)];

			const cm_trace_t tr = Cm_TransformedBoxTrace(trace->start, trace->end,
														 trace->mins, trace->maxs,
														 head_node, trace->contents,
														 &sent->matrix, &sent->inverse_matrix);

			// check for a full or partial intersection
			if (tr.all_solid || tr.fraction < trace->trace.fraction) {

				trace->trace = tr;
				trace->trace.ent = ent;

				if (tr.all_solid) { // we were actually blocked
					return;
				}
			}
		}
	}
}

/**
 * @brief Moves the given box volume through the world from start to end.
 *
 * The skipped edict, and edicts owned by him, are explicitly not checked.
 * This prevents players from clipping against their own projectiles, etc.
 */
cm_trace_t Sv_Trace(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs,
                    const g_entity_t *skip, const int32_t contents) {

	sv_trace_t trace;

	memset(&trace, 0, sizeof(trace));

	// clip to world
	trace.trace = Cm_BoxTrace(start, end, mins, maxs, 0, contents);
	if (trace.trace.fraction < 1.0f) {
		trace.trace.ent = svs.game->entities;

		if (trace.trace.start_solid) { // blocked entirely
			return trace.trace;
		}
	}

	trace.start = start;
	trace.end = end;
	trace.mins = mins;
	trace.maxs = maxs;
	trace.skip = skip;
	trace.contents = contents;

	// create the bounding box of the entire move
	Cm_TraceBounds(start, end, mins, maxs, &trace.box_mins, &trace.box_maxs);

	// clip to other solid entities
	Sv_ClipTraceToEntities(&trace);

	return trace.trace;
}
