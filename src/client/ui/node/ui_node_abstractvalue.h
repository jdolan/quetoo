/**
 * @file m_node_abstractvalue.h
 * @brief Define common thing for GUI controls which allow to
 * edit a value (scroolbar, spinner, and more)
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

#ifndef CLIENT_MENU_M_NODE_ABSTRACTVALUE_H
#define CLIENT_MENU_M_NODE_ABSTRACTVALUE_H

/**
 * @brief extradata for common GUI widget which allow to
 * edit a value (scrollbar, spinner, and more)
 */
typedef struct abstractValueExtraData_s {
	void* min;	/**< Min value can take the value field */
	void* max;	/**< Max value can take the value field */
	void* value;	/**< Current value */
	void* delta;	/**< Quantity the control add or remove in one step */
	float lastdiff;	/**< Different of the value from the last update. Its more an event property than a node property */
} abstractValueExtraData_t;

struct nodeBehaviour_s; /* prototype */

void MN_RegisterAbstractValueNode(struct nodeBehaviour_s *behaviour);

#endif
