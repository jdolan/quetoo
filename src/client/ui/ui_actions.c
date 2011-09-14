/**
 * @file m_actions.c
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
#include "ui_parse.h"
#include "ui_input.h"
#include "ui_actions.h"
#include "node/ui_node_abstractnode.h"

#include "client.h"

#define MAX_CONFUNC_SIZE 512
/**
 * @brief Executes confunc - just to identify those confuncs in the code - in
 * this frame.
 * @param[in] confunc The confunc id that should be executed
 */
void MN_ExecuteConfunc (const char *fmt, ...)
{
	va_list ap;
	char confunc[MAX_CONFUNC_SIZE];

	va_start(ap, fmt);
	Q_vsnprintf(confunc, sizeof(confunc), fmt, ap);
	Cmd_ExecuteString(confunc);
	va_end(ap);
}

/**
 * @brief read a property name from an input buffer to an output
 * @return last position into the input buffer if we find property, else NULL
 */
static inline const char* MN_GenCommandReadProperty (const char* input, char* output, int outputSize)
{
	assert(input[0] == '<');
	outputSize--;
	input++;

	while (outputSize && *input != '\0' && *input != ' ' && *input != '>') {
		*output++ = *input++;
		outputSize--;
	}

	if (input[0] != '>')
		return NULL;

	output[0] = '\0';
	return ++input;
}

/**
 * @brief Replace injection identifiers (e.g. <eventParam>) by a value
 * @note The injection identifier can be every node value - e.g. <image> or <width>.
 * It's also possible to do something like
 * @code cmd "set someCvar <min>/<max>"
 */
static const char* MN_GenInjectedString (const menuNode_t* source, boolean_t useCmdParam, const char* input, boolean_t addNewLine)
{
	static char cmd[256];
	int length = sizeof(cmd) - (addNewLine ? 2 : 1);
	static char propertyName[MAX_VAR];
	const char *cin = input;
	char *cout = cmd;

	while (length && cin[0] != '\0') {
		if (cin[0] == '<') {
			/* read propertyName between '<' and '>' */
			const char *next = MN_GenCommandReadProperty(cin, propertyName, sizeof(propertyName));
			if (next) {
				/* cvar injection */
				if (!strncmp(propertyName, "cvar:", 5)) {
					const cvar_t *cvar = Cvar_Get(propertyName + 5, "", 0, NULL);
					const int l = snprintf(cout, length, "%s", cvar->string);
					cout += l;
					cin = next;
					length -= l;
					continue;

				/* source path injection */
				} else if (!strncmp(propertyName, "path:", 5)) {
					if (source) {
						const char *command = propertyName + 5;
						const menuNode_t *node = NULL;
						if (!strcmp(command, "root"))
							node = source->root;
						else if (!strcmp(command, "this"))
							node = source;
						else if (!strcmp(command, "parent"))
							node = source->parent;
						else
							Com_Print("MN_GenCommand: Command '%s' for path injection unknown\n", command);

						if (node) {
							const int l = snprintf(cout, length, "%s", MN_GetPath(node));
							cout += l;
							cin = next;
							length -= l;
							continue;
						}
					}

				/* no prefix */
				} else {
					/* source property injection */
					if (source) {
						/* find property definition */
						const value_t *property = MN_GetPropertyFromBehaviour(source->behaviour, propertyName);
						if (property) {
							const char* value;
							int l;
							/* only allow common type or cvar */
							if ((property->type & V_SPECIAL_TYPE) != 0 && (property->type & V_SPECIAL_TYPE) != V_SPECIAL_CVAR)
								Com_Error(ERR_FATAL, "MN_GenCommand: Unsupported type injection for property '%s', node '%s'", property->string, MN_GetPath(source));

							/* inject the property value */
							value = Com_ValueToStr((const void*)source, property->type, property->ofs);
							l = snprintf(cout, length, "%s", value);
							cout += l;
							cin = next;
							length -= l;
							continue;
						}
					}

					/* param injection */
					if (useCmdParam) {
						int arg;
						const int checked = sscanf(propertyName, "%d", &arg);
						if (checked == 1 && Cmd_Argc() >= arg) {
							const int l = snprintf(cout, length, "%s", Cmd_Argv(arg));
							cout += l;
							cin = next;
							length -= l;
							continue;
						}
					}
				}
			}
		}
		*cout++ = *cin++;
		length--;
	}

	/* is buffer too small? */
	assert(cin[0] == '\0');

	if (addNewLine)
		*cout++ = '\n';

	*cout++ = '\0';

	return cmd;
}

