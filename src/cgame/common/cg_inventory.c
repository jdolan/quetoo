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

#include "cg_local.h"
#include "bg_item.h"

CGameItem cgItems[ITEM_TOTAL];
CGameWeapon cgWeapons[WEAPON_TOTAL];

/**
 * @brief Initializes the inventory cache: item icons and models, weapon and ammo tags.
 * Called once per map load from `Cg_LoadHudMedia`.
 */
void Cg_InitInventory(void) {

  memset(cgItems, 0, sizeof(cgItems));
  memset(cgWeapons, 0, sizeof(cgWeapons));

  for (GameItemTag t = ITEM_NONE + 1; t < ITEM_TOTAL; t++) {
    if (bgItemDefs[t].model) {
      cgItems[t].model = cgi.LoadModel(bgItemDefs[t].model);
    }
  }

  for (GameItemTag t = WEAPON_FIRST; t < WEAPON_LAST; t++) {
    CGameWeapon *w = &cgWeapons[t - WEAPON_FIRST];
    w->tag = t;
    w->ammoTag = bgItemDefs[t].ammo;
    w->model = cgItems[t].model;
  }
}

/**
 * @brief Returns true if the player has at least one weapon in inventory.
 */
bool Cg_HasWeapon(const PlayerState *ps) {

  for (GameItemTag i = WEAPON_FIRST; i < WEAPON_LAST; i++) {
    if (ps->inventory[i]) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Returns the active weapon index into `cgWeapons[]`, or `WEAPON_SELECT_OFF`.
 * Prefers the weapon being switched to over the one currently equipped.
 */
int16_t Cg_ActiveWeapon(const PlayerState *ps) {

  const int16_t weapon    = (ps->stats[STAT_WEAPON] >> 0) & 0xff;
  const int16_t switching = (ps->stats[STAT_WEAPON] >> 8) & 0xff;

  if (switching >= WEAPON_FIRST && switching < WEAPON_LAST) {
    return switching - WEAPON_FIRST;
  }

  if (weapon >= WEAPON_FIRST && weapon < WEAPON_LAST) {
    return weapon - WEAPON_FIRST;
  }

  return WEAPON_SELECT_OFF;
}

/**
 * @brief Returns the ammo count for the active weapon, or 0 if none.
 */
int16_t Cg_ActiveAmmo(const PlayerState *ps) {

  const int16_t active = Cg_ActiveWeapon(ps);
  if (active == WEAPON_SELECT_OFF) {
    return 0;
  }

  const GameItemTag ammoTag = cgWeapons[active].ammoTag;
  if (!ammoTag) {
    return 0;
  }

  return ps->inventory[ammoTag];
}
