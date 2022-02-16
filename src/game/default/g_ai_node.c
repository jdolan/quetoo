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

#include "g_local.h"
#include "bg_pmove.h"

/**
 * @brief
 */
static struct {
	vec3_t position;

	vec3_t floor_position;

	ai_node_id_t last_nodes[2];

	_Bool await_landing, is_jumping, is_water_jump, on_mover;

	button_state_t latched_buttons, old_buttons, buttons;

	GArray *test_path;

	guint file_nodes, file_links;

	_Bool do_noding;
} ai_player_roam;

/**
 * @brief
 */
GArray *Ai_Node_TestPath(void) {

	if (!ai_node_dev->integer) {
		return NULL;
	}

	if (ai_player_roam.test_path) {
		g_array_unref(ai_player_roam.test_path);
		ai_player_roam.test_path = NULL;
	}

	if (ai_player_roam.last_nodes[0] == AI_NODE_INVALID ||
		ai_player_roam.last_nodes[1] == AI_NODE_INVALID) {
		return NULL;
	}

	return ai_player_roam.test_path = Ai_Node_FindPath(ai_player_roam.last_nodes[1], ai_player_roam.last_nodes[0], Ai_Node_DefaultHeuristic, NULL);
}

/**
 * @brief
 */
typedef struct {

	ai_node_id_t id;
	float cost;
} ai_link_t;

/**
 * @brief
 */
typedef struct {
	// persisted to disk

	vec3_t position;
	GArray *links;

	// only used for nav purposes

	float cost;
	ai_node_id_t came_from;
} ai_node_t;

/**
 * @brief
 */
static GArray *ai_nodes;

/**
 * @brief
 */
static inline ai_node_id_t Ai_Node_Index(const ai_node_t *node) {
	return node - (ai_node_t *) ai_nodes->data;
}

/**
 * @brief
 */
static _Bool Ai_Node_Visible(const vec3_t position, const ai_node_id_t node) {

	return gi.Trace(position, Ai_Node_GetPosition(node), Box3_Zero(), NULL, CONTENTS_SOLID | CONTENTS_WINDOW).fraction == 1.0f;
}

/**
 * @brief
 */
guint Ai_Node_Count(void) {

	return ai_nodes ? ai_nodes->len : 0;
}

/**
 * @brief
 */
ai_node_id_t Ai_Node_FindClosest(const vec3_t position, const float max_distance, const bool only_visible, const bool prefer_level) {

	if (!ai_nodes)
		return AI_NODE_INVALID;

	ai_node_id_t closest = AI_NODE_INVALID;
	float closest_dist = 0;
	const float dist_squared = max_distance * max_distance;

	for (guint i = 0; i < ai_nodes->len; i++) {
		const ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, i);

		vec3_t dir = Vec3_Subtract(position, node->position);
		// weigh the Z axis more heavily
		if (prefer_level && !(gi.PointContents(node->position) & CONTENTS_MASK_LIQUID)) {
			dir.z *= 4.0f;
		}
		const float dist = Vec3_LengthSquared(dir);

		if (dist < dist_squared && (closest == AI_NODE_INVALID || dist < closest_dist) && (!only_visible || Ai_Node_Visible(position, i))) {
			closest = i;
			closest_dist = dist;
		}
	}

	return closest;
}

/**
 * @brief
 */
ai_node_id_t Ai_Node_CreateNode(const vec3_t position) {

	if (!ai_nodes) {
		ai_nodes = g_array_new(false, true, sizeof(ai_node_t));
	}

	g_array_append_val(ai_nodes, (ai_node_t) {
		.position = position
	});

	Ai_Debug("Dropped new node %d\n", ai_nodes->len - 1);

	return ai_nodes->len - 1;
}

/**
 * @brief
 */
_Bool Ai_Node_IsLinked(const ai_node_id_t a, const ai_node_id_t b) {
	const ai_node_t *node_a = &g_array_index(ai_nodes, ai_node_t, a);

	if (node_a->links) {
		for (guint i = 0; i < node_a->links->len; i++) {
			const ai_link_t *link = &g_array_index(node_a->links, ai_link_t, i);

			if (link->id == b) {
				return true;
			}
		}
	}

	return false;
}

/**
 * @brief
 */
GArray *Ai_Node_GetLinks(const ai_node_id_t a) {
	const ai_node_t *node_a = &g_array_index(ai_nodes, ai_node_t, a);

	if (!node_a->links) {
		return NULL;
	}

	GArray *node_copy = g_array_sized_new(false, false, sizeof(ai_node_id_t), node_a->links->len);
	g_array_set_size(node_copy, node_a->links->len);

	for (guint i = 0; i < node_a->links->len; i++) {
		const ai_link_t *link = &g_array_index(node_a->links, ai_link_t, i);
		*(((ai_node_id_t *)node_copy->data) + i) = link->id;
	}

	return node_copy;
}

