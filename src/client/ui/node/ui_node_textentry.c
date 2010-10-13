/**
 * @file m_node_textentry.c
 * @todo allow to edit text without any cvar
 * @todo add a custom max size
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
#include "ui_parse.h"
#include "ui_input.h"
#include "ui_actions.h"
#include "ui_render.h"
#include "ui_node_textentry.h"
#include "ui_node_abstractnode.h"
#include "ui_node_panel.h"
#include "utf8.h"

#include "client.h"

#define TILE_SIZE 64
#define CORNER_SIZE 17
#define MID_SIZE 1
#define MARGE 3

static const char CURSOR = '|';		/**< Use as the cursor when we edit the text */
static const char HIDECHAR = '*';	/**< use as a mask for password */

/* limit the input for cvar editing (base name, save slots and so on) */
#define MAX_CVAR_EDITING_LENGTH 256 /* MAXCMDLINE */

/* global data */
static char cvarValueBackup[MAX_CVAR_EDITING_LENGTH];
static cvar_t *editedCvar = NULL;
static qboolean isAborted = qfalse;

/**
 * @brief callback from the keyboard
 */
static void MN_TextEntryNodeValidateEdition (menuNode_t *node)
{
	/* invalidate cache */
	editedCvar = NULL;
	cvarValueBackup[0] = '\0';

	/* fire change event */
	if (node->onChange) {
		MN_ExecuteEventActions(node, node->onChange);
	}
}

/**
 * @brief callback from the keyboard
 */
static void MN_TextEntryNodeAbortEdition (menuNode_t *node)
{
	assert(editedCvar);

	/* set the old cvar value */
	Cvar_ForceSet(editedCvar->name, cvarValueBackup);

	/* invalidate cache */
	editedCvar = NULL;
	cvarValueBackup[0] = '\0';

	/* fire abort event */
	if (node->u.textentry.onAbort) {
		MN_ExecuteEventActions(node, node->u.textentry.onAbort);
	}
}

/**
 * @brief force edition of a textentry node
 * @note the textentry must be on the active menu
 */
static void MN_EditTextEntry_f (void)
{
	menuNode_t *node;
	const char* name;

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <textentrynode>\n", Cmd_Argv(0));
		return;
	}

	name = Cmd_Argv(1);
	node = MN_GetNode(MN_GetActiveMenu(), name);
	if (!node) {
		Com_Print("MN_EditTextEntry_f: node '%s' doesn't exist on the current active menu '%s'\n", name, MN_GetActiveMenu()->name);
		return;
	}

	/* remove the focus to show changes */
	if (!MN_HasFocus(node)) {
		MN_RequestFocus(node);
	}
}

/**
 * @brief Called when the user click with the right mouse button
 */
static void MN_TextEntryNodeClick (menuNode_t *node, int x, int y)
{
	if (node->disabled)
		return;

	/* no cvar */
	if (!node->text)
		return;
	if (strncmp(node->text, "*cvar", 5))
		return;

	if (!MN_HasFocus(node)) {
		if (node->onClick) {
			MN_ExecuteEventActions(node, node->onClick);
		}
		MN_RequestFocus(node);
	}
}

/**
 * @brief Called when the node got the focus
 */
static void MN_TextEntryFocusGained (menuNode_t *node)
{
	assert(editedCvar == NULL);
	/* skip '*cvar ' */
	editedCvar = Cvar_Get(&((char*)node->text)[6], "", 0, NULL);
	assert(editedCvar);
	Q_strncpyz(cvarValueBackup, editedCvar->string, sizeof(cvarValueBackup));
	isAborted = qfalse;
}

/**
 * @brief Called when the node lost the focus
 */
static void MN_TextEntryFocusLost (menuNode_t *node)
{
	/* already aborted/changed with the keyboard */
	if (editedCvar == NULL)
		return;

	/* release the keyboard */
	if (isAborted || node->u.textentry.clickOutAbort) {
		MN_TextEntryNodeAbortEdition(node);
	} else {
		MN_TextEntryNodeValidateEdition(node);
	}
}

/**
 * @brief edit the current cvar with a char
 */
static void MN_TextEntryNodeEdit (menuNode_t *node, unsigned int key)
{
	char buffer[MAX_CVAR_EDITING_LENGTH];
	int length;

	/* copy the cvar */
	Q_strncpyz(buffer, editedCvar->string, sizeof(buffer));
	length = strlen(buffer);

	/* compute result */
	if (key == K_BACKSPACE) {
		length = UTF8_delete_char(buffer, length - 1);
	} else {
		int charLength = UTF8_encoded_len(key);
		/* is buffer full? */
		if (length + charLength >= sizeof(buffer))
			return;

		length += UTF8_insert_char(buffer, sizeof(buffer), length, key);
	}

	/* update the cvar */
	Cvar_ForceSet(editedCvar->name, buffer);
}

