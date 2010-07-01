/**
 * @file m_node_text.c
 * @todo add getter/setter to cleanup access to extradata from cl_*.c files (check "u.text.")
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

#include "m_main.h"
#include "m_internal.h"
#include "m_actions.h"
#include "m_parse.h"
#include "m_font.h"
#include "m_render.h"
#include "m_node_text.h"
#include "m_node_abstractnode.h"

#include "client.h"

#define EXTRADATA(node) (node->u.text)

static void MN_TextNodeDraw(menuNode_t *node);

/**
 * @brief Change the selected line
 */
void MN_TextNodeSelectLine (menuNode_t *node, int num)
{
	EXTRADATA(node).textLineSelected = num;
}

/**
 * @brief Scrolls the text in a textbox up/down.
 * @param[in] node The node of the text to be scrolled.
 * @param[in] offset Number of lines to scroll. Positive values scroll down, negative up.
 * @return Returns qtrue if scrolling was possible otherwise qfalse.
 */
qboolean MN_TextScroll (menuNode_t *node, int offset)
{
	int textScroll_new;

	if (!node || !EXTRADATA(node).rows)
		return qfalse;

	if (EXTRADATA(node).textLines <= EXTRADATA(node).rows) {
		/* Number of lines are less than the height of the textbox. */
		EXTRADATA(node).textScroll = 0;
		return qfalse;
	}

	textScroll_new = EXTRADATA(node).textScroll + offset;

	if (textScroll_new <= 0) {
		/* Goto top line, no matter how big the offset was. */
		EXTRADATA(node).textScroll = 0;
		return qtrue;

	} else if (textScroll_new >= (EXTRADATA(node).textLines + 1 - EXTRADATA(node).rows)) {
		/* Goto last possible line, no matter how big the offset was. */
		EXTRADATA(node).textScroll = EXTRADATA(node).textLines - EXTRADATA(node).rows;
		return qtrue;

	} else {
		EXTRADATA(node).textScroll = textScroll_new;
		return qtrue;
	}
}

/**
 * @brief Scriptfunction that gets the wanted text node and scrolls the text.
 */
static void MN_TextScroll_f (void)
{
	int offset = 0;
	menuNode_t *node;
	menuNode_t *menu;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <nodename> <+/-offset>\n", Cmd_Argv(0));
		return;
	}

	menu = MN_GetActiveMenu();
	if (!menu) {
		Com_Printf("MN_TextScroll_f: No active menu\n");
		return;
	}

	node = MN_GetNodeByPath(va("%s.%s", menu->name, Cmd_Argv(1)));
	if (!node) {
		Com_DPrintf(DEBUG_CLIENT, "MN_TextScroll_f: Node '%s' not found.\n", Cmd_Argv(1));
		return;
	}

	if (!strncmp(Cmd_Argv(2), "reset", 5)) {
		EXTRADATA(node).textScroll = 0;
		return;
	}

	offset = atoi(Cmd_Argv(2));

	if (offset == 0)
		return;

	MN_TextScroll(node, offset);
}

/**
 * @brief Scroll to the bottom
 * @todo fix param to use absolute path
 */
void MN_TextScrollBottom (const char* nodeName)
{
	menuNode_t *node = MN_GetNode(MN_GetActiveMenu(), nodeName);
	if (!node) {
		Com_DPrintf(DEBUG_CLIENT, "Node '%s' could not be found\n", nodeName);
		return;
	}

	if (EXTRADATA(node).textLines > EXTRADATA(node).rows) {
		Com_DPrintf(DEBUG_CLIENT, "\nMN_TextScrollBottom: Scrolling to line %i\n", EXTRADATA(node).textLines - EXTRADATA(node).rows + 1);
		EXTRADATA(node).textScroll = EXTRADATA(node).textLines - EXTRADATA(node).rows + 1;
	}
}

/**
 * @brief Draw a scrollbar, if necessary
 * @note Needs node->textLines to be accurate
 */
static void MN_DrawScrollBar (const menuNode_t *node)
{
	static const vec4_t scrollbarBackground = {0.03, 0.41, 0.05, 0.5};
	static const vec4_t scrollbarColor = {0.03, 0.41, 0.05, 1.0};

	if (EXTRADATA(node).scrollbar && EXTRADATA(node).rows && EXTRADATA(node).textLines > EXTRADATA(node).rows) {
		vec2_t nodepos;
		int scrollbarX;
		float scrollbarY;

		MN_GetNodeAbsPos(node, nodepos);
		scrollbarX = nodepos[0] + node->size[0] - MN_SCROLLBAR_WIDTH;
		scrollbarY = node->size[1] * EXTRADATA(node).rows / EXTRADATA(node).textLines * MN_SCROLLBAR_HEIGHT;

		MN_DrawFill(scrollbarX, nodepos[1],
			MN_SCROLLBAR_WIDTH, node->size[1],
			ALIGN_UL, scrollbarBackground);

		MN_DrawFill(scrollbarX, nodepos[1] + (node->size[1] - scrollbarY) * EXTRADATA(node).textScroll / (EXTRADATA(node).textLines - EXTRADATA(node).rows),
			MN_SCROLLBAR_WIDTH, scrollbarY,
			ALIGN_UL, scrollbarColor);
	}
}