/**
 * @brief
 */
void Ai_Node_CreateLink(const ai_node_id_t a, const ai_node_id_t b, const float cost) {

	if (a == b || Ai_Node_IsLinked(a, b)) {
		return;
	}

	ai_node_t *node_a = &g_array_index(ai_nodes, ai_node_t, a);

	if (!node_a->links) {
		node_a->links = g_array_new(false, false, sizeof(ai_link_t));
	}

	g_array_append_vals(node_a->links, &(ai_link_t) {
		.id = b,
		.cost = cost
	}, 1);

	Ai_Debug("Connected %d -> %d\n", a, b);
}

/**
 * @brief
 */
static inline void Ai_Node_CreateDefaultLink(const ai_node_id_t a, const ai_node_id_t b, const _Bool bidirectional) {

	if (a == b) {
		return;
	}

	Ai_Node_CreateLink(a, b, Ai_Node_DefaultCost(a, b));

	if (bidirectional) {
		Ai_Node_CreateDefaultLink(b, a, false);
	}
}

/**
 * @brief
 */
static void Ai_Node_DestroyLink(const ai_node_id_t a, const ai_node_id_t b) {
	{
		ai_node_t *node_a = &g_array_index(ai_nodes, ai_node_t, a);

		if (node_a->links) {
			for (guint i = 0; i < node_a->links->len; i++) {
				const ai_link_t *link = &g_array_index(node_a->links, ai_link_t, i);

				if (link->id == b) {
					node_a->links = g_array_remove_index_fast(node_a->links, i);

					if (!node_a->links->len) {
						g_array_free(node_a->links, true);
						node_a->links = NULL;
					}
					break;
				}
			}
		}
	}

	{
		ai_node_t *node_b = &g_array_index(ai_nodes, ai_node_t, b);
	
		if (node_b->links) {
			for (guint i = 0; i < node_b->links->len; i++) {
				const ai_link_t *link = &g_array_index(node_b->links, ai_link_t, i);

				if (link->id == a) {
					node_b->links = g_array_remove_index_fast(node_b->links, i);

					if (!node_b->links->len) {
						g_array_free(node_b->links, true);
						node_b->links = NULL;
					}
					break;
				}
			}
		}
	}
}

/**
 * @brief
 */
static void Ai_Node_DestroyLinks(const ai_node_id_t id) {
	const ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, id);

	if (!node->links) {
		return;
	}

	for (guint i = node->links->len - 1; ; i--) {
		Ai_Node_DestroyLink(id, g_array_index(node->links, ai_link_t, i).id);
		
		if (i == 0 || !node->links) {
			break;
		}
	}
}

/**
 * @brief Node `id` has been removed, so we need to fix up connection IDs
 * so that they don't shift.
*/
static void Ai_Node_AdjustConnections(const ai_node_id_t id) {

	for (guint i = 0; i < ai_nodes->len; i++) {
		const ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, i);

		if (!node->links) {
			continue;
		}

		for (guint l = 0; l < node->links->len; l++) {
			ai_link_t *link = &g_array_index(node->links, ai_link_t, l);

			if (link->id >= id) {
				link->id--;
			}
		}
	}
}

/**
 * @brief
 */
static void Ai_Node_Destroy(const ai_node_id_t id) {

	Ai_Node_DestroyLinks(id);
	ai_nodes = g_array_remove_index(ai_nodes, id);

	if (!ai_nodes->len) {
		g_array_free(ai_nodes, true);
		ai_nodes = NULL;
	} else {
		Ai_Node_AdjustConnections(id);
	}
}

/**
 * @brief
 */
static _Bool Ai_Node_PlayerIsOnFloor(const g_entity_t *player) {
	const cm_trace_t tr = gi.Trace(player->s.origin, Vec3_Add(player->s.origin, Vec3(0, 0, -PM_GROUND_DIST)), player->s.bounds, NULL, CONTENTS_MASK_CLIP_CORPSE);

	return tr.fraction < 1.0f && tr.plane.normal.z > PM_STEP_NORMAL;
}

/**
 * @brief
 */
vec3_t Ai_Node_GetPosition(const ai_node_id_t node) {

	return g_array_index(ai_nodes, ai_node_t, node).position;
}

/**
 * @brief
 */
static void Ai_Node_RecalculateCosts(const ai_node_id_t id) {

	for (guint i = 0; i < ai_nodes->len; i++) {
		const ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, i);

		if (!node->links || !node->links->len) {
			continue;
		}

		for (guint l = 0; l < node->links->len; l++) {
			ai_link_t *link = &g_array_index(node->links, ai_link_t, l);

			if (id == i || link->id == id) {
				link->cost = Ai_Node_DefaultCost(i, link->id);
			}
		}
	}
}

/**
 * @brief Check if the node we want to move towards is currently pathable.
 */
