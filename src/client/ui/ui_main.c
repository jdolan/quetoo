/**
 * @file m_main.c
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
#include "ui_draw.h"
#include "ui_timer.h"

#include "client.h"

menuGlobal_t mn;
struct memPool_s *mn_dynStringPool;
struct memPool_s *cl_menuSysPool;

/**
 * @sa MN_DisplayNotice
 * @todo move it into a better file
 */
static void MN_CheckCvar (const cvar_t *cvar)
{
	if (cvar->modified) {
		if (cvar->flags & CVAR_R_CONTEXT) {
			MN_DisplayNotice(_("This change requires a restart"), 2000);
		} else if (cvar->flags & CVAR_R_IMAGES) {
			MN_DisplayNotice(_("This change might require a restart"), 2000);
		}
	}
}

/**
 * @param[in] str Might be NULL if you want to set a float value
 * @todo move it into a better file
 */
void MN_SetCvar (const char *name, const char *str, float value)
{
	const cvar_t *cvar;
	cvar = Cvar_FindVar(name);
	if (!cvar) {
		Com_Print("Could not find cvar '%s'\n", name);
		return;
	}
	/* strip '*cvar ' from data[0] - length is already checked above */
	if (str)
		Cvar_Set(cvar->name, str);
	else
		Cvar_SetValue(cvar->name, value);
	MN_CheckCvar(cvar);
}

/**
 * @brief Adds a given value to the numeric representation of a cvar. Also
 * performs min and max check for that value.
 * @sa MN_ModifyWrap_f
 */
static void MN_Modify_f (void)
{
	float value;

	if (Cmd_Argc() < 5)
		Com_Print("Usage: %s <name> <amount> <min> <max>\n", Cmd_Argv(0));

	value = Cvar_GetValue(Cmd_Argv(1));
	value += atof(Cmd_Argv(2));
	if (value < atof(Cmd_Argv(3)))
		value = atof(Cmd_Argv(3));
	else if (value > atof(Cmd_Argv(4)))
		value = atof(Cmd_Argv(4));

	Cvar_SetValue(Cmd_Argv(1), value);
}

/**
 * @brief Adds a given value to the numeric representation of a cvar. Also
 * performs min and max check for that value. If there would be an overflow
 * we use the min value - and if there would be an underrun, we use the max
 * value.
 * @sa MN_Modify_f
 */
static void MN_ModifyWrap_f (void)
{
	float value;

	if (Cmd_Argc() < 5) {
		Com_Print("Usage: %s <name> <amount> <min> <max>\n", Cmd_Argv(0));
		return;
	}

	value = Cvar_GetValue(Cmd_Argv(1));
	value += atof(Cmd_Argv(2));
	if (value < atof(Cmd_Argv(3)))
		value = atof(Cmd_Argv(4));
	else if (value > atof(Cmd_Argv(4)))
		value = atof(Cmd_Argv(3));

	Cvar_SetValue(Cmd_Argv(1), value);
}

/**
 * @brief Shows the corresponding strings in menu
 * Example: Optionsmenu - fullscreen: yes
 */
static void MN_Translate_f (void)
{
	qboolean next;
	const char *current, *list;
	char *orig, *trans;
	char original[MAX_VAR], translation[MAX_VAR];

	if (Cmd_Argc() < 4) {
		Com_Print("Usage: %s <source> <dest> <list>\n", Cmd_Argv(0));
		return;
	}

	current = Cvar_GetString(Cmd_Argv(1));
	list = Cmd_Argv(3);
	next = qfalse;

	while (list[0] != '\0') {
		orig = original;
		while (list[0] != '\0' && list[0] != ':') {
			/** @todo overflow check */
			*orig++ = *list++;
		}
		*orig = '\0';
		list++;

		trans = translation;
		while (list[0] != '\0' && list[0] != ',') {
			/** @todo overflow check */
			*trans++ = *list++;
		}
		*trans = '\0';
		list++;

		if (!strcmp(current, original)) {
			Cvar_Set(Cmd_Argv(2), _(translation));
			return;
		}
	}

	if (current[0] != '\0') {
		/* nothing found, copy value */
		Cvar_Set(Cmd_Argv(2), _(current));
	} else {
		Cvar_Set(Cmd_Argv(2), "");
	}
}

#ifdef DEBUG
/**
 * @brief display info about menu memory
 * @todo it can be nice to have number of nodes per behaviours
 */
