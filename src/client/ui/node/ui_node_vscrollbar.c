/**
 * @file m_node_vscrollbar.c
 * @todo implement disabled
 * @todo robustness
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
#include "ui_timer.h"
#include "ui_actions.h"
#include "ui_input.h"
#include "ui_render.h"
#include "ui_node_abstractnode.h"
#include "ui_node_abstractscrollbar.h"
#include "ui_node_vscrollbar.h"

#include "client.h"
#include "cl_keys.h"

static const int TILE_WIDTH = 32;
static const int TILE_HEIGHT = 18;
static const int ELEMENT_WIDTH = 19;
static const int ELEMENT_HEIGHT = 16;

static int oldPos;
static int oldMouseY;
static menuTimer_t *capturedTimer;
static int capturedElement;

#define EXTRADATA(node) (node->u.abstractscrollbar)

/**
 * @brief Return size of all elements of the scrollbar
 */
static void MN_VScrollbarNodeGetElementSize (menuNode_t *node, int description[5])
{
	const int cuttableSize = node->size[1] - (ELEMENT_HEIGHT * 4);
	const int low = cuttableSize * ((float)(EXTRADATA(node).pos + 0) / (float)EXTRADATA(node).fullsize);
	const int middle = cuttableSize * ((float)(EXTRADATA(node).viewsize) / (float)EXTRADATA(node).fullsize);
	const int hight = cuttableSize - low - middle;
	description[0] = ELEMENT_HEIGHT;
	description[1] = low;
	description[2] = middle + 2 * ELEMENT_HEIGHT;
	description[3] = hight;
	description[4] = ELEMENT_HEIGHT;
	assert(description[0] + description[1] + description[2] + description[3] + description[4] == node->size[1]);
}

/**
 * @brief Get an element of the scrollbar at a position
 * @param[in] x Absolute position x
 * @param[in] y Absolute position y
 */
static int MN_VScrollbarNodeGetElement (menuNode_t *node, int description[5], int x, int y)
{
	int i;
	MN_NodeAbsoluteToRelativePos(node, &x, &y);
	for (i = 0; i < 5; i++) {
		if (y < description[i])
			return i;
		y -= description[i];
	}
	return -1;
}

/**
 * @brief Set the position of the scrollbar to a value
 */
static void MN_VScrollbarNodeSet (menuNode_t *node, int value)
{
	int pos = value;

	if (pos < 0) {
		pos = 0;
	} else if (pos > EXTRADATA(node).fullsize - EXTRADATA(node).viewsize) {
		pos = EXTRADATA(node).fullsize - EXTRADATA(node).viewsize;
	}

	/* nothing change */
	if (EXTRADATA(node).pos == pos)
		return;

	/* update status */
	EXTRADATA(node).lastdiff = pos - EXTRADATA(node).pos;
	EXTRADATA(node).pos = pos;

	/* fire change event */
	if (node->onChange) {
		MN_ExecuteEventActions(node, node->onChange);
	}
}

/**
 * @brief Translate the position to a value
 */
static inline void MN_VScrollbarNodeDiff (menuNode_t *node, int value)
{
	MN_VScrollbarNodeSet(node, EXTRADATA(node).pos + value);
}

static inline void MN_VScrollbarNodeAction(menuNode_t *node, int hoveredElement, qboolean allowCapture);

static void MN_VScrollbarNodeRepeat (menuNode_t *node, menuTimer_t *timer)
{
	MN_VScrollbarNodeAction(node, capturedElement, qfalse);
	switch (timer->calledTime) {
	case 1:
		timer->delay = 50;
		break;
	}
}

static inline void MN_VScrollbarNodeAction (menuNode_t *node, int hoveredElement, qboolean allowCapture)
{
	switch (hoveredElement) {
	case 0:
		MN_VScrollbarNodeDiff(node, -1);
		if (allowCapture) {
			MN_SetMouseCapture(node);
			capturedElement = hoveredElement;
			capturedTimer = MN_AllocTimer(node, 500, MN_VScrollbarNodeRepeat);
			MN_TimerStart(capturedTimer);
		}
		break;
	case 1:
		MN_VScrollbarNodeDiff(node, -10);
		if (allowCapture) {
			MN_SetMouseCapture(node);
			capturedElement = hoveredElement;
			capturedTimer = MN_AllocTimer(node, 500, MN_VScrollbarNodeRepeat);
			MN_TimerStart(capturedTimer);
		}
		break;
	case 2:
		if (allowCapture) {
			MN_SetMouseCapture(node);
			/* save start value */
			oldMouseY = mousePosY;
			oldPos = EXTRADATA(node).pos;
			capturedElement = hoveredElement;
		}
		break;
	case 3:
		MN_VScrollbarNodeDiff(node, 10);
		if (allowCapture) {
			MN_SetMouseCapture(node);
			capturedElement = hoveredElement;
			capturedTimer = MN_AllocTimer(node, 500, MN_VScrollbarNodeRepeat);
			MN_TimerStart(capturedTimer);
		}
		break;
	case 4:
		MN_VScrollbarNodeDiff(node, 1);
		if (allowCapture) {
			MN_SetMouseCapture(node);
			capturedElement = hoveredElement;
			capturedTimer = MN_AllocTimer(node, 500, MN_VScrollbarNodeRepeat);
			MN_TimerStart(capturedTimer);
		}
		break;
	default:
		assert(qfalse);
	}
}

