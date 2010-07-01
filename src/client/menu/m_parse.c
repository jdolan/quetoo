/**
 * @file m_parse.c
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

#include "client.h"
#include "m_parse.h"
#include "m_main.h"
#include "m_data.h"
#include "m_internal.h"
#include "m_actions.h"
#include "m_icon.h"
#include "node/m_node_window.h"
#include "node/m_node_abstractnode.h"

/** prototypes */
static qboolean MN_ParseProperty(void* object, const value_t *property, const char* objectName, const char **text, const char **token);
static menuAction_t *MN_ParseActionList(menuNode_t *menuNode, const char **text, const const char **token);
static qboolean MN_ParseNode(menuNode_t * parent, const char **text, const char **token, const char *errhead);

/** @brief valid node event actions */
static const char *ea_strings[EA_NUM_EVENTACTION] = {
	"",
	"cmd",
	"call",
	"set",
	"if",
	"else",
};
CASSERT(lengthof(ea_strings) == EA_NUM_EVENTACTION);

/** @brief reserved token preventing calling a node with it */
static const char *reserved_tokens[] = {
	"this",
	"parent",
	"root",
	NULL
};

static qboolean MN_IsReservedToken (const char *name)
{
	const char **token = reserved_tokens;
	while (*token) {
		if (!strcmp(*token, name))
			return qtrue;
		token++;
	}
	return qfalse;
}

/**
 * @brief Find a value_t by name into a array of value_t
 * @param[in] propertyList Array of value_t, with null termination
 * @param[in] name Property name we search
 * @return A value_t with the requested name, else NULL
 */
const value_t* MN_FindPropertyByName (const value_t* propertyList, const char* name)
{
	const value_t* current = propertyList;
	while (current->string != NULL) {
		if (!Q_strcasecmp(name, current->string))
			return current;
		current++;
	}
	return NULL;
}

/**
 * @brief Allocate a float into the menu memory
 * @note Its not a dynamic memory allocation. Please only use it at the loading time
 * @param[in] count number of element need to allocate
 * @todo Assert out when we are not in parsing/loading stage
 */
float* MN_AllocFloat (int count)
{
	float *result;
	assert(count > 0);
	mn.curadata = ALIGN_PTR(mn.curadata, sizeof(float));
	result = (float*) mn.curadata;
	mn.curadata += sizeof(float) * count;
	if (mn.curadata - mn.adata > mn.adataize)
		Sys_Error("MN_AllocFloat: Menu memory hunk exceeded - increase the size");
	return result;
}

/**
 * @brief Allocate pointer into the menu memory
 * @note Its not a dynamic memory allocation. Please only use it at the loading time
 * @param[in] count number of element need to allocate
 * @todo Assert out when we are not in parsing/loading stage
 */
static void** MN_AllocPointer (int count)
{
	void **result;
	assert(count > 0);
	mn.curadata = ALIGN_PTR(mn.curadata, sizeof(void*));
	result = (void**) mn.curadata;
	mn.curadata += sizeof(void*) * count;
	if (mn.curadata - mn.adata > mn.adataize)
		Sys_Error("MN_AllocPointer: Menu memory hunk exceeded - increase the size");
	return result;
}

/**
 * @brief Allocate a color into the menu memory
 * @note Its not a dynamic memory allocation. Please only use it at the loading time
 * @param[in] count number of element need to allocate
 * @todo Assert out when we are not in parsing/loading stage
 */
vec4_t* MN_AllocColor (int count)
{
	vec4_t *result;
	assert(count > 0);
	mn.curadata = ALIGN_PTR(mn.curadata, sizeof(vec_t));
	result = (vec4_t*) mn.curadata;
	mn.curadata += sizeof(vec_t) * 4 * count;
	if (mn.curadata - mn.adata > mn.adataize)
		Sys_Error("MN_AllocColor: Menu memory hunk exceeded - increase the size");
	return result;
}

/**
 * @brief Allocate a string into the menu memory
 * @note Its not a dynamic memory allocation. Please only use it at the loading time
 * @param[in] string Use to initialize the string
 * @param[in] size request a fixed memory size, if 0 the string size is used
 * @todo Assert out when we are not in parsing/loading stage
 */
char* MN_AllocString (const char* string, int size)
{
	char* result = (char *)mn.curadata;
	mn.curadata = ALIGN_PTR(mn.curadata, sizeof(char));
	if (size != 0) {
		if (mn.curadata - mn.adata + size > mn.adataize)
			Sys_Error("MN_AllocString: Menu memory hunk exceeded - increase the size");
		strncpy((char *)mn.curadata, string, size);
		mn.curadata += size;
	} else {
		if (mn.curadata - mn.adata + strlen(string) + 1 > mn.adataize)
			Sys_Error("MN_AllocString: Menu memory hunk exceeded - increase the size");
		mn.curadata += sprintf((char *)mn.curadata, "%s", string) + 1;
	}
	return result;
}

