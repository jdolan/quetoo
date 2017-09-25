/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "ai_local.h"

ai_item_t ai_items[MAX_ITEMS];
uint16_t ai_num_items;
uint16_t ai_num_weapons;

void Ai_RegisterItem(const uint16_t index, const ai_item_t *item) {

	if (index >= MAX_ITEMS) {
		aim.gi->Warn("Bad item ID for item\n");
		return;
	}

	if (ai_items[index].flags) { // already registered, just ignore
		return;
	}

	ai_num_items++;

	if (item->flags & AI_ITEM_WEAPON) {
		ai_num_weapons++;
	}

	memcpy(&ai_items[index], item, sizeof(ai_item_t));
}

uint16_t Ai_ItemIndex(const ai_item_t *item) {
	return item - ai_items;
}

ai_item_t *Ai_ItemForGameItem(const g_item_t *item) {
	return ai_items + aim.ItemIndex(item);
}
