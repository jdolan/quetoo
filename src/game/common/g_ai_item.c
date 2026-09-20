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
bool G_Ai_CanPickup(const GameClient *cl, const GameEntity *other) {
  const GameItem *item = other->item;

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

      return cl->entity->health < cl->entity->maxHealth;
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
          return inventory[item->def.ammo] < gItems[item->def.ammo].def.max;
        }

        return false;
      }

      return true;
#if defined(G_TECH)
    case ITEM_TYPE_TECH:
      for (GameItemTag tag = TECH_FIRST; tag < TECH_LAST; tag++) {
        if (inventory[tag]) {
          return false;
        }
      }

      return true;
#endif
#if defined(G_CTF)
    case ITEM_TYPE_FLAG: {
      const GameTeamId team = cl->persistent.team->id;
      const GameTeamId flagTeam = (item->def.tag - FLAG_FIRST);
      if (flagTeam == team && other->owner == NULL) {
        return false;
      }

      return true;
    }
#endif

    default:
      return true;
  }
}