static inline qboolean MN_ParseSetAction (menuNode_t *menuNode, menuAction_t *action, const char **text, const char **token, const char *errhead)
{
	char cast[32] = "abstractnode";
	const char *nodeName = *token + 1;
	nodeBehaviour_t *castedBehaviour;
	const value_t *property;

	/* cvar setter */
	if (strncmp(nodeName, "cvar:", 5) == 0) {
		action->data = MN_AllocString(nodeName + 5, 0);
		action->type.param1 = EA_CVARNAME;
		action->type.param2 = EA_VALUE;

		/* get the value */
		*token = Com_EParse(text, errhead, NULL);
		if (!*text)
			return qfalse;
		action->data2 = MN_AllocString(*token, 0);
		return qtrue;
	}

	/* copy the menu name, and move to the node name */
	if (strncmp(nodeName, "node:", 5) == 0) {
		nodeName = nodeName + 5;
		action->type.param1 = EA_PATHPROPERTY;
	} else {
		action->type.param1 = EA_THISMENUNODENAMEPROPERTY;
	}

	/* check cast */
	if (nodeName[0] == '(') {
		const char *end = strchr(nodeName, ')');
		assert(end != NULL);
		assert(end - nodeName < sizeof(cast));

		/* copy the cast and update the node name */
		if (end != NULL) {
			Q_strncpyz(cast, nodeName + 1, end - nodeName);
			nodeName = end + 1;
		}
	}

	/* copy the node path */
	action->data = (byte*) MN_AllocString(nodeName, 0);

	/* get the node property */
	*token = Com_EParse(text, errhead, NULL);
	if (!*text)
		return qfalse;

	castedBehaviour = MN_GetNodeBehaviour(cast);
	property = MN_GetPropertyFromBehaviour(castedBehaviour, *token);
	if (!property) {
		menuNode_t *node = NULL;
		/* do we ALREADY know this node? and his type */
		switch (action->type.param1) {
		case EA_PATHPROPERTY:
			MN_ReadNodePath((char*)action->data, menuNode, &node, NULL);
			break;
		case EA_THISMENUNODENAMEPROPERTY:
			node = MN_GetNodeByPath(va("%s.%s", menuNode->root->name, (char*)action->data));
			break;
		default:
			assert(qfalse);
		}
		if (node)
			property = MN_GetPropertyFromBehaviour(node->behaviour, *token);
		else
			Com_Printf("MN_ParseSetAction: node \"%s\" not already know (in event), you can cast it\n", (char*)action->data);
	}

	action->scriptValues = property;

	if (!property || !property->type) {
		Com_Printf("MN_ParseSetAction: token \"%s\" isn't a node property (in event)\n", *token);
		action->type.op = EA_NULL;
		return qtrue;
	}

	action->type.param2 = EA_RAWVALUE;

	/* get the value */
	*token = Com_EParse(text, errhead, NULL);
	if (!*text)
		return qfalse;

	if (property->type == V_SPECIAL_ACTION) {
		menuAction_t** actionRef = (menuAction_t**) MN_AllocPointer(1);
		*actionRef = MN_ParseActionList(menuNode, text, token);
		if (*actionRef == NULL)
			return qfalse;
		action->data2 = actionRef;
	} else if (property->type == V_SPECIAL_ICONREF) {
		menuIcon_t* icon = MN_GetIconByName(*token);
		menuIcon_t** icomRef;
		if (icon == NULL) {
			Com_Printf("MN_ParseSetAction: icon '%s' not found (%s)\n", *token, MN_GetPath(menuNode));
			return qfalse;
		}
		icomRef = (menuIcon_t**) MN_AllocPointer(1);
		*icomRef = icon;
		action->data2 = icomRef;
	} else {
		if (MN_IsInjectedString(*token)) {
			action->type.param2 = EA_VALUE;
			action->data2 = MN_AllocString(*token, 0);
		} else {
			const int baseType = property->type & V_SPECIAL_TYPE;
			if (baseType != 0 && baseType != V_SPECIAL_CVAR) {
				Com_Printf("MN_ParseSetAction: setter for property '%s' (type %d, 0x%X) is not supported (%s)\n", property->string, property->type, property->type, MN_GetPath(menuNode));
				return qfalse;
			}
			mn.curadata = Com_AlignPtr(mn.curadata, property->type & V_BASETYPEMASK);
			action->data2 = mn.curadata;
			mn.curadata += Com_EParseValue(mn.curadata, *token, property->type & V_BASETYPEMASK, 0, property->size);
		}
	}
	return qtrue;
}

