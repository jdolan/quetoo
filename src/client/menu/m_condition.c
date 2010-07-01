/**
 * @file m_condition.c
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

#include "m_condition.h"
#include "m_main.h"
#include "m_internal.h"
#include "m_parse.h"
#include "node/m_node_abstractnode.h"

static const char *const if_strings[] = {
	"==",
	"<=",
	">=",
	">",
	"<",
	"!=",
	"",
	"eq",
	"ne"
};
CASSERT(lengthof(if_strings) == IF_SIZE);

/**
 * @todo code the FLOAT/STRING type into the opcode; we should not check it like that
 */
static inline qboolean MN_IsFloatOperator (menuConditionOpCodeType_t op)
{
	return op == IF_EQ
	 || op == IF_LE
	 || op == IF_GE
	 || op == IF_GT
	 || op == IF_LT
	 || op == IF_NE;
}

/**
 * @return A float value, else 0
 */
static float MN_GetFloatFromParam (const menuNode_t *source, const char* value, menuConditionValueType_t type)
{
	switch (type) {
	case IF_VALUE_STRING:
		return atof(value);
	case IF_VALUE_FLOAT:
		return *(const float*)value;
	case IF_VALUE_CVARNAME:
		{
			cvar_t *cvar = NULL;
			cvar = Cvar_Get(value, "", 0, "Menu if condition cvar");
			return cvar->value;
		}
	case IF_VALUE_NODEPROPERTY:
		{
			menuNode_t *node;
			const value_t *property;
			MN_ReadNodePath(value, source, &node, &property);
			if (!node) {
				Com_Printf("MN_GetFloatFromParam: Node '%s' wasn't found; '0' returned", value);
				return 0;
			}
			if (!property) {
				Com_Printf("MN_GetFloatFromParam: Property '%s' wasn't found; '0' returned", value);
				return 0;
			}
			return MN_GetFloatFromNodeProperty (node, property);
		}
	}
	return 0;
}

/**
 * @return A string, else an empty string
 */
static const char* MN_GetStringFromParam (const menuNode_t *source, const char* value, menuConditionValueType_t type)
{
	switch (type) {
	case IF_VALUE_STRING:
		return value;
	case IF_VALUE_FLOAT:
		assert(qfalse);
	case IF_VALUE_CVARNAME:
	{
		cvar_t *cvar = NULL;
		cvar = Cvar_Get(value, "", 0, "Menu if condition cvar");
		return cvar->string;
	}
	case IF_VALUE_NODEPROPERTY:
		Com_Printf("MN_GetStringFromParam: Node property '%s' to string unsupported", value);
		break;
	}
	return "";
}


/**
 * @brief Check the if conditions for a given node
 * @return True if the condition is qfalse if the node is not drawn
 */
qboolean MN_CheckCondition (const menuNode_t *source, menuCondition_t *condition)
{
	if (MN_IsFloatOperator(condition->type.opCode)) {
		const float value1 = MN_GetFloatFromParam(source, condition->leftValue, condition->type.left);
		const float value2 = MN_GetFloatFromParam(source, condition->rightValue, condition->type.right);

		switch (condition->type.opCode) {
		case IF_EQ:
			return value1 == value2;
		case IF_LE:
			return value1 <= value2;
		case IF_GE:
			return value1 >= value2;
		case IF_GT:
			return value1 > value2;
		case IF_LT:
			return value1 < value2;
		case IF_NE:
			return value1 != value2;
		default:
			assert(qfalse);
		}
	}

	if (condition->type.opCode == IF_EXISTS) {
		if (condition->type.left == IF_VALUE_CVARNAME) {
			return Cvar_FindVar(condition->leftValue) != NULL;
		}
		assert(qfalse);
	}

	if (condition->type.opCode == IF_STR_EQ || condition->type.opCode == IF_STR_NE) {
		const char* value1 = MN_GetStringFromParam(source, condition->leftValue, condition->type.left);
		const char* value2 = MN_GetStringFromParam(source, condition->rightValue, condition->type.right);

		switch (condition->type.opCode) {
		case IF_STR_EQ:
			return strcmp(value1, value2) == 0;
		case IF_STR_NE:
			return strcmp(value1, value2) != 0;
		default:
			assert(qfalse);
		}
	}

	Sys_Error("Unknown condition for if statement: %i", condition->type.opCode);
}

