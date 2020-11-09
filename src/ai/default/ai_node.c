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

#include "ai_local.h"
#include "game/default/bg_pmove.h"

/**
 * @brief
 */
static struct {
	vec3_t position;

	vec3_t last_good_position;

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

	if (ai_player_roam.last_nodes[0] == NODE_INVALID ||
		ai_player_roam.last_nodes[1] == NODE_INVALID) {
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
	g_entity_t *mover;
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

	return aim.gi->Trace(position, Ai_Node_GetPosition(node), Vec3_Zero(), Vec3_Zero(), NULL, CONTENTS_SOLID | CONTENTS_WINDOW).fraction == 1.0f;
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
ai_node_id_t Ai_Node_FindClosest(const vec3_t position, const float max_distance, const bool only_visible) {

	if (!ai_nodes)
		return NODE_INVALID;

	ai_node_id_t closest = NODE_INVALID;
	float closest_dist = 0;
	const float dist_squared = max_distance * max_distance;

	for (guint i = 0; i < ai_nodes->len; i++) {
		const ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, i);

		// don't find nodes a few steps above us.
		if (fabs(position.z - node->position.z) > PM_STEP_HEIGHT * 2.f) {
			continue;
		}

		float dist = Vec3_DistanceSquared(position, node->position);

		if (dist < dist_squared && (closest == NODE_INVALID || dist < closest_dist) && (!only_visible || Ai_Node_Visible(position, i))) {
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

	if (Ai_Node_IsLinked(a, b)) {
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
}

/**
 * @brief
 */
static inline void Ai_Node_CreateDefaultLink(const ai_node_id_t a, const ai_node_id_t b, const _Bool bidirectional) {

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
		
		if (i == 0) {
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
	const cm_trace_t tr = aim.gi->Trace(player->s.origin, Vec3_Add(player->s.origin, Vec3(0, 0, -PM_GROUND_DIST)), player->s.mins, player->s.maxs, NULL, CONTENTS_MASK_SOLID);

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
_Bool Ai_Path_CanPathTo(const GArray *path, const guint index) {

	// sanity
	if (index >= path->len) {
		return true;
	}

	// if we're heading onto a mover node, only allow us to go forth
	// if the mover is there
	const ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, g_array_index(path, ai_node_id_t, index));

	if (node->mover) {

		// if we're the first in the list somehow, or the prior node
		// was also a mover node, we're good to go.
		if (index == 0 || g_array_index(ai_nodes, ai_node_t, g_array_index(path, ai_node_id_t, index - 1)).mover != NULL) {
			return true;
		}

		// check if the mover is in place
		cm_trace_t tr = aim.gi->Trace(node->position, Vec3_Subtract(node->position, Vec3(0, 0, 128.f)), PM_MINS, PM_MAXS, NULL, CONTENTS_MASK_SOLID);

		if (!tr.ent || tr.ent != node->mover || tr.start_solid || tr.all_solid) {
			tr = aim.gi->Trace(node->position, Vec3_Subtract(node->position, Vec3(0, 0, 128.f)), Vec3_Zero(), Vec3_Zero(), NULL, CONTENTS_MASK_SOLID);
		}

		if (tr.ent == node->mover && !tr.start_solid && !tr.all_solid) {
			return true;
		}

		return false;
	}

	return true;
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
		ai_player_roam.last_nodes[0] = ai_player_roam.last_nodes[1] = NODE_INVALID;
	}

	const _Bool in_water = aim.gi->PointContents(player->s.origin) & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA);
	const float last_node_distance_compare = ai_player_roam.last_nodes[0] == NODE_INVALID ? FLT_MAX : Vec3_Distance(player->s.origin, Ai_Node_GetPosition(ai_player_roam.last_nodes[0]));
	const float player_distance_compare = Vec3_Distance(player->s.origin, ai_player_roam.position);

	if (do_noding) {
		// we're waiting to land to drop a node; we jumped, fell, got sent by a jump pad, something like that.
		if (ai_player_roam.await_landing) {

			if (Ai_Node_PlayerIsOnFloor(player) || in_water) {

				// we landed!
				ai_player_roam.await_landing = false;
				ai_player_roam.position = player->s.origin;

				ai_node_id_t landed_near_node = Ai_Node_FindClosest(player->s.origin, WALKING_DISTANCE / 2, true);
				_Bool dropped_node = false;

				if (landed_near_node == NODE_INVALID) {

					landed_near_node = Ai_Node_CreateNode(player->s.origin);
					dropped_node = true;
				}

				// one-way node from where we were to here
				if (ai_player_roam.last_nodes[0] != NODE_INVALID) {
					_Bool bidirectional = false;

					if (ai_player_roam.is_water_jump) {
						bidirectional = true;
						aim.gi->Debug("Most likely waterjump; connecting both ends\n");
					} else if (ai_player_roam.is_jumping || in_water) {
						const float z_diff = (float)fabs(Ai_Node_GetPosition(ai_player_roam.last_nodes[0]).z - Ai_Node_GetPosition(landed_near_node).z);

						if (z_diff < PM_STEP_HEIGHT || (in_water && z_diff < PM_STEP_HEIGHT * 3.f)) {
							bidirectional = true;
							aim.gi->Debug("Most likely jump or drop-into-water link; connecting both ends\n");
						}
					}

					Ai_Node_CreateDefaultLink(ai_player_roam.last_nodes[0], landed_near_node, bidirectional);
				}

				ai_player_roam.last_nodes[1] = ai_player_roam.last_nodes[0];
				ai_player_roam.last_nodes[0] = landed_near_node;
				ai_player_roam.is_water_jump = false;
				aim.gi->Debug("%s A->B node\n", dropped_node ? "Dropped new" : "Connected existing");
			}

			return;
		}

		// we probably teleported; no node, just start dropping here when we land
		if (player_distance_compare > TELEPORT_DISTANCE) {

			ai_player_roam.last_nodes[0] = ai_player_roam.last_nodes[1] = NODE_INVALID;
			ai_player_roam.position = player->s.origin;
			ai_player_roam.await_landing = true;

			aim.gi->Debug("Teleport detected; awaiting landing...\n");
			return;
		}

		// we just left the floor (or water); drop a node here
		if (!Ai_Node_PlayerIsOnFloor(player) && !in_water) {
			const ai_node_id_t jumped_near_node = Ai_Node_FindClosest(player->s.origin, WALKING_DISTANCE / 2, true);
			const _Bool is_jump = player->client->ps.pm_state.velocity.z > 0;

			if (jumped_near_node == NODE_INVALID) {
				const ai_node_id_t id = Ai_Node_CreateNode(player->s.origin);

				if (ai_player_roam.last_nodes[0] != NODE_INVALID) {

					Ai_Node_CreateDefaultLink(ai_player_roam.last_nodes[0], id, true);
				}
			
				ai_player_roam.last_nodes[1] = ai_player_roam.last_nodes[0];
				ai_player_roam.last_nodes[0] = id;
				aim.gi->Debug("Dropped new A<->B node\n");
			} else {
				
				if (ai_player_roam.last_nodes[0] != NODE_INVALID) {

					Ai_Node_CreateDefaultLink(ai_player_roam.last_nodes[0], jumped_near_node, true);

					aim.gi->Debug("Connected existing A<->B node\n");
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

			aim.gi->Debug("Left ground; jumping? %s\n", is_jump ? "yes" : "nop");
			return;
		}
	}

	// we're walkin'

	const ai_node_id_t closest_node = Ai_Node_FindClosest(player->s.origin, WALKING_DISTANCE / 4, true);
	const _Bool on_mover = ENTITY_DATA(player, ground_entity) && ENTITY_DATA(player, ground_entity)->s.number != 0;

	// attack button moves node
	if (allow_adjustments && (ai_player_roam.latched_buttons & BUTTON_ATTACK)) {
		if (ai_player_roam.last_nodes[0] != NODE_INVALID) {
			ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, ai_player_roam.last_nodes[0]);
			node->position = player->s.origin;

			// recalculate links
			Ai_Node_RecalculateCosts(ai_player_roam.last_nodes[0]);
		}
		ai_player_roam.latched_buttons &= ~BUTTON_ATTACK;
	// hook destroys node
	} else if ((allow_adjustments) && (ai_player_roam.latched_buttons & BUTTON_HOOK)) {
		if (ai_player_roam.last_nodes[0] != NODE_INVALID) {
			Ai_Node_Destroy(ai_player_roam.last_nodes[0]);
			ai_player_roam.position = player->s.origin;
			ai_player_roam.last_nodes[0] = ai_player_roam.last_nodes[1] = NODE_INVALID;
			ai_player_roam.last_nodes[0] = Ai_Node_FindClosest(player->s.origin, WALKING_DISTANCE * 2.5f, true);
		}
		ai_player_roam.latched_buttons &= ~BUTTON_HOOK;
	// score adjusts link connections
	} else if ((allow_adjustments) && (ai_player_roam.latched_buttons & BUTTON_SCORE)) {

		if (ai_player_roam.last_nodes[1] != NODE_INVALID && ai_player_roam.last_nodes[0] != NODE_INVALID) {
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
			ai_node_id_t id = Ai_Node_FindClosest(player->s.origin, WALKING_DISTANCE / 8, true);
			
			if (id == NODE_INVALID) {
				id = Ai_Node_CreateNode(player->s.origin);
			}

			if (ai_player_roam.last_nodes[0] != NODE_INVALID) {

				Ai_Node_CreateDefaultLink(ai_player_roam.last_nodes[0], id, false);
			}
		
			ai_player_roam.last_nodes[1] = ai_player_roam.last_nodes[0];
			ai_player_roam.last_nodes[0] = id;
			aim.gi->Debug("Dropped mover A->B node\n");
		}

	// if we touched another node and had another node lit up; connect us if we aren't already
	} else if (closest_node != NODE_INVALID && closest_node != ai_player_roam.last_nodes[0] && Ai_Node_Visible(player->s.origin, closest_node)) {

		if (do_noding && ai_player_roam.last_nodes[0] != NODE_INVALID) {

			Ai_Node_CreateDefaultLink(ai_player_roam.last_nodes[0], closest_node, !ai_player_roam.on_mover);
			aim.gi->Debug("Connected existing A<->B node\n");
		}
		
		ai_player_roam.last_nodes[1] = ai_player_roam.last_nodes[0];
		ai_player_roam.last_nodes[0] = closest_node;
	// we got far enough from the last node, so drop a new node
	} else if (last_node_distance_compare > WALKING_DISTANCE) {

		if (do_noding) {
			ai_node_id_t id = Ai_Node_FindClosest(player->s.origin, WALKING_DISTANCE / 2, true);
			
			if (id == NODE_INVALID) {
				id = Ai_Node_CreateNode(player->s.origin);
			}

			if (ai_player_roam.last_nodes[0] != NODE_INVALID) {

				Ai_Node_CreateDefaultLink(ai_player_roam.last_nodes[0], id, !ai_player_roam.on_mover);
			}
		
			ai_player_roam.last_nodes[1] = ai_player_roam.last_nodes[0];
			ai_player_roam.last_nodes[0] = id;
			aim.gi->Debug("Dropped new A<->B node\n");
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

	aim.gi->WriteByte(SV_CMD_TEMP_ENTITY);
	aim.gi->WriteByte(TE_AI_NODE_LINK);
	aim.gi->WritePosition(node_a->position);
	aim.gi->WritePosition(node_b->position);
	aim.gi->WriteByte(bits);
	aim.gi->Multicast(node_a->position, MULTICAST_PVS, NULL);
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

		aim.gi->WriteByte(SV_CMD_TEMP_ENTITY);
		aim.gi->WriteByte(TE_AI_NODE);
		aim.gi->WritePosition(node->position);
		aim.gi->WriteByte(in_path ? 3 : (ai_player_roam.last_nodes[0] == i) ? 1 : (ai_player_roam.last_nodes[1] == i) ? 2 : 0);
		aim.gi->Multicast(node->position, MULTICAST_PVS, NULL);

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

				if (!g_hash_table_contains(unique_links, GINT_TO_POINTER(ulink.v))) {
					g_hash_table_insert(unique_links, GINT_TO_POINTER(ulink.v), GINT_TO_POINTER(bit));
				} else {
					int32_t existing_bit = GPOINTER_TO_INT(g_hash_table_lookup(unique_links, GINT_TO_POINTER(ulink.v)));
					g_hash_table_replace(unique_links, GINT_TO_POINTER(ulink.v), GINT_TO_POINTER(existing_bit | bit));
				}
			}
		}

		if (node->mover) {

			aim.gi->WriteByte(SV_CMD_TEMP_ENTITY);
			aim.gi->WriteByte(TE_AI_NODE_LINK);
			aim.gi->WritePosition(node->position);
			aim.gi->WritePosition(Vec3_Mix(node->mover->abs_mins, node->mover->abs_maxs, 0.5));
			aim.gi->WriteByte(8);
			aim.gi->Multicast(node->position, MULTICAST_PVS, NULL);
		}
	}

	g_hash_table_foreach(unique_links, Ai_Node_RenderLinks, NULL);

	g_hash_table_destroy(unique_links);

	for (uint32_t i = 1; i <= sv_max_clients->integer; i++) {
		const g_entity_t *ent = ENTITY_FOR_NUM(i);

		if (!ent->in_use || !ent->client->ai) {
			continue;
		}

		const ai_locals_t *ai = Ai_GetLocals(ent);

		if (ai->move_target.type != AI_GOAL_PATH) {
			continue;
		}
		
		aim.gi->WriteByte(SV_CMD_TEMP_ENTITY);
		aim.gi->WriteByte(TE_AI_NODE_LINK);
		aim.gi->WritePosition(ent->s.origin);
		aim.gi->WritePosition(ai->move_target.path.path_position);
		aim.gi->WriteByte(8);
		aim.gi->Multicast(ent->s.origin, MULTICAST_PVS, NULL);
	}
}

/**
 * @brief Only used for Ai_OptimizeNodes; adds links to node a to table
 */
static void Ai_Node_GetLinksTo(const ai_node_id_t a, GHashTable *table) {

	for (guint i = 0; i < ai_nodes->len; i++) {
		const ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, i);

		if (!node->links) {
			continue;
		}

		for (guint l = 0; l < node->links->len; l++) {
			const ai_link_t *link = &g_array_index(node->links, ai_link_t, l);

			if (link->id == a) {

				g_hash_table_add(table, GINT_TO_POINTER((gint)i));
			}
		}
	}
}

/**
 * @brief 
 */
static vec3_t Ai_Node_GetDirection(const ai_node_id_t a, const ai_node_id_t b) {

	return Vec3_Normalize(Vec3_Subtract(Ai_Node_GetPosition(a), Ai_Node_GetPosition(b)));
}

/**
 * @brief
 */
static uint8_t Ai_Node_LinkType(const ai_node_id_t a, const ai_node_id_t b) {
	uint8_t bits = 0;
	
	if (Ai_Node_IsLinked(a, b)) {
		bits |= 1;
	}

	if (Ai_Node_IsLinked(b, a)) {
		bits |= 2;
	}

	return bits;
}

/**
 * @brief Optimizes graph by removing long chains of nodes with very little deviation from direction
 */
static uint32_t Ai_OptimizeNodes(void) {

	uint32_t total = 0;
	GHashTable *links = g_hash_table_new(g_direct_hash, g_direct_equal);

	for (;;) {
		uint32_t count = 0;

		for (guint i = 0; i < ai_nodes->len; i++) {
			ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, i);

			// we have no links, or more than two links, so we can't optimize this node
			if (!node->links || node->links->len > 2) {
				continue;
			}

			// find the two nodes that are between node

			for (guint l = 0; l < node->links->len; l++) {
				const ai_link_t *link = &g_array_index(node->links, ai_link_t, l);
				g_hash_table_add(links, GINT_TO_POINTER((gint)link->id));
			}

			Ai_Node_GetLinksTo(i, links);

			// too many links; need two total
			if (g_hash_table_size(links) != 2) {
				g_hash_table_remove_all(links);
				continue;
			}

			// convert links to array for simplicity
			gpointer *keys = g_hash_table_get_keys_as_array(links, NULL);
			ai_node_id_t links[2] = {
				(ai_node_id_t)GPOINTER_TO_INT(keys[0]),
				(ai_node_id_t)GPOINTER_TO_INT(keys[1])
			};
			g_free(keys);

			uint8_t link_type;

			// need to be same link type to work properly
			if ((link_type = Ai_Node_LinkType(links[0], i)) != Ai_Node_LinkType(i, links[1])) {
				continue;
			}

			// check that we're in a fairly straight-ish line
			const vec3_t dir_a = Ai_Node_GetDirection(links[0], i);
			const vec3_t dir_b = Ai_Node_GetDirection(i, links[1]);
			const float dot = Vec3_Dot(dir_a, dir_b);

			if (dot < 0.9) {
				continue;
			}

			// good 2 go
			Ai_Node_Destroy(i);
			
			if (links[0] >= i) {
				links[0]--;
			}
			if (links[1] >= i) {
				links[1]--;
			}

			i--;

			if (link_type & 1) {
				Ai_Node_CreateDefaultLink(links[0], links[1], false);
			}
			if (link_type & 2) {
				Ai_Node_CreateDefaultLink(links[1], links[0], false);
			}

			count++;
		}

		if (!count)
			break;

		total += count;
	}

	g_hash_table_destroy(links);
	return total;
}

#define AI_NODE_MAGIC ('Q' | '2' << 8 | 'N' << 16 | 'S' << 24)
#define AI_NODE_VERSION 2

/**
 * @brief 
 */
void Ai_InitNodes(const char *mapname) {

	ai_player_roam.position = Vec3(MAX_WORLD_DIST, MAX_WORLD_DIST, MAX_WORLD_DIST);
	ai_player_roam.last_nodes[0] = ai_player_roam.last_nodes[1] = NODE_INVALID;
	ai_player_roam.await_landing = true;

	g_strlcpy(ai_level.mapname, mapname, sizeof(ai_level.mapname));

	char filename[MAX_OS_PATH];

	g_snprintf(filename, sizeof(filename), "ai/%s.nav", ai_level.mapname);

	if (!aim.gi->FileExists(filename)) {
		aim.gi->Warn("No navigation file exists for this map; bots will be dumb!\nUse `ai_node_dev` to set up nodes.\n");
		return;
	}

	file_t *file = aim.gi->OpenFile(filename);
	int32_t magic, version;
	
	aim.gi->ReadFile(file, &magic, sizeof(magic), 1);

	if (magic != AI_NODE_MAGIC) {
		aim.gi->Warn("Nav file invalid format!\n");
		aim.gi->CloseFile(file);
		return;
	}

	aim.gi->ReadFile(file, &version, sizeof(version), 1);

	if (version != AI_NODE_VERSION) {
		aim.gi->Warn("Nav file out of date!\n");
		aim.gi->CloseFile(file);
		return;
	}

	guint num_nodes;
	aim.gi->ReadFile(file, &num_nodes, sizeof(num_nodes), 1);

	ai_nodes = g_array_sized_new(false, true, sizeof(ai_node_t), num_nodes);
	g_array_set_size(ai_nodes, num_nodes);

	guint total_links = 0;

	for (guint i = 0; i < ai_nodes->len; i++) {
		ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, i);

		aim.gi->ReadFile(file, &node->position, sizeof(node->position), 1);

		guint num_links;
	
		aim.gi->ReadFile(file, &num_links, sizeof(num_links), 1);

		if (num_links) {
			node->links = g_array_sized_new(false, false, sizeof(ai_link_t), num_links);
			g_array_set_size(node->links, num_links);
			aim.gi->ReadFile(file, node->links->data, sizeof(ai_link_t), num_links);
			total_links += num_links;
		} else {
			node->links = NULL;
		}
	}