/**
 * @brief Parse actions and return action list
 * @return The first element from a list of action
 * @sa ea_t
 * @todo need cleanup, compute action out of the final memory; reduce number of var
 */
static menuAction_t *MN_ParseActionList (menuNode_t *menuNode, const char **text, const const char **token)
{
	const char *errhead = "MN_ParseActionList: unexpected end of file (in event)";
	menuAction_t *firstAction;
	menuAction_t *lastAction;
	menuAction_t *action;
	qboolean result;
	int i;

	lastAction = NULL;
	firstAction = NULL;

	/* prevent bad position */
	assert(*token[0] == '{');

	while (qtrue) {
		int type = EA_NULL;

		/* get new token */
		*token = Com_EParse(text, errhead, NULL);
		if (!*token)
			return NULL;

		if (*token[0] == '}')
			break;

		/* decode action type */
		for (i = 0; i < EA_NUM_EVENTACTION; i++) {
			if (Q_strcasecmp(*token, ea_strings[i]) == 0) {
				type = i;
				break;
			}
		}
		/* short setter form */
		if (type == EA_NULL && *token[0] == '*') {
			type = EA_SET;
		}

		/* unknown, we break the parsing */
		if (type == EA_NULL) {
			Com_Printf("MN_ParseActionList: unknown token \"%s\" ignored (in event) (node: %s)\n", *token, MN_GetPath(menuNode));
			return NULL;
		}

		/** @todo better to append the action after initialization */
		/* add the action */
		if (mn.numActions >= MAX_MENUACTIONS)
			Sys_Error("MN_ParseActionList: MAX_MENUACTIONS exceeded (%i)\n", mn.numActions);
		action = &mn.menuActions[mn.numActions++];
		memset(action, 0, sizeof(*action));
		if (lastAction) {
			lastAction->next = action;
		}
		if (!firstAction) {
			firstAction = action;
		}
		action->type.op = type;

		/* decode action */
		switch (action->type.op) {
		case EA_CMD:
			/* get parameter values */
			*token = Com_EParse(text, errhead, NULL);
			if (!*text)
				return NULL;

			/* get the value */
			action->data = MN_AllocString(*token, 0);
			break;

		case EA_SET:
			/* if not short syntax */
			if (Q_strcasecmp(*token, ea_strings[EA_SET]) == 0) {
				*token = Com_EParse(text, errhead, NULL);
				if (!*token)
					return NULL;
			}
			result = MN_ParseSetAction(menuNode, action, text, token, errhead);
			if (!result)
				return NULL;
			break;

		case EA_CALL:
			{
				menuNode_t* callNode = NULL;
				/* get the function name */
				*token = Com_EParse(text, errhead, NULL);
				callNode = MN_GetNodeByPath(va("%s.%s", menuNode->root->name, *token));
				if (!callNode) {
					Com_Printf("MN_ParseActionList: function '%s' not found (%s)\n", *token, MN_GetPath(menuNode));
					return NULL;
				}
				action->data = (void*) callNode;
				action->data2 = (void*) &callNode->onClick;
				lastAction = action;
			}
			break;

		case EA_IF:
			/* get the condition */
			*token = Com_EParse(text, errhead, NULL);
			if (!*text)
				return NULL;

			action->data = MN_AllocCondition(*token);
			if (action->data == NULL)
				return NULL;

			/* get the action block */
			*token = Com_EParse(text, errhead, NULL);
			if (!*text)
				return NULL;
			action->scriptValues = (const value_t *) MN_ParseActionList(menuNode, text, token);
			if (action->scriptValues == NULL)
				return NULL;
			break;

		case EA_ELSE:
			/* check previous action */
			if (!lastAction || lastAction->type.op != EA_IF) {
				Com_Printf("MN_ParseActionList: 'else' must be set after an 'if' (node: %s)\n", MN_GetPath(menuNode));
				return NULL;
			}

			/* get the action block */
			*token = Com_EParse(text, errhead, NULL);
			if (!*text)
				return NULL;
			action->scriptValues = (const value_t *) MN_ParseActionList(menuNode, text, token);
			if (action->scriptValues == NULL)
				return NULL;
			break;

		default:
			assert(qfalse);
		}

		/* step */
		lastAction = action;
	}

	assert(*token[0] == '}');

	/* return none NULL value */
	if (firstAction == NULL) {
		firstAction = &mn.menuActions[mn.numActions++];
		memset(firstAction, 0, sizeof(*firstAction));
	}

	return firstAction;
}

