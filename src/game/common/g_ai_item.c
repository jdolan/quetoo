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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "g_local.h"

/**
 * @return The share, from 0 to 1, of the ammo item `ammo` that the client would gain from `count`.
 */
static float G_Ai_AmmoNeed(const GameClient *cl, const GameItem *ammo, const int32_t count) {

  if (!ammo || count <= 0) {
    return 0.f;
  }

  const int32_t max = G_AmmoMax(cl, ammo);
  if (!max) {
    return 0.f;
  }

  const int32_t room = max - cl->inventory[ammo->def.tag];

  return Clampf(room / (float) count, 0.f, 1.f);
}

/**
 * @see g_ai_item.h
 */
float G_Ai_ItemNeed(const GameClient *cl, const GameEntity *other) {

  const GameItem *item = other->item;
  const GameEntity *ent = cl->entity;

  switch (item->def.type) {
    case ITEM_TYPE_HEALTH: {
      const bool boost = item->def.tag == HEALTH_SMALL || item->def.tag == HEALTH_QUAKE2_SMALL ||
                         item->def.tag == HEALTH_MEGA || item->def.tag == HEALTH_QUAKE_MEGA ||
                         item->def.tag == HEALTH_QUAKE2_MEGA;
      const int32_t max = boost ? cl->maxBoostHealth : ent->maxHealth;

      return Clampf((max - ent->health) / (float) Maxi(1, item->def.quantity), 0.f, 1.f);
    }
    case ITEM_TYPE_ARMOR: {
      const GameItem *drop, *give;
      int16_t count;

      if (!G_ResolveArmorPickup(cl, item, &drop, &give, &count) || !give) {
        return 0.f;
      }

      const GameItem *current = G_ClientArmor(cl);
      const float before = current ?
                           cl->inventory[current->def.tag] * G_ArmorInfo(current)->normalProtection :
                           0.f;
      const float after = count * G_ArmorInfo(give)->normalProtection;
      const GameItem *armor = (item->def.tag == ARMOR_SHARD || item->def.tag == ARMOR_QUAKE2_SHARD)
        ? give : item;
      const float full = Maxi(1, item->def.quantity) * G_ArmorInfo(armor)->normalProtection;

      return Clampf((after - before) / full, 0.f, 1.f);
    }
    case ITEM_TYPE_AMMO:
      return G_Ai_AmmoNeed(cl, item, other->count ? other->count : item->def.quantity);
    case ITEM_TYPE_WEAPON:
      if (cl->inventory[item->def.tag]) {
        return G_Ai_AmmoNeed(cl, item->def.ammo ? &gameItems[item->def.ammo] : NULL, other->health);
      }
      return 1.f;
    case ITEM_TYPE_BACKPACK: {
      if (item->def.tag == BACKPACK_QUAKE2) {
        return 1.f;
      }

      float need = 0.f;
      for (GameItemTag tag = AMMO_FIRST; tag < AMMO_LAST; tag++) {
        const int16_t count = other->backpack.ammo[tag - AMMO_FIRST];
        need = Maxf(need, G_Ai_AmmoNeed(cl, &gameItems[tag], count));
      }
      for (GameItemTag tag = WEAPON_FIRST; tag < WEAPON_LAST; tag++) {
        if ((other->backpack.weapons & (1ull << (tag - WEAPON_FIRST))) && !cl->inventory[tag]) {
          return 1.f;
        }
      }
      return need;
    }
    default:
      return 1.f;
  }
}

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
      if (item->def.tag == HEALTH_SMALL || item->def.tag == HEALTH_QUAKE2_SMALL ||
          item->def.tag == HEALTH_MEGA || item->def.tag == HEALTH_QUAKE2_MEGA) {
        return true;
      }

      return cl->entity->health < cl->entity->maxHealth;
    case ITEM_TYPE_ARMOR: {
      const GameItem *drop, *give;
      int16_t count;
      return G_ResolveArmorPickup(cl, item, &drop, &give, &count);
    }
    case ITEM_TYPE_AMMO:
      return inventory[item->def.tag] < G_AmmoMax(cl, item);
    case ITEM_TYPE_WEAPON:
      if (inventory[item->def.tag]) {
        if (g_weaponStay->integer && other->team == NULL) {
          return false;
        }

        if (item->def.ammo) {
          return inventory[item->def.ammo] < G_AmmoMax(cl, &gameItems[item->def.ammo]);
        }

        return false;
      }

      return true;
    case ITEM_TYPE_BACKPACK:
      if (item->def.tag == BACKPACK_QUAKE2) {
        return !inventory[BACKPACK_QUAKE2];
      }

      return G_Ai_ItemNeed(cl, other) > 0.f;
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
      if (flagTeam == team) {
        return other->owner != NULL;
      }

      return G_GetFlag(cl) == NULL;
    }
#endif

    default:
      return true;
  }
}

