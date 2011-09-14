/**
 * @file m_nodes.c
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
#include "ui_nodes.h"
#include "ui_parse.h"
#include "ui_input.h"

#include "node/ui_node_abstractnode.h"
#include "node/ui_node_abstractscrollbar.h"
#include "node/ui_node_abstractvalue.h"
#include "node/ui_node_button.h"
#include "node/ui_node_checkbox.h"
#include "node/ui_node_controls.h"
#include "node/ui_node_custombutton.h"
#include "node/ui_node_editor.h"
#include "node/ui_node_image.h"
#include "node/ui_node_panel.h"
#include "node/ui_node_radiobutton.h"
#include "node/ui_node_rows.h"
#include "node/ui_node_string.h"
#include "node/ui_node_special.h"
#include "node/ui_node_text.h"
#include "node/ui_node_textentry.h"
#include "node/ui_node_vscrollbar.h"
#include "node/ui_node_zone.h"

typedef void (*registerFunction_t)(nodeBehaviour_t *node);

/**
 * @brief List of functions to register nodes
 * @note Functions must be sorted by node name
 */
const static registerFunction_t registerFunctions[] = {
	MN_RegisterNullNode,
	MN_RegisterAbstractNode,
	MN_RegisterAbstractScrollbarNode,
	MN_RegisterAbstractValueNode,
	MN_RegisterButtonNode,
	MN_RegisterCheckBoxNode,
	MN_RegisterConFuncNode,
	MN_RegisterControlsNode,
	MN_RegisterCustomButtonNode,
	MN_RegisterCvarFuncNode,
	MN_RegisterEditorNode,
	MN_RegisterFuncNode,
	MN_RegisterPanelNode,
	MN_RegisterImageNode,	/* pic */
	MN_RegisterRadioButtonNode,
	MN_RegisterRowsNode,
	MN_RegisterStringNode,
	MN_RegisterTextNode,
	MN_RegisterTextEntryNode,
	MN_RegisterVScrollbarNode,
	MN_RegisterWindowNode,
	MN_RegisterZoneNode
};
#define NUMBER_OF_BEHAVIOURS lengthof(registerFunctions)

/**
 * @brief List of all node behaviours, indexes by nodetype num.
 */
static nodeBehaviour_t nodeBehaviourList[NUMBER_OF_BEHAVIOURS];

/**
 * @brief Get a property from a behaviour or his inheritance
 * @param[in] node Requested node
 * @param[in] name Property name we search
 * @return A value_t with the requested name, else NULL
 */
const value_t *MN_GetPropertyFromBehaviour (const nodeBehaviour_t *behaviour, const char* name)
{
	for (; behaviour; behaviour = behaviour->super) {
		const value_t *result;
		if (behaviour->properties == NULL)
			continue;
		result = MN_FindPropertyByName(behaviour->properties, name);
		if (result)
			return result;
	}
	return NULL;
}

/**
 * @brief Check the if conditions for a given node
 * @sa V_SPECIAL_IF
 * @returns qfalse if the node is not drawn due to not meet if conditions
 */
boolean_t MN_CheckVisibility (menuNode_t *node)
{
	if (!node->visibilityCondition)
		return qtrue;
	return MN_CheckCondition(node, node->visibilityCondition);
}

/**
 * @brief Return a path from a menu to a node
 * @return A path "menuname.nodename.nodename.givennodename"
 * @note Use a static buffer for the result
 */
const char* MN_GetPath (const menuNode_t* node)
{
	static char result[MAX_VAR];
	const menuNode_t* nodes[8];
	int i = 0;

	while (node) {
		assert(i < 8);
		nodes[i] = node;
		node = node->parent;
		i++;
	}

	/** @todo we can use something faster than cat */
	result[0] = '\0';
	while (i) {
		i--;
		Q_strcat(result, nodes[i]->name, sizeof(result));
		if (i > 0)
			Q_strcat(result, ".", sizeof(result));
	}

	return result;
}

