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

typedef struct {
	vec3_t position;
	GArray *links;
} ai_node_t;

static GArray *ai_nodes;

ai_node_id_t Ai_Node_FindClosest(const vec3_t position, const float max_distance) {

	if (!ai_nodes)
		return NODE_INVALID;

	ai_node_id_t closest = NODE_INVALID;
	float closest_dist = 0;
	const float dist_squared = max_distance * max_distance;

	for (guint i = 0; i < ai_nodes->len; i++) {
		const ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, i);
		float dist = Vec3_DistanceSquared(position, node->position);

		if (dist < dist_squared && (closest == NODE_INVALID || dist < closest_dist)) {
			closest = i;
			closest_dist = dist;
		}
	}

	return closest;
}

ai_node_id_t Ai_Node_Create(const vec3_t position) {

	if (!ai_nodes) {
		ai_nodes = g_array_new(false, false, sizeof(ai_node_t));
	}

	g_array_append_val(ai_nodes, (ai_node_t) {
		.position = position
	});

	return ai_nodes->len - 1;
}

_Bool Ai_Node_IsLinked(const ai_node_id_t a, const ai_node_id_t b) {
	ai_node_t *node_a = &g_array_index(ai_nodes, ai_node_t, a);

	if (node_a->links) {
		for (guint i = 0; i < node_a->links->len; i++) {
			const ai_node_id_t link = g_array_index(node_a->links, ai_node_id_t, i);

			if (link == b) {
				return true;
			}
		}
	}

	return false;
}

void Ai_Node_CreateLink(const ai_node_id_t a, const ai_node_id_t b) {

	if (Ai_Node_IsLinked(a, b)) {
		return;
	}

	ai_node_t *node_a = &g_array_index(ai_nodes, ai_node_t, a);

	if (!node_a->links) {
		node_a->links = g_array_new(false, false, sizeof(ai_node_id_t));
	}

	g_array_append_val(node_a->links, b);
}

static void Ai_Node_DestroyLink(const ai_node_id_t a, const ai_node_id_t b) {
	ai_node_t *node_a = &g_array_index(ai_nodes, ai_node_t, a);

	if (node_a->links) {
		for (guint i = 0; i < node_a->links->len; i++) {
			const ai_node_id_t link = g_array_index(node_a->links, ai_node_id_t, i);

			if (link == b) {
				node_a->links = g_array_remove_index_fast(node_a->links, i);
				break;
			}
		}
	}

	ai_node_t *node_b = &g_array_index(ai_nodes, ai_node_t, b);
	
	if (node_b->links) {
		for (guint i = 0; i < node_b->links->len; i++) {
			const ai_node_id_t link = g_array_index(node_b->links, ai_node_id_t, i);

			if (link == a) {
				node_b->links = g_array_remove_index_fast(node_b->links, i);
				break;
			}
		}
	}
}

static void Ai_Node_DestroyLinks(const ai_node_id_t id) {
	const ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, id);

	if (!node->links) {
		return;
	}

	for (guint i = node->links->len - 1; ; i--) {
		Ai_Node_DestroyLink(id, g_array_index(node->links, ai_node_id_t, i));
		
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
			ai_node_id_t *link = &g_array_index(node->links, ai_node_id_t, l);

			if (*link > id) {
				(*link)--;
			}
		}
	}
}

void Ai_Node_Destroy(const ai_node_id_t id) {

	Ai_Node_DestroyLinks(id);
	ai_nodes = g_array_remove_index(ai_nodes, id);

	if (!ai_nodes->len) {
		g_array_free(ai_nodes, true);
		ai_nodes = NULL;
	} else {
		Ai_Node_AdjustConnections(id);
	}
}

static struct {
	vec3_t position;

	ai_node_id_t last_node;

	_Bool await_landing, is_jumping, is_water_jump;
} ai_player_roam = {
	.position = { MAX_WORLD_DIST, MAX_WORLD_DIST, MAX_WORLD_DIST },
	.last_node = NODE_INVALID,
	.await_landing = true
};

