/**
 * @file m_icon.c
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
#include "m_parse.h"
#include "m_icon.h"
#include "m_render.h"

/**
 * @brief normal, hovered, disabled, highlighted status are store in the same texture 256x256.
 * Each use a row of 64 pixels
 */
#define TILE_HEIGHT 64

const value_t mn_iconProperties[] = {
	{"texl", V_POS, offsetof(menuIcon_t, pos), MEMBER_SIZEOF(menuIcon_t, pos)},
	{"size", V_POS, offsetof(menuIcon_t, size), MEMBER_SIZEOF(menuIcon_t, size)},
	{"image", V_REF_OF_STRING, offsetof(menuIcon_t, image), 0},
	{"single", V_BOOL, offsetof(menuIcon_t, single), 0},
	{NULL, V_NULL, 0, 0}
};

/**
 * @brief Return an icon by is name
 * @param[in] name Name of the icon
 * @return The requested icon, else NULL
 * @note not very fast; if we use it often we should improve the search
 */
menuIcon_t* MN_GetIconByName (const char* name)
{
	int i;

	for (i = 0; i < mn.numIcons; i++) {
		if (strncmp(name, mn.menuIcons[i].name, MEMBER_SIZEOF(menuIcon_t, name)) != 0)
			continue;
		return &mn.menuIcons[i];
	}
	return NULL;
}

/**
 * @brief Allocate an icon from the menu memory
 * @note Its not a dynamic memory allocation. Please only use it at the loading time
 * @param[in] name Name of the icon
 * @todo Assert out when we are not in parsing/loading stage
 */
menuIcon_t* MN_AllocIcon (const char* name)
{
	menuIcon_t* result;
	assert(MN_GetIconByName(name) == NULL);
	if (mn.numIcons >= MAX_MENUICONS)
		Sys_Error("MN_AllocIcon: MAX_MENUICONS hit");

	result = &mn.menuIcons[mn.numIcons];
	mn.numIcons++;

	memset(result, 0, sizeof(*result));
	Q_strncpyz(result->name, name, sizeof(result->name));
	return result;
}

/**
 * @param[in] status 0:normal, 1:hover, 2:disabled, 3:clicked
 * @todo use named const for status
 */
void MN_DrawIconInBox (menuIcon_t* icon, int status, int posX, int posY, int sizeX, int sizeY)
{
	const int texX = icon->pos[0];
	int texY;
	assert(icon->image != NULL);

	if (icon->single)
		texY = icon->pos[1];
	else
		texY = icon->pos[1] + (TILE_HEIGHT * status);

	posX += (sizeX - icon->size[0]) / 2;
	posY += (sizeY - icon->size[1]) / 2;

	MN_DrawNormImageByName(posX, posY, icon->size[0], icon->size[1],
		texX + icon->size[0], texY + icon->size[1], texX, texY, ALIGN_UL, icon->image);
}