/**
 * @brief Get the line number under an absolute position
 * @param[in] node a text node
 * @param[in] x position x on the screen
 * @param[in] y position y on the screen
 * @return The line number under the position (0 = first line)
 */
int MN_TextNodeGetLine (const menuNode_t *node, int x, int y)
{
	assert(MN_NodeInstanceOf(node, "text"));

	/* if no texheight, its not a text list, result is not important */
	if (!node->u.text.lineHeight)
		return 0;

	MN_NodeAbsoluteToRelativePos(node, &x, &y);

	if (EXTRADATA(node).textScroll)
		return (int) (y / node->u.text.lineHeight) + EXTRADATA(node).textScroll;
	else
		return (int) (y / node->u.text.lineHeight);
}

static void MN_TextNodeMouseMove (menuNode_t *node, int x, int y)
{
	EXTRADATA(node).lineUnderMouse = MN_TextNodeGetLine(node, x, y);
}

/**
 * @brief Handles line breaks and drawing for MN_TEXT menu nodes
 * @param[in] text Text to draw
 * @param[in] font Font string to use
 * @param[in] node The current menu node
 * @param[in] x The fixed x position every new line starts
 * @param[in] y The fixed y position the text node starts
 */
static void MN_TextNodeDrawText (menuNode_t* node, const char *text)
{
	char textCopy[MAX_MENUTEXTLEN];
	char newFont[MAX_VAR];
	const char* oldFont = NULL;
	vec4_t colorHover;
	vec4_t colorSelectedHover;
	char *cur, *tab, *end;
	int lines;
	int x1; /* variable x position */
	const char *font = MN_GetFontFromNode(node);
	vec2_t pos;
	int x, y, width, height;

	if (!text)
		return;	/**< Nothing to draw */

	MN_GetNodeAbsPos(node, pos);

	/* text box */
	x = pos[0] + node->padding;
	y = pos[1] + node->padding;
	width = node->size[0] - node->padding - node->padding;
	height = node->size[1] - node->padding - node->padding;

	Q_strncpyz(textCopy, text, sizeof(textCopy));

	cur = textCopy;

	/* Hover darkening effect for normal text lines. */
	VectorScale(node->color, 0.8, colorHover);
	colorHover[3] = node->color[3];

	/* Hover darkening effect for selected text lines. */
	VectorScale(node->selectedColor, 0.8, colorSelectedHover);
	colorSelectedHover[3] = node->selectedColor[3];

	/* scrollbar space */
	if (EXTRADATA(node).scrollbar)
		width -= MN_SCROLLBAR_WIDTH + MN_SCROLLBAR_PADDING;

	/* fix position of the start of the draw according to the align */
	switch (node->textalign % 3) {
	case 0:	/* left */
		break;
	case 1:	/* middle */
		x += width / 2;
		break;
	case 2:	/* right */
		x += width;
		break;
	}

	R_Color(node->color);

	/*Com_Printf("\n\n\nEXTRADATA(node).textLines: %i \n", EXTRADATA(node).textLines);*/
	lines = 0;
	do {
		/* new line starts from node x position */
		x1 = x;
		if (oldFont) {
			font = oldFont;
			oldFont = NULL;
		}

		/* text styles and inline images */
		if (cur[0] == '^') {
			switch (toupper(cur[1])) {
			case 'B':
				Com_sprintf(newFont, sizeof(newFont), "%s_bold", font);
				oldFont = font;
				font = newFont;
				cur += 2; /* don't print the format string */
				break;
			}
		} else if (!strncmp(cur, TEXT_IMAGETAG, strlen(TEXT_IMAGETAG))) {
			const char *token;
			const image_t *image;
			int y1 = y;
			/* cut the image tag */
			cur += strlen(TEXT_IMAGETAG);
			token = Com_Parse((const char **)&cur);
			/** @todo fix scrolling images */
			if (lines > EXTRADATA(node).textScroll)
				y1 += (lines - EXTRADATA(node).textScroll) * node->u.text.lineHeight;
			/* don't draw images that would be out of visible area */
			if (y + height > y1 && lines >= EXTRADATA(node).textScroll) {
				/** @todo (menu) we should scale the height here with font->height, too */
				image = MN_DrawNormImageByName(x1, y1, 0, 0, 0, 0, 0, 0, node->textalign, token);
				if (image)
					x1 += image->height;
			}
		}

		/* get the position of the next newline - otherwise end will be null */
		end = strchr(cur, '\n');
		if (end)
			/* set the \n to \0 to draw only this part (before the \n) with our font renderer */
			/* let end point to the next char after the \n (or \0 now) */
			*end++ = '\0';

		/* highlighting */
		if (lines == EXTRADATA(node).textLineSelected && EXTRADATA(node).textLineSelected >= 0) {
			/* Draw current line in "selected" color (if the linenumber is stored). */
			R_Color(node->selectedColor);
		}

		if (node->state && node->mousefx && lines == EXTRADATA(node).lineUnderMouse) {
			/* Highlight line if mousefx is true. */
			/** @todo what about multiline text that should be highlighted completely? */
			if (lines == EXTRADATA(node).textLineSelected && EXTRADATA(node).textLineSelected >= 0) {
				R_Color(colorSelectedHover);
			} else {
				R_Color(colorHover);
			}
		}

		/* tabulation, we assume all the tabs fit on a single line */
		do {
			int tabwidth;
			int numtabs;

			tab = strchr(cur, '\t');
			if (!tab)
				break;

			/* use tab stop as given via menu definition format string
			 * or use 1/3 of the node size (width) */
			if (!node->u.text.tabWidth)
				tabwidth = width / 3;
			else
				tabwidth = node->u.text.tabWidth;

			numtabs = strspn(tab, "\t");
			tabwidth *= numtabs;
			while (*tab == '\t')
				*tab++ = '\0';

			/*Com_Printf("tab - first part - lines: %i \n", lines);*/
			MN_DrawString(font, node->textalign, x1, y, x, y, tabwidth - 1, height, node->u.text.lineHeight, cur, EXTRADATA(node).rows, EXTRADATA(node).textScroll, &lines, qfalse, LONGLINES_PRETTYCHOP);
			x1 += tabwidth;
			/* now skip to the first char after the \t */
			cur = tab;
		} while (1);

		/*Com_Printf("until newline - lines: %i\n", lines);*/
		/* the conditional expression at the end is a hack to draw "/n/n" as a blank line */
		/* prevent line from being drawn if there is nothing that should be drawn after it */
		if (cur && (cur[0] || end)) {
			/* is it a white line? */
			if (!cur) {
				lines++;
			} else {
				MN_DrawString(font, node->textalign, x1, y, x, y, width, height, node->u.text.lineHeight, cur, EXTRADATA(node).rows, EXTRADATA(node).textScroll, &lines, qtrue, node->longlines);
			}
		}

		if (node->mousefx)
			R_Color(node->color); /* restore original color */

		/* now set cur to the next char after the \n (see above) */
		cur = end;

		y += MN_FontGetHeight(font);
	} while (cur);

	/* content have change */
	if (EXTRADATA(node).textLines != lines) {
		EXTRADATA(node).textLines = lines;
		/* fire the change of the lines */
		if (EXTRADATA(node).onLinesChange) {
			MN_ExecuteEventActions(node, EXTRADATA(node).onLinesChange);
		}
	}

	R_Color(NULL);
	MN_DrawScrollBar(node);
}