static _Bool Ai_Node_PlayerIsOnFloor(const g_entity_t *player) {
	const cm_trace_t tr = aim.gi->Trace(player->s.origin, Vec3_Add(player->s.origin, Vec3(0, 0, -PM_GROUND_DIST)), player->s.mins, player->s.maxs, NULL, CONTENTS_SOLID | CONTENTS_WINDOW);

	return tr.fraction < 1.0f && tr.plane.normal.z > PM_STEP_NORMAL;
}

static vec3_t Ai_Node_GetPosition(const ai_node_id_t node) {

	return g_array_index(ai_nodes, ai_node_t, node).position;
}

static bool Ai_Node_Visible(const vec3_t position, const ai_node_id_t node) {

	return aim.gi->Trace(position, Ai_Node_GetPosition(node), Vec3_Zero(), Vec3_Zero(), NULL, CONTENTS_SOLID | CONTENTS_WINDOW).fraction == 1.0f;
}

/**
 * @brief The length of space that nodes will drop while walking.
 */
#define WALKING_DISTANCE	128.f

/**
 * @brief The length of space that couldn't be logically travelled by regular means.
 */
#define TELEPORT_DISTANCE	256.f

void Ai_Node_PlayerRoam(const g_entity_t *player, const pm_cmd_t *cmd) {

	if (!ai_node_dev->integer) {
		return;
	}

	const _Bool in_water = aim.gi->PointContents(player->s.origin) & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA);

	// we're waiting to land to drop a node; we jumped, fell, got sent by a jump pad, something like that.
	if (ai_player_roam.await_landing) {

		if (Ai_Node_PlayerIsOnFloor(player) || in_water) {

			// we landed!
			ai_player_roam.await_landing = false;
			ai_player_roam.position = player->s.origin;

			ai_node_id_t landed_near_node = Ai_Node_FindClosest(player->s.origin, WALKING_DISTANCE / 2);
			_Bool dropped_node = false;

			if (landed_near_node == NODE_INVALID) {

				landed_near_node = Ai_Node_Create(player->s.origin);
				dropped_node = true;
			}

			// one-way node from where we were to here
			if (ai_player_roam.last_node != NODE_INVALID) {

				Ai_Node_CreateLink(ai_player_roam.last_node, landed_near_node);

				if (ai_player_roam.is_water_jump) {
					Ai_Node_CreateLink(landed_near_node, ai_player_roam.last_node);
					aim.gi->Debug("Most likely waterjump; connecting both ends\n");
				} else if (ai_player_roam.is_jumping || in_water) {
					const float z_diff = (float)fabs(Ai_Node_GetPosition(ai_player_roam.last_node).z - Ai_Node_GetPosition(landed_near_node).z);

					if (z_diff < PM_STEP_HEIGHT || (in_water && z_diff < PM_STEP_HEIGHT * 3.f)) {
						Ai_Node_CreateLink(landed_near_node, ai_player_roam.last_node);
						aim.gi->Debug("Most likely jump or drop-into-water link; connecting both ends\n");
					}
				}
			}

			ai_player_roam.last_node = landed_near_node;
			ai_player_roam.is_water_jump = false;
			aim.gi->Debug("%s A->B node\n", dropped_node ? "Dropped new" : "Connected existing");
		}

		return;
	}
	
	const float distance_compare = Vec3_Distance(player->s.origin,
		ai_player_roam.last_node != NODE_INVALID ? Ai_Node_GetPosition(ai_player_roam.last_node) : ai_player_roam.position);

	// we probably teleported; no node, just start dropping here when we land
	if (distance_compare > TELEPORT_DISTANCE) {

		ai_player_roam.last_node = NODE_INVALID;
		ai_player_roam.position = player->s.origin;
		ai_player_roam.await_landing = true;

		aim.gi->Debug("Teleport detected; awaiting landing...\n");
		return;
	}

	// we just left the floor (or water); drop a node here
	if (!Ai_Node_PlayerIsOnFloor(player) && !in_water) {
		const ai_node_id_t jumped_near_node = Ai_Node_FindClosest(player->s.origin, WALKING_DISTANCE / 2);
		const _Bool is_jump = player->client->ps.pm_state.velocity.z > 0;

		if (jumped_near_node == NODE_INVALID) {
			const ai_node_id_t id = Ai_Node_Create(player->s.origin);

			if (ai_player_roam.last_node != NODE_INVALID) {

				Ai_Node_CreateLink(ai_player_roam.last_node, id);
				Ai_Node_CreateLink(id, ai_player_roam.last_node);
			}

			ai_player_roam.last_node = id;
			aim.gi->Debug("Dropped new A<->B node\n");
		} else {
				
			if (ai_player_roam.last_node != NODE_INVALID) {

				Ai_Node_CreateLink(ai_player_roam.last_node, jumped_near_node);
				Ai_Node_CreateLink(jumped_near_node, ai_player_roam.last_node);

				aim.gi->Debug("Connected existing A<->B node\n");
			}

			ai_player_roam.last_node = jumped_near_node;
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

	// we're walkin'

	const ai_node_id_t closest_node = Ai_Node_FindClosest(player->s.origin, WALKING_DISTANCE / 4);

	// attack button moves node
	if (cmd->buttons & BUTTON_ATTACK) {
		ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, closest_node);
		node->position = player->s.origin;
	}
	// if we touched another node and had another node lit up; connect us if we aren't already
	else if (closest_node != NODE_INVALID && closest_node != ai_player_roam.last_node && Ai_Node_Visible(player->s.origin, closest_node)) {

		if (ai_player_roam.last_node != NODE_INVALID) {

			Ai_Node_CreateLink(ai_player_roam.last_node, closest_node);
			Ai_Node_CreateLink(closest_node, ai_player_roam.last_node);

			aim.gi->Debug("Connected existing A<->B node\n");
		}

		ai_player_roam.last_node = closest_node;
	// we got far enough from the last node, so drop a new node
	} else if (distance_compare > WALKING_DISTANCE) {

		ai_player_roam.position = player->s.origin;

		const ai_node_id_t id = Ai_Node_Create(player->s.origin);

		if (ai_player_roam.last_node != NODE_INVALID) {

			Ai_Node_CreateLink(ai_player_roam.last_node, id);
			Ai_Node_CreateLink(id, ai_player_roam.last_node);
		}

		ai_player_roam.last_node = id;
		aim.gi->Debug("Dropped new A<->B node\n");
	}

	// we're currently in water; the only way we can get out
	// is by waterjumping, having something take us out of the water,
	// a ladder or walking straight out of water. mark waterjump as true,
	// we'll figure it out when we actually leave water what the intentions are.
	if (in_water) {
		ai_player_roam.is_water_jump = true;
	}
}

typedef struct {
	union {
		struct {
			ai_node_id_t a, b;
		};
		int32_t v;
	};
} ai_unique_link_t;

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

void Ai_Node_Render(const g_entity_t *player) {

	if (!ai_node_dev->integer) {
		return;
	}

	if (!ai_nodes) {
		return;
	}

	GHashTable *unique_links = g_hash_table_new(g_direct_hash, g_direct_equal);

	for (guint i = 0; i < ai_nodes->len; i++) {
		const ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, i);

		aim.gi->WriteByte(SV_CMD_TEMP_ENTITY);
		aim.gi->WriteByte(TE_AI_NODE);
		aim.gi->WritePosition(node->position);
		aim.gi->WriteByte((ai_player_roam.last_node == i) ? 1 : 0);
		aim.gi->Multicast(node->position, MULTICAST_PVS, NULL);

		if (node->links) {
			for (guint l = 0; l < node->links->len; l++) {
				const ai_node_id_t link = g_array_index(node->links, ai_node_id_t, l);
				ai_unique_link_t ulink;
				int32_t bit;

				if (link < i) {
					ulink.a = link;
					ulink.b = i;
					bit = 1;
				} else {
					ulink.a = i;
					ulink.b = link;
					bit = 2;
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
}