static qboolean MN_ParseExcludeRect (menuNode_t * node, const char **text, const char **token, const char *errhead)
{
	excludeRect_t rect;

	/* get parameters */
	*token = Com_EParse(text, errhead, node->name);
	if (!*text)
		return qfalse;
	if (**token != '{') {
		Com_Printf("MN_ParseExcludeRect: node with bad excluderect ignored (node \"%s\")\n", MN_GetPath(node));
		return qtrue;
	}

	do {
		*token = Com_EParse(text, errhead, node->name);
		if (!*text)
			return qfalse;
		if (!strcmp(*token, "pos")) {
			*token = Com_EParse(text, errhead, node->name);
			if (!*text)
				return qfalse;
			Com_EParseValue(&rect, *token, V_POS, offsetof(excludeRect_t, pos), sizeof(vec2_t));
		} else if (!strcmp(*token, "size")) {
			*token = Com_EParse(text, errhead, node->name);
			if (!*text)
				return qfalse;
			Com_EParseValue(&rect, *token, V_POS, offsetof(excludeRect_t, size), sizeof(vec2_t));
		}
	} while (**token != '}');


	if (mn.numExcludeRect >= MAX_EXLUDERECTS) {
		Com_Printf("MN_ParseExcludeRect: exluderect limit exceeded (max: %i)\n", MAX_EXLUDERECTS);
		return qfalse;
	}

	/* copy the rect into the global structure */
	mn.excludeRect[mn.numExcludeRect] = rect;

	/* link only the first element */
	if (node->excludeRect == NULL) {
		node->excludeRect = &mn.excludeRect[mn.numExcludeRect];
	}

	mn.numExcludeRect++;
	node->excludeRectNum++;

	return qtrue;
}

static qboolean MN_ParseEventProperty (menuNode_t * node, const value_t *event, const char **text, const char **token, const char *errhead)
{
	menuAction_t **action;

	/* add new actions to end of list */
	action = (menuAction_t **) ((byte *) node + event->ofs);
	for (; *action; action = &(*action)->next) {}

	/* get the action body */
	*token = Com_EParse(text, errhead, node->name);
	if (!*text)
		return qfalse;

	if (*token[0] != '{') {
		Com_Printf("MN_ParseEventProperty: Event '%s' without body (%s)\n", event->string, MN_GetPath(node));
		return qfalse;
	}
		/* get next token */
	*action = MN_ParseActionList(node, text, token);
	if (*action == NULL)
			return qfalse;

	/* block terminal already read */
	assert(*token[0] == '}');

	return qtrue;
}

/**
 * @brief Parse a property value
 * @todo don't read the next token (need to change the script language)
 */
