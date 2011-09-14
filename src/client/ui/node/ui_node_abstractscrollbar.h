/**
 * @file m_node_abstractscrollbar.h
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


#ifndef CLIENT_MENU_M_NODE_ABSTRACTSCROLLBAR_H
#define CLIENT_MENU_M_NODE_ABSTRACTSCROLLBAR_H

#include "scripts.h"

/**
 * @brief extra_data for scrollbar widget
 * @todo think about switching to percent when its possible (lowPosition, hightPosition)
 * @todo think about adding a "direction" property and merging v and h scrollbar
 */
typedef struct abstractScrollbarExtraData_s {
	int pos;		/**< Position of the visible size */
	int viewsize;	/**< Visible size */
	int fullsize;	/**< Full size allowed */
	int lastdiff;	/**< Different of the pos from the last update. Its more an event property than a node property */
	boolean_t hideWhenUnused; /** Hide the scrollbar when we can't scroll anything */
} abstractScrollbarExtraData_t;

struct nodeBehaviour_s; /* prototype */

void MN_RegisterAbstractScrollbarNode(struct nodeBehaviour_s *behaviour);

#endif