/**
 * @brief Read a path and return every we can use (node and property)
 * @details The path token must be a menu name, and then node child.
 * Reserved token 'menu' and 'parent' can be used to navigate.
 * If relativeNode is set, the path can start with reserved token
 * 'this', 'menu' and 'parent' (relative to this node).
 * The function can return a node property by using a '@',
 * the path 'foo@pos' will return the menu foo and the property 'pos'
 * from the 'window' behaviour.
 * @param[in] path Path to read. Contain a node location with dot seprator and a facultative property
 * @param[in] relativeNode relative node where the path start. It allow to use facultative command to start the path (this, parent, menu).
 * @param[out] resultNode Node found. Else NULL.
 * @param[out] resultProperty Property found. Else NULL.
 */
void MN_ReadNodePath (const char* path, const menuNode_t *relativeNode, menuNode_t **resultNode, const value_t **resultProperty)
{
	char name[MAX_VAR];
	menuNode_t* node = NULL;
	const char* nextName;
	char nextCommand = '^';

	*resultNode = NULL;
	if (resultProperty)
		*resultProperty = NULL;

	nextName = path;
	while (nextName && nextName[0] != '\0') {
		const char* begin = nextName;
		char command = nextCommand;
		nextName = strpbrk(begin, ".@");
		if (!nextName) {
			Q_strncpyz(name, begin, sizeof(name));
			nextCommand = '\0';
		} else {
			assert(nextName - begin + 1 <= sizeof(name));
			Q_strncpyz(name, begin, nextName - begin + 1);
			nextCommand = *nextName;
			nextName++;
		}

		switch (command) {
		case '^':	/* first string */
			if (!strcmp(name, "this"))
				/** @todo find a way to fix the bad cast. only here to remove "discards qualifiers" warning */
				node = *(menuNode_t**) ((void*)&relativeNode);
			else if (!strcmp(name, "parent"))
				node = relativeNode->parent;
			else if (!strcmp(name, "root"))
				node = relativeNode->root;
			else
				node = MN_GetMenu(name);
			break;
		case '.':	/* child node */
			if (!strcmp(name, "parent"))
				node = node->parent;
			else if (!strcmp(name, "root"))
				node = node->root;
			else
				node = MN_GetNode(node, name);
			break;
		case '@':	/* property */
			assert(nextCommand == '\0');
			*resultProperty = MN_GetPropertyFromBehaviour(node->behaviour, name);
			*resultNode = node;
			return;
		}

		if (!node)
			return;
	}

	*resultNode = node;
	return;
}

/**
 * @brief Return a node by a path name (names with dot separation)
 * @return The requested node, else NULL if not found
 * @code
 * // get keylist node from options_keys node from options menu
 * node = MN_GetNodeByPath("options.options_keys.keylist");
 * @endcode
 */
menuNode_t* MN_GetNodeByPath (const char* path)
{
	char name[MAX_VAR];
	menuNode_t* node = NULL;
	const char* nextName;

	nextName = path;
	while (nextName && nextName[0] != '\0') {
		const char* begin = nextName;
		nextName = strstr(begin, ".");
		if (!nextName) {
			Q_strncpyz(name, begin, sizeof(name));
		} else {
			assert(nextName - begin + 1 <= sizeof(name));
			Q_strncpyz(name, begin, nextName - begin + 1);
			nextName++;
		}

		if (node == NULL)
			node = MN_GetMenu(name);
		else
			node = MN_GetNode(node, name);

		if (!node)
			return NULL;
	}

	return node;
}

/**
 * @brief Allocate a node into the menu memory
 * @note Its not a dynamic memory allocation. Please only use it at the loading time
 * @todo Assert out when we are not in parsing/loading stage
 */
menuNode_t* MN_AllocNode (const char* type)
{
	menuNode_t* node = &mn.menuNodes[mn.numNodes++];
	if (mn.numNodes >= MAX_MENUNODES)
		Com_Error(ERR_FATAL, "MN_AllocNode: MAX_MENUNODES hit");
	memset(node, 0, sizeof(*node));
	node->behaviour = MN_GetNodeBehaviour(type);
	return node;
}

