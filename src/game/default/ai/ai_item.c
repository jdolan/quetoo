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

#include "../g_local.h"

size_t ai_num_weapons;

/**
 * @return True if the bot entity can pick up the item entity.
 */
_Bool Ai_CanPickupItem(const g_entity_t *self, const g_entity_t *other) {
	const g_item_t *item = other->locals.item;

	if (!item) {
		return false;
	}

	const int16_t *inventory = self->client->locals.inventory;

	switch (item->type)
	{
		case ITEM_HEALTH:
			if (item->tag == HEALTH_SMALL ||
				item->tag == HEALTH_MEGA) {
				return true;
			}

			return self->locals.health < self->locals.max_health;
		case ITEM_ARMOR:
			if (item->tag == ARMOR_SHARD ||
				inventory[item->index] < item->max) {
				return true;
			}

			return false;
		case ITEM_AMMO:
			return inventory[item->index] < item->max;
		case ITEM_WEAPON:
			if (inventory[item->index]) {
				if (item->ammo_item) {
					return inventory[item->ammo_item->index] < item->ammo_item->max;
				}

				return false;
			}

			return true;
		case ITEM_TECH:
			for (size_t i = 0; i < g_num_items; i++) {
				if (G_ItemByIndex(i)->type == ITEM_TECH) {
					if (inventory[i]) {
						return false;
					}
				}
			}

			return true;
		case ITEM_FLAG: {
			const g_team_id_t team = self->client->locals.persistent.team->id;
			if ((g_team_id_t) item->tag == team && other->owner == NULL) {
				return false;
			}

			return true;
		}

		default:
			return true;
	}
}

/**
 * @brief
 */
void Ai_InitItems(void) {

	ai_num_weapons = 0;

	for (uint16_t i = 0; i < g_num_items; i++) {
		if (G_ItemByIndex(i)->type == ITEM_WEAPON) {
			ai_num_weapons++;
		}
	}
}
