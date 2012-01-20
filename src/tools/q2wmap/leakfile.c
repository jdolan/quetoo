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

/*
 * LeakFile
 *
 * Finds the shortest possible chain of portals that leads from the 
 * outside leaf to a specifically occupied leaf.
 */
void LeakFile(tree_t *tree) {
	vec3_t mid;
	FILE *leakfile;
	char file_name[MAX_OSPATH];
	node_t *node;
	int count;

	if (!tree->outside_node.occupied)
		return;

	Com_Print("--- LeakFile ---\n");

	// write the points to the file
	StripExtension(map_name, file_name);
	strcat(file_name, ".lin");

	if (Fs_OpenFile(file_name, &leakfile, FILE_WRITE) == -1)
		Com_Error(ERR_FATAL, "Couldn't open %s\n", file_name);

	count = 0;
	node = &tree->outside_node;
	while (node->occupied > 1) {
		int next;
		portal_t *p, *nextportal = NULL;
		node_t *nextnode = NULL;
		int s;

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
		fprintf(leakfile, "%f %f %f\n", mid[0], mid[1], mid[2]);
		count++;
	}
	// add the occupant center
	GetVectorForKey(node->occupant, "origin", mid);

	fprintf(leakfile, "%f %f %f\n", mid[0], mid[1], mid[2]);
	Com_Debug("%5i point leakfile\n", count + 1);

	Fs_CloseFile(leakfile);
}