static qboolean MN_ParseProperty (void* object, const value_t *property, const char* objectName, const char **text, const char **token)
{
	const char *errhead = "MN_ParseProperty: unexpected end of file (object";
	size_t bytes;
	int result;
	const int specialType = property->type & V_SPECIAL_TYPE;

	if (property->type == V_NULL) {
		return qfalse;
	}

	switch (specialType) {
	case 0:	/* common type */

		*token = Com_EParse(text, errhead, objectName);
		if (!*text)
			return qfalse;

		{
			result = Com_ParseValue(object, *token, property->type, property->ofs, property->size, &bytes);
			if (result != RESULT_OK) {
				Com_Printf("MN_ParseProperty: Invalid value for property '%s': %s\n", property->string, Com_GetLastParseError());
				return qfalse;
			}
		}
		break;

	case V_SPECIAL_REF:
		*token = Com_EParse(text, errhead, objectName);
		if (!*text)
			return qfalse;

		/* a reference to data is handled like this */
		mn.curadata = Com_AlignPtr(mn.curadata, property->type & V_BASETYPEMASK);
		*(byte **) ((byte *) object + property->ofs) = mn.curadata;

		/** @todo check for the moment its not a cvar */
		assert (*token[0] != '*');

		/* sanity check */
		if ((property->type & V_BASETYPEMASK) == V_STRING && strlen(*token) > MAX_VAR - 1) {
			Com_Printf("MN_ParseProperty: Value '%s' is too long (key %s)\n", *token, property->string);
			return qfalse;
		}

		result = Com_ParseValue(mn.curadata, *token, property->type & V_BASETYPEMASK, 0, property->size, &bytes);
		if (result != RESULT_OK) {
			Com_Printf("MN_ParseProperty: Invalid value for property '%s': %s\n", property->string, Com_GetLastParseError());
			return qfalse;
		}
		mn.curadata += bytes;

		break;

	case V_SPECIAL_CVAR:	/* common type */
		*token = Com_EParse(text, errhead, objectName);
		if (!*text)
			return qfalse;

		/* references are parsed as string */
		if (*token[0] == '*') {
			/* a reference to data */
			mn.curadata = Com_AlignPtr(mn.curadata, V_STRING);
			*(byte **) ((byte *) object + property->ofs) = mn.curadata;

			/* sanity check */
			if (strlen(*token) > MAX_VAR - 1) {
				Com_Printf("MN_ParseProperty: Value '%s' is too long (key %s)\n", *token, property->string);
				return qfalse;
			}

			result = Com_ParseValue(mn.curadata, *token, V_STRING, 0, 0, &bytes);
			if (result != RESULT_OK) {
				Com_Printf("MN_ParseProperty: Invalid value for property '%s': %s\n", property->string, Com_GetLastParseError());
				return qfalse;
			}
			mn.curadata += bytes;
		} else {
			/* a reference to data */
			mn.curadata = Com_AlignPtr(mn.curadata, property->type & V_BASETYPEMASK);
			*(byte **) ((byte *) object + property->ofs) = mn.curadata;

			/* sanity check */
			if ((property->type & V_BASETYPEMASK) == V_STRING && strlen(*token) > MAX_VAR - 1) {
				Com_Printf("MN_ParseProperty: Value '%s' is too long (key %s)\n", *token, property->string);
				return qfalse;
			}

			result = Com_ParseValue(mn.curadata, *token, property->type & V_BASETYPEMASK, 0, property->size, &bytes);
			if (result != RESULT_OK) {
				Com_Printf("MN_ParseProperty: Invalid value for property '%s': %s\n", property->string, Com_GetLastParseError());
				return qfalse;
			}
			mn.curadata += bytes;
		}
		break;

	case V_SPECIAL:

		switch (property->type) {
		case V_SPECIAL_ACTION:
			result = MN_ParseEventProperty(object, property, text, token, errhead);
			if (!result)
				return qfalse;
			break;

		case V_SPECIAL_EXCLUDERECT:
			result = MN_ParseExcludeRect(object, text, token, errhead);
			if (!result)
				return qfalse;
			break;

		case V_SPECIAL_ICONREF:
			{
				menuIcon_t** icon = (menuIcon_t**) ((byte *) object + property->ofs);
				*token = Com_EParse(text, errhead, objectName);
				if (!*text)
					return qfalse;
				*icon = MN_GetIconByName(*token);
				if (*icon == NULL) {
					Com_Printf("MN_ParseProperty: icon '%s' not found (object %s)\n", *token, objectName);
				}
			}
			break;

		case V_SPECIAL_IF:
			{
				menuCondition_t **condition = (menuCondition_t **) ((byte *) object + property->ofs);

				*token = Com_EParse(text, errhead, objectName);
				if (!*text)
					return qfalse;

				*condition = MN_AllocCondition(*token);
				if (*condition == NULL)
					return qfalse;
			}
			break;

		case V_SPECIAL_DATAID:
			{
				int *dataId = (int*) ((byte *) object + property->ofs);
				*token = Com_EParse(text, errhead, objectName);
				if (!*text)
					return qfalse;

				*dataId = MN_GetDataIDByName(*token);
				if (*dataId < 0) {
					Com_Printf("MN_ParseProperty: Could not find menu dataId '%s' (%s@%s)\n",
							*token, objectName, property->string);
					return qfalse;
				}
			}
			break;

		default:
			Com_Printf("MN_ParseProperty: unknown property type '%d' (0x%X) (%s@%s)\n",
					property->type, property->type, objectName, property->string);
			return qfalse;
		}
		break;

	default:
		Com_Printf("MN_ParseNodeProperties: unknown property type '%d' (0x%X) (%s@%s)\n",
				property->type, property->type, objectName, property->string);
		return qfalse;
	}

	return qtrue;
}

static qboolean MN_ParseFunction (menuNode_t * node, const char **text, const char **token)
{
	menuAction_t **action;
	assert (node->behaviour->isFunction);

	/* add new actions to end of list */
	action = &node->onClick;
	for (; *action; action = &(*action)->next) {}

	if (mn.numActions >= MAX_MENUACTIONS)
		Sys_Error("MN_ParseFunction: MAX_MENUACTIONS exceeded (%i)", mn.numActions);
	*action = &mn.menuActions[mn.numActions++];
	memset(*action, 0, sizeof(**action));

	*action = MN_ParseActionList(node, text, token);
	if (*action == NULL)
		return qfalse;

	return *token[0] == '}';
}

