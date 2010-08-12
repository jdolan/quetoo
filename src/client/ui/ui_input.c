/**
 * @file m_input.c
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
#include "ui_internal.h"
#include "ui_actions.h"
#include "ui_input.h"
#include "ui_nodes.h"
#include "ui_draw.h"

#include "keys.h"

/**
 * @brief save the node with the focus
 */
static menuNode_t* focusNode;

/**
 * @brief save the current hovered node (first node under the mouse)
 * @sa MN_GetHoveredNode
 * @sa MN_MouseMove
 * @sa MN_CheckMouseMove
 */
static menuNode_t* hoveredNode;

/**
 * @brief save the previous hovered node
 */
static menuNode_t *oldHoveredNode;

/**
 * @brief save old position of the mouse
 */
static int oldMousePosX, oldMousePosY;

/**
 * @brief save the captured node
 * @sa MN_SetMouseCapture
 * @sa MN_GetMouseCapture
 * @sa MN_MouseRelease
 * @todo think about replacing it by a boolean. When capturedNode != NULL => hoveredNode == capturedNode
 * it create unneed case
 */
static menuNode_t* capturedNode;

/**
 * @brief Execute the current focused action node
 * @note Action nodes are nodes with click defined
 * @sa Key_Event
 * @sa MN_FocusNextActionNode
 */
static qboolean MN_FocusExecuteActionNode (void)
{
#if 0	/**< @todo need a cleanup */
	if (mouseSpace != MS_MENU)
		return qfalse;

	if (MN_GetMouseCapture())
		return qfalse;

	if (focusNode) {
		if (focusNode->onClick) {
			MN_ExecuteEventActions(focusNode, focusNode->onClick);
		}
		MN_ExecuteEventActions(focusNode, focusNode->onMouseLeave);
		focusNode = NULL;
		return qtrue;
	}
#endif
	return qfalse;
}

#if 0	/**< @todo need a cleanup */
/**
 * @sa MN_FocusExecuteActionNode
 * @note node must not be in menu
 */
static menuNode_t *MN_GetNextActionNode (menuNode_t* node)
{
	if (node)
		node = node->next;
	while (node) {
		if (MN_CheckVisibility(node) && !node->invis
		 && ((node->onClick && node->onMouseEnter) || node->onMouseEnter))
			return node;
		node = node->next;
	}
	return NULL;
}
#endif

/**
 * @brief Set the focus to the next action node
 * @note Action nodes are nodes with click defined
 * @sa Key_Event
 * @sa MN_FocusExecuteActionNode
 */
static qboolean MN_FocusNextActionNode (void)
{
#if 0	/**< @todo need a cleanup */
	menuNode_t* menu;
	static int i = MAX_MENUSTACK + 1;	/* to cycle between all menus */

	if (mouseSpace != MS_MENU)
		return qfalse;

	if (MN_GetMouseCapture())
		return qfalse;

	if (i >= mn.menuStackPos)
		i = MN_GetLastFullScreenWindow();

	assert(i >= 0);

	if (focusNode) {
		menuNode_t *node = MN_GetNextActionNode(focusNode);
		if (node)
			return MN_FocusSetNode(node);
	}

	while (i < mn.menuStackPos) {
		menu = mn.menuStack[i++];
		if (MN_FocusSetNode(MN_GetNextActionNode(menu->firstChild)))
			return qtrue;
	}
	i = MN_GetLastFullScreenWindow();

	/* no node to focus */
	MN_RemoveFocus();
#endif
	return qfalse;
}

/**
 * @brief request the focus for a node
 */
void MN_RequestFocus (menuNode_t* node)
{
	menuNode_t* tmp;
	assert(node);
	if (node == focusNode)
		return;

	/* invalidate the data before calling the event */
	tmp = focusNode;
	focusNode = NULL;

	/* lost the focus */
	if (tmp && tmp->behaviour->focusLost) {
		tmp->behaviour->focusLost(tmp);
	}

	/* get the focus */
	focusNode = node;
	if (focusNode->behaviour->focusGained) {
		focusNode->behaviour->focusGained(focusNode);
	}
}

/**
 * @brief check if a node got the focus
 */
qboolean MN_HasFocus (const menuNode_t* node)
{
	return node == focusNode;
}

/**
 * @sa MN_FocusExecuteActionNode
 * @sa MN_FocusNextActionNode
 * @sa IN_Frame
 * @sa Key_Event
 */
void MN_RemoveFocus (void)
{
	menuNode_t* tmp;

	if (MN_GetMouseCapture())
		return;

	if (!focusNode)
		return;

	/* invalidate the data before calling the event */
	tmp = focusNode;
	focusNode = NULL;

	/* callback the lost of the focus */
	if (tmp->behaviour->focusLost) {
		tmp->behaviour->focusLost(tmp);
	}
}

