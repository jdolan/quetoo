/**
 * @file m_draw.c
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
#include "ui_nodes.h"
#include "ui_internal.h"
#include "ui_draw.h"
#include "ui_actions.h"
#include "ui_input.h"
#include "ui_timer.h" /* define MN_HandleTimers */
#include "ui_render.h"
#include "node/ui_node_abstractnode.h"

#include "client.h"

#ifdef DEBUG
static cvar_t *mn_debug;
#endif

static int msgTime;
static char msgText[256];

/**
 * @brief Node we will draw over
 * @sa MN_CaptureDrawOver
 * @sa nodeBehaviour_t.drawOverMenu
 */
static menuNode_t *drawOverNode;

/**
 * @brief Capture a node we will draw over all nodes per menu
 * @note The node must be captured every frames
 * @todo it can be better to capture the draw over only one time (need new event like mouseEnter, mouseLeave)
 */
void MN_CaptureDrawOver (menuNode_t *node)
{
	drawOverNode = node;
}

#ifdef DEBUG

static int debugTextPositionY = 0;
static int debugPositionX = 0;
#define DEBUG_PANEL_WIDTH 300

static void MN_HighlightNode (const menuNode_t *node, const vec4_t color)
{
	static const vec4_t grey = {0.7, 0.7, 0.7, 1.0};
	vec2_t pos;
	int width;
	int lineDefinition[4];
	const char* text;

	if (node->parent)
		MN_HighlightNode(node->parent, grey);

	MN_GetNodeAbsPos(node, pos);

	text = va("%s (%s)", node->name, node->behaviour->name);
	R_FontTextSize("f_small_bold", text, DEBUG_PANEL_WIDTH, LONGLINES_PRETTYCHOP, &width, NULL, NULL, NULL);

	R_Color(color);
	MN_DrawString("f_small_bold", ALIGN_UL, debugPositionX + 20, debugTextPositionY, debugPositionX + 20, debugTextPositionY, DEBUG_PANEL_WIDTH, DEBUG_PANEL_WIDTH, 0, text, 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
	debugTextPositionY += 15;

	if (debugPositionX != 0) {
		lineDefinition[0] = debugPositionX + 20;
		lineDefinition[2] = pos[0] + node->size[0];
	} else {
		lineDefinition[0] = debugPositionX + 20 + width;
		lineDefinition[2] = pos[0];
	}
	lineDefinition[1] = debugTextPositionY - 5;
	lineDefinition[3] = pos[1];
	R_DrawLine(lineDefinition, 1);
	R_Color(NULL);

	/* exclude rect */
	if (node->excludeRectNum) {
		int i;
		vec4_t trans = {1, 1, 1, 1};
		Vector4Copy(color, trans);
		trans[3] = trans[3] / 2;
		for (i = 0; i < node->excludeRectNum; i++) {
			const int x = pos[0] + node->excludeRect[i].pos[0];
			const int y = pos[1] + node->excludeRect[i].pos[1];
			MN_DrawFill(x, y, node->excludeRect[i].size[0], node->excludeRect[i].size[1], ALIGN_UL, trans);
		}
	}

	/* bounded box */
	MN_DrawRect(pos[0] - 1, pos[1] - 1, node->size[0] + 2, node->size[1] + 2, color, 2.0, 0x3333);
}

/**
 * @brief Prints active node names for debugging
 */
static void MN_DrawDebugMenuNodeNames (void)
{
	static const vec4_t red = {1.0, 0.0, 0.0, 1.0};
	static const vec4_t green = {0.0, 0.5, 0.0, 1.0};
	static const vec4_t white = {1, 1.0, 1.0, 1.0};
	static const vec4_t background = {0.0, 0.0, 0.0, 0.5};
	menuNode_t *hoveredNode;
	int stackPosition;

	debugTextPositionY = 100;

	/* x panel position with hysteresis */
	if (mousePosX < viddef.virtual_width / 3)
		debugPositionX = viddef.virtual_width - DEBUG_PANEL_WIDTH;
	if (mousePosX > 2 * viddef.virtual_width / 3)
		debugPositionX = 0;

	/* background */
	MN_DrawFill(debugPositionX, debugTextPositionY, DEBUG_PANEL_WIDTH, VID_NORM_HEIGHT - debugTextPositionY - 100, ALIGN_UL, background);

	/* menu stack */
	R_Color(white);
	MN_DrawString("f_small_bold", ALIGN_UL, debugPositionX, debugTextPositionY, debugPositionX, debugTextPositionY, 200, 200, 0, "menu stack:", 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
	debugTextPositionY += 15;
	for (stackPosition = 0; stackPosition < mn.menuStackPos; stackPosition++) {
		menuNode_t *menu = mn.menuStack[stackPosition];
		MN_DrawString("f_small_bold", ALIGN_UL, debugPositionX+20, debugTextPositionY, debugPositionX+20, debugTextPositionY, 200, 200, 0, menu->name, 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
		debugTextPositionY += 15;
	}

	/* hovered node */
	hoveredNode = MN_GetHoveredNode();
	if (hoveredNode) {
		MN_DrawString("f_small_bold", ALIGN_UL, debugPositionX, debugTextPositionY, debugPositionX, debugTextPositionY, 200, 200, 0, "-----------------------", 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
		debugTextPositionY += 15;

		MN_DrawString("f_small_bold", ALIGN_UL, debugPositionX, debugTextPositionY, debugPositionX, debugTextPositionY, 200, 200, 0, "hovered node:", 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
		debugTextPositionY += 15;
		MN_HighlightNode(hoveredNode, red);
		R_Color(white);
	}

	R_Color(NULL);
}
#endif

static void MN_DrawNode (menuNode_t *node)
{
	menuNode_t *child;

	/* skip invisible, virtual, and undrawable nodes */
	if (node->invis || node->behaviour->isVirtual)
		return;
	/* if construct */
	if (!MN_CheckVisibility(node))
		return;

	/** @todo remove it when its possible:
	 * we can create a 'box' node with this properties,
	 * but we often don't need it */
	/* check node size x and y value to check whether they are zero */
	if (node->size[0] && node->size[1]) {
		vec2_t pos;
		MN_GetNodeAbsPos(node, pos);
		if (node->bgcolor[3] != 0)
			MN_DrawFill(pos[0], pos[1], node->size[0], node->size[1], ALIGN_UL, node->bgcolor);

		if (node->border && node->bordercolor[3] != 0) {
			MN_DrawRect(pos[0], pos[1], node->size[0], node->size[1],
				node->bordercolor, node->border, 0xFFFF);
		}
	}

	/* draw the node */
	if (node->behaviour->draw) {
		node->behaviour->draw(node);
	}

	/* draw all child */
	for (child = node->firstChild; child; child = child->next) {
		MN_DrawNode(child);
	}
}

/**
 * @brief Generic notice function that renders a message to the screen
 */
static int MN_DrawNotice (int x, int y, const char *noticeText)
{
	const vec4_t noticeBG = { 1.0f, 0.0f, 0.0f, 0.2f };
	const vec4_t noticeColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	int height = 0, width = 0;
	const int maxWidth = 320;
	const int maxHeight = 100;
	const char *font = "f_normal";
	int lines = 5;
	int dx; /**< Delta-x position. Relative to original x position. */

	R_FontTextSize(font, noticeText, maxWidth, LONGLINES_WRAP, &width, &height, NULL, NULL);

	if (!width)
		return 0;

	if (x + width + 3 > VID_NORM_WIDTH)
		dx = -(width + 10);
	else
		dx = 0;

	MN_DrawFill(x - 1 + dx, y - 1, width + 4, height + 4, ALIGN_UL, noticeBG);
	R_Color(noticeColor);
	MN_DrawString(font, 0, x + 1 + dx, y + 1, x + 1, y + 1, maxWidth, maxHeight, 0, noticeText, lines, 0, NULL, qfalse, LONGLINES_WRAP);
	R_Color(NULL);

	return width;
}

/**
 * @brief Draws the menu stack
 * @sa SCR_UpdateScreen
 */
void MN_Draw (void)
{
	menuNode_t *hoveredNode;
	menuNode_t *menu;
	int pos;
	qboolean mouseMoved = qfalse;

	MN_HandleTimers();

	assert(mn.menuStackPos >= 0);

	mouseMoved = MN_CheckMouseMove();
	hoveredNode = MN_GetHoveredNode();

	/* under a fullscreen, menu should not be visible */
	pos = MN_GetLastFullScreenWindow();
	if (pos < 0)
		return;

	/* draw all visible menus */
	for (; pos < mn.menuStackPos; pos++) {
		menu = mn.menuStack[pos];

		/* update the layout */
		menu->behaviour->doLayout(menu);

		drawOverNode = NULL;

		MN_DrawNode(menu);

		/* draw a node over the menu */
		if (drawOverNode && drawOverNode->behaviour->drawOverMenu) {
			drawOverNode->behaviour->drawOverMenu(drawOverNode);
		}
	}

	/* draw a special notice */
	menu = MN_GetActiveMenu();
	if (cl.time < msgTime) {
		if (menu && (menu->u.window.noticePos[0] || menu->u.window.noticePos[1]))
			MN_DrawNotice(menu->u.window.noticePos[0], menu->u.window.noticePos[1], msgText);
		else
			MN_DrawNotice(500, 110, msgText);
	}

#ifdef DEBUG
	/* debug information */
	if (mn_debug->integer == 2) {
		MN_DrawDebugMenuNodeNames();
	}
#endif
}

void MN_DrawCursor (void)
{
}

/**
 * @brief Displays a message over all menus.
 * @sa HUD_DisplayMessage
 * @param[in] time is a ms values
 * @param[in] text text is already translated here
 */
void MN_DisplayNotice (const char *text, int time)
{
	msgTime = cl.time + time;
	Q_strncpyz(msgText, text, sizeof(msgText));
}

void MN_InitDraw (void)
{
#ifdef DEBUG
	mn_debug = Cvar_Get("debug_menu", "0", 0, "Prints node names for debugging purposes - valid values are 1 and 2");
#endif
}
