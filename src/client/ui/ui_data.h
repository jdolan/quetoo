/**
 * @file m_data.h
 * @brief Data and interface to share data
 * @todo clean up the interface
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

#ifndef CLIENT_MENU_M_DATA_H
#define CLIENT_MENU_M_DATA_H

/* prototype */
struct linkedList_s;
struct menuOption_s;

/** @brief linked into mn.menuText - defined in menu scripts via num */
typedef enum {
	TEXT_NULL,		/**< default value, should not be used */
	TEXT_STANDARD,
	TEXT_LIST,
	TEXT_POPUP,
	TEXT_POPUP_INFO,

	MAX_MENUTEXTS
} menuTextIDs_t;

typedef enum {
	MN_SHARED_NONE = 0,
	MN_SHARED_TEXT
} menuSharedType_t;

typedef struct menuSharedData_s {
	menuSharedType_t type;		/**< Type of the shared data */
	union {
		/** @brief Holds static array of characters to display */
		const char *text;
	} data;						/**< The data */
	int versionId;				/**< Id identify the value, to check changes */
} menuSharedData_t;

/* common */
int MN_GetDataVersion(int textId) __attribute__ ((warn_unused_result));
void MN_ResetData(int dataId);
int MN_GetDataIDByName(const char* name) __attribute__ ((warn_unused_result));
void MN_InitData(void);

/* text */
void MN_RegisterText(int textId, const char *text);
const char *MN_GetText(int textId) __attribute__ ((warn_unused_result));

#endif