static void MN_Memory_f (void)
{
	Com_Print("\tAllocation:\n");
	Com_Print("\t-Option allocation: %i/%i\n", mn.numOptions, MAX_MENUOPTIONS);
	Com_Print("\t-Node allocation: %i/%i\n", mn.numNodes, MAX_MENUNODES);
	Com_Print("\t-Menu allocation: %i/%i\n", mn.numMenus, MAX_MENUS);
	Com_Print("\t-Rendering menu stack slot: %i\n", MAX_MENUSTACK);
	Com_Print("\t-Action allocation: %i/%i\n", mn.numActions, MAX_MENUACTIONS);
	Com_Print("\t-Model allocation: %i/%i\n", mn.numMenuModels, MAX_MENUMODELS);
	Com_Print("\t-Exclude rect allocation: %i/%i\n", mn.numExcludeRect, MAX_EXLUDERECTS);
	Com_Print("\t-Condition allocation: %i/%i\n", mn.numConditions, MAX_MENUCONDITIONS);
	Com_Print("\t-AData allocation: "UFO_SIZE_T"/%i B\n", (ptrdiff_t)(mn.curadata - mn.adata), mn.adataize);
	Com_Print("\tMemory:\n");
	Com_Print("\t-Option structure size: "UFO_SIZE_T" B\n", sizeof(menuOption_t));
	Com_Print("\t-Node structure size: "UFO_SIZE_T" B\n", sizeof(menuNode_t));
	Com_Print("\t-Extra data node structure size: "UFO_SIZE_T" B\n", MEMBER_SIZEOF(menuNode_t, u));
	Com_Print("\t\t-abstractvalue: "UFO_SIZE_T" B\n", MEMBER_SIZEOF(menuNode_t, u.abstractvalue));
	Com_Print("\t\t-abstractscrollbar: "UFO_SIZE_T" B\n", MEMBER_SIZEOF(menuNode_t, u.abstractscrollbar));
	Com_Print("\t\t-container: "UFO_SIZE_T" B\n", MEMBER_SIZEOF(menuNode_t, u.container));
	Com_Print("\t\t-linechart: "UFO_SIZE_T" B\n", MEMBER_SIZEOF(menuNode_t, u.linechart));
	Com_Print("\t\t-model: "UFO_SIZE_T" B\n", MEMBER_SIZEOF(menuNode_t, u.model));
	Com_Print("\t\t-option: "UFO_SIZE_T" B\n", MEMBER_SIZEOF(menuNode_t, u.option));
	Com_Print("\t\t-textentry: "UFO_SIZE_T" B\n", MEMBER_SIZEOF(menuNode_t, u.textentry));
	Com_Print("\t\t-text: "UFO_SIZE_T" B\n", MEMBER_SIZEOF(menuNode_t, u.text));
	Com_Print("\t\t-window: "UFO_SIZE_T" B\n", MEMBER_SIZEOF(menuNode_t, u.window));
	Com_Print("\t-Action structure size: "UFO_SIZE_T" B\n", sizeof(menuAction_t));
	Com_Print("\t-Model structure size: "UFO_SIZE_T" B\n", sizeof(menuModel_t));
	Com_Print("\t-Condition structure size: "UFO_SIZE_T" B\n", sizeof(menuCondition_t));
	Com_Print("\t-AData size: %i B\n", mn.adataize);
	Com_Print("\t-Full size: "UFO_SIZE_T" B\n", sizeof(menuGlobal_t) + mn.adataize);
}
#endif

/**
 * @brief Reset and free the menu data hunk
 * @note Even called in case of an error when CL_Shutdown was called - maybe even
 * before CL_InitLocal (and thus MN_InitStartup) was called
 * @sa CL_Shutdown
 * @sa MN_InitStartup
 */
void MN_Shutdown (void)
{
	if (mn.adataize)
		Mem_Free(mn.adata);
	mn.adata = NULL;
	mn.adataize = 0;

	Mem_Shutdown();
}

#define MENU_HUNK_SIZE 0x40000

void MN_Init (void)
{
#ifdef DEBUG
	MN_UnittestTimer();
#endif

	/* reset menu structures */
	memset(&mn, 0, sizeof(mn));

	/* add menu commands */
	Cmd_AddCommand("mn_modify", MN_Modify_f, NULL);
	Cmd_AddCommand("mn_modifywrap", MN_ModifyWrap_f, NULL);
	Cmd_AddCommand("mn_translate", MN_Translate_f, NULL);
#ifdef DEBUG
	Cmd_AddCommand("debug_mnmemory", MN_Memory_f, "Display info about menu memory allocation");
#endif

	Mem_Init();

	cl_menuSysPool = Mem_CreatePool("Menu: Main");
	mn_dynStringPool = Mem_CreatePool("Menu: Dynamic string");

	/* 256kb */
	mn.adataize = MENU_HUNK_SIZE;
	mn.adata = (byte*)Mem_PoolAlloc(mn.adataize, cl_menuSysPool, 0);
	mn.curadata = mn.adata;

	MN_InitData();
	MN_InitNodes();
	MN_InitMenus();
	MN_InitDraw();
	MN_InitActions();
}