static inline void MN_ExecuteSetAction (const menuNode_t* source, boolean_t useCmdParam, const menuAction_t* action)
{
	const char* path;
	byte *value;
	menuNode_t *node;

	if (!action->data)
		return;

	if (action->type.param1 == EA_CVARNAME) {
		char cvarName[MAX_VAR];
		const char* textValue;
		assert(action->type.param2 == EA_VALUE);
		Q_strncpyz(cvarName, MN_GenInjectedString(source, useCmdParam, action->data, qfalse), MAX_VAR);

		textValue = action->data2;
		textValue = MN_GenInjectedString(source, useCmdParam, textValue, qfalse);
		if (textValue[0] == '_') {
			textValue = _(textValue + 1);
		}
		Cvar_ForceSet(cvarName, textValue);
		return;
	}

	/* search the node */
	path = MN_GenInjectedString(source, useCmdParam, action->data, qfalse);
	switch (action->type.param1) {
	case EA_THISMENUNODENAMEPROPERTY:
		{
			const menuNode_t *root = source->root;
			node = MN_GetNodeByPath(va("%s.%s", root->name, path));
			if (!node) {
				Com_Print("MN_ExecuteSetAction: node \"%s.%s\" doesn't exist (source: %s)\n", root->name, path, MN_GetPath(source));
				return;
			}
		}
		break;
	case EA_PATHPROPERTY:
		MN_ReadNodePath(path, source, &node, NULL);
		if (!node) {
			Com_Print("MN_ExecuteSetAction: node \"%s\" doesn't exist (source: %s)\n", path, MN_GetPath(source));
			return;
		}
		break;
	default:
		node = NULL;
		Com_Error(ERR_FATAL, "MN_ExecuteSetAction: Invalid actiontype (source: %s)", MN_GetPath(source));
	}

	value = action->data2;

	/* decode text value */
	if (action->type.param2 == EA_VALUE) {
		const char* v = MN_GenInjectedString(source, useCmdParam, (char*) value, qfalse);
		MN_NodeSetProperty(node, action->scriptValues, v);
		return;
	}

	/* decode RAW value */
	assert(action->type.param2 == EA_RAWVALUE);
	if ((action->scriptValues->type & V_SPECIAL_TYPE) == 0)
		Com_SetValue(node, (char *) value, action->scriptValues->type, action->scriptValues->ofs, action->scriptValues->size);
	else if ((action->scriptValues->type & V_SPECIAL_TYPE) == V_SPECIAL_CVAR) {
		void *mem = ((byte *) node + action->scriptValues->ofs);
		MN_FreeStringProperty(*(void**)mem);
		switch (action->scriptValues->type & V_BASETYPEMASK) {
		case V_FLOAT:
			**(float **) mem = *(float*) value;
			break;
		case V_INT:
			**(int **) mem = *(int*) value;
			break;
		default:
			*(byte **) mem = value;
		}
	} else if (action->scriptValues->type == V_SPECIAL_ACTION) {
		void *mem = ((byte *) node + action->scriptValues->ofs);
		*(menuAction_t**) mem = *(menuAction_t**)value;
	} else if (action->scriptValues->type == V_SPECIAL_ICONREF) {
		void *mem = ((byte *) node + action->scriptValues->ofs);
		*(menuIcon_t**) mem = *(menuIcon_t**)value;
	} else {
		assert(qfalse);
	}
}

static void MN_ExecuteInjectedActions(const menuNode_t* source, boolean_t useCmdParam, const menuAction_t* firstAction);

/**
 * @brief Execute an action from a source
 * @param[in] useCmdParam If true, inject every where its possible command line param
 */