	aim.gi->CloseFile(file);
	aim.gi->Print("  Loaded %u nodes with %u total links.\n", num_nodes, total_links);

	ai_player_roam.file_nodes = num_nodes;
	ai_player_roam.file_links = total_links;
}

/**
 * @brief 
 */
static void Ai_CheckNodes(void) {

	for (size_t i = sv_max_clients->value; i < aim.ge->num_entities; i++) {
		
		g_entity_t *ent = ENTITY_FOR_NUM(i);

		if (!ent->in_use) {
			continue;
		}

		// only warn for item nodes
		if (!ENTITY_DATA(ent, item)) {
			continue;
		}

		const ai_node_id_t node = Ai_Node_FindClosest(ent->s.origin, WALKING_DISTANCE * 2.5f, true);

		if (node == NODE_INVALID) {
			aim.gi->Warn("Entity %s @ %s appears to be unreachable by nodes\n", ENTITY_DATA(ent, class_name), vtos(ent->s.origin));
		}
	}
}

/**
 * @brief Adds all nodes that are directly connected to this node (whether one-way or not).
 */
static GList *Ai_Node_FindAttachedNodes(const ai_node_t *node, GList *list) {

	for (guint i = 0; i < ai_nodes->len; i++) {
		ai_node_t *check = &g_array_index(ai_nodes, ai_node_t, i);

		if (Ai_Node_IsLinked(Ai_Node_Index(node), Ai_Node_Index(check)) ||
			Ai_Node_IsLinked(Ai_Node_Index(check), Ai_Node_Index(node))) {
			list = g_list_prepend(list, check);
		}
	}

	return list;
}