/**
 * @sa MN_ParseNodeProperties
 * @brief parse all sequencial properties into a block
 * @note allow to use an extra block
 * @code
 * foobehaviour foonode {
 *   { properties }
 *   // the function stop reading here
 *   nodes
 * }
 * foobehaviour foonode {
 *   properties
 *   // the function stop reading here
 *   nodes
 * }
 * @endcode
 */
static qboolean MN_ParseNodeProperties (menuNode_t * node, const char **text, const char **token)
{
	const char *errhead = "MN_ParseNodeProperties: unexpected end of file (node";
	qboolean nextTokenAlreadyRead = qfalse;

	if (*token[0] != '{') {
		nextTokenAlreadyRead = qtrue;
	}

	do {
		const value_t *val;
		int result;

		/* get new token */
		if (!nextTokenAlreadyRead) {
			*token = Com_EParse(text, errhead, node->name);
			if (!*text)
				return qfalse;
		} else {
			nextTokenAlreadyRead = qfalse;
		}

		/* is finished */
		if (*token[0] == '}') {
			break;
		}

		/* find the property */

		val = MN_GetPropertyFromBehaviour(node->behaviour, *token);
		if (!val) {
			/* unknown token, print message and continue */
			Com_Printf("MN_ParseNodeProperties: unknown property \"%s\", node ignored (node %s)\n",
					*token, MN_GetPath(node));
			return qfalse;
		}

		/* get parameter values */
		result = MN_ParseProperty(node, val, node->name, text, token);
		if (!result) {
			Com_Printf("MN_ParseNodeProperties: Problem with parsing of node property '%s@%s'. See upper\n",
					MN_GetPath(node), val->string);
			return qfalse;
		}

	} while (*text);

	return qtrue;
}

/**
 * @brief Read a node body
 * @note Node header already read, we are over the node name, or '{'
 * @code
 * Allowed syntax
 * { properties }
 * OR
 * { nodes }
 * OR
 * { { properties } nodes }
 * @endcode
 */
static qboolean MN_ParseNodeBody (menuNode_t * node, const char **text, const char **token, const char *errhead)
{
	qboolean result = qtrue;

	if (*token[0] != '{') {
		/* read the body block start */
		*token = Com_EParse(text, errhead, node->name);
		if (!*text)
			return qfalse;
		if (*token[0] != '{') {
			Com_Printf("MN_ParseNodeBody: node doesn't have body, token '%s' read (node \"%s\")\n", *token, MN_GetPath(node));
			mn.numNodes--;
			return qfalse;
		}
	}

	/* functions are a special case */
	if (node->behaviour->isFunction) {
		result = MN_ParseFunction(node, text, token);
	} else {

		/* check the content */
		*token = Com_EParse(text, errhead, node->name);
		if (!*text)
			return qfalse;

		if (*token[0] == '{') {
			/* we have a special block for properties */
			result = MN_ParseNodeProperties(node, text, token);
			if (!result)
				return qfalse;

			/* move token over the next node behaviour */
			*token = Com_EParse(text, errhead, node->name);
			if (!*text)
				return qfalse;

			/* and then read all nodes */
			while (*token[0] != '}') {
				result = MN_ParseNode(node, text, token, errhead);
				if (!result)
					return qfalse;

				*token = Com_EParse(text, errhead, node->name);
				if (*text == NULL)
					return qfalse;
			}
		} else if (MN_GetPropertyFromBehaviour(node->behaviour, *token)) {
			/* we should have a block with properties only */
			result = MN_ParseNodeProperties(node, text, token);
		} else {
			/* we should have a block with nodes only */
			while (*token[0] != '}') {
				result = MN_ParseNode(node, text, token, errhead);
				if (!result)
					return qfalse;

				*token = Com_EParse(text, errhead, node->name);
				if (*text == NULL)
					return qfalse;
			}
		}
	}
	if (!result) {
		Com_Printf("MN_ParseNodeBody: node with bad body ignored (node \"%s\")\n", MN_GetPath(node));
		mn.numNodes--;
		return qfalse;
	}

	/* already check on MN_ParseNodeProperties */
	assert(*token[0] == '}');
	return qtrue;
}

/**
 * @brief parse a node and complet the menu with it
 * @sa MN_ParseMenuBody
 * @sa MN_ParseNodeProperties
 * @todo we can think about merging MN_ParseNodeProperties here
 * @node first token already read
 * @node dont read more than the need token (last right token is '}' of end of node)
 */
