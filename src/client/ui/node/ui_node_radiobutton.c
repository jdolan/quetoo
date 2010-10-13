/**
 * @file m_node_radiobutton.c
 * @code
 * radiobutton foo {
 *   cvar "*cvar mn_serverday"
 *   value 4
 *   icon boo
 * }
 * @endcode
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

#include "ui_main.h"
#include "ui_icon.h"
#include "ui_parse.h"
#include "ui_input.h"
#include "ui_node_radiobutton.h"
#include "ui_node_abstractnode.h"

#define EPSILON 0.001f

/**
 * @brief Handles RadioButton draw
 * @todo need to implement image. We can't do everything with only one icon (or use nother icon)
 */
static void MN_RadioButtonNodeDraw (menuNode_t *node)
{
	vec2_t pos;
	int status = 0;
	const float current = MN_GetReferenceFloat(node, node->cvar);

	if (node->disabled) {
		status = 2;
	} else if (current > node->extraData1 - EPSILON && current < node->extraData1 + EPSILON) {
		status = 3;
	} else if (node->state) {
		status = 1;
	} else {
		status = 0;
	}

	MN_GetNodeAbsPos(node, pos);

	if (node->icon) {
		MN_DrawIconInBox(node->icon, status, pos[0], pos[1], node->size[0], node->size[1]);
	}
}

/**
 * @brief Handles radio button clicks
 */
static void MN_RadioButtonNodeClick (menuNode_t * node, int x, int y)
{
	float current;

	/* the cvar string is stored in dataModelSkinOrCVar
	 * no cvar given? */
	if (!node->cvar || !*(char*)node->cvar) {
		Com_Print("MN_RadioButtonNodeClick: node '%s' doesn't have a valid cvar assigned\n", MN_GetPath(node));
		return;
	}

	/* its not a cvar! */
	/** @todo the parser should already check that the property value is a right cvar */
	if (strncmp((const char *)node->cvar, "*cvar", 5))
		return;

	current = MN_GetReferenceFloat(node, node->cvar);
	/* Is we click on the action button, we can continue */
	if (current > node->extraData1 - EPSILON && current < node->extraData1 + EPSILON)
		return;

	{
		const char *cvarName = &((const char *)node->cvar)[6];
		MN_SetCvar(cvarName, NULL, node->extraData1);
		if (node->onChange) {
			MN_ExecuteEventActions(node, node->onChange);
		}
	}
}

static const value_t properties[] = {
	{"value", V_FLOAT, offsetof(menuNode_t, extraData1), MEMBER_SIZEOF(menuNode_t, extraData1)},
	{NULL, V_NULL, 0, 0}
};

void MN_RegisterRadioButtonNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "radiobutton";
	behaviour->draw = MN_RadioButtonNodeDraw;
	behaviour->leftClick = MN_RadioButtonNodeClick;
	behaviour->properties = properties;
}