/**
 * @brief Translate the condition string to menuIfCondition_t enum value
 * @param[in] conditionString The string from scriptfiles (see if_strings)
 * @return menuIfCondition_t value
 * @return enum value for condition string
 * @note Produces a Sys_Error if conditionString was not found in if_strings array
 */
static int MN_GetOperatorByName (const char* operatorName)
{
	int i;
	for (i = 0; i < IF_SIZE; i++) {
		if (!strcmp(if_strings[i], operatorName)) {
			return i;
		}
	}
	return IF_INVALID;
}

/**
 * @brief Set a condition param from a string and an opcode
 * @param[in] string Into string
 * @param[in] opCode Current operator type
 * @param[out] value Data to identify the value
 * @param[out] type Type on the value
 */
static inline void MN_SetParam (const char *string, menuConditionOpCodeType_t opCode, const char** value, menuConditionValueType_t* type)
{
	/* it is a cvarname */
	if (!strncmp(string, "*cvar:", 6)) {
		const char* cvarName = string + 6;
		*value = MN_AllocString(cvarName, 0);
		*type = IF_VALUE_CVARNAME;
		return;
	}

	/* it is a node property */
	if (!strncmp(string, "*node:", 6)) {
		const char* path = string + 6;
		*value = MN_AllocString(path, 0);
		*type = IF_VALUE_NODEPROPERTY;
		return;
	}

	/* it is a const float */
	if (MN_IsFloatOperator(opCode)) {
		float *f = MN_AllocFloat(1);
		*f = atof(string);
		*value = (char*) f;
		*type = IF_VALUE_FLOAT;
		return;
	}

	/* it is a const string */
	*value = MN_AllocString(string, 0);
	*type = IF_VALUE_STRING;
}

#define BUF_SIZE (MAX_VAR - 1)
/**
 * @brief Initialize a condition according to a string
 * @param[out] condition Condition to initialize
 * @param[in] token String describing a condition
 * @return True if the condition is initialized
 */
qboolean MN_InitCondition (menuCondition_t *condition, const char *token)
{
	memset(condition, 0, sizeof(*condition));
	if (!strstr(token, " ")) {
		if (strncmp(token, "*cvar:", 6) != 0) {
			Com_Printf("Invalid 'if' statement. '%s' is not a cvar\n", token);
			return qfalse;
		}
		condition->leftValue = MN_AllocString(token + 6, 0);
		condition->type.left = IF_VALUE_CVARNAME;
		condition->type.opCode = IF_EXISTS;
	} else {
		char param1[BUF_SIZE + 1];
		char operator[BUF_SIZE + 1];
		char param2[BUF_SIZE + 1];
		if (sscanf(token, "%"DOUBLEQUOTE(MAX_VAR)"s %"DOUBLEQUOTE(MAX_VAR)"s %"DOUBLEQUOTE(MAX_VAR)"s", param1, operator, param2) != 3) {
			Com_Printf("MN_InitCondition: Could not parse node condition.\n");
			return qfalse;
		}

		/* operator code */
		condition->type.opCode = MN_GetOperatorByName(operator);
		if (condition->type.opCode == IF_INVALID) {
			Com_Printf("Invalid 'if' statement. Unknown '%s' operator from token: '%s'\n", operator, token);
			return qfalse;
		}
		MN_SetParam(param1, condition->type.opCode, &condition->leftValue, &condition->type.left);
		MN_SetParam(param2, condition->type.opCode, &condition->rightValue, &condition->type.right);
	}

	/* prevent wrong code */
	if (condition->type.left == condition->type.right
		 && (condition->type.left == IF_VALUE_STRING || condition->type.left == IF_VALUE_FLOAT)) {
		Com_Printf("MN_InitCondition: '%s' dont use any var operand.\n", token);
	}

	return qtrue;
}

/**
 * @brief Allocate and initialize a condition according to a string
 * @param[in] token String describing a condition
 * @param[out] condition Condition to initialize
 * @return The condition if everything is ok, NULL otherwise
 */
menuCondition_t *MN_AllocCondition (const char *description)
{
	menuCondition_t condition;
	qboolean result;

	if (mn.numConditions >= MAX_MENUCONDITIONS)
		Sys_Error("MN_AllocCondition: Too many menu conditions");

	result = MN_InitCondition(&condition, description);
	if (!result)
		return NULL;

	/* allocates memory */
	mn.menuConditions[mn.numConditions] = condition;
	mn.numConditions++;

	return &mn.menuConditions[mn.numConditions - 1];
}