_Bool Ai_Node_CanPathTo(const vec3_t position) {

	// if we're heading onto a mover node, only allow us to go forth
	// if the mover is there
	const vec3_t end = Vec3_Subtract(position, Vec3(0, 0, PM_GROUND_DIST * 3.f));

	// check if the destination has ground
	cm_trace_t tr = gi.Trace(position, end, Box3_Expand3(PM_BOUNDS, Vec3(1.f, 1.f, 0.f)), NULL, CONTENTS_MASK_CLIP_CORPSE | CONTENTS_MASK_LIQUID);

	// bad ground
	_Bool stuck_in_mover = tr.ent
			&& (tr.start_solid || tr.all_solid)
			&& (tr.ent->s.number != 0
			&& !(tr.contents & CONTENTS_MASK_LIQUID));

	if (tr.fraction == 1.0 || stuck_in_mover) {

		// check with a thinner box; it might be a button press or rotating thing
		if (stuck_in_mover) {
			tr = gi.Trace(position,
							   Vec3_Subtract(position, Vec3(0, 0, PM_GROUND_DIST * 3.f)),
							   Box3(Vec3(-4.f, -4.f, PM_BOUNDS.mins.z), Vec3(4.f, 4.f, PM_BOUNDS.maxs.z)),
							   NULL,
							   CONTENTS_MASK_CLIP_CORPSE | CONTENTS_MASK_LIQUID);
			stuck_in_mover = tr.ent && (tr.start_solid || tr.all_solid) && (tr.ent->s.number != 0 && !(tr.contents & CONTENTS_MASK_LIQUID));

			if (!stuck_in_mover) {
				return true;
			}
		}

		return false;
	}

	return true;
}

/**
 * @brief Check if the node we want to move towards is currently pathable.
 */
_Bool Ai_Path_CanPathTo(const GArray *path, const guint index) {

	// sanity
	if (index >= path->len) {
		return true;
	}

	// if we're heading onto a mover node, only allow us to go forth
	// if the mover is there
	return Ai_Node_CanPathTo(g_array_index(ai_nodes, ai_node_t, g_array_index(path, ai_node_id_t, index)).position);
}

/**
 * @brief The length of space that nodes will drop while walking.
 */
#define WALKING_DISTANCE	128.f

/**
 * @brief The length of space that couldn't be logically travelled by regular means.
 */
#define TELEPORT_DISTANCE	64.f

/**
 * @brief
 */
