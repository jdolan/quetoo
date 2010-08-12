/**
 * @file m_node_checkbox.c
 * @code
 * checkbox check_item {
 *   cvar "*cvar mn_serverday"
 *   pos  "410 100"
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

#include "ui_nodes.h"
#include "ui_parse.h"
#include "ui_main.h"
#include "ui_actions.h"
#include "ui_render.h"
#include "ui_node_checkbox.h"
#include "ui_node_abstractnode.h"


static void MN_CheckBoxNodeDraw (menuNode_t* node)
{
	const float value = MN_GetReferenceFloat(node, node->u.abstractvalue.value);
	vec2_t pos;
	const char *image = MN_GetReferenceString(node, node->image);
	int texx, texy;

	/* image set? */
	if (!image || image[0] == '\0')
		return;

	/* outer status */
	if (node->disabled) {
		texy = 96;
	} else if (node->state) {
		texy = 32;
	} else {
		texy = 0;
	}

	/* inner status */
	if (value == 0) {
		texx = 0;
	} else if (value > 0) {
		texx = 32;
	} else { /* value < 0 */
		texx = 64;
	}

	MN_GetNodeAbsPos(node, pos);
	MN_DrawNormImageByName(pos[0], pos[1], node->size[0], node->size[1],
		texx + node->size[0], texy + node->size[1], texx, texy, ALIGN_UL, image);
}

/**
 * @brief Handles checkboxes clicks
 */
static void MN_CheckBoxNodeClick (menuNode_t * node, int x, int y)
{
	const float last = MN_GetReferenceFloat(node, node->u.abstractvalue.value);
	float value;

	/* update value */
	value = (last > 0) ? 0 : 1;
	if (last == value)
		return;

	/* save result */
	node->u.abstractvalue.lastdiff = value - last;
	if (!strncmp(node->u.abstractvalue.value, "*cvar", 5)) {
		MN_SetCvar(&((char*)node->u.abstractvalue.value)[6], NULL, value);
	} else {
		*(float*) node->u.abstractvalue.value = value;
	}

	/* fire change event */
	if (node->onChange) {
		MN_ExecuteEventActions(node, node->onChange);
	}
}

/**
 * @brief Handled before the begin of the load of the node from the script
 */
static void MN_CheckBoxNodeLoading (menuNode_t *node)
{
}

void MN_RegisterCheckBoxNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "checkbox";
	behaviour->extends = "abstractvalue";
	behaviour->draw = MN_CheckBoxNodeDraw;
	behaviour->leftClick = MN_CheckBoxNodeClick;
	behaviour->loading = MN_CheckBoxNodeLoading;
}