/**
 * @brief Return the first visible node at a position
 * @param[in] node Node where we must search
 * @param[in] rx Relative x position to the parent of the node
 * @param[in] ry Relative y position to the parent of the node
 * @return The first visible node at position, else NULL
 */
static menuNode_t *MN_GetNodeInTreeAtPosition (menuNode_t *node, int rx, int ry)
{
	menuNode_t *find;
	menuNode_t *child;
	int i;

	if (node->invis || node->behaviour->isVirtual || !MN_CheckVisibility(node))
		return NULL;

	/* relative to the node */
	rx -= node->pos[0];
	ry -= node->pos[1];

	/* check bounding box */
	if (rx < 0 || ry < 0 || rx >= node->size[0] || ry >= node->size[1])
		return NULL;

	/** @todo we should improve the loop (last-to-first) */
	find = NULL;
	for (child = node->firstChild; child; child = child->next) {
		menuNode_t *tmp;
		tmp = MN_GetNodeInTreeAtPosition(child, rx, ry);
		if (tmp)
			find = tmp;
	}
	if (find)
		return find;

	/* is the node tangible */
	if (node->ghost)
		return NULL;

	/* check excluded box */
	for (i = 0; i < node->excludeRectNum; i++) {
		if (rx >= node->excludeRect[i].pos[0]
		 && rx < node->excludeRect[i].pos[0] + node->excludeRect[i].size[0]
		 && ry >= node->excludeRect[i].pos[1]
		 && ry < node->excludeRect[i].pos[1] + node->excludeRect[i].size[1])
			return NULL;
	}

	/* we are over the node */
	return node;
}

/**
 * @brief Return the first visible node at a position
 */
menuNode_t *MN_GetNodeAtPosition (int x, int y)
{
	int pos;

	/* find the first menu under the mouse */
	for (pos = mn.menuStackPos - 1; pos >= 0; pos--) {
		menuNode_t *menu = mn.menuStack[pos];
		menuNode_t *find;

		/* update the layout */
		menu->behaviour->doLayout(menu);

		find = MN_GetNodeInTreeAtPosition(menu, x, y);
		if (find)
			return find;

		/* we must not search anymore */
		if (menu->u.window.dropdown)
			break;
		if (menu->u.window.modal)
			break;
		if (MN_WindowIsFullScreen(menu))
			break;
	}

	return NULL;
}

/**
 * @brief Return a node behaviour by name
 * @note Use a dichotomic search. nodeBehaviourList must be sorted by name.
 */
nodeBehaviour_t* MN_GetNodeBehaviour (const char* name)
{
	unsigned char min = 0;
	unsigned char max = NUMBER_OF_BEHAVIOURS;

	while (min != max) {
		const int mid = (min + max) >> 1;
		const char diff = strcmp(nodeBehaviourList[mid].name, name);
		assert(mid < max);
		assert(mid >= min);

		if (diff == 0)
			return &nodeBehaviourList[mid];

		if (diff > 0)
			max = mid;
		else
			min = mid + 1;
	}

	Com_Error(ERR_FATAL, "Node behaviour '%s' doesn't exist", name);
	/* return NULL; */
}

/**
 * @brief Clone a node
 * @param[in] node Node to clone
 * @param[in] recursive True if we also must clone subnodes
 * @param[in] newMenu Menu where the nodes must be add (this function only link node into menu, note menu into the new node)
 * @todo exclude rect is not safe cloned.
 * @todo actions are not cloned. It is be a problem if we use add/remove listener into a cloned node.
 */