/**
 * @brief Active an element of a vscrollbarnode.
 * @note This command work like an user, so, if need, change event are fired
 */
static void MN_ActiveVScrollbarNode_f ()
{
	menuNode_t *node;
	int actionId;

	if (Cmd_Argc() != 3) {
		Com_Print("Usage: %s <node-path> <action-id>\n", Cmd_Argv(0));
		return;
	}

	node = MN_GetNodeByPath(Cmd_Argv(1));
	if (node == NULL) {
		Com_Print("MN_ActiveVScrollbarNode_f: node '%s' not found\n", Cmd_Argv(1));
		return;
	}
	if (!MN_NodeInstanceOf(node, "vscrollbar")) {
		Com_Print("MN_ActiveVScrollbarNode_f: node '%s' is not a 'vscrollbar'\n", Cmd_Argv(1));
		return;
	}

	actionId = atoi(Cmd_Argv(2));
	MN_VScrollbarNodeAction(node, actionId, qfalse);
}

static void MN_VScrollbarNodeMouseDown (menuNode_t *node, int x, int y, int button)
{
	int hoveredElement = -1;
	int description[5];

	if (EXTRADATA(node).fullsize == 0 || EXTRADATA(node).fullsize < EXTRADATA(node).viewsize)
		return;
	if (button != K_MOUSE1)
		return;

	MN_VScrollbarNodeGetElementSize(node, description);
	hoveredElement = MN_VScrollbarNodeGetElement(node, description, x, y);
	MN_VScrollbarNodeAction(node, hoveredElement, qtrue);
}

static void MN_VScrollbarNodeMouseUp (menuNode_t *node, int x, int y, int button)
{
	if (EXTRADATA(node).fullsize == 0 || EXTRADATA(node).fullsize < EXTRADATA(node).viewsize)
		return;
	if (button != K_MOUSE1)
		return;

	if (MN_GetMouseCapture() == node) {
		MN_MouseRelease();
	}
}

/**
 * @brief Called when the node have lost the captured node
 * We clean cached data
 */
static void MN_VScrollbarNodeCapturedMouseLost (menuNode_t *node)
{
	if (capturedTimer) {
		MN_TimerRelease(capturedTimer);
		capturedTimer = NULL;
	}
}

/**
 * @brief Called when the user wheel the mouse over the node
 */
static void MN_VScrollbarNodeWheel (menuNode_t *node, qboolean down, int x, int y)
{
	const int diff = (down)?1:-1;

	if (EXTRADATA(node).fullsize == 0 || EXTRADATA(node).fullsize < EXTRADATA(node).viewsize)
		return;

	MN_VScrollbarNodeSet(node, EXTRADATA(node).pos + diff);
}

/**
 * @brief Called when the node is captured by the mouse
 */
static void MN_VScrollbarNodeCapturedMouseMove (menuNode_t *node, int x, int y)
{
	const int posSize = EXTRADATA(node).fullsize;
	const int graphicSize = node->size[1] - (4 * ELEMENT_HEIGHT);
	int pos = 0;

	if (capturedElement != 2)
		return;

	/* compute mouse mouse */
	y -= oldMouseY;

	/* compute pos projection */
	pos = oldPos + (((float)y * (float)posSize) / (float)graphicSize);

	MN_VScrollbarNodeSet(node, pos);
}

/**
 * @brief Call to draw the node
 */
