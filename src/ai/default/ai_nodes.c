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

	_Bool await_landing, is_jumping, is_water_jump;

	button_state_t latched_buttons, old_buttons, buttons;
} ai_player_roam = {
	.position = { MAX_WORLD_DIST, MAX_WORLD_DIST, MAX_WORLD_DIST },
	.last_nodes = { NODE_INVALID, NODE_INVALID },
	.await_landing = true
};

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
static bool Ai_Node_Visible(const vec3_t position, const ai_node_id_t node) {

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
static ai_node_id_t Ai_Node_Create(const vec3_t position) {

	if (!ai_nodes) {
		ai_nodes = g_array_new(false, false, sizeof(ai_node_t));
	}

	g_array_append_val(ai_nodes, (ai_node_t) {
		.position = position
	});

	return ai_nodes->len - 1;
}

/**
 * @brief
 */
static _Bool Ai_Node_IsLinked(const ai_node_id_t a, const ai_node_id_t b) {
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

/**
 * @brief
 */
static void Ai_Node_CreateLink(const ai_node_id_t a, const ai_node_id_t b) {

	if (Ai_Node_IsLinked(a, b)) {
		return;
	}

	ai_node_t *node_a = &g_array_index(ai_nodes, ai_node_t, a);

	if (!node_a->links) {
		node_a->links = g_array_new(false, false, sizeof(ai_node_id_t));
	}

	g_array_append_val(node_a->links, b);
}

/**
 * @brief
 */
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

/**
 * @brief
 */
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
	const cm_trace_t tr = aim.gi->Trace(player->s.origin, Vec3_Add(player->s.origin, Vec3(0, 0, -PM_GROUND_DIST)), player->s.mins, player->s.maxs, NULL, CONTENTS_SOLID | CONTENTS_WINDOW);

	return tr.fraction < 1.0f && tr.plane.normal.z > PM_STEP_NORMAL;
}

/**
 * @brief
 */
vec3_t Ai_Node_GetPosition(const ai_node_id_t node) {

	return g_array_index(ai_nodes, ai_node_t, node).position;
}

/**
 * @brief The length of space that nodes will drop while walking.
 */
#define WALKING_DISTANCE	128.f

/**
 * @brief The length of space that couldn't be logically travelled by regular means.
 */
#define TELEPORT_DISTANCE	256.f

/**
 * @brief The length of space that couldn't be logically travelled by regular means.
 */
void Ai_Node_PlayerRoam(const g_entity_t *player, const pm_cmd_t *cmd) {

	if (!ai_node_dev->integer) {
		return;
	}

	ai_player_roam.old_buttons = ai_player_roam.buttons;
	ai_player_roam.buttons = cmd->buttons;
	ai_player_roam.latched_buttons |= ai_player_roam.buttons & ~ai_player_roam.old_buttons;

	const _Bool in_water = aim.gi->PointContents(player->s.origin) & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA);

	// we're waiting to land to drop a node; we jumped, fell, got sent by a jump pad, something like that.
	if (ai_player_roam.await_landing) {

		if (Ai_Node_PlayerIsOnFloor(player) || in_water) {

			// we landed!
			ai_player_roam.await_landing = false;
			ai_player_roam.position = player->s.origin;

			ai_node_id_t landed_near_node = Ai_Node_FindClosest(player->s.origin, WALKING_DISTANCE / 2, true);
			_Bool dropped_node = false;

			if (landed_near_node == NODE_INVALID) {

				landed_near_node = Ai_Node_Create(player->s.origin);
				dropped_node = true;
			}

			// one-way node from where we were to here
			if (ai_player_roam.last_nodes[0] != NODE_INVALID) {

				Ai_Node_CreateLink(ai_player_roam.last_nodes[0], landed_near_node);

				if (ai_player_roam.is_water_jump) {
					Ai_Node_CreateLink(landed_near_node, ai_player_roam.last_nodes[0]);
					aim.gi->Debug("Most likely waterjump; connecting both ends\n");
				} else if (ai_player_roam.is_jumping || in_water) {
					const float z_diff = (float)fabs(Ai_Node_GetPosition(ai_player_roam.last_nodes[0]).z - Ai_Node_GetPosition(landed_near_node).z);

					if (z_diff < PM_STEP_HEIGHT || (in_water && z_diff < PM_STEP_HEIGHT * 3.f)) {
						Ai_Node_CreateLink(landed_near_node, ai_player_roam.last_nodes[0]);
						aim.gi->Debug("Most likely jump or drop-into-water link; connecting both ends\n");
					}
				}
			}

			ai_player_roam.last_nodes[1] = ai_player_roam.last_nodes[0];
			ai_player_roam.last_nodes[0] = landed_near_node;
			ai_player_roam.is_water_jump = false;
			aim.gi->Debug("%s A->B node\n", dropped_node ? "Dropped new" : "Connected existing");
		}

		return;
	}
	
	const float distance_compare = Vec3_Distance(player->s.origin,
		ai_player_roam.last_nodes[0] != NODE_INVALID ? Ai_Node_GetPosition(ai_player_roam.last_nodes[0]) : ai_player_roam.position);

	// we probably teleported; no node, just start dropping here when we land
	if (distance_compare > TELEPORT_DISTANCE) {

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
			const ai_node_id_t id = Ai_Node_Create(player->s.origin);

			if (ai_player_roam.last_nodes[0] != NODE_INVALID) {

				Ai_Node_CreateLink(ai_player_roam.last_nodes[0], id);
				Ai_Node_CreateLink(id, ai_player_roam.last_nodes[0]);
			}
			
			ai_player_roam.last_nodes[1] = ai_player_roam.last_nodes[0];
			ai_player_roam.last_nodes[0] = id;
			aim.gi->Debug("Dropped new A<->B node\n");
		} else {
				
			if (ai_player_roam.last_nodes[0] != NODE_INVALID) {

				Ai_Node_CreateLink(ai_player_roam.last_nodes[0], jumped_near_node);
				Ai_Node_CreateLink(jumped_near_node, ai_player_roam.last_nodes[0]);

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

	// we're walkin'

	const ai_node_id_t closest_node = Ai_Node_FindClosest(player->s.origin, WALKING_DISTANCE / 4, true);

	// attack button moves node
	if (ai_player_roam.latched_buttons & BUTTON_ATTACK) {
		if (ai_player_roam.last_nodes[0] != NODE_INVALID) {
			ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, ai_player_roam.last_nodes[0]);
			node->position = player->s.origin;
		}
		ai_player_roam.latched_buttons &= ~BUTTON_ATTACK;
	} else if (ai_player_roam.latched_buttons & BUTTON_HOOK) {
		if (ai_player_roam.last_nodes[0] != NODE_INVALID) {
			Ai_Node_Destroy(ai_player_roam.last_nodes[0]);
			ai_player_roam.position = player->s.origin;
			ai_player_roam.last_nodes[0] = ai_player_roam.last_nodes[1] = NODE_INVALID;
		}
		ai_player_roam.latched_buttons &= ~BUTTON_HOOK;
	// if we touched another node and had another node lit up; connect us if we aren't already
	} else if (closest_node != NODE_INVALID && closest_node != ai_player_roam.last_nodes[0] && Ai_Node_Visible(player->s.origin, closest_node)) {

		if (ai_player_roam.last_nodes[0] != NODE_INVALID) {

			Ai_Node_CreateLink(ai_player_roam.last_nodes[0], closest_node);
			Ai_Node_CreateLink(closest_node, ai_player_roam.last_nodes[0]);

			aim.gi->Debug("Connected existing A<->B node\n");
		}
		
		ai_player_roam.last_nodes[1] = ai_player_roam.last_nodes[0];
		ai_player_roam.last_nodes[0] = closest_node;
	// we got far enough from the last node, so drop a new node
	} else if (distance_compare > WALKING_DISTANCE) {

		ai_player_roam.position = player->s.origin;

		const ai_node_id_t id = Ai_Node_Create(player->s.origin);

		if (ai_player_roam.last_nodes[0] != NODE_INVALID) {

			Ai_Node_CreateLink(ai_player_roam.last_nodes[0], id);
			Ai_Node_CreateLink(id, ai_player_roam.last_nodes[0]);
		}
		
		ai_player_roam.last_nodes[1] = ai_player_roam.last_nodes[0];
		ai_player_roam.last_nodes[0] = id;
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

/**
 * @brief The length of space that couldn't be logically travelled by regular means.
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
 * @brief The length of space that couldn't be logically travelled by regular means.
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
 * @brief The length of space that couldn't be logically travelled by regular means.
 */
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
		const _Bool in_path = false;

		aim.gi->WriteByte(SV_CMD_TEMP_ENTITY);
		aim.gi->WriteByte(TE_AI_NODE);
		aim.gi->WritePosition(node->position);
		aim.gi->WriteByte(in_path ? 3 : (ai_player_roam.last_nodes[0] == i) ? 1 : (ai_player_roam.last_nodes[1] == i) ? 2 : 0);
		aim.gi->Multicast(node->position, MULTICAST_PVS, NULL);

		if (node->links) {

			for (guint l = 0; l < node->links->len; l++) {
				const ai_node_id_t link = g_array_index(node->links, ai_node_id_t, l);
				ai_unique_link_t ulink;
				int32_t bit;

				if (link > i) {
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

#define AI_NODE_MAGIC ('Q' | '2' << 8 | 'N' << 16 | 'S' << 24)

#define AI_NODE_VERSION 1

/**
 * @brief 
 */
void Ai_InitNodes(const char *mapname) {

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

	ai_nodes = g_array_sized_new(false, false, sizeof(ai_node_t), num_nodes);
	g_array_set_size(ai_nodes, num_nodes);

	guint total_links = 0;

	for (guint i = 0; i < ai_nodes->len; i++) {
		ai_node_t *node = &g_array_index(ai_nodes, ai_node_t, i);

		aim.gi->ReadFile(file, &node->position, sizeof(node->position), 1);

		guint num_links;

		aim.gi->ReadFile(file, &num_links, sizeof(num_links), 1);

		if (num_links) {
			node->links = g_array_sized_new(false, false, sizeof(ai_node_id_t), num_links);
			g_array_set_size(node->links, num_links);
			aim.gi->ReadFile(file, node->links->data, sizeof(ai_node_id_t), node->links->len);
			total_links += num_links;
		} else {
			node->links = NULL;
		}
	}

	aim.gi->CloseFile(file);
	aim.gi->Print("  Loaded %u nodes with %u total links.\n", num_nodes, total_links);
}

/**
 * @brief 
 */
void Ai_SaveNodes(void) {

	if (!ai_node_dev->integer) {
		aim.gi->Warn("This command only works in `ai_node_dev` mode.\n");
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
			aim.gi->WriteFile(file, node->links->data, sizeof(ai_node_id_t), node->links->len);
		} else {
			guint len = 0;
			aim.gi->WriteFile(file, &len, sizeof(len), 1);
		}
	}

	aim.gi->CloseFile(file);
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

/**
 * @brief
 */
GArray *Ai_Node_FindPath(const ai_node_id_t start, const ai_node_id_t end, const Ai_NodeCost_Func heuristic, const Ai_NodeCost_Func cost) {
	
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
			const ai_node_id_t link = g_array_index(node->links, ai_node_id_t, i);
			ai_node_t *link_node = &g_array_index(ai_nodes, ai_node_t, link);
			const float new_cost = node->cost + cost(current.id, link);

			if (!g_hash_table_lookup(costs_started, link_node) || new_cost < link_node->cost) {

				g_hash_table_add(costs_started, link_node);

				link_node->cost = new_cost;
				const float priority = new_cost + heuristic(link, end);

				if (!queue->len) {
					g_array_insert_vals(queue, 0, &(ai_node_priority_t) {
						.id = link,
						.priority = priority
					}, 1);
				} else {
					for (gint x = queue->len - 1; ; x--) {

						if (priority < g_array_index(queue, ai_node_priority_t, x).priority) {
							g_array_insert_vals(queue, x + 1, &(ai_node_priority_t) {
								.id = link,
								.priority = priority
							}, 1);
							break;
						}

						if (x == 0) {
							g_array_insert_vals(queue, 0, &(ai_node_priority_t) {
								.id = link,
								.priority = priority
							}, 1);
							break;
						}
					}
				}

				link_node->came_from = (node - (ai_node_t *) ai_nodes->data);
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
		}
	} else {
		aim.gi->Debug("Couldn't find path from %u -> %u\n", start, end);
	}

	g_array_free(queue, true);
	g_hash_table_destroy(costs_started);

	return return_path;
}