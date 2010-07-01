/**
 * @file m_node_abstractvalue.c
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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
#include "m_parse.h"
#include "m_node_abstractvalue.h"

static const nodeBehaviour_t const *localBehaviour;

static inline void MN_InitCvarOrFloat (float** adress, float defaultValue)
{
	if (*adress == NULL) {
		*adress = MN_AllocFloat(1);
		**adress = defaultValue;
	}
}

static void MN_AbstractValueLoaded (menuNode_t * node)
{
	MN_InitCvarOrFloat((float**)&node->u.abstractvalue.value, 0);
	MN_InitCvarOrFloat((float**)&node->u.abstractvalue.delta, 1);
	MN_InitCvarOrFloat((float**)&node->u.abstractvalue.max, 0);
	MN_InitCvarOrFloat((float**)&node->u.abstractvalue.min, 0);
}

static void MN_CloneCvarOrFloat (const float*const* source, float** clone)
{
	/* dont update cvar */
	if (!strncmp(*(const char*const*)source, "*cvar", 5))
		return;

	/* clone float */
	*clone = MN_AllocFloat(1);
	**clone = **source;
}

/**
 * @brief Call to update a cloned node
 */
static void MN_AbstractValueClone (const menuNode_t *source, menuNode_t *clone)
{
	localBehaviour->super->clone(source, clone);
	MN_CloneCvarOrFloat((const float*const*)&source->u.abstractvalue.value, (float**)&clone->u.abstractvalue.value);
	MN_CloneCvarOrFloat((const float*const*)&source->u.abstractvalue.delta, (float**)&clone->u.abstractvalue.delta);
	MN_CloneCvarOrFloat((const float*const*)&source->u.abstractvalue.max, (float**)&clone->u.abstractvalue.max);
	MN_CloneCvarOrFloat((const float*const*)&source->u.abstractvalue.min, (float**)&clone->u.abstractvalue.min);
}

static const value_t properties[] = {
	{"current", V_CVAR_OR_FLOAT, offsetof(menuNode_t, u.abstractvalue.value), 0},
	{"delta", V_CVAR_OR_FLOAT, offsetof(menuNode_t, u.abstractvalue.delta), 0},
	{"max", V_CVAR_OR_FLOAT, offsetof(menuNode_t, u.abstractvalue.max), 0},
	{"min", V_CVAR_OR_FLOAT, offsetof(menuNode_t, u.abstractvalue.min), 0},
	{"lastdiff", V_FLOAT, offsetof(menuNode_t, u.abstractvalue.lastdiff), MEMBER_SIZEOF(menuNode_t, u.abstractvalue.lastdiff)},
	{NULL, V_NULL, 0, 0}
};

void MN_RegisterAbstractValueNode (nodeBehaviour_t *behaviour)
{
	localBehaviour = behaviour;
	behaviour->name = "abstractvalue";
	behaviour->loaded = MN_AbstractValueLoaded;
	behaviour->clone = MN_AbstractValueClone;
	behaviour->isAbstract = qtrue;
	behaviour->properties = properties;
}
