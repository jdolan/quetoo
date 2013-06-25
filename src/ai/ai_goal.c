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

#include "ai_local.h"

typedef struct {
	GList *nodes;
} ai_node_state_t;

static ai_node_state_t ai_node_state;

/*
 * @brief Utility function for instantiating ai_node_link_t.
 */
static ai_node_link_t *Ai_CreateNodeLink(ai_node_t *from, ai_node_t *to) {
	ai_node_link_t *link = Z_LinkMalloc(sizeof(*link), from);

	link->to = to;

	// append the link to the list

	from->links = g_list_prepend(from->links, to);

	return link;
}

/*
 * @brief Utility function for instantiating ai_node_t.
 */
static ai_node_t *Ai_CreateNode(const ai_node_type_t type, const vec3_t origin) {
	ai_node_t *node = Z_TagMalloc(sizeof(*node), Z_TAG_AI);

	node->type = type;
	VectorCopy(origin, node->origin);

	// create the hash table for this node's links
	node->links = g_hash_table_new(g_direct_hash, g_direct_equal);

	// append the node to the list
	ai_node_state.nodes = g_list_prepend(ai_node_state.nodes, node);

	return node;
}

/*
 * @brief Creates paths for neighboring nodes.
 */
static void Ai_CreateNodes_CreatePaths(gpointer data, gpointer user_data __attribute__((unused))) {
	ai_node_t *node = (ai_node_t *) data;

	GList *e = ai_node_state.nodes;
	while (e) {
		ai_node_t *n = (ai_node_t *) e->data;

		if (node != n) {
			if (gi.inPVS(node->origin, n->origin)) {
				ai_node_link_t *l = Ai_CreateNodeLink(node, n);
			}
		}

		e = e->next;
	}
}

/*
 * @brief GDestroyNotify for ai_node_t.
 */
static void Ai_FreeNode(gpointer *data) {
	ai_node_t *node = (ai_node_t *) data;

	g_hash_table_destroy(node->links);

	Z_Free(node);
}

/*
 * @brief Creates nodes and
 *
 */
void Ai_CreateNodes(const char *bsp_name) {

	int32_t i;

	g_list_free_full(ai_node_state.nodes, Ai_FreeNode);

	memset(&ai_node_state, 0, sizeof(ai_node_state));

	g_list_foreach(ai_node_state.nodes, Ai_CreateNodes_CreatePaths, NULL);
}