void Ai_Node_PlayerRoam(const g_entity_t *player, const pm_cmd_t *cmd) {

	if (!ai_node_dev->integer) {
		return;
	}

	ai_player_roam.old_buttons = ai_player_roam.buttons;
	ai_player_roam.buttons = cmd->buttons;
	ai_player_roam.latched_buttons |= ai_player_roam.buttons & ~ai_player_roam.old_buttons;
	
	const _Bool allow_adjustments = ai_node_dev->integer == 1;
	const _Bool do_noding = allow_adjustments && player->client->ps.pm_state.type == PM_NORMAL;	

	// we just switched between noclip/not noclip, clear some stuff
	// so we don't accidentally drop nodes
	if (ai_player_roam.do_noding != do_noding) {
		ai_player_roam.do_noding = do_noding;

		ai_player_roam.position = player->s.origin;
		ai_player_roam.last_nodes[0] = ai_player_roam.last_nodes[1] = AI_NODE_INVALID;
		ai_player_roam.await_landing = true;
	}

	const _Bool in_water = gi.PointContents(player->s.origin) & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA);
	const float last_node_distance_compare = ai_player_roam.last_nodes[0] == AI_NODE_INVALID ? FLT_MAX : Vec3_Distance(player->s.origin, Ai_Node_GetPosition(ai_player_roam.last_nodes[0]));
	const float player_distance_compare = Vec3_Distance(player->s.origin, ai_player_roam.position);

	if (Ai_Node_PlayerIsOnFloor(player)) {
		ai_player_roam.floor_position = player->s.origin;
	}

	if (do_noding) {
		// we're waiting to land to drop a node; we jumped, fell, got sent by a jump pad, something like that.
		if (ai_player_roam.await_landing) {

			if (Ai_Node_PlayerIsOnFloor(player) || in_water) {

				// we landed!
				ai_player_roam.await_landing = false;
				ai_player_roam.position = player->s.origin;

				ai_node_id_t landed_near_node = Ai_Node_FindClosest(player->s.origin, WALKING_DISTANCE / 2, true, true);

				if (landed_near_node == AI_NODE_INVALID) {

					landed_near_node = Ai_Node_CreateNode(player->s.origin);
				}

				// one-way node from where we were to here
				if (ai_player_roam.last_nodes[0] != AI_NODE_INVALID) {
					_Bool bidirectional = false;

					if (ai_player_roam.is_water_jump) {
						bidirectional = true;
						Ai_Debug("Most likely waterjump; connecting both ends\n");
					} else if (ai_player_roam.is_jumping || in_water) {
						const float z_diff = fabsf(Ai_Node_GetPosition(ai_player_roam.last_nodes[0]).z - Ai_Node_GetPosition(landed_near_node).z);

						if (z_diff < PM_STEP_HEIGHT || (in_water && z_diff < PM_STEP_HEIGHT * 3.f)) {
							bidirectional = true;
							Ai_Debug("Most likely jump or drop-into-water link; connecting both ends\n");
						}
					}

					Ai_Node_CreateDefaultLink(ai_player_roam.last_nodes[0], landed_near_node, bidirectional);
				}

				ai_player_roam.last_nodes[1] = ai_player_roam.last_nodes[0];
				ai_player_roam.last_nodes[0] = landed_near_node;
				ai_player_roam.is_water_jump = false;
			}

			return;
		}

		// we probably teleported; no node, just start dropping here when we land
		if (player_distance_compare > TELEPORT_DISTANCE) {

			ai_player_roam.last_nodes[0] = ai_player_roam.last_nodes[1] = AI_NODE_INVALID;
			ai_player_roam.position = player->s.origin;
			ai_player_roam.await_landing = true;

			Ai_Debug("Teleport detected; awaiting landing...\n");
			return;
		}

		// we just left the floor (or water); drop a node here
		if (!Ai_Node_PlayerIsOnFloor(player) && !in_water) {
			// for water leavings, we want to drop where we are, not where we went into the water from
			const vec3_t where = ai_player_roam.is_water_jump ? player->s.origin : ai_player_roam.floor_position;
			const ai_node_id_t jumped_near_node = Ai_Node_FindClosest(where, WALKING_DISTANCE / 2, true, false);
			const _Bool is_jump = player->client->ps.pm_state.velocity.z > 0;

			if (jumped_near_node == AI_NODE_INVALID) {
				const ai_node_id_t id = Ai_Node_CreateNode(where);

				if (ai_player_roam.last_nodes[0] != AI_NODE_INVALID) {

					Ai_Node_CreateDefaultLink(ai_player_roam.last_nodes[0], id, true);
				}
			
				ai_player_roam.last_nodes[1] = ai_player_roam.last_nodes[0];
				ai_player_roam.last_nodes[0] = id;
			} else {
				
				if (ai_player_roam.last_nodes[0] != AI_NODE_INVALID) {

					Ai_Node_CreateDefaultLink(ai_player_roam.last_nodes[0], jumped_near_node, true);
				}
			
				ai_player_roam.last_nodes[1] = ai_player_roam.last_nodes[0];
				ai_player_roam.last_nodes[0] = jumped_near_node;
			}

			ai_player_roam.await_landing = true;
			ai_player_roam.is_jumping = is_jump;

			// we didn't leave from water jump, so turn this off.
			if (!is_jump) {
				ai_player_roam.is_water_jump = false;
			}

			Ai_Debug("Left ground; jumping? %s\n", is_jump ? "yes" : "nop");
			return;
		}
	}

	// we're walkin'

	const ai_node_id_t closest_node = Ai_Node_FindClosest(player->s.origin, WALKING_DISTANCE / 4, true, false);
	const _Bool on_mover = player->locals.ground.ent && player->locals.ground.ent->s.number != 0;

	// attack button moves node
	if (allow_adjustments && (ai_player_roam.latched_buttons & BUTTON_ATTACK)) {
		if (ai_player_roam.last_nodes[0] != AI_NODE_INVALID) {
			ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, ai_player_roam.last_nodes[0]);
			node->position = player->s.origin;

			if (cmd->up < 0) {
				const cm_trace_t tr = gi.Trace(node->position, Vec3_Subtract(node->position, Vec3(0.f, 0.f, MAX_WORLD_COORD)), PM_BOUNDS, player, CONTENTS_MASK_SOLID);
				node->position = tr.end;
			}

			// recalculate links
			Ai_Node_RecalculateCosts(ai_player_roam.last_nodes[0]);
		}
		ai_player_roam.latched_buttons &= ~BUTTON_ATTACK;
	// hook destroys node
	} else if ((allow_adjustments) && (ai_player_roam.latched_buttons & BUTTON_HOOK)) {
		if (ai_player_roam.last_nodes[0] != AI_NODE_INVALID) {
			Ai_Node_Destroy(ai_player_roam.last_nodes[0]);
			ai_player_roam.position = player->s.origin;
			ai_player_roam.last_nodes[0] = ai_player_roam.last_nodes[1] = AI_NODE_INVALID;
			ai_player_roam.last_nodes[0] = Ai_Node_FindClosest(player->s.origin, WALKING_DISTANCE * 2.5f, true, false);
		}
		ai_player_roam.latched_buttons &= ~BUTTON_HOOK;
	// score adjusts link connections
	} else if ((allow_adjustments) && (ai_player_roam.latched_buttons & BUTTON_SCORE)) {

		if (ai_player_roam.last_nodes[1] != AI_NODE_INVALID && ai_player_roam.last_nodes[0] != AI_NODE_INVALID) {
			uint8_t bits = 0;

			if (Ai_Node_IsLinked(ai_player_roam.last_nodes[0], ai_player_roam.last_nodes[1])) {
				bits |= 1;
			}
			if (Ai_Node_IsLinked(ai_player_roam.last_nodes[1], ai_player_roam.last_nodes[0])) {
				bits |= 2;
			}

			bits = (bits + 1) % 4;

			Ai_Node_DestroyLink(ai_player_roam.last_nodes[0], ai_player_roam.last_nodes[1]);
			Ai_Node_DestroyLink(ai_player_roam.last_nodes[1], ai_player_roam.last_nodes[0]);
			
			if (bits & 1) {
				Ai_Node_CreateDefaultLink(ai_player_roam.last_nodes[0], ai_player_roam.last_nodes[1], false);
			}
			if (bits & 2) {
				Ai_Node_CreateDefaultLink(ai_player_roam.last_nodes[1], ai_player_roam.last_nodes[0], false);
			}
		}
		
		ai_player_roam.latched_buttons &= ~BUTTON_SCORE;
	// we're stepping on/off a mover; connect us one-way
	} else if (on_mover != ai_player_roam.on_mover) {

		ai_player_roam.on_mover = on_mover;

		if (do_noding) {
			ai_node_id_t id = Ai_Node_FindClosest(player->s.origin, WALKING_DISTANCE / 8, true, false);
			
			if (id == AI_NODE_INVALID) {
				id = Ai_Node_CreateNode(player->s.origin);
			}

			if (ai_player_roam.last_nodes[0] != AI_NODE_INVALID) {

				Ai_Node_CreateDefaultLink(ai_player_roam.last_nodes[0], id, false);
			}
		
			ai_player_roam.last_nodes[1] = ai_player_roam.last_nodes[0];
			ai_player_roam.last_nodes[0] = id;
		}

	// if we touched another node and had another node lit up; connect us if we aren't already
	} else if (closest_node != AI_NODE_INVALID && closest_node != ai_player_roam.last_nodes[0] && Ai_Node_Visible(player->s.origin, closest_node)) {

		if (do_noding && ai_player_roam.last_nodes[0] != AI_NODE_INVALID) {

			Ai_Node_CreateDefaultLink(ai_player_roam.last_nodes[0], closest_node, !ai_player_roam.on_mover);
		}
		
		ai_player_roam.last_nodes[1] = ai_player_roam.last_nodes[0];
		ai_player_roam.last_nodes[0] = closest_node;
	// we got far enough from the last node, so drop a new node
	} else if (last_node_distance_compare > WALKING_DISTANCE) {

		if (do_noding) {
			ai_node_id_t id = Ai_Node_FindClosest(player->s.origin, WALKING_DISTANCE / 2, true, !in_water);
			
			if (id == AI_NODE_INVALID) {
				id = Ai_Node_CreateNode(player->s.origin);
			}

			if (ai_player_roam.last_nodes[0] != AI_NODE_INVALID) {

				Ai_Node_CreateDefaultLink(ai_player_roam.last_nodes[0], id, !ai_player_roam.on_mover);
			}
		
			ai_player_roam.last_nodes[1] = ai_player_roam.last_nodes[0];
			ai_player_roam.last_nodes[0] = id;
		}
	}

	// we're currently in water; the only way we can get out
	// is by waterjumping, having something take us out of the water,
	// a ladder or walking straight out of water. mark waterjump as true,
	// we'll figure it out when we actually leave water what the intentions are.
	if (in_water) {
		ai_player_roam.is_water_jump = true;
	}

	ai_player_roam.position = player->s.origin;
}