/**
 * @brief This function links any floating nodes connected to a mover to that mover.
 * In theory, these should only be dropped if the mover moved us onto these spots.
 */
static guint Ai_Node_FloodFillEntity(const ai_node_t *node) {

	guint count = 0;
	GList *head = NULL;

	// add seed nodes
	head = Ai_Node_FindAttachedNodes(node, head);

	while (head != NULL) {
		ai_node_t *check = (ai_node_t *)head->data;

		head = g_list_delete_link(head, head);

		// flood fill nodes should only have one link
		if (check->links->len > 1) {
			continue;
		}

		// already have a mover, we're fine
		if (check->mover) {
			continue;
		}

		// check that we're in the air
		const cm_trace_t tr = aim.gi->Trace(check->position, Vec3_Subtract(check->position, Vec3(0, 0, PM_GROUND_DIST * 2)), Vec3_Scale(PM_MINS, 0.5f), Vec3_Scale(PM_MAXS, 0.5f), NULL, CONTENTS_MASK_SOLID);

		// touched something, so we shouldn't consider this node part of a mover
		if (tr.fraction < 1.0) {
			continue;
		}

		// link us!
		check->mover = node->mover;
		count++;

		head = Ai_Node_FindAttachedNodes(check, head);
	}

	return count;
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
		added_links += node->links->len;
	}

	added_links -= ai_player_roam.file_links;
	aim.gi->Print("  Game loaded %u additional nodes with %u new links.\n", added_nodes, added_links);

	guint linked_movers = 0;

	// link up movers with nodes that need them
	for (guint i = 0; i < ai_nodes->len; i++) {
		ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, i);
		cm_trace_t tr = aim.gi->Trace(node->position, Vec3_Subtract(node->position, Vec3(0, 0, 128.f)), PM_MINS, PM_MAXS, NULL, CONTENTS_MASK_SOLID);

		if (!tr.ent || tr.ent->s.number == 0) {
			tr = aim.gi->Trace(node->position, Vec3_Subtract(node->position, Vec3(0, 0, 128.f)), Vec3_Zero(), Vec3_Zero(), NULL, CONTENTS_MASK_SOLID);
		}

		if (!tr.ent || tr.ent->s.number == 0) {
			continue;
		}

		linked_movers++;
		node->mover = tr.ent;

		linked_movers += Ai_Node_FloodFillEntity(node);
	}

	aim.gi->Print("  Linked %u movers to navigation graph.\n", linked_movers);

	const guint optimized = Ai_OptimizeNodes();
	aim.gi->Print("  %u nodes optimized\n", optimized);

	if (ai_node_dev->integer) {
		Ai_CheckNodes();
	}
}