menuNode_t* MN_CloneNode (const menuNode_t* node, menuNode_t *newMenu, boolean_t recursive)
{
	menuNode_t* newNode = MN_AllocNode(node->behaviour->name);

	/* clone all data */
	*newNode = *node;

	/* clean up node navigation */
	newNode->root = newMenu;
	newNode->parent = NULL;
	newNode->firstChild = NULL;
	newNode->lastChild = NULL;
	newNode->next = NULL;

	newNode->behaviour->clone(node, newNode);

	/* clone child */
	if (recursive) {
		menuNode_t* childNode;
		for (childNode = node->firstChild; childNode; childNode = childNode->next) {
			menuNode_t* newChildNode = MN_CloneNode(childNode, newMenu, recursive);
			MN_AppendNode(newNode, newChildNode);
		}
	}
	return newNode;
}

/** @brief position of virtual function into node behaviour */
static const size_t virtualFunctions[] = {
	offsetof(nodeBehaviour_t, draw),
	offsetof(nodeBehaviour_t, leftClick),
	offsetof(nodeBehaviour_t, rightClick),
	offsetof(nodeBehaviour_t, middleClick),
	offsetof(nodeBehaviour_t, mouseWheel),
	offsetof(nodeBehaviour_t, mouseMove),
	offsetof(nodeBehaviour_t, mouseDown),
	offsetof(nodeBehaviour_t, mouseUp),
	offsetof(nodeBehaviour_t, capturedMouseMove),
	offsetof(nodeBehaviour_t, loading),
	offsetof(nodeBehaviour_t, loaded),
	offsetof(nodeBehaviour_t, clone),
	offsetof(nodeBehaviour_t, doLayout),
	offsetof(nodeBehaviour_t, focusGained),
	offsetof(nodeBehaviour_t, focusLost),
	0
};

static void MN_InitializeNodeBehaviour (nodeBehaviour_t* behaviour)
{
	if (behaviour->isInitialized)
		return;

	/** @todo check (when its possible) properties are ordered by name */
	/* check and update properties data */
	if (behaviour->properties) {
		int num = 0;
		const value_t* current = behaviour->properties;
		while (current->string != NULL) {
			num++;
			current++;
		}
		behaviour->propertyCount = num;
	}

	/* everything inherits 'abstractnode' */
	if (behaviour->extends == NULL && strcmp(behaviour->name, "abstractnode") != 0) {
		behaviour->extends = "abstractnode";
	}

	if (behaviour->extends) {
		int i = 0;
		behaviour->super = MN_GetNodeBehaviour(behaviour->extends);
		MN_InitializeNodeBehaviour(behaviour->super);

		while (qtrue) {
			const size_t pos = virtualFunctions[i];
			size_t superFunc;
			size_t func;
			if (pos == 0)
				break;

			/* cache super function if we don't overwrite it */
			superFunc = *(size_t*)((char*)behaviour->super + pos);
			func = *(size_t*)((char*)behaviour + pos);
			if (func == 0 && superFunc != 0)
				*(size_t*)((char*)behaviour + pos) = superFunc;

			i++;
		}
	}

	behaviour->isInitialized = qtrue;
}

void MN_InitNodes (void)
{
	int i = 0;
	nodeBehaviour_t *current = nodeBehaviourList;

	/* compute list of node behaviours */
	for (i = 0; i < NUMBER_OF_BEHAVIOURS; i++) {
		registerFunctions[i](current);
		current++;
	}

	/* check for safe data: list must be sorted by alphabet */
	current = nodeBehaviourList;
	assert(current);
	for (i = 0; i < NUMBER_OF_BEHAVIOURS - 1; i++) {
		const nodeBehaviour_t *a = current;
		const nodeBehaviour_t *b = current + 1;
		assert(b);
		if (strcmp(a->name, b->name) >= 0) {
#ifdef DEBUG
			Com_Error(ERR_FATAL, "MN_InitNodes: '%s' is before '%s'. Please order node behaviour registrations by name\n", a->name, b->name);
#else
			Com_Error(ERR_FATAL, "MN_InitNodes: Error: '%s' is before '%s'\n", a->name, b->name);
#endif
		}
		current++;
	}

	/* finalize node behaviour initialization */
	current = nodeBehaviourList;
	for (i = 0; i < NUMBER_OF_BEHAVIOURS; i++) {
		MN_InitializeNodeBehaviour(current);
		current++;
	}
}