/**
 * @brief
 */
typedef struct {
	union {
		struct {
			ai_node_id_t a, b;
		};
		int32_t v;
	};
} ai_unique_link_t;

/**
 * @brief
 */
static void Ai_Node_RenderLinks(gpointer key, gpointer value, gpointer userdata) {

	const ai_unique_link_t ulink = { .v = GPOINTER_TO_INT(key) };
	const int32_t bits = GPOINTER_TO_INT(value);
	
	const ai_node_t *node_a = &g_array_index(ai_nodes, ai_node_t, ulink.a);
	const ai_node_t *node_b = &g_array_index(ai_nodes, ai_node_t, ulink.b);

	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_AI_NODE_LINK);
	gi.WritePosition(node_a->position);
	gi.WritePosition(node_b->position);
	gi.WriteByte(bits);
	gi.Multicast(node_a->position, MULTICAST_PVS, NULL);
}

/**
 * @brief
 */
static _Bool Ai_NodeInPath(GArray *path, ai_node_id_t node) {

	if (!path) {
		return false;
	}

	for (guint i = 0; i < path->len; i++) {
		
		if (g_array_index(path, ai_node_id_t, i) == node) {
			return true;
		}
	}

	return false;
}

/**
 * @brief
 */
void Ai_Node_Render(void) {

	if (!ai_node_dev->integer) {
		return;
	}

	if (!ai_nodes) {
		return;
	}

	GHashTable *unique_links = g_hash_table_new(g_direct_hash, g_direct_equal);

	for (guint i = 0; i < ai_nodes->len; i++) {
		const ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, i);
		const _Bool in_path = Ai_NodeInPath(ai_player_roam.test_path, i);

		gi.WriteByte(SV_CMD_TEMP_ENTITY);
		gi.WriteByte(TE_AI_NODE);
		gi.WritePosition(node->position);
		gi.WriteShort(i);

		byte bits = 0;

		if (in_path) {
			bits = 3;
		} else if (ai_player_roam.last_nodes[0] == i) {
			bits = 1;
		} else if (ai_player_roam.last_nodes[1] == i) {
			bits = 2;
		}

		if (!Ai_Node_CanPathTo(node->position)) {
			bits |= 16;
		}

		gi.WriteByte(bits);
		gi.Multicast(node->position, MULTICAST_PVS, NULL);

		if (node->links) {

			for (guint l = 0; l < node->links->len; l++) {
				const ai_link_t *link = &g_array_index(node->links, ai_link_t, l);
				ai_unique_link_t ulink;
				int32_t bit;

				if (link->id > i) {
					ulink.a = link->id;
					ulink.b = i;
					bit = 1;
				} else {
					ulink.a = i;
					ulink.b = link->id;
					bit = 2;
				}

				if (!in_path) {
					bit |= 4;
				}

				if (Ai_ShouldSlowDrop(i, link->id)) {
					bit |= 16;
				}

				if (!g_hash_table_contains(unique_links, GINT_TO_POINTER(ulink.v))) {
					g_hash_table_insert(unique_links, GINT_TO_POINTER(ulink.v), GINT_TO_POINTER(bit));
				} else {
					int32_t existing_bit = GPOINTER_TO_INT(g_hash_table_lookup(unique_links, GINT_TO_POINTER(ulink.v)));
					g_hash_table_replace(unique_links, GINT_TO_POINTER(ulink.v), GINT_TO_POINTER(existing_bit | bit));
				}
			}
		}
	}

	g_hash_table_foreach(unique_links, Ai_Node_RenderLinks, NULL);

	g_hash_table_destroy(unique_links);

	// draw the bots' targets
	for (int32_t i = 1; i <= sv_max_clients->integer; i++) {
		const g_entity_t *ent = ENTITY_FOR_NUM(i);

		if (!ent->in_use || !ent->client->ai) {
			continue;
		}

		const ai_locals_t *ai = Ai_GetLocals(ent);

		if (ai->move_target.type != AI_GOAL_PATH) {
			continue;
		}
		
		gi.WriteByte(SV_CMD_TEMP_ENTITY);
		gi.WriteByte(TE_AI_NODE_LINK);
		gi.WritePosition(ent->s.origin);
		gi.WritePosition(ai->move_target.path.path_position);
		gi.WriteByte(8);
		gi.Multicast(ent->s.origin, MULTICAST_PVS, NULL);
	}
}