/**
 * @brief Called by the client when the user type a key
 * @param[in] key key code, either K_ value or lowercase ascii
 * @param[in] unicode translated meaning of keypress in unicode
 * @return qtrue, if we used the event
 */
qboolean MN_KeyPressed (unsigned int key, unsigned short unicode)
{
	/* translate event into the node with focus */
	if (focusNode && focusNode->behaviour->keyPressed) {
		if (focusNode->behaviour->keyPressed(focusNode, key, unicode))
			return qtrue;
	}

	/* else use common behaviour */
	switch (key) {
	case K_TAB:
		if (MN_FocusNextActionNode())
			return qtrue;
		break;
	case K_ENTER:
	case K_KP_ENTER:
		if (MN_FocusExecuteActionNode())
			return qtrue;
		break;
	case K_ESCAPE:
		MN_PopMenuWithEscKey();
		return qtrue;
	}

	return qfalse;
}

/**
 * @brief Release all captured input (keyboard or mouse)
 */
void MN_ReleaseInput (void)
{
	MN_RemoveFocus();
	MN_MouseRelease();
}

/**
 * @brief Return the captured node
 * @return Return a node, else NULL
 */
menuNode_t* MN_GetMouseCapture (void)
{
	return capturedNode;
}

/**
 * @brief Captured the mouse into a node
 */
void MN_SetMouseCapture (menuNode_t* node)
{
	assert(capturedNode == NULL);
	assert(node != NULL);
	capturedNode = node;
}

/**
 * @brief Release the captured node
 */
void MN_MouseRelease (void)
{
	menuNode_t *tmp = capturedNode;

	if (capturedNode == NULL)
		return;

	capturedNode = NULL;
	if (tmp->behaviour->capturedMouseLost)
		tmp->behaviour->capturedMouseLost(tmp);

	MN_InvalidateMouse();
}

/**
 * @brief Get the current hovered node
 * @return A node, else NULL if the mouse hover nothing
 */
menuNode_t *MN_GetHoveredNode (void)
{
	return hoveredNode;
}

/**
 * @brief Force to invalidate the mouse position and then to update hover nodes...
 */
void MN_InvalidateMouse (void)
{
	oldMousePosX = -1;
	oldMousePosY = -1;
}

/**
 * @brief Call mouse move only if the mouse position change
 */
qboolean MN_CheckMouseMove (void)
{
	/* is hovered node no more draw */
	if (hoveredNode && (hoveredNode->invis || !MN_CheckVisibility(hoveredNode)))
		MN_InvalidateMouse();

	if (mousePosX != oldMousePosX || mousePosY != oldMousePosY) {
		oldMousePosX = mousePosX;
		oldMousePosY = mousePosY;
		MN_MouseMove(mousePosX, mousePosY);
		return qtrue;
	}

	return qfalse;
}

/**
 * @brief Is called every time the mouse position change
 */
void MN_MouseMove (int x, int y)
{
	/* send the captured move mouse event */
	if (capturedNode) {
		if (capturedNode->behaviour->capturedMouseMove)
			capturedNode->behaviour->capturedMouseMove(capturedNode, x, y);
		return;
	}

	hoveredNode = MN_GetNodeAtPosition(x, y);

	/* update nodes: send 'in' and 'out' event */
	if (oldHoveredNode != hoveredNode) {
		if (oldHoveredNode) {
			MN_ExecuteEventActions(oldHoveredNode, oldHoveredNode->onMouseLeave);
			oldHoveredNode->state = qfalse;
		}
		if (hoveredNode) {
			hoveredNode->state = qtrue;
			MN_ExecuteEventActions(hoveredNode, hoveredNode->onMouseEnter);
		}
	}
	oldHoveredNode = hoveredNode;

	/* send the move event */
	if (hoveredNode && hoveredNode->behaviour->mouseMove) {
		hoveredNode->behaviour->mouseMove(hoveredNode, x, y);
	}
}

/**
 * @brief Is called every time one clicks on a menu/screen. Then checks if anything needs to be executed in the area of the click
 * (e.g. button-commands, inventory-handling, geoscape-stuff, etc...)
 * @sa MN_ExecuteEventActions
 * @sa MN_RightClick
 * @sa Key_Message
 */
