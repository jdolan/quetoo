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

#include "g_local.h"

/**
 * @return True if the bot entity can pick up the item entity.
 */
bool G_Ai_CanPickup(const g_client_t *cl, const g_entity_t *other) {
  const g_item_t *item = other->item;

  if (!item) {
    return false;
  }

  const int16_t *inventory = cl->inventory;

  switch (item->def.type)
  {
    case ITEM_TYPE_HEALTH:
      if (item->def.tag == HEALTH_SMALL ||
        item->def.tag == HEALTH_MEGA) {
        return true;
      }

      return cl->entity->health < cl->entity->max_health;
    case ITEM_TYPE_ARMOR:
      if (item->def.tag == ARMOR_SHARD ||
        inventory[item->def.tag] < item->def.max) {
        return true;
      }

      return false;
    case ITEM_TYPE_AMMO:
      return inventory[item->def.tag] < item->def.max;
    case ITEM_TYPE_WEAPON:
      if (inventory[item->def.tag]) {
        if (item->def.ammo) {
          return inventory[item->def.ammo] < g_items[item->def.ammo].def.max;
        }

        return false;
      }

      return true;
    case ITEM_TYPE_TECH:
      for (g_item_tag_t tag = TECH_FIRST; tag < TECH_LAST; tag++) {
        if (inventory[tag]) {
          return false;
        }
      }

      return true;
    case ITEM_TYPE_FLAG: {
      const g_team_id_t team = cl->persistent.team->id;
      const g_team_id_t flag_team = (item->def.tag - FLAG_FIRST);
      if (flag_team == team && other->owner == NULL) {
        return false;
      }

      return true;
    }

    default:
      return true;
  }
}