#define AI_NODE_MAGIC ('Q' | '2' << 8 | 'N' << 16 | 'S' << 24)
#define AI_NODE_VERSION 2

/**
 * @brief 
 */
void Ai_InitNodes(void) {

	ai_player_roam.position = Vec3(MAX_WORLD_DIST, MAX_WORLD_DIST, MAX_WORLD_DIST);
	ai_player_roam.last_nodes[0] = ai_player_roam.last_nodes[1] = AI_NODE_INVALID;
	ai_player_roam.await_landing = true;

	char filename[MAX_OS_PATH];

	g_snprintf(filename, sizeof(filename), "maps/%s.nav", g_level.name);

	if (!gi.FileExists(filename)) {
		gi.Warn("No navigation file exists for this map; bots will be dumb!\nUse `ai_node_dev` to set up nodes.\n");
		return;
	}

	file_t *file = gi.OpenFile(filename);
	int32_t magic, version;
	
	gi.ReadFile(file, &magic, sizeof(magic), 1);

	if (magic != AI_NODE_MAGIC) {
		gi.Warn("Nav file invalid format!\n");
		gi.CloseFile(file);
		return;
	}

	gi.ReadFile(file, &version, sizeof(version), 1);

	if (version != AI_NODE_VERSION) {
		gi.Warn("Nav file out of date!\n");
		gi.CloseFile(file);
		return;
	}

	guint num_nodes;
	gi.ReadFile(file, &num_nodes, sizeof(num_nodes), 1);

	ai_nodes = g_array_sized_new(false, true, sizeof(ai_node_t), num_nodes);
	g_array_set_size(ai_nodes, num_nodes);

	guint total_links = 0;

	for (guint i = 0; i < ai_nodes->len; i++) {
		ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, i);

		gi.ReadFile(file, &node->position, sizeof(node->position), 1);

		guint num_links;
	
		gi.ReadFile(file, &num_links, sizeof(num_links), 1);

		if (num_links) {
			node->links = g_array_sized_new(false, false, sizeof(ai_link_t), num_links);
			g_array_set_size(node->links, num_links);
			gi.ReadFile(file, node->links->data, sizeof(ai_link_t), num_links);
			total_links += num_links;

			Ai_Node_DestroyLink(i, i);
		} else {
			node->links = NULL;
		}
	}

	gi.CloseFile(file);
	gi.Print("  Loaded %u nodes with %u total links.\n", num_nodes, total_links);

	ai_player_roam.file_nodes = num_nodes;
	ai_player_roam.file_links = 0;

	for (guint i = 0; i < ai_nodes->len; i++) {
		const ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, i);

		if (node->links) {
			ai_player_roam.file_links += node->links->len;
		}
	}
}

