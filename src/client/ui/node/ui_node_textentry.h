/**
 * @file m_node_textentry.h
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


#ifndef CLIENT_MENU_M_NODE_TEXT_ENTRY_H
#define CLIENT_MENU_M_NODE_TEXT_ENTRY_H

#include "ui_nodes.h"

/**
 * @brief extradata for the textentry, to custom draw and behaviour
 */
typedef struct textEntryExtraData_s {
	qboolean isPassword;	/**< Display '*' instead of the real text */
	qboolean clickOutAbort;	/**< If we click out an activated node, it abort the edition */
	struct menuAction_s *onAbort;
} textEntryExtraData_t;

void MN_RegisterTextEntryNode(struct nodeBehaviour_s *behaviour);

#endif