/**
 * @brief Draw a text node
 */
static void MN_TextNodeDraw (menuNode_t *node)
{
	switch (mn.sharedData[EXTRADATA(node).num].type) {
	case MN_SHARED_TEXT:
		{
			const char* t = mn.sharedData[EXTRADATA(node).num].data.text;
			if (t[0] == '_')
				t++;
			MN_TextNodeDrawText(node, t);
		}
		break;
	default:
		break;
	}
}

/**
 * @brief Calls the script command for a text node that is clickable
 * @sa MN_TextNodeRightClick
 */
static void MN_TextNodeClick (menuNode_t * node, int x, int y)
{
	int line = MN_TextNodeGetLine(node, x, y);

	if (line < 0 || line >= EXTRADATA(node).textLines)
		return;

	MN_TextNodeSelectLine(node, line);

	if (node->onClick)
		MN_ExecuteEventActions(node, node->onClick);
}

/**
 * @brief Calls the script command for a text node that is clickable via right mouse button
 * @sa MN_TextNodeClick
 */
static void MN_TextNodeRightClick (menuNode_t * node, int x, int y)
{
	int line = MN_TextNodeGetLine(node, x, y);

	if (line < 0 || line >= EXTRADATA(node).textLines)
		return;

	MN_TextNodeSelectLine(node, line);

	if (node->onRightClick)
		MN_ExecuteEventActions(node, node->onRightClick);
}

static void MN_TextNodeMouseWheel (menuNode_t *node, qboolean down, int x, int y)
{
	if (node->onWheelUp && node->onWheelDown) {
		MN_ExecuteEventActions(node, (down ? node->onWheelDown : node->onWheelUp));
	} else {
		MN_TextScroll(node, (down ? 1 : -1));
		/* they can also have script commands assigned */
		MN_ExecuteEventActions(node, node->onWheel);
	}
}