/**
 * @brief 
 */
static void Ai_CheckNodes(void) {

	if (ai_node_dev->integer) {
		for (int32_t i = sv_max_clients->integer; i < ge.num_entities; i++) {
		
			g_entity_t *ent = ENTITY_FOR_NUM(i);

			if (!ent->in_use) {
				continue;
			}

			// only warn for item nodes
			if (!ent->locals.item) {
				continue;
			}

			const ai_node_id_t node = Ai_Node_FindClosest(ent->s.origin, WALKING_DISTANCE * 2.5f, true, false);

			if (node == AI_NODE_INVALID) {
				gi.Warn("Entity %s @ %s appears to be unreachable by nodes\n", ent->class_name, vtos(ent->s.origin));
			}
		}
	}

	for (guint i = 0; i < ai_nodes->len; i++) {
		const ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, i);
		
		if (gi.PointContents(node->position) & CONTENTS_MASK_SOLID) {
			gi.Warn("Node %i @ %s is inside of solid\n", i, vtos(node->position));
		}
	}
}

/**
 * @brief 
 */
void Ai_NodesReady(void) {

	if (!ai_nodes) {
		return;
	}

	const guint added_nodes = ai_nodes->len - ai_player_roam.file_nodes;
	guint added_links = 0;

	for (guint i = 0; i < ai_nodes->len; i++) {
		const ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, i);

		if (node->links) {
			added_links += node->links->len;
		}
	}

	added_links -= ai_player_roam.file_links;
	gi.Print("  Game loaded %u additional nodes with %u new links.\n", added_nodes, added_links);

	/*if (ai_node_dev->integer != 1) {
		const guint optimized = Ai_OptimizeNodes();
		gi.Print("  %u nodes optimized\n", optimized);
	}*/

	Ai_CheckNodes();
}

/**
 * @brief 
 */
void Ai_SaveNodes(void) {

	if (ai_node_dev->integer != 1) {
		gi.Warn("This command only works with `ai_node_dev` set to 1.\n");
		return;
	}

	char filename[MAX_OS_PATH];

	g_snprintf(filename, sizeof(filename), "maps/%s.nav", g_level.name);

	if (!ai_nodes) {
		gi.Warn("No nodes to write.\n");
		return;
	}

	file_t *file = gi.OpenFileWrite(filename);
	int32_t magic = AI_NODE_MAGIC;
	int32_t version = AI_NODE_VERSION;
	
	gi.WriteFile(file, &magic, sizeof(magic), 1);
	gi.WriteFile(file, &version, sizeof(version), 1);

	gi.WriteFile(file, &ai_nodes->len, sizeof(ai_nodes->len), 1);

	for (guint i = 0; i < ai_nodes->len; i++) {
		const ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, i);

		gi.WriteFile(file, &node->position, sizeof(node->position), 1);

		if (node->links) {
			gi.WriteFile(file, &node->links->len, sizeof(node->links->len), 1);
			gi.WriteFile(file, node->links->data, sizeof(ai_link_t), node->links->len);
		} else {
			guint len = 0;
			gi.WriteFile(file, &len, sizeof(len), 1);
		}
	}

	gi.CloseFile(file);

	gi.Print("Wrote nodes to %s.\n", gi.RealPath(filename));

	Ai_CheckNodes();
}

/**
 * @brief 
 */
void Ai_ShutdownNodes(void) {

	if (ai_nodes) {
		for (guint i = 0; i < ai_nodes->len; i++) {
			ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, i);

			if (node->links) {
				g_array_free(node->links, true);
			}
		}

		g_array_free(ai_nodes, true);
		ai_nodes = NULL;
	}
}

typedef struct {
	ai_node_id_t id;
	float priority;
} ai_node_priority_t;