/**
 * @brief Called when we press a key when the node got the focus
 * @return True, if we use the event
 */
static qboolean MN_TextEntryNodeKeyPressed (menuNode_t *node, unsigned int key, unsigned short unicode)
{
	switch (key) {
	/* remove the last char */
	case K_BACKSPACE:
		MN_TextEntryNodeEdit(node, K_BACKSPACE);
		return qtrue;
	/* cancel the edition */
	case K_ESCAPE:
		isAborted = qtrue;
		MN_RemoveFocus();
		return qtrue;
	/* validate the edition */
	case K_ENTER:
	case K_KP_ENTER:
		MN_TextEntryNodeValidateEdition(node);
		MN_RemoveFocus();
		return qtrue;
	}

	/* non printable */
	if (unicode < 32 || (unicode >= 127 && unicode < 192))
		return qfalse;

	/* add a char */
	MN_TextEntryNodeEdit(node, unicode);
	return qtrue;
}

static void MN_TextEntryNodeDraw (menuNode_t *node)
{
	static const int panelTemplate[] = {
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		MARGE
	};
	const char *text;
	int texX, texY;
	const float *textColor;
	const char *image;
	vec2_t pos;
	static vec4_t disabledColor = {0.5, 0.5, 0.5, 1.0};

	if (node->disabled) {
		/** @todo need custom color when node is disabled */
		textColor = disabledColor;
		texX = TILE_SIZE;
		texY = TILE_SIZE;
	} else if (node->state) {
		textColor = node->color;
		texX = TILE_SIZE;
		texY = 0;
	} else {
		textColor = node->color;
		texX = 0;
		texY = 0;
	}
	if (MN_HasFocus(node))
		textColor = node->selectedColor;

	MN_GetNodeAbsPos(node, pos);

	image = MN_GetReferenceString(node, node->image);
	if (image)
		MN_DrawPanel(pos, node->size, image, texX, texY, panelTemplate);

	text = MN_GetReferenceString(node, node->text);
	if (text != NULL) {
		/** @todo we don't need to edit the text to draw the cursor */
		if (MN_HasFocus(node)) {
			if (cl.time % 1000 < 500) {
				text = va("%s%c", text, CURSOR);
			}
		}

		if (node->u.textentry.isPassword) {
			char *c = va("%s", text);
			int size = UTF8_strlen(c);
			text = c;
			/* hide the text with a special char */
			assert(strlen(c) >= size);	/* trustable, but it can't be false */
			while (size) {
				*c++ = HIDECHAR;
				size--;
			}
			/* readd the cursor */
			if (MN_HasFocus(node)) {
				if (cl.time % 1000 < 500) {
					c--;
					*c++ = CURSOR;
				}
			}
			*c = '\0';
		}

		if (*text != '\0') {
			R_Color(textColor);
			MN_DrawStringInBox(node, node->textalign,
				pos[0] + node->padding, pos[1] + node->padding,
				node->size[0] - node->padding - node->padding, node->size[1] - node->padding - node->padding,
				text, LONGLINES_PRETTYCHOP);
			R_Color(NULL);
		}
	}

}

/**
 * @brief Call before the script initialization of the node
 */
static void MN_TextEntryNodeLoading (menuNode_t *node)
{
	node->padding = 8;
	node->textalign = ALIGN_CL;
	Vector4Set(node->color, 1, 1, 1, 1);
	Vector4Set(node->selectedColor, 1, 1, 1, 1);
}

static const value_t properties[] = {
	{"ispassword", V_BOOL, offsetof(menuNode_t, u.textentry.isPassword), MEMBER_SIZEOF(menuNode_t, u.textentry.isPassword) },
	{"clickoutabort", V_BOOL, offsetof(menuNode_t, u.textentry.clickOutAbort), MEMBER_SIZEOF(menuNode_t, u.textentry.clickOutAbort)},
	{"onabort", V_SPECIAL_ACTION, offsetof(menuNode_t, u.textentry.onAbort), MEMBER_SIZEOF(menuNode_t, u.textentry.onAbort)},

	{NULL, V_NULL, 0, 0}
};

void MN_RegisterTextEntryNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "textentry";
	behaviour->leftClick = MN_TextEntryNodeClick;
	behaviour->focusGained = MN_TextEntryFocusGained;
	behaviour->focusLost = MN_TextEntryFocusLost;
	behaviour->keyPressed = MN_TextEntryNodeKeyPressed;
	behaviour->draw = MN_TextEntryNodeDraw;
	behaviour->loading = MN_TextEntryNodeLoading;
	behaviour->properties = properties;

	Cmd_AddCommand("mn_edittextentry", MN_EditTextEntry_f, "Force edition of the textentry.");
}
