/**
 * @file m_node_special.c
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
#include "m_timer.h"
#include "m_actions.h"
#include "m_node_zone.h"
#include "m_node_window.h"

#include "keys.h"

static menuTimer_t *capturedTimer;

static void MN_ZoneNodeRepeat (menuNode_t *node, menuTimer_t *timer)
{
	if (node->onClick) {
		MN_ExecuteEventActions(node, node->onClick);
	}
}

static void MN_ZoneNodeDown (menuNode_t *node, int x, int y, int button)
{
	if (!node->repeat)
		return;
	if (button == K_MOUSE1) {
		MN_SetMouseCapture(node);
		capturedTimer = MN_AllocTimer(node, node->clickDelay, MN_ZoneNodeRepeat);
		MN_TimerStart(capturedTimer);
	}
}

static void MN_ZoneNodeUp (menuNode_t *node, int x, int y, int button)
{
	if (!node->repeat)
		return;
	if (button == K_MOUSE1) {
		MN_MouseRelease();
	}
}

/**
 * @brief Called when the node have lost the captured node
 * We clean cached data
 */
static void MN_ZoneNodeCapturedMouseLost (menuNode_t *node)
{
	if (capturedTimer) {
		MN_TimerRelease(capturedTimer);
		capturedTimer = NULL;
	}
}

/**
 * @brief Call before the script initialized the node
 */
static void MN_ZoneNodeLoading (menuNode_t *node)
{
	node->clickDelay = 1000;
}

/**
 * @brief Call after the script initialized the node
 */
static void MN_ZoneNodeLoaded (menuNode_t *node)
{
	menuNode_t * root = node->root;
	if (!strncmp(node->name, "render", 6)) {
		if (!root->u.window.renderNode)
			root->u.window.renderNode = node;
		else
			Com_Printf("MN_ZoneNodeLoaded: second render node ignored (\"%s\")\n", MN_GetPath(node));
	}
}

static const value_t properties[] = {
	{"repeat", V_BOOL, offsetof(menuNode_t, repeat), MEMBER_SIZEOF(menuNode_t, repeat)},
	{"clickdelay", V_INT, offsetof(menuNode_t, clickDelay), MEMBER_SIZEOF(menuNode_t, clickDelay)},
	/** @todo delete it went its possible */
	{"mousefx", V_BOOL, offsetof(menuNode_t, mousefx), MEMBER_SIZEOF(menuNode_t, mousefx)},
	{NULL, V_NULL, 0, 0}
};

void MN_RegisterZoneNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "zone";
	behaviour->loading = MN_ZoneNodeLoading;
	behaviour->loaded = MN_ZoneNodeLoaded;
	behaviour->mouseDown = MN_ZoneNodeDown;
	behaviour->mouseUp = MN_ZoneNodeUp;
	behaviour->capturedMouseLost = MN_ZoneNodeCapturedMouseLost;
	behaviour->properties = properties;
}
