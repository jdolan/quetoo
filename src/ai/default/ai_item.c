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

ai_item_data_t ai_item_data;

g_item_t const *ai_items[MAX_ITEMS];

uint16_t ai_num_items;
uint16_t ai_num_weapons;

/**
 * @brief
 */
void Ai_RegisterItem(const g_item_t *item) {

	const uint16_t index = ITEM_DATA(item, index);

	if (index >= MAX_ITEMS) {
		aim.gi->Warn("Bad index for item: %d\n", index);
		return;
	}

	if (ai_items[index]) {
		return;
	}

	ai_items[index] = item;
	ai_num_items++;

	if (ITEM_DATA(item, type) == ITEM_WEAPON) {
		ai_num_weapons++;
	}
}

/**
 * @return True if the bot entity can pick up the item entity.
 */
_Bool Ai_CanPickupItem(const g_entity_t *self, const g_entity_t *other) {
	const g_item_t *item = ENTITY_DATA(other, item);

	if (!item) {
		return false;
	}

	const int16_t *inventory = &CLIENT_DATA(self->client, inventory);

	if (ITEM_DATA(item, type) == ITEM_HEALTH) {
		if (ITEM_DATA(item, tag) == HEALTH_SMALL ||
			ITEM_DATA(item, tag) == HEALTH_MEGA) {
			return true;
		}

		return ENTITY_DATA(self, health) < ENTITY_DATA(self, max_health);
	} else if (ITEM_DATA(item, type) == ITEM_ARMOR) {

		if (ITEM_DATA(item, tag) == ARMOR_SHARD ||
			inventory[ITEM_DATA(item, index)] < ITEM_DATA(item, max)) {
			return true;
		}

		return false;
	} else if (ITEM_DATA(item, type) == ITEM_AMMO) {
		return inventory[ITEM_DATA(item, index)] < ITEM_DATA(item, max);
	} else if (ITEM_DATA(item, type) == ITEM_WEAPON) {

		if (inventory[ITEM_DATA(item, index)]) {
			const g_item_t *ammo = ITEM_DATA(item, ammo);
			if (ammo) {
				return inventory[ITEM_DATA(ammo, index)] < ITEM_DATA(ammo, max);
			}

			return false;
		}

		return true;
	} else if (ITEM_DATA(item, type) == ITEM_TECH) {

		const g_item_t **it = ai_items;
		for (int32_t i = 0; i < ai_num_items; i++, it++) {
			if (ITEM_DATA(*it, type) == ITEM_TECH) {
				if (inventory[ITEM_DATA(*it, index)]) {
					return false;
				}
			}
		}

		return true;
	} else if (ITEM_DATA(item, type) == ITEM_FLAG) {

		const g_team_id_t team = CLIENT_DATA(self->client, team);
		if ((g_team_id_t) ITEM_DATA(item, tag) == team && other->owner == NULL) {
			return false;
		} else {
			return true;
		}
	}

	return true;
}

/**
 * @brief
 */
void Ai_InitItems(void) {

	memset(ai_items, 0, sizeof(ai_items));

	ai_num_items = 0;
	ai_num_weapons = 0;
}

/**
 * @brief
 */
void Ai_ShutdownItems(void) {

}
