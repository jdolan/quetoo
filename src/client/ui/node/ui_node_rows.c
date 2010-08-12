/**
 * @file m_node_panel.c
 * @todo clean up all properties
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

#include "ui_render.h"
#include "ui_node_rows.h"
#include "ui_node_abstractnode.h"

/**
 * @brief Handles Button draw
 */
static void MN_RowsNodeDraw (menuNode_t *node)
{
	int current = 0;
	int i = node->texh[0];
	vec2_t pos;
	MN_GetNodeAbsPos(node, pos);

	while (current < node->size[1]) {
		const float *color;
		if (i % 2)
			color = node->color;
		else
			color = node->selectedColor;
		MN_DrawFill(pos[0], pos[1] + current, node->size[0], node->texh[1], ALIGN_UL, color);
		current += node->texh[1];
		i++;
	}
}

static void MN_RowsNodeLoaded (menuNode_t *node)
{
	/* prevent infinite loop into the draw */
	if (node->texh[1] == 0) {
		node->texh[1] = 10;
	}
}

static const value_t properties[] = {
	{"color1", V_COLOR, offsetof(menuNode_t, color), MEMBER_SIZEOF(menuNode_t, color)},
	{"color2", V_COLOR, offsetof(menuNode_t, selectedColor), MEMBER_SIZEOF(menuNode_t, selectedColor)},
	{"lineheight", V_FLOAT, offsetof(menuNode_t, texh[1]), MEMBER_SIZEOF(menuNode_t, texh[1])},
	{"current", V_FLOAT, offsetof(menuNode_t, texh[0]), MEMBER_SIZEOF(menuNode_t, texh[0])},
	{NULL, V_NULL, 0, 0}
};

void MN_RegisterRowsNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "rows";
	behaviour->draw = MN_RowsNodeDraw;
	behaviour->loaded = MN_RowsNodeLoaded;
	behaviour->properties = properties;
}