static void MN_LeftClick (int x, int y)
{
	/* send it to the captured mouse node */
	if (capturedNode) {
		if (capturedNode->behaviour->leftClick)
			capturedNode->behaviour->leftClick(capturedNode, x, y);
		return;
	}

	/* if we click out side a dropdown menu, we close it */
	/** @todo need to refactoring it with a the focus code (cleaner) */
	/** @todo at least should be moved on the mouse down event (when the focus should change) */
	if (!hoveredNode && mn.menuStackPos != 0) {
		if (mn.menuStack[mn.menuStackPos - 1]->u.window.dropdown) {
			MN_PopMenu(qfalse);
		}
	}

	if (hoveredNode && !hoveredNode->disabled) {
		if (hoveredNode->behaviour->leftClick) {
			hoveredNode->behaviour->leftClick(hoveredNode, x, y);
		} else {
			MN_ExecuteEventActions(hoveredNode, hoveredNode->onClick);
		}
	}
}

/**
 * @sa MAP_ResetAction
 * @sa MN_ExecuteEventActions
 * @sa MN_LeftClick
 * @sa MN_MiddleClick
 * @sa MN_MouseWheel
 */
static void MN_RightClick (int x, int y)
{
	/* send it to the captured mouse node */
	if (capturedNode) {
		if (capturedNode->behaviour->rightClick)
			capturedNode->behaviour->rightClick(capturedNode, x, y);
		return;
	}

	if (hoveredNode && !hoveredNode->disabled) {
		if (hoveredNode->behaviour->rightClick) {
			hoveredNode->behaviour->rightClick(hoveredNode, x, y);
		} else {
			MN_ExecuteEventActions(hoveredNode, hoveredNode->onRightClick);
		}
	}
}

/**
 * @sa MN_LeftClick
 * @sa MN_RightClick
 * @sa MN_MouseWheel
 */
static void MN_MiddleClick (int x, int y)
{
	/* send it to the captured mouse node */
	if (capturedNode) {
		if (capturedNode->behaviour->middleClick)
			capturedNode->behaviour->middleClick(capturedNode, x, y);
		return;
	}

	if (hoveredNode && !hoveredNode->disabled) {
		if (hoveredNode->behaviour->middleClick) {
			hoveredNode->behaviour->middleClick(hoveredNode, x, y);
		} else {
			MN_ExecuteEventActions(hoveredNode, hoveredNode->onMiddleClick);
		}
		return;
	}
}

/**
 * @brief Called when we are in menu mode and scroll via mousewheel
 * @note The geoscape zooming code is here, too (also in @see CL_ParseInput)
 * @sa MN_LeftClick
 * @sa MN_RightClick
 * @sa MN_MiddleClick
 * @sa CL_ZoomInQuant
 * @sa CL_ZoomOutQuant
 */
void MN_MouseWheel (qboolean down, int x, int y)
{
	/* send it to the captured mouse node */
	if (capturedNode) {
		if (capturedNode->behaviour->mouseWheel)
			capturedNode->behaviour->mouseWheel(capturedNode, down, x, y);
		return;
	}

	if (hoveredNode) {
		if (hoveredNode->behaviour->mouseWheel) {
			hoveredNode->behaviour->mouseWheel(hoveredNode, down, x, y);
		} else {
			if (hoveredNode->onWheelUp && hoveredNode->onWheelDown)
				MN_ExecuteEventActions(hoveredNode, (down ? hoveredNode->onWheelDown : hoveredNode->onWheelUp));
			else
				MN_ExecuteEventActions(hoveredNode, hoveredNode->onWheel);
		}
	}
}

/**
 * @brief Called when we are in menu mode and down a mouse button
 * @sa MN_LeftClick
 * @sa MN_RightClick
 * @sa MN_MiddleClick
 * @sa CL_ZoomInQuant
 * @sa CL_ZoomOutQuant
 */
void MN_MouseDown (int x, int y, int button)
{
	menuNode_t *node;

	/* captured or hover node */
	node = capturedNode ? capturedNode : hoveredNode;

	if (node != NULL) {
		if (node->behaviour->mouseDown)
			node->behaviour->mouseDown(node, x, y, button);
	}

	/* click event */
	/** @todo should be send this event when the mouse up (after a down on the same node) */
	switch (button) {
	case K_MOUSE1:
		MN_LeftClick(x, y);
		break;
	case K_MOUSE2:
		MN_RightClick(x, y);
		break;
	case K_MOUSE3:
		MN_MiddleClick(x, y);
		break;
	}
}

/**
 * @brief Called when we are in menu mode and up a mouse button
 * @sa MN_LeftClick
 * @sa MN_RightClick
 * @sa MN_MiddleClick
 * @sa CL_ZoomInQuant
 * @sa CL_ZoomOutQuant
 */
void MN_MouseUp (int x, int y, int button)
{
	menuNode_t *node;

	/* captured or hover node */
	node = capturedNode ? capturedNode : hoveredNode;

	if (node == NULL)
		return;

	if (node->behaviour->mouseUp)
		node->behaviour->mouseUp(node, x, y, button);
}