static inline float Ai_Link_Cost(const ai_node_id_t a, const ai_node_id_t b) {
	const ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, a);

	assert(node->links);

	for (guint i = 0; i < node->links->len; i++) {
		const ai_link_t *link = &g_array_index(node->links, ai_link_t, i);

		if (link->id == b) {
			return link->cost;
		}
	}

	assert(false);
	return -1;
}

/**
 * @brief
 */
GArray *Ai_Node_FindPath(const ai_node_id_t start, const ai_node_id_t end, const Ai_NodeCost_Func heuristic, float *length) {
	
	if (length) {
		*length = 0;
	}

	// sanity
	if (start == AI_NODE_INVALID || end == AI_NODE_INVALID) {
		return NULL;
	}
	
	GHashTable *costs_started = g_hash_table_new(g_direct_hash, g_direct_equal);
	GArray *queue = g_array_new(false, false, sizeof(ai_node_priority_t));
	_Bool finished = false;

	g_array_append_vals(queue, &(ai_node_priority_t) {
		.id = start,
		.priority = 0
	}, 1);

	ai_node_t *start_node = &g_array_index(ai_nodes, ai_node_t, start);
	start_node->cost = 0;
	g_hash_table_add(costs_started, start_node);

	while (queue->len) {
		
		ai_node_priority_t current = g_array_index(queue, ai_node_priority_t, queue->len - 1);
		queue = g_array_remove_index_fast(queue, queue->len - 1);

		if (current.id == end) {
			finished = true;
			break;
		}

		ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, current.id);

		if (!node->links || !node->links->len) {
			continue;
		}

		for (guint i = 0; i < node->links->len; i++) {
			const ai_link_t *link = &g_array_index(node->links, ai_link_t, i);
			ai_node_t *link_node = &g_array_index(ai_nodes, ai_node_t, link->id);
			const float new_cost = node->cost + link->cost;

			if (!g_hash_table_lookup(costs_started, link_node) || new_cost < link_node->cost) {

				g_hash_table_add(costs_started, link_node);

				link_node->cost = new_cost;
				const float priority = new_cost + heuristic(link->id, end);

				if (!queue->len) {
					g_array_insert_vals(queue, 0, &(ai_node_priority_t) {
						.id = link->id,
						.priority = priority
					}, 1);
				} else {
					for (gint x = queue->len - 1; ; x--) {

						if (priority < g_array_index(queue, ai_node_priority_t, x).priority) {
							g_array_insert_vals(queue, x + 1, &(ai_node_priority_t) {
								.id = link->id,
								.priority = priority
							}, 1);
							break;
						}

						if (x == 0) {
							g_array_insert_vals(queue, 0, &(ai_node_priority_t) {
								.id = link->id,
								.priority = priority
							}, 1);
							break;
						}
					}
				}

				link_node->came_from = Ai_Node_Index(node);
			}
		}
	}

	GArray *return_path = NULL;

	if (finished) {
		Ai_Debug("Found path from %u -> %u with %u nodes visited\n", start, end, g_hash_table_size(costs_started));

		return_path = g_array_sized_new(false, false, sizeof(ai_node_id_t), g_hash_table_size(costs_started));

		g_array_prepend_val(return_path, end);

		if (start != end) {
			ai_node_id_t from = end;

			for (;;) {
				const ai_node_t *from_node = &g_array_index(ai_nodes, ai_node_t, from);
				from = from_node->came_from;
				g_array_prepend_val(return_path, from);

				if (from == start) {
					break;
				}
			}

			if (length) {
				for (guint i = 0; i < return_path->len - 1; i++) {
					const ai_node_id_t a = g_array_index(return_path, ai_node_id_t, i);
					const ai_node_id_t b = g_array_index(return_path, ai_node_id_t, i + 1);

					*length += Ai_Link_Cost(a, b);
				}
			}
		}
	} else {
		Ai_Debug("Couldn't find path from %u -> %u\n", start, end);
	}

	g_array_free(queue, true);
	g_hash_table_destroy(costs_started);

	return return_path;
}

void Ai_OffsetNodes_f(void) {

	vec3_t translate;

	if (gi.Argc() <= 1) {
		if (ai_player_roam.last_nodes[0] == AI_NODE_INVALID) {
			return;
		}

		const vec3_t node = Ai_Node_GetPosition(ai_player_roam.last_nodes[0]);
		const vec3_t player_position = ai_player_roam.position;
		translate = Vec3_Subtract(player_position, node);
	} else {	
		const char *offset = gi.Argv(1);

		if (Parse_QuickPrimitive(offset, PARSER_DEFAULT, PARSE_DEFAULT, PARSE_FLOAT, &translate, 3) != 3) {
			return;
		}
	}

	for (guint i = 0; i < ai_nodes->len; i++) {
		ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, i);
		node->position = Vec3_Add(node->position, translate);
	}
}