static void MN_TextNodeLoading (menuNode_t *node)
{
	EXTRADATA(node).textLineSelected = -1; /**< Invalid/no line selected per default. */
	Vector4Set(node->selectedColor, 1.0, 1.0, 1.0, 1.0);
	Vector4Set(node->color, 1.0, 1.0, 1.0, 1.0);
}

static void MN_TextNodeLoaded (menuNode_t *node)
{
	int lineheight = node->u.text.lineHeight;
	/* auto compute lineheight */
	/* we don't overwrite node->u.text.lineHeight, because "0" is dynamically replaced by font height on draw function */
	if (lineheight == 0) {
		/* the font is used */
		const char *font = MN_GetFontFromNode(node);
		lineheight = MN_FontGetHeight(font) / 2;
	}

	/* auto compute rows */
	if (EXTRADATA(node).rows == 0) {
		if (node->size[1] != 0 && lineheight != 0) {
			EXTRADATA(node).rows = node->size[1] / lineheight;
		} else {
			EXTRADATA(node).rows = 1;
			Com_Printf("MN_TextNodeLoaded: node '%s' has no rows value\n", MN_GetPath(node));
		}
	}

	/* auto compute height */
	if (node->size[1] == 0) {
		node->size[1] = EXTRADATA(node).rows * lineheight;
	}

	/* is text slot exists */
	if (EXTRADATA(node).num >= MAX_MENUTEXTS)
		Sys_Error("Error in node %s - max menu num exceeded (num: %i, max: %i)", MN_GetPath(node), EXTRADATA(node).num, MAX_MENUTEXTS);

#ifdef DEBUG
	if (EXTRADATA(node).rows != (int)(node->size[1] / lineheight)) {
		Com_Printf("MN_TextNodeLoaded: rows value (%i) of node '%s' differs from size (%.0f) and format (%i) values\n",
			EXTRADATA(node).rows, MN_GetPath(node), node->size[1], lineheight);
	}
#endif

	if (EXTRADATA(node).num == TEXT_NULL)
		Com_Printf("MN_TextNodeLoaded: 'textid' property of node '%s' is not set\n", MN_GetPath(node));
}

static const value_t properties[] = {
	{"scrollbar", V_BOOL, offsetof(menuNode_t, u.text.scrollbar), MEMBER_SIZEOF(menuNode_t, u.text.scrollbar)},
	{"lineselected", V_INT, offsetof(menuNode_t, u.text.textLineSelected), MEMBER_SIZEOF(menuNode_t, u.text.textLineSelected)},
	{"textid", V_SPECIAL_DATAID, offsetof(menuNode_t, u.text.num), MEMBER_SIZEOF(menuNode_t, u.text.num)},
	{"rows", V_INT, offsetof(menuNode_t, u.text.rows), MEMBER_SIZEOF(menuNode_t, u.text.rows)},
	{"text_scroll", V_INT, offsetof(menuNode_t, u.text.textScroll), MEMBER_SIZEOF(menuNode_t, u.text.textScroll)},
	{"onlineschange", V_SPECIAL_ACTION, offsetof(menuNode_t, u.text.onLinesChange), MEMBER_SIZEOF(menuNode_t, u.text.onLinesChange)},
	{"lines", V_INT, offsetof(menuNode_t, u.text.textLines), MEMBER_SIZEOF(menuNode_t, u.text.textLines)},
	{"lineheight", V_INT, offsetof(menuNode_t, u.text.lineHeight), MEMBER_SIZEOF(menuNode_t, u.text.lineHeight)},
	{"tabwidth", V_INT, offsetof(menuNode_t, u.text.tabWidth), MEMBER_SIZEOF(menuNode_t, u.text.tabWidth)},
	{"longlines", V_LONGLINES, offsetof(menuNode_t, longlines), MEMBER_SIZEOF(menuNode_t, longlines)},
	/** @todo delete it went its possible (need to create a textlist) */
	{"mousefx", V_BOOL, offsetof(menuNode_t, mousefx), MEMBER_SIZEOF(menuNode_t, mousefx)},
	{NULL, V_NULL, 0, 0}
};

void MN_RegisterTextNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "text";
	behaviour->draw = MN_TextNodeDraw;
	behaviour->leftClick = MN_TextNodeClick;
	behaviour->rightClick = MN_TextNodeRightClick;
	behaviour->mouseWheel = MN_TextNodeMouseWheel;
	behaviour->mouseMove = MN_TextNodeMouseMove;
	behaviour->loading = MN_TextNodeLoading;
	behaviour->loaded = MN_TextNodeLoaded;
	behaviour->properties = properties;

	Cmd_AddCommand("mn_textscroll", MN_TextScroll_f, NULL);
}
