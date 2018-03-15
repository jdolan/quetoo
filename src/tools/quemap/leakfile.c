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

#include "qbsp.h"

/**
 * @brief Finds the shortest possible chain of portals that leads from the
 * outside leaf to a specifically occupied leaf.
 */
void LeakFile(tree_t *tree) {
	vec3_t mid;
	file_t *leakfile;
	char filename[MAX_OS_PATH];
	node_t *node;
	int32_t count;

	if (!tree->outside_node.occupied) {
		return;
	}

	Com_Print("--- LeakFile ---\n");

	g_snprintf(filename, sizeof(filename), "maps/%s.lin", map_base);

	if (!(leakfile = Fs_OpenWrite(filename))) {
		Com_Error(ERROR_FATAL, "Couldn't open %s\n", filename);
	}

	count = 0;
	node = &tree->outside_node;
	while (node->occupied > 1) {
		int32_t next;
		portal_t *p, *nextportal = NULL;
		node_t *nextnode = NULL;
		int32_t s;

		// find the best portal exit
		next = node->occupied;
		for (p = node->portals; p; p = p->next[!s]) {
			s = (p->nodes[0] == node);
			if (p->nodes[s]->occupied && p->nodes[s]->occupied < next) {
				nextportal = p;
				nextnode = p->nodes[s];
				next = nextnode->occupied;
			}
		}
		node = nextnode;
		WindingCenter(nextportal->winding, mid);
		Fs_Print(leakfile, "%f %f %f\n", mid[0], mid[1], mid[2]);
		count++;
	}
	
	// add the occupant center
	VectorForKey(node->occupant, "origin", mid, NULL);

	Fs_Print(leakfile, "%f %f %f\n", mid[0], mid[1], mid[2]);
	Com_Debug(DEBUG_ALL, "%5i point leakfile\n", count + 1);

	Fs_Close(leakfile);
}
