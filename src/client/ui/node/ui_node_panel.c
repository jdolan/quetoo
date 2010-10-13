/**
 * @file m_node_panel.c
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
#include "ui_parse.h"
#include "ui_render.h"
#include "ui_node_abstractnode.h"
#include "ui_node_panel.h"

#define CORNER_SIZE 25
#define MID_SIZE 1
#define MARGE 3

/**
 * @brief Handles Button draw
 */
static void MN_PanelNodeDraw (menuNode_t *node)
{
	static const int panelTemplate[] = {
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		MARGE
	};
	const char *image;
	vec2_t pos;

	MN_GetNodeAbsPos(node, pos);

	image = MN_GetReferenceString(node, node->image);
	if (image)
		MN_DrawPanel(pos, node->size, image, 0, 0, panelTemplate);
}

/**
 * @brief Handled after the end of the load of the node from script (all data and/or child are set)
 */
static void MN_PanelNodeLoaded (menuNode_t *node)
{
#ifdef DEBUG
	if (node->size[0] < CORNER_SIZE + MID_SIZE + CORNER_SIZE || node->size[1] < CORNER_SIZE + MID_SIZE + CORNER_SIZE)
		Com_Debug("Node '%s' too small. It can create graphical glitches\n", MN_GetPath(node));
#endif
}

void MN_RegisterPanelNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "panel";
	behaviour->draw = MN_PanelNodeDraw;
	behaviour->loaded = MN_PanelNodeLoaded;
}