static void MN_ExecuteInjectedAction (const menuNode_t* source, boolean_t useCmdParam, const menuAction_t* action)
{
	switch (action->type.op) {
	case EA_NULL:
		/* do nothing */
		break;

	case EA_CMD:
		/* execute a command */
		if (action->data)
			Cbuf_AddText(MN_GenInjectedString(source, useCmdParam, action->data, qtrue));
		break;

	case EA_CALL:
		/* call another function */
		MN_ExecuteInjectedActions((const menuNode_t *) action->data, qfalse, *(menuAction_t **) action->data2);
		break;

	case EA_SET:
		MN_ExecuteSetAction(source, useCmdParam, action);
		break;

	case EA_IF:
		if (MN_CheckCondition(source, (menuCondition_t *) action->data)) {
			MN_ExecuteInjectedActions(source, useCmdParam, (const menuAction_t* const) action->scriptValues);
		} else if (action->next && action->next->type.op == EA_ELSE) {
			MN_ExecuteInjectedActions(source, useCmdParam, (const menuAction_t* const) action->next->scriptValues);
		}
		break;

	case EA_ELSE:
		/* previous EA_IF execute this action */
		break;

	default:
		Com_Error(ERR_FATAL, "unknown action type");
	}
}

static void MN_ExecuteInjectedActions (const menuNode_t* source, boolean_t useCmdParam, const menuAction_t* firstAction)
{
	static int callnumber = 0;
	const menuAction_t *action;
	if (callnumber++ > 20) {
		Com_Print("MN_ExecuteInjectedActions: Possible recursion\n");
		return;
	}
	for (action = firstAction; action; action = action->next) {
		MN_ExecuteInjectedAction(source, useCmdParam, action);
	}
	callnumber--;
}

/**
 * @brief allow to inject command param into cmd of confunc command
 */
void MN_ExecuteConFuncActions (const menuNode_t* source, const menuAction_t* firstAction)
{
	MN_ExecuteInjectedActions(source, qtrue, firstAction);
}

void MN_ExecuteEventActions (const menuNode_t* source, const menuAction_t* firstAction)
{
	MN_ExecuteInjectedActions(source, qfalse, firstAction);
}

/**
 * @brief Test if a string use an injection syntax
 * @return True if we find in the string a syntaxe "<" {thing without space} ">"
 */
boolean_t MN_IsInjectedString (const char *string)
{
	const char *c = string;
	assert(string);
	while (*c != '\0') {
		if (*c == '<') {
			const char *d = c + 1;
			if (*d != '>') {
				while (*d) {
					if (*d == '>')
						return qtrue;
					if (*d == ' ' || *d == '\t' || *d == '\n' || *d == '\r')
						break;
					d++;
				}
			}
		}
		c++;
	}
	return qfalse;
}

/**
 * @brief Free a string property if it is allocated into mn_dynStringPool
 * @sa mn_dynStringPool
 */
void MN_FreeStringProperty (void* pointer)
{
	/* skip const string */
	if ((uintptr_t)mn.adata <= (uintptr_t)pointer && (uintptr_t)pointer < (uintptr_t)mn.adata + (uintptr_t)mn.adataize)
		return;

	/* skip pointer out of mn_dynStringPool */
	if (!_Mem_AllocatedInPool(mn_dynStringPool, pointer))
		return;

	Mem_Free(pointer);
}

/**
 * @brief Allocate and initialize a command action
 * @param[in] command A command for the action
 * @return An initialised action
 */
menuAction_t* MN_AllocCommandAction (char *command)
{
	menuAction_t* action = &mn.menuActions[mn.numActions++];
	memset(action, 0, sizeof(*action));
	action->type.op = EA_CMD;
	action->data = command;
	return action;
}

/**
 * @brief Set a new action to a @c menuAction_t pointer
 * @param[in] type Only @c EA_CMD is supported
 * @param[in] data The data for this action - in case of @c EA_CMD this is the commandline
 * @note You first have to free existing node actions - only free those that are
 * not static in @c mn.menuActions array
 * @todo we should create a function to free the memory. We can use a tag in the Mem_PoolAlloc
 * calls and use use Mem_FreeTag.
 */