static void MN_VScrollbarNodeDraw (menuNode_t *node)
{
	vec2_t pos;
	int y = 0;
	int texX = 0;
	int texY = 0;
	const char *texture;
	const r_image_t *image;

	MN_GetNodeAbsPos(node, pos);
	y = pos[1];

	texture = MN_GetReferenceString(node, node->image);
	if (!texture)
		return;

	image = MN_LoadImage(texture);

	if (EXTRADATA(node).fullsize == 0 || EXTRADATA(node).fullsize <= EXTRADATA(node).viewsize) {
		/* hide the scrollbar */
		if (EXTRADATA(node).hideWhenUnused)
			return;

		texX = TILE_WIDTH * 3;

		/* top */
		MN_DrawNormImage(pos[0], y, ELEMENT_WIDTH, ELEMENT_HEIGHT,
			texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY,
			ALIGN_UL, image);
		texY += TILE_HEIGHT;
		y += ELEMENT_HEIGHT;

		/* top to bottom */
		MN_DrawNormImage(pos[0], y, ELEMENT_WIDTH, node->size[1] - (ELEMENT_HEIGHT * 2),
			texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY,
			ALIGN_UL, image);
		texY += TILE_HEIGHT * 5;
		y += node->size[1] - (ELEMENT_HEIGHT * 2);
		assert(y == pos[1] + node->size[1] - ELEMENT_HEIGHT);

		/* bottom */
		MN_DrawNormImage(pos[0], y, ELEMENT_WIDTH, ELEMENT_HEIGHT,
			texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY,
			ALIGN_UL, image);

	} else {
		int houveredElement = -1;
		int description[5];
		MN_VScrollbarNodeGetElementSize(node, description);
		if (MN_GetMouseCapture() == node)
			houveredElement = capturedElement;
		else if (node->state)
			houveredElement = MN_VScrollbarNodeGetElement(node, description, mousePosX, mousePosY);

		/* top */
		texX = (houveredElement == 0)?TILE_WIDTH:0;
		MN_DrawNormImage(pos[0], y, ELEMENT_WIDTH, ELEMENT_HEIGHT,
			texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY,
			ALIGN_UL, image);
		texY += TILE_HEIGHT;
		y += ELEMENT_HEIGHT;

		/* top to slider */
		if (description[1]) {
			texX = (houveredElement == 1)?TILE_WIDTH:0;
			MN_DrawNormImage(pos[0], y, ELEMENT_WIDTH, description[1],
				texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY,
				ALIGN_UL, image);
		}
		texY += TILE_HEIGHT;
		y += description[1];

		/* slider */
		texX = (houveredElement == 2)?TILE_WIDTH:0;
		/* top slider */
		MN_DrawNormImage(pos[0], y, ELEMENT_WIDTH, ELEMENT_HEIGHT,
			texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY,
			ALIGN_UL, image);
		texY += TILE_HEIGHT;
		y += ELEMENT_HEIGHT;

		/* middle slider */
		if (description[2]) {
			MN_DrawNormImage(pos[0], y, ELEMENT_WIDTH, description[2]-ELEMENT_HEIGHT-ELEMENT_HEIGHT,
				texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY,
				ALIGN_UL, image);
		}
		texY += TILE_HEIGHT;
		y += description[2]-ELEMENT_HEIGHT-ELEMENT_HEIGHT;

		/* bottom slider */
		MN_DrawNormImage(pos[0], y, ELEMENT_WIDTH, ELEMENT_HEIGHT,
			texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY,
			ALIGN_UL, image);
		texY += TILE_HEIGHT;
		y += ELEMENT_HEIGHT;

		/* slider to bottom */
		if (description[3]) {
			texX = (houveredElement == 3)?TILE_WIDTH:0;
			MN_DrawNormImage(pos[0], y, ELEMENT_WIDTH, description[3],
				texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY,
				ALIGN_UL, image);
		}
		texY += TILE_HEIGHT;
		y += description[3];
		assert(y == pos[1] + node->size[1] - ELEMENT_HEIGHT);

		/* bottom */
		texX = (houveredElement == 4)?TILE_WIDTH:0;
		MN_DrawNormImage(pos[0], y, ELEMENT_WIDTH, ELEMENT_HEIGHT,
			texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY,
			ALIGN_UL, image);
	}

}

static void MN_VScrollbarNodeLoading (menuNode_t *node)
{
	node->size[0] = 19;
}

static void MN_VScrollbarNodeLoaded (menuNode_t *node)
{
#ifdef DEBUG
	if (node->size[1] - (ELEMENT_HEIGHT * 4) < 0)
		Com_Debug("Node '%s' too small. It can create graphical glitches\n", MN_GetPath(node));
#endif
}

void MN_RegisterVScrollbarNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "vscrollbar";
	behaviour->extends = "abstractscrollbar";
	behaviour->mouseWheel = MN_VScrollbarNodeWheel;
	behaviour->mouseDown = MN_VScrollbarNodeMouseDown;
	behaviour->mouseUp = MN_VScrollbarNodeMouseUp;
	behaviour->capturedMouseMove = MN_VScrollbarNodeCapturedMouseMove;
	behaviour->capturedMouseLost = MN_VScrollbarNodeCapturedMouseLost;
	behaviour->draw = MN_VScrollbarNodeDraw;
	behaviour->loaded = MN_VScrollbarNodeLoaded;
	behaviour->loading = MN_VScrollbarNodeLoading;

	Cmd_AddCommand("mn_active_vscrollbar", MN_ActiveVScrollbarNode_f, "Active an element of a scrollbar node, (dummy mouse/user)");
}
