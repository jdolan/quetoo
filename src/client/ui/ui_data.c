/**
 * @file m_data.c
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

/**
 * @brief
 */
static const char *const menutextid_names[] = {
	"", /**< NULL value */
	"TEXT_STANDARD",
	"TEXT_LIST",
	"TEXT_POPUP",
	"TEXT_POPUP_INFO"
};
CASSERT(lengthof(menutextid_names) == MAX_MENUTEXTS);

/**
 * @brief Return a dataId by name
 * @return A dataId if data found, else -1
 */
int MN_GetDataIDByName (const char* name)
{
	int num;
	for (num = 0; num < MAX_MENUTEXTS; num++)
		if (!strcmp(name, menutextid_names[num]))
			return num;

	return -1;
}

/**
 * @brief link a text to a menu text id
 * @note The menu doesn't manage the text memory, only save a pointer
 */
void MN_RegisterText (int textId, const char *text)
{
	mn.sharedData[textId].type = MN_SHARED_TEXT;
	mn.sharedData[textId].data.text = text;
	mn.sharedData[textId].versionId++;
}

const char *MN_GetText (int textId)
{
	if (mn.sharedData[textId].type != MN_SHARED_TEXT)
		return NULL;
	return mn.sharedData[textId].data.text;
}

int MN_GetDataVersion (int textId)
{
	return mn.sharedData[textId].versionId;
}

/**
 * @brief Reset a shared data. Type became NONE and value became NULL
 */
void MN_ResetData (int dataId)
{
	assert(dataId < MAX_MENUTEXTS);
	assert(dataId >= 0);

	mn.sharedData[dataId].type = MN_SHARED_NONE;
	mn.sharedData[dataId].data.text = NULL;
	mn.sharedData[dataId].versionId++;
}

/**
 * @brief Resets the mn.menuText pointers from a func node
 * @note You can give this function a parameter to only delete a specific data
 * @sa menutextid_names
 */
static void MN_ResetData_f (void)
{
	if (Cmd_Argc() == 2) {
		const char *menuTextID = Cmd_Argv(1);
		const int id = MN_GetDataIDByName(menuTextID);
		if (id < 0)
			Com_Print("%s: invalid mn.menuText ID: %s\n", Cmd_Argv(0), menuTextID);
		else
			MN_ResetData(id);
	} else {
		int i;
		for (i = 0; i < MAX_MENUTEXTS; i++)
			MN_ResetData(i);
	}
}

/**
 * @brief Initialize console command about shared menu data
 * @note called by MN_Init
 */
void MN_InitData (void)
{
	Cmd_AddCommand("mn_datareset", MN_ResetData_f, "Resets a menu data pointers");
}
