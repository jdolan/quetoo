/**
 * @file m_node_abstractscrollbar.c
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

#include "m_nodes.h"
#include "m_node_abstractscrollbar.h"

static const value_t properties[] = {
	{"current", V_INT, offsetof(menuNode_t, u.abstractscrollbar.pos),  MEMBER_SIZEOF(menuNode_t, u.abstractscrollbar.pos)},
	{"viewsize", V_INT, offsetof(menuNode_t, u.abstractscrollbar.viewsize),  MEMBER_SIZEOF(menuNode_t, u.abstractscrollbar.viewsize)},
	{"fullsize", V_INT, offsetof(menuNode_t, u.abstractscrollbar.fullsize),  MEMBER_SIZEOF(menuNode_t, u.abstractscrollbar.fullsize)},
	{"lastdiff", V_INT, offsetof(menuNode_t, u.abstractscrollbar.lastdiff),  MEMBER_SIZEOF(menuNode_t, u.abstractscrollbar.lastdiff)},
	{"hidewhenunused", V_BOOL, offsetof(menuNode_t, u.abstractscrollbar.hideWhenUnused),  MEMBER_SIZEOF(menuNode_t, u.abstractscrollbar.hideWhenUnused)},
	{NULL, V_NULL, 0, 0}
};

void MN_RegisterAbstractScrollbarNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "abstractscrollbar";
	behaviour->isAbstract = qtrue;
	behaviour->properties = properties;
}
