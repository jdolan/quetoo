/**
 * @file m_node_abstractnode.h
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef CLIENT_MENU_M_NODE_ABSTRACTNODE_H
#define CLIENT_MENU_M_NODE_ABSTRACTNODE_H

#include "ui_nodes.h"

void MN_RegisterAbstractNode(nodeBehaviour_t *);

struct menuNode_s;

boolean_t MN_NodeInstanceOf(const menuNode_t *node, const char* behaviourName);
boolean_t MN_NodeSetProperty(menuNode_t* node, const value_t *property, const char* value);
float MN_GetFloatFromNodeProperty(const struct menuNode_s* node, const value_t* property);
const char* MN_GetStringFromNodeProperty(const menuNode_t* node, const value_t* property);

/* visibility */
void MN_UnHideNode(struct menuNode_s* node);
void MN_HideNode(struct menuNode_s* node);
void MN_Invalidate(struct menuNode_s* node);

/* position */
void MN_SetNewNodePos(struct menuNode_s* node, int x, int y);
void MN_GetNodeAbsPos(const struct menuNode_s* node, vec2_t pos);
void MN_NodeAbsoluteToRelativePos(const struct menuNode_s* node, int *x, int *y);
void MN_NodeRelativeToAbsolutePoint(const menuNode_t* node, vec2_t pos);
void MN_NodeGetPoint(const menuNode_t* node, vec2_t pos, byte pointDirection);

/* navigation */
struct menuNode_s *MN_GetNode(const struct menuNode_s* const node, const char *name);
void MN_InsertNode(struct menuNode_s* const node, struct menuNode_s *prevNode, struct menuNode_s *newNode);
void MN_AppendNode(struct menuNode_s* const node, struct menuNode_s *newNode);

#endif
