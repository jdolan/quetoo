/**
 * @file m_node_controls.c
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
#include "m_parse.h"
#include "m_input.h"
#include "m_main.h"
#include "m_node_image.h"
#include "m_node_controls.h"
#include "m_node_abstractnode.h"

#include "keys.h"

static int deltaMouseX;
static int deltaMouseY;

static void MN_ControlsNodeMouseDown (menuNode_t *node, int x, int y, int button)
{
	if (button == K_MOUSE1) {
		MN_SetMouseCapture(node);

		/* save position between mouse and menu pos */
		MN_NodeAbsoluteToRelativePos(node, &x, &y);
		deltaMouseX = x + node->pos[0];
		deltaMouseY = y + node->pos[1];
	}
}

static void MN_ControlsNodeMouseUp (menuNode_t *node, int x, int y, int button)
{
	if (button == K_MOUSE1)
		MN_MouseRelease();
}

/**
 * @brief Called when the node is captured by the mouse
 */
static void MN_ControlsNodeCapturedMouseMove (menuNode_t *node, int x, int y)
{
	/* compute new x position of the menu */
	x -= deltaMouseX;
	if (x < 0)
		x = 0;
	if (x + node->root->size[0] > r_state.virtualWidth)
		x = r_state.virtualWidth - node->root->size[0];

	/* compute new y position of the menu */
	y -= deltaMouseY;
	if (y < 0)
		y = 0;
	if (y + node->root->size[1] > r_state.virtualHeight)
		y = r_state.virtualHeight - node->root->size[1];

	MN_SetNewMenuPos(node->root, x, y);
}

void MN_RegisterControlsNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "controls";
	behaviour->extends = "pic";
	behaviour->mouseDown = MN_ControlsNodeMouseDown;
	behaviour->mouseUp = MN_ControlsNodeMouseUp;
	behaviour->capturedMouseMove = MN_ControlsNodeCapturedMouseMove;
}