void MN_PoolAllocAction (menuAction_t** action, int type, const void *data)
{
	if (*action)
		Com_Error(ERR_FATAL, "There is already an action assigned");
	*action = (menuAction_t *)Mem_PoolAlloc(sizeof(**action), cl_menuSysPool, 0);
	(*action)->type.op = type;
	switch (type) {
	case EA_CMD:
		(*action)->data = Mem_PoolStrDup((const char *)data, cl_menuSysPool, 0);
		break;
	default:
		Com_Error(ERR_FATAL, "Action type %i is not yet implemented", type);
	}
}

/**
 * @brief add a call of a function into a nodfe event
 */
static void MN_AddListener_f (void)
{
	menuNode_t *node;
	menuNode_t *function;
	const value_t *property;
	menuAction_t *action;
	menuAction_t *lastAction;

	if (Cmd_Argc() != 3) {
		Com_Print("Usage: %s <pathnode@event> <pathnode>\n", Cmd_Argv(0));
		return;
	}

	MN_ReadNodePath(Cmd_Argv(1), NULL, &node, &property);
	if (node == NULL) {
		Com_Print("MN_AddListener_f: '%s' node not found.\n", Cmd_Argv(1));
		return;
	}
	if (property == NULL || property->type != V_SPECIAL_ACTION) {
		Com_Print("MN_AddListener_f: '%s' property not found, or is not an event.\n", Cmd_Argv(1));
		return;
	}

	function = MN_GetNodeByPath(Cmd_Argv(2));
	if (function == NULL) {
		Com_Print("MN_AddListener_f: '%s' node not found.\n", Cmd_Argv(2));
		return;
	}

	/* create the action */
	action = (menuAction_t*) Mem_PoolAlloc(sizeof(*action), cl_menuSysPool, 0);
	action->type.op = EA_CALL;
	action->data = (void*) function;
	action->data2 = (void*) &function->onClick;
	action->next = NULL;

	/* insert the action */
	lastAction = *(menuAction_t**)((char*)node + property->ofs);
	while (lastAction)
		lastAction = lastAction->next;
	if (lastAction)
		lastAction->next = action;
	else
		*(menuAction_t**)((char*)node + property->ofs) = action;
}

/**
 * @brief add a call of a function from a nodfe event
 */
static void MN_RemoveListener_f (void)
{
	menuNode_t *node;
	menuNode_t *function;
	const value_t *property;
	void *data;
	menuAction_t *lastAction;

	if (Cmd_Argc() != 3) {
		Com_Print("Usage: %s <pathnode@event> <pathnode>\n", Cmd_Argv(0));
		return;
	}

	MN_ReadNodePath(Cmd_Argv(1), NULL, &node, &property);
	if (node == NULL) {
		Com_Print("MN_RemoveListener_f: '%s' node not found.\n", Cmd_Argv(1));
		return;
	}
	if (property == NULL || property->type != V_SPECIAL_ACTION) {
		Com_Print("MN_RemoveListener_f: '%s' property not found, or is not an event.\n", Cmd_Argv(1));
		return;
	}

	function = MN_GetNodeByPath(Cmd_Argv(2));
	if (function == NULL) {
		Com_Print("MN_RemoveListener_f: '%s' node not found.\n", Cmd_Argv(2));
		return;
	}

	/* data we must remove */
	data = (void*) &function->onClick;

	/* remove the action */
	lastAction = *(menuAction_t**)((char*)node + property->ofs);
	if (lastAction) {
		menuAction_t *tmp = NULL;
		if (lastAction->data2 == data) {
			tmp = lastAction;
			*(menuAction_t**)((char*)node + property->ofs) = lastAction->next;
		} else {
			while (lastAction->next) {
				if (lastAction->next->data2 == data)
					break;
				lastAction = lastAction->next;
			}
			if (lastAction->next) {
				tmp = lastAction->next;
				lastAction->next = lastAction->next->next;
			}
		}
		if (tmp)
			Mem_Free(tmp);
		else
			Com_Print("MN_RemoveListener_f: '%s' into '%s' not found.\n", Cmd_Argv(2), Cmd_Argv(1));
	}
}

void MN_InitActions (void)
{
	Cmd_AddCommand("mn_addlistener", MN_AddListener_f, "Add a function into a node event");
	Cmd_AddCommand("mn_removelistener", MN_RemoveListener_f, "Remove a function from a node event");
}
