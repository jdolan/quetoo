/**
 * @file m_parse.h
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

#ifndef CLIENT_MENU_M_PARSE_H
#define CLIENT_MENU_M_PARSE_H

#include "scripts.h"

struct menuNode_s;

void MN_ParseMenu(const char *type, const char *name, const char **text);
void MN_ParseIcon(const char *name, const char **text);
float MN_GetReferenceFloat(const struct menuNode_s* const node, void *ref);
const char *MN_GetReferenceString(const struct menuNode_s* const node, const char *ref) __attribute__ ((warn_unused_result));
const value_t* MN_FindPropertyByName(const value_t* propertyList, const char* name) __attribute__ ((warn_unused_result));
char* MN_AllocString(const char* string, int size) __attribute__ ((warn_unused_result));
float* MN_AllocFloat(int count) __attribute__ ((warn_unused_result));
vec4_t* MN_AllocColor(int count) __attribute__ ((warn_unused_result));

qboolean MN_ScriptSanityCheck(void);

/* main special type */
/** @todo we should split/flag parse type (type need only 1 lex; and other) */
#define	V_SPECIAL_TYPE			0x8F00
#define	V_SPECIAL				0x8000
#define	V_SPECIAL_ACTION		(V_SPECIAL + 0)			/**< Identify an action type into the value_t structure */
#define V_SPECIAL_EXCLUDERECT	(V_SPECIAL + 1)			/**< Identify a special attribute, use special parse function */
#define V_SPECIAL_ICONREF		(V_SPECIAL + 3) 		/**< Identify a special attribute, use special parse function */
#define V_SPECIAL_IF			(V_SPECIAL + 4)			/**< Identify a special attribute, use special parse function */
#define V_SPECIAL_DATAID		(V_SPECIAL + 5)
#define V_SPECIAL_CVAR			(V_SPECIAL + 0x0100) 	/**< Property is a CVAR string */
#define V_SPECIAL_REF			(V_SPECIAL + 0x0200) 	/**< Property is a ref into a value */

/* composite special type */
#define V_CVAR_OR_FLOAT			(V_SPECIAL_CVAR + V_FLOAT)
#define V_CVAR_OR_STRING		(V_SPECIAL_CVAR + V_STRING)
#define V_CVAR_OR_LONGSTRING	(V_SPECIAL_CVAR + V_LONGSTRING)
#define V_REF_OF_STRING			(V_SPECIAL_REF + V_STRING)

#endif