/**
 * @brief 
 */
void Ai_SaveNodes(void) {

	if (ai_node_dev->integer != 1) {
		aim.gi->Warn("This command only works with `ai_node_dev` set to 1.\n");
		return;
	}

	char filename[MAX_OS_PATH];

	g_snprintf(filename, sizeof(filename), "ai/%s.nav", ai_level.mapname);

	if (!ai_nodes) {
		aim.gi->Warn("No nodes to write.\n");
		return;
	}

	file_t *file = aim.gi->OpenFileWrite(filename);
	int32_t magic = AI_NODE_MAGIC;
	int32_t version = AI_NODE_VERSION;
	
	aim.gi->WriteFile(file, &magic, sizeof(magic), 1);
	aim.gi->WriteFile(file, &version, sizeof(version), 1);

	aim.gi->WriteFile(file, &ai_nodes->len, sizeof(ai_nodes->len), 1);

	for (guint i = 0; i < ai_nodes->len; i++) {
		const ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, i);

		aim.gi->WriteFile(file, &node->position, sizeof(node->position), 1);

		if (node->links) {
			aim.gi->WriteFile(file, &node->links->len, sizeof(node->links->len), 1);
			aim.gi->WriteFile(file, node->links->data, sizeof(ai_link_t), node->links->len);
		} else {
			guint len = 0;
			aim.gi->WriteFile(file, &len, sizeof(len), 1);
		}
	}

	aim.gi->CloseFile(file);

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
	if (start == NODE_INVALID || end == NODE_INVALID) {
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

		if (!node->links->len) {
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
		aim.gi->Debug("Found path from %u -> %u with %u nodes visited\n", start, end, g_hash_table_size(costs_started));

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
		aim.gi->Debug("Couldn't find path from %u -> %u\n", start, end);
	}

	g_array_free(queue, true);
	g_hash_table_destroy(costs_started);

	return return_path;
}
