/**
 * @file m_condition.h
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

#ifndef CLIENT_MENU_M_CONDITION_H
#define CLIENT_MENU_M_CONDITION_H

#include "scripts.h"

/* prototype */
struct cvar_s;
struct menuNode_s;

#define MAX_MENUCONDITIONS 512

/**
 * @brief conditions for V_SPECIAL_IF
 */
typedef enum menuConditionOpCodeType_s {
	IF_INVALID = -1,
	/** float compares */
	IF_EQ = 0, /**< == */
	IF_LE, /**< <= */
	IF_GE, /**< >= */
	IF_GT, /**< > */
	IF_LT, /**< < */
	IF_NE = 5, /**< != */
	IF_EXISTS, /**< only cvar given - check for existence */

	/** string compares */
	IF_STR_EQ,	/**< eq */
	IF_STR_NE,	/**< ne */

	IF_SIZE
} menuConditionOpCodeType_t;

typedef enum {
	IF_VALUE_STRING,
	IF_VALUE_FLOAT,
	IF_VALUE_CVARNAME,
	IF_VALUE_NODEPROPERTY,
} menuConditionValueType_t;

typedef struct {
	menuConditionOpCodeType_t opCode;
	menuConditionValueType_t left;
	menuConditionValueType_t right;
} menuConditionType_t;

/**
 * @sa menuIfCondition_t
 */
typedef struct menuCondition_s {
	menuConditionType_t type;
	const char *leftValue;
	const char *rightValue;
} menuCondition_t;

boolean_t MN_CheckCondition(const struct menuNode_s *source, menuCondition_t *condition);
boolean_t MN_InitCondition(menuCondition_t *condition, const char *token);
menuCondition_t *MN_AllocCondition(const char *description) __attribute__ ((warn_unused_result));

#endif
