/**
 * @file m_node_text.h
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

#ifndef CLIENT_MENU_M_NODE_TEXT_H
#define CLIENT_MENU_M_NODE_TEXT_H

#define MAX_MENUTEXTLEN		32768
/* used to speed up buffer safe string copies */
#define MAX_SMALLMENUTEXTLEN	1024

#define TEXT_IMAGETAG "img:"

#define MAX_MESSAGE_TEXT 256

/* bar and background have the same width */
#define MN_SCROLLBAR_WIDTH 10
/* actual height: node height * displayed lines / all lines * this multiplier, must be less than or equal one */
#define MN_SCROLLBAR_HEIGHT 1
/* space between text and scrollbar */
#define MN_SCROLLBAR_PADDING 10

struct nodeBehaviour_s;
struct menuAction_s;
struct menuNode_s;

void MN_TextScrollBottom(const char* nodeName);
qboolean MN_TextScroll(struct menuNode_s *node, int offset);
int MN_TextNodeGetLine(const struct menuNode_s *node, int x, int y);
void MN_TextNodeSelectLine(struct menuNode_s *node, int num);

void MN_RegisterTextNode(struct nodeBehaviour_s *behaviour);

typedef struct {
	qboolean scrollbar;			/**< if you want to add a scrollbar to a text node, set this to true */
	int textScroll;				/**< textfields - current scroll position */
	int textLines;				/**< How many lines there are */
	int textLineSelected;		/**< Which line is currenlty selected? This counts only visible lines). Add textScroll to this value to get total linecount. @sa selectedColor below.*/
	int lineUnderMouse;			/**< MN_TEXT: The line under the mouse, when the mouse is over the node */
	int num;					/**< textfields: menutexts-id - baselayouts: baseID */
	int rows;					/**< textfields: max. rows to show */
	struct menuAction_s *onLinesChange;
	int lineHeight;			/**< size between two lines */
	int tabWidth;				/**< max size of a tabulation */

} textExtraData_t;

#endif