static qboolean MN_ParseNode (menuNode_t * parent, const char **text, const char **token, const char *errhead)
{
	menuNode_t *node;
	nodeBehaviour_t *behaviour;
	qboolean result;

	/* allow to begin with the identifier "node" before the behaviour name */
	if (!Q_strcasecmp(*token, "node")) {
		*token = Com_EParse(text, errhead, parent->name);
		if (!*text)
			return qfalse;
	}

	/* get the behaviour */
	behaviour = MN_GetNodeBehaviour(*token);
	if (behaviour == NULL) {
		Com_Printf("MN_ParseNode: node behaviour '%s' doesn't exist (into \"%s\")\n", *token, MN_GetPath(parent));
		return qfalse;
	}

	/* get the name */
	*token = Com_EParse(text, errhead, parent->name);
	if (!*text)
		return qfalse;
	if (MN_IsReservedToken(*token)) {
		Com_Printf("MN_ParseNode: \"%s\" is a reserved token, we can't call a node with it (node \"%s.%s\")\n", *token, MN_GetPath(parent), *token);
		return qfalse;
	}

	/* test if node already exists */
	node = MN_GetNode(parent, *token);
	if (node) {
		if (node->behaviour != behaviour) {
			Com_Printf("MN_ParseNode: we can't change node type (node \"%s\")\n", MN_GetPath(node));
			return qfalse;
		}
		Com_DPrintf(DEBUG_CLIENT, "... over-riding node %s\n", MN_GetPath(node));
		/* reset action list of node */
		node->onClick = NULL;	/**< @todo understand why this strange hack exists (there is a lot of over actions) */

	/* else initialize node */
	} else {
		node = MN_AllocNode(behaviour->name);
		node->parent = parent;
		node->root = parent->root;
		Q_strncpyz(node->name, *token, sizeof(node->name));
		MN_AppendNode(parent, node);
	}

	/* throw to the node, we begin to read attributes */
	if (node->behaviour->loading)
		node->behaviour->loading(node);

	/* get body */
	result = MN_ParseNodeBody(node, text, token, errhead);
	if (!result)
		return qfalse;

	/* initialize the node according to its behaviour */
	if (node->behaviour->loaded) {
		node->behaviour->loaded(node);
	}
	return qtrue;
}

void MN_ParseIcon (const char *name, const char **text)
{
	menuIcon_t *icon;
	const char *token;

	/* search for menus with same name */
	icon = MN_AllocIcon(name);

	/* get it's body */
	token = Com_Parse(text);
	assert(strcmp(token, "{") == 0);

	/* read properties */
	while (qtrue) {
		const value_t *property;
		qboolean result;

		token = Com_Parse(text);
		if (*text == NULL)
			return;

		if (token[0] == '}')
			break;

		property = MN_FindPropertyByName(mn_iconProperties, token);
		if (!property) {
			Com_Printf("MN_ParseIcon: unknown options property: '%s' - ignore it\n", token);
			return;
		}

		/* get parameter values */
		result = MN_ParseProperty(icon, property, icon->name, text, &token);
		if (!result) {
			Com_Printf("MN_ParseIcon: Parsing for icon '%s'. See upper\n", icon->name);
			return;
		}
	}

	return;
}

/**
 * @sa CL_ParseClientData
 */
