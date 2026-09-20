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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "portal.h"
#include "entity.h"
#include "leakfile.h"

/**
 * @brief Finds the shortest possible chain of portals that leads from the
 * outside leaf to a specifically occupied leaf.
 */
void WriteLeakFile(const Tree *tree) {
  Vec3 point;

  char path[MAX_OS_PATH];
  q_snprintf(path, sizeof(path), "maps/%s.lin", mapBase);

  File *file = Fs_OpenWrite(path);
  if (!file) {
    Com_Error(ERROR_FATAL, "Couldn't open %s\n", path);
  }

  const Node *node = &tree->outsideNode;
  while (node->occupied > 1) {
    int32_t occupied = node->occupied;

    const Portal *nextPortal = NULL;
    const Node *nextNode = NULL;

    int32_t s;

    // find the most sparse portal in this node
    for (const Portal *p = node->portals; p; p = p->next[!s]) {
      s = (p->nodes[0] == node);
      if (p->nodes[s]->occupied && p->nodes[s]->occupied < occupied) {
        nextPortal = p;
        nextNode = p->nodes[s];
        occupied = nextNode->occupied;
      }
    }
    node = nextNode;

    // add the portal center
    point = Cm_WindingCenter(nextPortal->winding);
    Fs_Print(file, "%f %f %f\n", point.x, point.y, point.z);
  }
  
  // add the entity origin
  point = VectorForKey(node->occupant, "origin", Vec3_Zero());
  Fs_Print(file, "%f %f %f\n", point.x, point.y, point.z);

  Fs_Close(file);
}