void MN_ParseMenu (const char *type, const char *name, const char **text)
{
	const char *errhead = "MN_ParseMenu: unexpected end of file (menu";
	menuNode_t *menu;
	menuNode_t *node;
	const char *token;
	qboolean result;
	int i;

	if (strcmp(type, "window") != 0) {
		Sys_Error("MN_ParseMenu: '%s %s' is not a window node\n", type, name);
		return;	/* never reached */
	}

	if (MN_IsReservedToken(name)) {
		Com_Printf("MN_ParseMenu: \"%s\" is a reserved token, we can't call a node with it (node \"%s\")\n", name, name);
		return;
	}

	/* search for menus with same name */
	for (i = 0; i < mn.numMenus; i++)
		if (!strncmp(name, mn.menus[i]->name, sizeof(mn.menus[i]->name)))
			break;

	if (i < mn.numMenus) {
		Com_Printf("MN_ParseMenus: %s \"%s\" with same name found, second ignored\n", type, name);
	}

	if (mn.numMenus >= MAX_MENUS) {
		Sys_Error("MN_ParseMenu: max menus exceeded (%i) - ignore '%s'\n", MAX_MENUS, name);
		return;	/* never reached */
	}

	/* initialize the menu */
	menu = MN_AllocNode(type);
	Q_strncpyz(menu->name, name, sizeof(menu->name));
	menu->root = menu;

	menu->behaviour->loading(menu);

	MN_InsertMenu(menu);

	/* get it's body */
	token = Com_Parse(text);

	/* does this menu inherit data from another menu? */
	if (!strncmp(token, "extends", 7)) {
		menuNode_t *superMenu;
		menuNode_t *newNode;

		token = Com_Parse(text);
		Com_DPrintf(DEBUG_CLIENT, "MN_ParseMenus: %s \"%s\" inheriting node \"%s\"\n", type, name, token);
		superMenu = MN_GetMenu(token);
		if (!superMenu)
			Sys_Error("MN_ParseMenu: %s '%s' can't inherit from node '%s' - because '%s' was not found\n", type, name, token, token);
		*menu = *superMenu;
		menu->super = superMenu;
		menu->root = menu;
		Q_strncpyz(menu->name, name, sizeof(menu->name));

		/* start a new list */
		menu->firstChild = NULL;
		menu->lastChild = NULL;

		/* clone all super menu's nodes */
		for (node = superMenu->firstChild; node; node = node->next) {
			if (mn.numNodes >= MAX_MENUNODES)
				Sys_Error("MAX_MENUNODES exceeded\n");

			newNode = MN_CloneNode(node, menu, qtrue);
			newNode->super = node;
			MN_AppendNode(menu, newNode);

			/* update special links */
			if (superMenu->u.window.renderNode == node)
				menu->u.window.renderNode = newNode;
			else if (superMenu->u.window.onTimeOut == node->onClick) {
				menu->u.window.onTimeOut = newNode->onClick;
			}
		}

		token = Com_Parse(text);
	}

	/* parse it's body */
	result = MN_ParseNodeBody(menu, text, &token, errhead);
	if (!result) {
		Sys_Error("MN_ParseMenu: menu \"%s\" has a bad body\n", menu->name);
		return;	/* never reached */
	}

	menu->behaviour->loaded(menu);
}

/**
 * @sa Com_MacroExpandString
 * @todo should review this code, '*' dont check very well every things
 */
const char *MN_GetReferenceString (const menuNode_t* const node, const char *ref)
{
	if (!ref)
		return NULL;

	/* its a cvar */
	if (*ref == '*') {
		char ident[MAX_VAR];
		const char *text, *token;

		/* get the reference and the name */
		text = Com_MacroExpandString(ref);
		if (text)
			return text;

		text = ref + 1;
		token = Com_Parse(&text);
		if (!text)
			return NULL;
		Q_strncpyz(ident, token, sizeof(ident));

		{
			menuNode_t *refNode;
			const value_t *val;

			token = Com_Parse(&text);
			if (!text)
				return NULL;

			/* draw a reference to a node property */
			refNode = MN_GetNode(node->root, ident);
			if (!refNode)
				return NULL;

			/* get the property */
			val = MN_GetPropertyFromBehaviour(refNode->behaviour, token);
			if (!val)
				return NULL;

			/* get the string */
			return Com_ValueToStr(refNode, val->type & V_BASETYPEMASK, val->ofs);
		}

	/* traslatable string */
	} else if (*ref == '_') {
		ref++;
		return _(ref);

	/* just a string */
	} else {
		return ref;
	}
}

float MN_GetReferenceFloat (const menuNode_t* const node, void *ref)
{
	if (!ref)
		return 0.0;
	if (*(const char *) ref == '*') {
		char ident[MAX_VAR];
		const char *token, *text;

		/* get the reference and the name */
		text = (const char *) ref + 1;
		token = Com_Parse(&text);
		if (!text)
			return 0.0;
		Q_strncpyz(ident, token, sizeof(ident));
		token = Com_Parse(&text);
		if (!text)
			return 0.0;

		if (!strncmp(ident, "cvar", 4)) {
			/* get the cvar value */
			return Cvar_GetValue(token);
		} else {
			menuNode_t *refNode;
			const value_t *val;

			/* draw a reference to a node property */
			refNode = MN_GetNode(node->root, ident);
			if (!refNode)
				return 0.0;

			/* get the property */
			val = MN_GetPropertyFromBehaviour(refNode->behaviour, token);
			if (!val || val->type != V_FLOAT)
				return 0.0;

			/* get the string */
			return *(float *) ((byte *) refNode + val->ofs);
		}
	} else {
		/* just get the data */
		return *(float *) ref;
	}
}
